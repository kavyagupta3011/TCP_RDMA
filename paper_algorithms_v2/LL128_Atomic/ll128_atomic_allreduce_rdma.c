// ll128_atomic_allreduce_rdma.c
//
// Two-shot AllReduce (sum) for N nodes where an RDMA atomic add is the completion signal
// (Section IV.B.4 of the paper, arXiv:2607.16100). In the paper this runs on GPUs, where the
// atomic add sums the floating-point data itself. InfiniBand atomics only work on 64-bit
// integers, so here the data goes by an ordinary RDMA WRITE, the atomic is only the counter that
// says the data has arrived, and the CPU does the sum. The chunk layout is the same as in
// Twoshot_LL/.
//
// Every peer adds 1 to each of my counters (one counter per phase) once per round, so a counter
// only grows and never needs resetting. After round `it` (the correctness round is it = -1) it
// equals (it+2)*(num_ranks-1); that is `expect` in the code. The data WRITE is posted before the
// atomic on the same QP, so RC ordering guarantees the data is there when the counter shows it.
//
// Usage:
//   ./ll128_atomic_allreduce_rdma <my_rank> <num_ranks> <base_port> <ip0> ... <ip(N-1)> [numFloats] [iters]
// No MPI -- launch once per rank by hand. Example for 3 ranks:
//   node A: ./ll128_atomic_allreduce_rdma 0 3 20000 10.1.2.1 10.1.2.2 10.1.2.3
//   node B: ./ll128_atomic_allreduce_rdma 1 3 20000 10.1.2.1 10.1.2.2 10.1.2.3
//   node C: ./ll128_atomic_allreduce_rdma 2 3 20000 10.1.2.1 10.1.2.2 10.1.2.3

// Changes since the first version (paper_algorithms/):
// * Writes are posted non-blocking (rdma_post_write*), small ones inline, to
//   every peer back to back; completions are collected once, after the
//   reduce, instead of one blocking NIC round trip per peer in sequence.
//   The data write is posted unsignaled and only the trailing flag/atomic is
//   signaled (RC completes in order, so that one completion covers both).
// * Fix: receive buffers are now double-buffered by round parity. Before, one
//   buffer was reused every round, so a fast peer's next-round write could
//   overwrite data still being read (invisible to verification because every
//   round used identical input).
// * Fix: flag/counter waits use `<` instead of `!=` -- they only ever increase,
//   and `!=` would spin forever if a peer is already one round ahead.
// * Per-iteration timing (min/median/p99/max), a start barrier and an end
//   barrier, as in Sentinel/ and Twoshot_Sentinel/.

#include "../common/rdma_common.h"

