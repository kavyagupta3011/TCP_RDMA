// baseline_barrier_rdma.c
//
// Baseline AllReduce (sum) for N nodes. Every rank writes its whole input into every
// other rank's receive buffer, then an explicit two-sided barrier (rdma_barrier) makes
// sure the writes have landed before anyone reads. This is the "with barrier" case the
// other algorithms are compared against.
//
// Usage:
//   ./baseline_barrier_rdma <my_rank> <num_ranks> <base_port> <ip0> ... <ip(N-1)> [numFloats] [iters]
// Example for 3 ranks (one process per node):
//   ./baseline_barrier_rdma 0 3 20000 10.1.2.1 10.1.2.2 10.1.2.3

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

    float* (*data_recv)[2] = calloc((size_t)n, sizeof(*data_recv));  // [peer][round parity]
    RegionInfo (*peer_data)[2] = calloc((size_t)n, sizeof(*peer_data));
    float** input_reg = calloc((size_t)n, sizeof(float*));
    struct ibv_mr** input_mr = calloc((size_t)n, sizeof(struct ibv_mr*));

    for (int j = 0; j < n; ++j) {
        if (j == my_rank) continue;
        struct ibv_mr* data_recv_mr[2];
        for (int b = 0; b < 2; ++b)
            data_recv[j][b] = rdma_reg_buffer(conns[j], M * sizeof(float), &data_recv_mr[b]);
        input_reg[j] = rdma_reg_buffer(conns[j], M * sizeof(float), &input_mr[j]);
        memcpy(input_reg[j], input, M * sizeof(float));

        RegionInfo local[2] = {
            {(uint64_t)(uintptr_t)data_recv[j][0], data_recv_mr[0]->rkey,
             (uint32_t)(M * sizeof(float))},
            {(uint64_t)(uintptr_t)data_recv[j][1], data_recv_mr[1]->rkey,
             (uint32_t)(M * sizeof(float))},
        };
        RegionInfo remote[2];
        rdma_exchange_regions(conns[j], local, 2, remote);
        peer_data[j][0] = remote[0];
        peer_data[j][1] = remote[1];
    }

    double* lat = malloc((size_t)(iters > 0 ? iters : 1) * sizeof(double));

    // Barrier so all ranks start together (keeps connection-setup skew out of the first timed round).
    for (int j = 0; j < n; ++j)
        if (j != my_rank) rdma_barrier(conns[j]);

    int ok = 1;
    for (int it = -1; it < iters; ++it) {
        int par = (it + 1) % 2;  // double-buffered so a peer's next-round write can't clobber this one
        double t0 = now_us();

        // Post every write first, then wait for all of them. They must finish before the barrier
        // below because rdma_barrier() polls the same CQ and would consume a write completion.
        for (int j = 0; j < n; ++j) {
            if (j == my_rank) continue;
            rdma_post_write(conns[j], input_reg[j], input_mr[j]->lkey, M * sizeof(float),
                            peer_data[j][par].addr, peer_data[j][par].rkey);
        }
        for (int j = 0; j < n; ++j)
            if (j != my_rank) rdma_wait_write(conns[j]);
        // Barrier: a two-sided round trip with each peer. Only after these return can we
        // assume every peer's write has landed.
        for (int j = 0; j < n; ++j) {
            if (j == my_rank) continue;
            rdma_barrier(conns[j]);
        }

        for (size_t i = 0; i < M; ++i) {
            float sum = input[i];
            for (int j = 0; j < n; ++j) {
                if (j == my_rank) continue;
                sum += data_recv[j][par][i];
            }
            output[i] = sum;
        }

        double elapsed = now_us() - t0;
        if (it == -1) {
            ok = verify(output, ref, M, 1e-3f);
            printf("Correctness check vs. CPU reference: %s\n", ok ? "PASSED" : "FAILED");
        } else {
            lat[it] = elapsed;
        }
    }
    rdma_print_stats("barrier-based one-shot", lat, iters, n);
    free(lat);
    printf(
        "Compare this against LL/, Sentinel/, Twoshot_LL/, Twoshot_Sentinel/ and "
        "LL128_Atomic/ at the same numFloats/numFloats-per-rank to see the barrier-free "
        "speedup directly.\n");

    // Barrier so no rank closes its connections while a peer is still in its last round.
    for (int j = 0; j < n; ++j)
        if (j != my_rank) rdma_barrier(conns[j]);

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
