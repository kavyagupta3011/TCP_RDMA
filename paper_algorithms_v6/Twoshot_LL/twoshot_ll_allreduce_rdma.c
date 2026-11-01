// twoshot_ll_allreduce_rdma.c
//
// Two-shot AllReduce (sum) for N nodes with LL-style flags in both phases, written with
// InfiniBand RDMA verbs. The vector is split into N chunks and rank c owns chunk c.
//   Phase 1 (ReduceScatter): every rank sends its slice of chunk c to rank c, and rank c
//   sums the N slices of its chunk.
//   Phase 2 (AllGather): every rank sends its finished chunk to all other ranks.
// Each data write is followed by an epoch flag write on the same QP (see LL/).
//
// v6: a chunk is cut into blocks (at most MAX_BLOCKS, at least MIN_BLOCK floats each) and every
// block has its own flag. Phase 1 posts all blocks at once. As soon as block b has arrived from
// every peer it is summed and sent on (phase 2) while later blocks are still coming in. Phase 2
// writes go straight into the peer's output vector, so nothing is copied on arrival; the round
// ends when every phase 2 flag has been seen. A chunk smaller than MIN_BLOCK is one block.
//
// For peer j, rs_recv[j]/rs_flag[j] receive j's phase 1 data for my chunk (rs_flag[j][b] is the
// flag of block b), and ag_flag[j][b] tells me block b of j's finished chunk is in my output.
//
// Usage:
//   ./twoshot_ll_allreduce_rdma <my_rank> <num_ranks> <base_port> <ip0> ... <ip(N-1)> [numFloats] [iters]
// Example for 3 ranks (one process per node):
//   ./twoshot_ll_allreduce_rdma 0 3 20000 10.1.2.1 10.1.2.2 10.1.2.3

#include "../common/rdma_common.h"

