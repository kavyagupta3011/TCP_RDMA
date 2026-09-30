// baseline_barrier_rdma.c
//
// A "conventional" one-shot, N-NODE AllReduce-sum that DOES use an explicit
// synchronization barrier between the push and the reduce step -- the
// design the paper's Section III-B examines and then eliminates (Fig. 3),
// over real InfiniBand RDMA verbs. See ../README.md and the original CUDA
// project's Baseline_Barrier/README.md for the full rationale.
//
// Mechanism: every rank writes its full input directly into every other
// rank's per-peer scratch buffer (plain RDMA WRITE, no flag, no polling for
// arrival at all). Correctness then depends ENTIRELY on rdma_barrier() -- a
// genuine two-sided SEND + RECV round trip -- confirming, on EVERY peer
// connection, that both sides' writes have landed before either side reads
// its scratch buffers. With N ranks that's N-1 sequential barrier round
// trips per round (one per connection) -- deliberately not parallelized,
// since the whole point of this file is to be the "before" number the
// other six programs' barrier-free "after" numbers get compared against;
// making the barrier itself faster would undercut that comparison.
//
// Usage:
//   ./baseline_barrier_rdma <my_rank> <num_ranks> <base_port> <ip0> ... <ip(N-1)> [numFloats] [iters]
// No MPI -- launch once per rank by hand. Example for 3 ranks:
//   node A: ./baseline_barrier_rdma 0 3 20000 10.1.2.1 10.1.2.2 10.1.2.3
//   node B: ./baseline_barrier_rdma 1 3 20000 10.1.2.1 10.1.2.2 10.1.2.3
//   node C: ./baseline_barrier_rdma 2 3 20000 10.1.2.1 10.1.2.2 10.1.2.3

#include "../common/rdma_common.h"

#define BASELINE_SEED_BASE 7000

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

    printf("Baseline (explicit RDMA barrier) one-shot AllReduce (N-node): M=%zu floats, rank=%d/%d\n",
           M, my_rank, n);

    RdmaConn** conns = rdma_mesh_connect(my_rank, n, ips, base_port);

    float** all_vecs = malloc((size_t)n * sizeof(float*));
    for (int r = 0; r < n; ++r) all_vecs[r] = make_random_vector(M, BASELINE_SEED_BASE + r);
    float* input = all_vecs[my_rank];
    float* ref = malloc(M * sizeof(float));
    reference_sum_n(all_vecs, n, ref, M);
    float* output = malloc(M * sizeof(float));

    float** data_recv = calloc((size_t)n, sizeof(float*));
    RegionInfo* peer_data = calloc((size_t)n, sizeof(RegionInfo));
    float** input_reg = calloc((size_t)n, sizeof(float*));
    struct ibv_mr** input_mr = calloc((size_t)n, sizeof(struct ibv_mr*));

    for (int j = 0; j < n; ++j) {
        if (j == my_rank) continue;
        struct ibv_mr* data_recv_mr;
        data_recv[j] = rdma_reg_buffer(conns[j], M * sizeof(float), &data_recv_mr);
        input_reg[j] = rdma_reg_buffer(conns[j], M * sizeof(float), &input_mr[j]);
        memcpy(input_reg[j], input, M * sizeof(float));

        RegionInfo local[1] = {
            {(uint64_t)(uintptr_t)data_recv[j], data_recv_mr->rkey, (uint32_t)(M * sizeof(float))}};
        RegionInfo remote[1];
        rdma_exchange_regions(conns[j], local, 1, remote);
        peer_data[j] = remote[0];
    }

    int ok = 1;
    double total_us = 0;
    for (int it = -1; it < iters; ++it) {
        double t0 = now_us();

        for (int j = 0; j < n; ++j) {
            if (j == my_rank) continue;
            rdma_write(conns[j], input_reg[j], input_mr[j]->lkey, M * sizeof(float), peer_data[j].addr,
                       peer_data[j].rkey);
        }
        // ---- THE BARRIER ----
        // A genuine two-sided round trip PER PEER CONNECTION: only once
        // every one of these returns is any side allowed to assume every
        // peer's write has landed.
        for (int j = 0; j < n; ++j) {
            if (j == my_rank) continue;
            rdma_barrier(conns[j]);
        }

        for (size_t i = 0; i < M; ++i) {
            float sum = input[i];
            for (int j = 0; j < n; ++j) {
                if (j == my_rank) continue;
                sum += data_recv[j][i];
            }
            output[i] = sum;
        }

        double elapsed = now_us() - t0;
        if (it == -1) {
            ok = verify(output, ref, M, 1e-3f);
            printf("Correctness check vs. CPU reference: %s\n", ok ? "PASSED" : "FAILED");
        } else {
            total_us += elapsed;
        }
    }
    printf("Average barrier-based one-shot AllReduce latency over %d iters (%d ranks): %.3f us\n",
           iters, n, total_us / iters);
    printf(
        "Compare this against LL/, Sentinel/, Twoshot_LL/, Twoshot_Sentinel/ and "
        "LL128_Atomic/ at the same numFloats/numFloats-per-rank to see the barrier-free "
        "speedup directly.\n");

    rdma_mesh_close(conns, n, my_rank);
    for (int r = 0; r < n; ++r) free(all_vecs[r]);
    free(all_vecs);
    free(ref);
    free(output);
    free(data_recv);
    free(peer_data);
    free(input_reg);
    free(input_mr);
    free(ips);
    return ok ? 0 : 1;
}