#define LL128_SEED_BASE 6000

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

    printf(
        "Two-shot LL128 Atomic AllReduce (RDMA verbs, N-node): M=%zu floats, %d chunks of %zu, "
        "rank=%d/%d\n",
        M, n, chunk, my_rank, n);

    RdmaConn** conns = rdma_mesh_connect(my_rank, n, ips, base_port);

    // This algorithm REQUIRES hardware RDMA atomics. Some adapters/firmware (including the
    // ConnectX-3 on this cluster, measured with Atomic_Probe/) report atomic_cap = NONE, and
    // every atomic post then fails with EINVAL. Every rank sees the same adapter model, so all
    // ranks take this branch together and nobody is left waiting.
    {
        struct ibv_device_attr dattr;
        memset(&dattr, 0, sizeof(dattr));
        int peer0 = (my_rank == 0) ? 1 : 0;
        if (ibv_query_device(conns[peer0]->cm_id->verbs, &dattr) == 0 &&
            dattr.atomic_cap == IBV_ATOMIC_NONE) {
            printf(
                "SKIPPED: this adapter reports atomic_cap=NONE (no RDMA atomics); LL128_Atomic "
                "cannot run here. See results/ and Atomic_Probe/.\n");
            rdma_mesh_close(conns, n, my_rank);
            free(ips);
            return 2;
        }
    }

    float** all_vecs = malloc((size_t)n * sizeof(float*));
    for (int r = 0; r < n; ++r) all_vecs[r] = make_random_vector(M, LL128_SEED_BASE + r);
    float* input = all_vecs[my_rank];
    float* ref = malloc(M * sizeof(float));
    reference_sum_n(all_vecs, n, ref, M);
    float* output = malloc(M * sizeof(float));

    float* (*rs_recv)[2] = calloc((size_t)n, sizeof(*rs_recv));  // [peer][round parity]
    float* (*ag_recv)[2] = calloc((size_t)n, sizeof(*ag_recv));
    int64_t** rs_ctr = calloc((size_t)n, sizeof(int64_t*));  // starts at 0, monotonic forever
    int64_t** ag_ctr = calloc((size_t)n, sizeof(int64_t*));
    RegionInfo (*peer_rs)[2] = calloc((size_t)n, sizeof(*peer_rs));
    RegionInfo (*peer_ag)[2] = calloc((size_t)n, sizeof(*peer_ag));
    RegionInfo* peer_rsc = calloc((size_t)n, sizeof(RegionInfo));
    RegionInfo* peer_agc = calloc((size_t)n, sizeof(RegionInfo));

    float** phase1_src = calloc((size_t)n, sizeof(float*));
    struct ibv_mr** phase1_src_mr = calloc((size_t)n, sizeof(struct ibv_mr*));
    float** reduced = calloc((size_t)n, sizeof(float*));
    struct ibv_mr** reduced_mr = calloc((size_t)n, sizeof(struct ibv_mr*));

    for (int j = 0; j < n; ++j) {
        if (j == my_rank) continue;
        struct ibv_mr *rs_mr[2], *ag_mr[2], *rsc_mr, *agc_mr;
        for (int b = 0; b < 2; ++b) {
            rs_recv[j][b] = rdma_reg_buffer(conns[j], chunk * sizeof(float), &rs_mr[b]);
            ag_recv[j][b] = rdma_reg_buffer(conns[j], chunk * sizeof(float), &ag_mr[b]);
        }
        rs_ctr[j] = rdma_reg_buffer(conns[j], sizeof(int64_t), &rsc_mr);
        ag_ctr[j] = rdma_reg_buffer(conns[j], sizeof(int64_t), &agc_mr);

        phase1_src[j] = rdma_reg_buffer(conns[j], chunk * sizeof(float), &phase1_src_mr[j]);
        memcpy(phase1_src[j], input + (size_t)j * chunk, chunk * sizeof(float));
        reduced[j] = rdma_reg_buffer(conns[j], chunk * sizeof(float), &reduced_mr[j]);

        uint32_t csz = (uint32_t)(chunk * sizeof(float));
        RegionInfo local[6] = {
            {(uint64_t)(uintptr_t)rs_recv[j][0], rs_mr[0]->rkey, csz},
            {(uint64_t)(uintptr_t)rs_recv[j][1], rs_mr[1]->rkey, csz},
            {(uint64_t)(uintptr_t)rs_ctr[j], rsc_mr->rkey, sizeof(int64_t)},
            {(uint64_t)(uintptr_t)ag_recv[j][0], ag_mr[0]->rkey, csz},
            {(uint64_t)(uintptr_t)ag_recv[j][1], ag_mr[1]->rkey, csz},
            {(uint64_t)(uintptr_t)ag_ctr[j], agc_mr->rkey, sizeof(int64_t)},
        };
        RegionInfo remote[6];
        rdma_exchange_regions(conns[j], local, 6, remote);
        peer_rs[j][0] = remote[0];
        peer_rs[j][1] = remote[1];
        peer_rsc[j] = remote[2];
        peer_ag[j][0] = remote[3];
        peer_ag[j][1] = remote[4];
        peer_agc[j] = remote[5];
    }

    double* lat = malloc((size_t)(iters > 0 ? iters : 1) * sizeof(double));

    // Start all ranks together so the first timed round doesn't absorb
    // connection-setup skew between processes.
    for (int j = 0; j < n; ++j)
        if (j != my_rank) rdma_barrier(conns[j]);

    int ok = 1;
    for (int it = -1; it < iters; ++it) {
        // FIX: each peer increments ITS OWN per-connection counter (rs_ctr[j]) by 1 per round,
        // so after round `it` that counter equals it+2. The old (it+2)*(n-1) is only equal at
        // n==2 and would spin forever at n>=3.
        int64_t expect = it + 2;
        int par = (int)(expect & 1);
        double t0 = now_us();

        // Phase 1 (ReduceScatter): data WRITE (unsignaled) then atomic +1 (signaled), no flag.
        for (int j = 0; j < n; ++j) {
            if (j == my_rank) continue;
            rdma_post_write_ex(conns[j], phase1_src[j], phase1_src_mr[j]->lkey, chunk * sizeof(float),
                               peer_rs[j][par].addr, peer_rs[j][par].rkey, 0);
            rdma_post_atomic_add(conns[j], peer_rsc[j].addr, peer_rsc[j].rkey, 1);
        }
        for (int j = 0; j < n; ++j) {
            if (j == my_rank) continue;
            volatile int64_t* poll = (volatile int64_t*)rs_ctr[j];
            while (*poll < expect) { /* spin */
            }
        }
        __atomic_thread_fence(__ATOMIC_ACQUIRE);
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
        for (int j = 0; j < n; ++j)
            if (j != my_rank) rdma_wait_atomic(conns[j]);

        // Phase 2 (AllGather): same pattern, broadcasting the completed chunk.
        for (int j = 0; j < n; ++j) {
            if (j == my_rank) continue;
            rdma_post_write_ex(conns[j], reduced[j], reduced_mr[j]->lkey, chunk * sizeof(float),
                               peer_ag[j][par].addr, peer_ag[j][par].rkey, 0);
            rdma_post_atomic_add(conns[j], peer_agc[j].addr, peer_agc[j].rkey, 1);
        }
        for (int j = 0; j < n; ++j) {
            if (j == my_rank) continue;
            volatile int64_t* poll = (volatile int64_t*)ag_ctr[j];
            while (*poll < expect) { /* spin */
            }
            __atomic_thread_fence(__ATOMIC_ACQUIRE);
            memcpy(output + (size_t)j * chunk, ag_recv[j][par], chunk * sizeof(float));
        }
        for (int j = 0; j < n; ++j)
            if (j != my_rank) rdma_wait_atomic(conns[j]);

        double elapsed = now_us() - t0;
        if (it == -1) {
            ok = verify(output, ref, M, 1e-3f);
            printf("Correctness check vs. CPU reference: %s\n", ok ? "PASSED" : "FAILED");
        } else {
            lat[it] = elapsed;
        }
    }
    rdma_print_stats("two-shot LL128 Atomic", lat, iters, n);
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
    free(rs_ctr);
    free(ag_ctr);
    free(peer_rs);
    free(peer_ag);
    free(peer_rsc);
    free(peer_agc);
    free(phase1_src);
    free(phase1_src_mr);
    free(reduced);
    free(reduced_mr);
    free(ips);
    return ok ? 0 : 1;
}