#define TWOSHOT_LL_SEED_BASE 4000
#define MAX_BLOCKS 32
#define MIN_BLOCK 16384  // floats

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

    rdma_use_shared_pd();  // v5: one protection domain for all connections
    RdmaConn** conns = rdma_mesh_connect(my_rank, n, ips, base_port);
    RDMA_CHECK(rdma_pd_is_shared(conns, n, my_rank), "connections do not share a protection domain");
    RdmaConn* any = conns[my_rank == 0 ? 1 : 0];

    float** all_vecs = malloc((size_t)n * sizeof(float*));
    for (int r = 0; r < n; ++r) all_vecs[r] = make_random_vector(M, TWOSHOT_LL_SEED_BASE + r);
    float* input = all_vecs[my_rank];
    float* ref = malloc(M * sizeof(float));
    reference_sum_n(all_vecs, n, ref, M);
    float* output = malloc(M * sizeof(float));

    // Block layout of one chunk.
    size_t bs = (chunk + MAX_BLOCKS - 1) / MAX_BLOCKS;
    if (bs < MIN_BLOCK) bs = MIN_BLOCK;
    if (bs > chunk) bs = chunk;
    int nb = (int)((chunk + bs - 1) / bs);
    if (my_rank == 0) printf("blocks per chunk: %d of up to %zu floats\n", nb, bs);

    float* (*rs_recv)[2] = calloc((size_t)n, sizeof(*rs_recv));  // [peer][round parity]
    int64_t** rs_flag = calloc((size_t)n, sizeof(int64_t*));      // [peer][block]
    int64_t** ag_flag = calloc((size_t)n, sizeof(int64_t*));
    RegionInfo (*peer_rs)[2] = calloc((size_t)n, sizeof(*peer_rs));
    RegionInfo* peer_rsf = calloc((size_t)n, sizeof(RegionInfo));
    RegionInfo* peer_out = calloc((size_t)n, sizeof(RegionInfo));  // peer's output vector
    RegionInfo* peer_agf = calloc((size_t)n, sizeof(RegionInfo));

    // The input and output vectors are registered once (all connections share one protection
    // domain), and so is the epoch word every flag write is sent from.
    struct ibv_mr* in_mr = rdma_reg_existing(any, input, M * sizeof(float));
    struct ibv_mr* out_mr = rdma_reg_existing(any, output, M * sizeof(float));
    struct ibv_mr* epoch_mr;
    int64_t* epoch_send = rdma_reg_buffer(any, sizeof(int64_t), &epoch_mr);

    for (int j = 0; j < n; ++j) {
        if (j == my_rank) continue;
        struct ibv_mr *rs_mr[2], *rsf_mr, *agf_mr;
        for (int b = 0; b < 2; ++b)
            rs_recv[j][b] = rdma_reg_buffer(conns[j], chunk * sizeof(float), &rs_mr[b]);
        rs_flag[j] = rdma_reg_buffer(conns[j], (size_t)nb * sizeof(int64_t), &rsf_mr);
        ag_flag[j] = rdma_reg_buffer(conns[j], (size_t)nb * sizeof(int64_t), &agf_mr);

        uint32_t csz = (uint32_t)(chunk * sizeof(float));
        RegionInfo local[5] = {
            {(uint64_t)(uintptr_t)rs_recv[j][0], rs_mr[0]->rkey, csz},
            {(uint64_t)(uintptr_t)rs_recv[j][1], rs_mr[1]->rkey, csz},
            {(uint64_t)(uintptr_t)rs_flag[j], rsf_mr->rkey, (uint32_t)(nb * sizeof(int64_t))},
            {(uint64_t)(uintptr_t)output, out_mr->rkey, (uint32_t)(M * sizeof(float))},
            {(uint64_t)(uintptr_t)ag_flag[j], agf_mr->rkey, (uint32_t)(nb * sizeof(int64_t))},
        };
        RegionInfo remote[5];
        rdma_exchange_regions(conns[j], local, 5, remote);
        peer_rs[j][0] = remote[0];
        peer_rs[j][1] = remote[1];
        peer_rsf[j] = remote[2];
        peer_out[j] = remote[3];
        peer_agf[j] = remote[4];
    }

    double* lat = malloc((size_t)(iters > 0 ? iters : 1) * sizeof(double));

    // Barrier so all ranks start together (keeps connection-setup skew out of the first timed round).
    for (int j = 0; j < n; ++j)
        if (j != my_rank) rdma_barrier(conns[j]);

    int ok = 1, bad_rounds = 0;
    for (int it = -1; it < iters; ++it) {
        int64_t epoch = it + 2;
        int par = (int)(epoch & 1);
        if (it >= 0) {  // per-round check stamps in the first and last element of the input
            float s = round_stamp(epoch, my_rank);
            input[0] = input[M - 1] = s;
        }
        double t0 = now_us();

        // Phase 1 (ReduceScatter): post my slice of every other rank's chunk, block by block.
        // Only the last flag of the phase is signaled; RC completes in order, so it covers
        // every earlier write on that connection.
        *epoch_send = epoch;
        for (int b = 0; b < nb; ++b) {
            size_t off = (size_t)b * bs;
            size_t len = chunk - off < bs ? chunk - off : bs;
            for (int j = 0; j < n; ++j) {
                if (j == my_rank) continue;
                rdma_post_write_ex(conns[j], input + (size_t)j * chunk + off, in_mr->lkey,
                                   len * sizeof(float), peer_rs[j][par].addr + off * sizeof(float),
                                   peer_rs[j][par].rkey, 0);
                rdma_post_write_ex(conns[j], epoch_send, epoch_mr->lkey, sizeof(int64_t),
                                   peer_rsf[j].addr + (size_t)b * sizeof(int64_t),
                                   peer_rsf[j].rkey, b == nb - 1);
            }
        }

        // For each block: wait for it from every peer, sum it into my part of output, and send
        // it on to everyone (phase 2) straight away.
        for (int b = 0; b < nb; ++b) {
            size_t off = (size_t)b * bs;
            size_t len = chunk - off < bs ? chunk - off : bs;
            const float* srcs[n];
            int ns = 0;
            for (int j = 0; j < n; ++j) {
                if (j == my_rank) continue;
                volatile int64_t* poll = (volatile int64_t*)&rs_flag[j][b];
                while (*poll < epoch) { /* spin */
                }
                srcs[ns++] = rs_recv[j][par] + off;
            }
            __atomic_thread_fence(__ATOMIC_ACQUIRE);
            float* mine = output + (size_t)my_rank * chunk + off;
            reduce_blocked(mine, input + (size_t)my_rank * chunk + off, srcs, ns, len);
            for (int j = 0; j < n; ++j) {
                if (j == my_rank) continue;
                rdma_post_write_ex(conns[j], mine, out_mr->lkey, len * sizeof(float),
                                   peer_out[j].addr +
                                       ((size_t)my_rank * chunk + off) * sizeof(float),
                                   peer_out[j].rkey, 0);
                rdma_post_write_ex(conns[j], epoch_send, epoch_mr->lkey, sizeof(int64_t),
                                   peer_agf[j].addr + (size_t)b * sizeof(int64_t),
                                   peer_agf[j].rkey, b == nb - 1);
            }
        }

        // Phase 2 arrival: the peers' finished chunks are written into output directly; wait
        // for every block's flag.
        for (int j = 0; j < n; ++j) {
            if (j == my_rank) continue;
            for (int b = 0; b < nb; ++b) {
                volatile int64_t* poll = (volatile int64_t*)&ag_flag[j][b];
                while (*poll < epoch) { /* spin */
                }
            }
        }
        __atomic_thread_fence(__ATOMIC_ACQUIRE);
        // Two signaled writes per connection are outstanding (end of phase 1, end of phase 2).
        for (int j = 0; j < n; ++j) {
            if (j == my_rank) continue;
            rdma_wait_write(conns[j]);
            rdma_wait_write(conns[j]);
        }

        double elapsed = now_us() - t0;
        if (it == -1) {
            ok = verify(output, ref, M, 1e-3f);
            printf("Correctness check vs. CPU reference: %s\n", ok ? "PASSED" : "FAILED");
        } else {
            lat[it] = elapsed;
            bad_rounds += !round_check(output, M, epoch, n);
        }
    }
    printf("Correctness check, every timed round (first/last element): %s\n",
           bad_rounds ? "FAILED" : "PASSED");
    rdma_print_stats("two-shot LL", lat, iters, n);
    free(lat);

    // Barrier so no rank closes its connections while a peer is still in its last round.
    for (int j = 0; j < n; ++j)
        if (j != my_rank) rdma_barrier(conns[j]);

    rdma_mesh_close(conns, n, my_rank);
    for (int r = 0; r < n; ++r) free(all_vecs[r]);
    free(all_vecs);
    free(ref);
    free(output);
    free(rs_recv);
    free(rs_flag);
    free(ag_flag);
    free(peer_rs);
    free(peer_rsf);
    free(peer_out);
    free(peer_agf);
    free(ips);
    return ok ? 0 : 1;
}
