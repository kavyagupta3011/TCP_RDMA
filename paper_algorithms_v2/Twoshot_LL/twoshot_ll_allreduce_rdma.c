// twoshot_ll_allreduce_rdma.c
//
// Two-shot, N-NODE AllReduce-sum using LL-style synchronization for both
// phases, Table I row "Two-shot (LL)" of "Every us Matters: Achieving Near
// Speed-of-Light Latency in GPU Collectives" (arXiv:2607.16100), over real
// InfiniBand RDMA verbs. See ../README.md for why this version exists, and
// LL/ll_allreduce_rdma.c for the RC-ordering trick (data WRITE, then a
// trailing flag WRITE, same QP) both phases here reuse, and for the
// per-peer-registration detail (every connection has its own protection
// domain).
//
// General N ReduceScatter + AllGather: the M-element vector splits into N
// chunks, one per rank -- rank c "owns" chunk c (index range
// [c*chunk, (c+1)*chunk)).
//
//   Phase 1 (ReduceScatter): every rank r sends, to EVERY other rank c, its
//   own local slice of chunk c. Once rank c has received chunk c's slice
//   from all N-1 other ranks, it sums those with its OWN slice of chunk c
//   and ends up as the sole holder of chunk c, fully reduced.
//
//   Phase 2 (AllGather): every rank c broadcasts its now-complete chunk c
//   to all N-1 other ranks. Once every rank has received every chunk (its
//   own, computed locally in Phase 1, plus N-1 received in Phase 2), every
//   rank holds the full result.
//
// This is the real N-ary form Table I's "two-shot" row describes -- at N=2
// it specializes to exactly the earlier pairwise version (see
// ../README.md's "one-shot vs. two-shot at N=2" note); the traffic
// reduction two-shot is usually chosen for only shows up once N>2, which is
// exactly what this generalization now demonstrates.
//
// Per-peer buffer naming: for peer j, `rs_recv[j]`/`rs_flag[j]` is MY
// receive slot for peer j's Phase-1 contribution to MY chunk; `ag_recv[j]`/
// `ag_flag[j]` is MY receive slot for peer j's completed chunk in Phase 2.
// Each connection's rdma_exchange_regions call swaps these addresses
// symmetrically, so `peer_rs_data[j]` ends up meaning "the address I write
// MY Phase-1 contribution to rank j's chunk into" -- i.e. rank j's
// rs_recv[my_rank].
//
// Usage:
//   ./twoshot_ll_allreduce_rdma <my_rank> <num_ranks> <base_port> <ip0> ... <ip(N-1)> [numFloats] [iters]
// No MPI -- launch once per rank by hand. Example for 3 ranks:
//   node A: ./twoshot_ll_allreduce_rdma 0 3 20000 10.1.2.1 10.1.2.2 10.1.2.3
//   node B: ./twoshot_ll_allreduce_rdma 1 3 20000 10.1.2.1 10.1.2.2 10.1.2.3
//   node C: ./twoshot_ll_allreduce_rdma 2 3 20000 10.1.2.1 10.1.2.2 10.1.2.3

// ---- v3 optimization notes (see results/ for measurements) ----------------
// * Writes are posted non-blocking (rdma_post_write*), small ones INLINE, to
//   every peer back to back; completions are collected once, after the
//   reduce, instead of one blocking NIC round trip per peer in sequence.
//   The DATA write is posted unsignaled and only the trailing flag/atomic is
//   signaled (RC completes in order, so that one completion covers both).
// * FIX: receive buffers are now DOUBLE-BUFFERED by round parity. Before, one
//   buffer was reused every round, so a fast peer's next-round write could
//   overwrite data still being read (invisible to verification because every
//   round used identical input).
// * FIX: flag/counter waits use `<` instead of `!=` -- they only ever increase,
//   and `!=` would spin forever if a peer is already one round ahead.
// * Per-iteration timing (min/median/p99/max), a start barrier and an end
//   barrier, as in Sentinel/ and Twoshot_Sentinel/.
// ---------------------------------------------------------------------------

#include "../common/rdma_common.h"

#define TWOSHOT_LL_SEED_BASE 4000

static void usage(const char* prog) {
    fprintf(stderr,
            "Usage: %s <my_rank> <num_ranks> <base_port> <ip0> <ip1> ... <ip(N-1)> "
            "[numFloats] [iters]\n"
            "No MPI -- launch once per rank by hand. Example for 3 ranks:\n"
            "  node A: %s 0 3 20000 10.1.2.1 10.1.2.2 10.1.2.3\n"
            "  node B: %s 1 3 20000 10.1.2.1 10.1.2.2 10.1.2.3\n"
            "  node C: %s 2 3 20000 10.1.2.1 10.1.2.2 10.1.2.3\n",
            prog, prog, prog, prog);
}

int main(int argc, char** argv) {
    if (argc < 5) {
        usage(argv[0]);
        return 1;
    }
    int my_rank = atoi(argv[1]);
    int n = atoi(argv[2]);
    int base_port = atoi(argv[3]);
    if (n < 2 || my_rank < 0 || my_rank >= n) {
        fprintf(stderr, "num_ranks must be >= 2 and 0 <= my_rank < num_ranks\n");
        return 1;
    }
    if (argc < 4 + n) {
        usage(argv[0]);
        return 1;
    }
    const char** ips = malloc((size_t)n * sizeof(char*));
    for (int i = 0; i < n; ++i) ips[i] = argv[4 + i];
    int arg_idx = 4 + n;
    size_t M = (argc > arg_idx) ? (size_t)atol(argv[arg_idx]) : (1u << 16);
    arg_idx++;
    int iters = (argc > arg_idx) ? atoi(argv[arg_idx]) : 100;

    if (M % (size_t)n != 0) M += n - (M % n);
    size_t chunk = M / n;

    printf("Two-shot LL AllReduce (RDMA verbs, N-node): M=%zu floats, %d chunks of %zu, rank=%d/%d\n",
           M, n, chunk, my_rank, n);

    RdmaConn** conns = rdma_mesh_connect(my_rank, n, ips, base_port);

    float** all_vecs = malloc((size_t)n * sizeof(float*));
    for (int r = 0; r < n; ++r) all_vecs[r] = make_random_vector(M, TWOSHOT_LL_SEED_BASE + r);
    float* input = all_vecs[my_rank];
    float* ref = malloc(M * sizeof(float));
    reference_sum_n(all_vecs, n, ref, M);
    float* output = malloc(M * sizeof(float));

    float* (*rs_recv)[2] = calloc((size_t)n, sizeof(*rs_recv));  // [peer][round parity]
    float* (*ag_recv)[2] = calloc((size_t)n, sizeof(*ag_recv));
    int64_t** rs_flag = calloc((size_t)n, sizeof(int64_t*));
    int64_t** ag_flag = calloc((size_t)n, sizeof(int64_t*));
    RegionInfo (*peer_rs)[2] = calloc((size_t)n, sizeof(*peer_rs));
    RegionInfo (*peer_ag)[2] = calloc((size_t)n, sizeof(*peer_ag));
    RegionInfo* peer_rsf = calloc((size_t)n, sizeof(RegionInfo));
    RegionInfo* peer_agf = calloc((size_t)n, sizeof(RegionInfo));

    // Per-peer registered copies (separate PD per connection): my chunk-c
    // slice to send in Phase 1, and my own completed chunk to broadcast in
    // Phase 2, plus the epoch value.
    float** phase1_src = calloc((size_t)n, sizeof(float*));
    struct ibv_mr** phase1_src_mr = calloc((size_t)n, sizeof(struct ibv_mr*));
    float** reduced = calloc((size_t)n, sizeof(float*));
    struct ibv_mr** reduced_mr = calloc((size_t)n, sizeof(struct ibv_mr*));
    int64_t** epoch_send = calloc((size_t)n, sizeof(int64_t*));
    struct ibv_mr** epoch_mr = calloc((size_t)n, sizeof(struct ibv_mr*));

    for (int j = 0; j < n; ++j) {
        if (j == my_rank) continue;
        struct ibv_mr *rs_mr[2], *ag_mr[2], *rsf_mr, *agf_mr;
        for (int b = 0; b < 2; ++b) {
            rs_recv[j][b] = rdma_reg_buffer(conns[j], chunk * sizeof(float), &rs_mr[b]);
            ag_recv[j][b] = rdma_reg_buffer(conns[j], chunk * sizeof(float), &ag_mr[b]);
        }
        rs_flag[j] = rdma_reg_buffer(conns[j], sizeof(int64_t), &rsf_mr);
        ag_flag[j] = rdma_reg_buffer(conns[j], sizeof(int64_t), &agf_mr);

        phase1_src[j] = rdma_reg_buffer(conns[j], chunk * sizeof(float), &phase1_src_mr[j]);
        memcpy(phase1_src[j], input + (size_t)j * chunk, chunk * sizeof(float));
        reduced[j] = rdma_reg_buffer(conns[j], chunk * sizeof(float), &reduced_mr[j]);
        epoch_send[j] = rdma_reg_buffer(conns[j], sizeof(int64_t), &epoch_mr[j]);

        uint32_t csz = (uint32_t)(chunk * sizeof(float));
        RegionInfo local[6] = {
            {(uint64_t)(uintptr_t)rs_recv[j][0], rs_mr[0]->rkey, csz},
            {(uint64_t)(uintptr_t)rs_recv[j][1], rs_mr[1]->rkey, csz},
            {(uint64_t)(uintptr_t)rs_flag[j], rsf_mr->rkey, sizeof(int64_t)},
            {(uint64_t)(uintptr_t)ag_recv[j][0], ag_mr[0]->rkey, csz},
            {(uint64_t)(uintptr_t)ag_recv[j][1], ag_mr[1]->rkey, csz},
            {(uint64_t)(uintptr_t)ag_flag[j], agf_mr->rkey, sizeof(int64_t)},
        };
        RegionInfo remote[6];
        rdma_exchange_regions(conns[j], local, 6, remote);
        peer_rs[j][0] = remote[0];
        peer_rs[j][1] = remote[1];
        peer_rsf[j] = remote[2];
        peer_ag[j][0] = remote[3];
        peer_ag[j][1] = remote[4];
        peer_agf[j] = remote[5];
    }

    double* lat = malloc((size_t)(iters > 0 ? iters : 1) * sizeof(double));

    // Start all ranks together so the first timed round doesn't absorb
    // connection-setup skew between processes.
    for (int j = 0; j < n; ++j)
        if (j != my_rank) rdma_barrier(conns[j]);

    int ok = 1;
    for (int it = -1; it < iters; ++it) {
        int64_t epoch = it + 2;
        int par = (int)(epoch & 1);
        double t0 = now_us();

        // Phase 1 (ReduceScatter): send my slice of every other rank's
        // chunk, then locally reduce my own chunk once all N-1 arrive.
        for (int j = 0; j < n; ++j) {
            if (j == my_rank) continue;
            *epoch_send[j] = epoch;
            rdma_post_write_ex(conns[j], phase1_src[j], phase1_src_mr[j]->lkey, chunk * sizeof(float),
                               peer_rs[j][par].addr, peer_rs[j][par].rkey, 0);
            rdma_post_write(conns[j], epoch_send[j], epoch_mr[j]->lkey, sizeof(int64_t),
                            peer_rsf[j].addr, peer_rsf[j].rkey);
        }
        for (int j = 0; j < n; ++j) {
            if (j == my_rank) continue;
            volatile int64_t* poll = (volatile int64_t*)rs_flag[j];
            while (*poll < epoch) { /* spin */
            }
        }
        __atomic_thread_fence(__ATOMIC_ACQUIRE);
        // reduced[] holds a per-peer registered COPY of my finished chunk
        // (one per outbound connection, since each has its own protection
        // domain) -- fill every copy with the same freshly-reduced data.
        for (size_t i = 0; i < chunk; ++i) {
            float sum = input[(size_t)my_rank * chunk + i];
            for (int j = 0; j < n; ++j) {
                if (j == my_rank) continue;
                sum += rs_recv[j][par][i];
            }
            for (int j = 0; j < n; ++j) {
                if (j == my_rank) continue;
                reduced[j][i] = sum;
            }
            output[(size_t)my_rank * chunk + i] = sum;
        }
        // Phase-1 sends finished long ago (we just received every peer's data); collect them
        // so only one signaled write per connection is ever outstanding.
        for (int j = 0; j < n; ++j)
            if (j != my_rank) rdma_wait_write(conns[j]);

        // Phase 2 (AllGather): broadcast my completed chunk to everyone.
        for (int j = 0; j < n; ++j) {
            if (j == my_rank) continue;
            rdma_post_write_ex(conns[j], reduced[j], reduced_mr[j]->lkey, chunk * sizeof(float),
                               peer_ag[j][par].addr, peer_ag[j][par].rkey, 0);
            rdma_post_write(conns[j], epoch_send[j], epoch_mr[j]->lkey, sizeof(int64_t),
                            peer_agf[j].addr, peer_agf[j].rkey);
        }
        for (int j = 0; j < n; ++j) {
            if (j == my_rank) continue;
            volatile int64_t* poll = (volatile int64_t*)ag_flag[j];
            while (*poll < epoch) { /* spin */
            }
            __atomic_thread_fence(__ATOMIC_ACQUIRE);
            memcpy(output + (size_t)j * chunk, ag_recv[j][par], chunk * sizeof(float));
        }
        for (int j = 0; j < n; ++j)
            if (j != my_rank) rdma_wait_write(conns[j]);

        double elapsed = now_us() - t0;
        if (it == -1) {
            ok = verify(output, ref, M, 1e-3f);
            printf("Correctness check vs. CPU reference: %s\n", ok ? "PASSED" : "FAILED");
        } else {
            lat[it] = elapsed;
        }
    }
    rdma_print_stats("two-shot LL", lat, iters, n);
    free(lat);

    // Don't tear down connections while a peer may still be finishing its last round.
    for (int j = 0; j < n; ++j)
        if (j != my_rank) rdma_barrier(conns[j]);

    rdma_mesh_close(conns, n, my_rank);
    for (int r = 0; r < n; ++r) free(all_vecs[r]);
    free(all_vecs);
    free(ref);
    free(output);
    free(rs_recv);
    free(ag_recv);
    free(rs_flag);
    free(ag_flag);
    free(peer_rs);
    free(peer_ag);
    free(peer_rsf);
    free(peer_agf);
    free(phase1_src);
    free(phase1_src_mr);
    free(reduced);
    free(reduced_mr);
    free(epoch_send);
    free(epoch_mr);
    free(ips);
    return ok ? 0 : 1;
}
