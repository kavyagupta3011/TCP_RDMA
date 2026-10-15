// ll_allreduce_rdma.c
//
// One-shot AllReduce (sum) for N nodes using the LL idea (Section IV.B.1 of the paper),
// written with InfiniBand RDMA verbs. Every rank writes its whole input vector to every
// other rank, then writes a small epoch flag on the same QP. RC delivers writes in order,
// so once the flag shows the current epoch the data is already there and the receiver
// can sum. No barrier is needed. The epoch grows every round, so the flag never needs
// resetting.
//
// Each connection has its own protection domain, so every outgoing buffer is registered
// once per peer (input_reg[j], epoch_send[j]).
//
// Usage:
//   ./ll_allreduce_rdma <my_rank> <num_ranks> <base_port> <ip0> ... <ip(N-1)> [numFloats] [iters]
// Example for 3 ranks (one process per node):
//   ./ll_allreduce_rdma 0 3 20000 10.1.2.1 10.1.2.2 10.1.2.3

#include "../common/rdma_common.h"

#define LL_SEED_BASE 1000

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

    printf("LL one-shot AllReduce (RDMA verbs, N-node): M=%zu floats, rank=%d/%d\n", M, my_rank, n);

    RdmaConn** conns = rdma_mesh_connect(my_rank, n, ips, base_port);

    // Every rank regenerates every rank's input (same seed scheme, BASE+rank) only to compute
    // a reference sum. Only this rank's own vector is sent.
    float** all_vecs = malloc((size_t)n * sizeof(float*));
    for (int r = 0; r < n; ++r) all_vecs[r] = make_random_vector(M, LL_SEED_BASE + r);
    float* input = all_vecs[my_rank];
    float* ref = malloc(M * sizeof(float));
    reference_sum_n(all_vecs, n, ref, M);
    float* output = malloc(M * sizeof(float));

    float* (*data_recv)[2] = calloc((size_t)n, sizeof(*data_recv));  // [peer][round parity]
    int64_t** flag_recv = calloc((size_t)n, sizeof(int64_t*));
    RegionInfo (*peer_data)[2] = calloc((size_t)n, sizeof(*peer_data));
    RegionInfo* peer_flag = calloc((size_t)n, sizeof(RegionInfo));
    float** input_reg = calloc((size_t)n, sizeof(float*));
    struct ibv_mr** input_mr = calloc((size_t)n, sizeof(struct ibv_mr*));
    int64_t** epoch_send = calloc((size_t)n, sizeof(int64_t*));
    struct ibv_mr** epoch_mr = calloc((size_t)n, sizeof(struct ibv_mr*));

    for (int j = 0; j < n; ++j) {
        if (j == my_rank) continue;
        struct ibv_mr *data_mr[2], *flag_recv_mr;
        for (int b = 0; b < 2; ++b)
            data_recv[j][b] = rdma_reg_buffer(conns[j], M * sizeof(float), &data_mr[b]);
        flag_recv[j] = rdma_reg_buffer(conns[j], sizeof(int64_t), &flag_recv_mr);

        input_reg[j] = rdma_reg_buffer(conns[j], M * sizeof(float), &input_mr[j]);
        memcpy(input_reg[j], input, M * sizeof(float));
        epoch_send[j] = rdma_reg_buffer(conns[j], sizeof(int64_t), &epoch_mr[j]);

        RegionInfo local[3] = {
            {(uint64_t)(uintptr_t)data_recv[j][0], data_mr[0]->rkey, (uint32_t)(M * sizeof(float))},
            {(uint64_t)(uintptr_t)data_recv[j][1], data_mr[1]->rkey, (uint32_t)(M * sizeof(float))},
            {(uint64_t)(uintptr_t)flag_recv[j], flag_recv_mr->rkey, sizeof(int64_t)},
        };
        RegionInfo remote[3];
        rdma_exchange_regions(conns[j], local, 3, remote);
        peer_data[j][0] = remote[0];
        peer_data[j][1] = remote[1];
        peer_flag[j] = remote[2];
    }

    double* lat = malloc((size_t)(iters > 0 ? iters : 1) * sizeof(double));

    // Barrier so all ranks start together (keeps connection-setup skew out of the first timed round).
    for (int j = 0; j < n; ++j)
        if (j != my_rank) rdma_barrier(conns[j]);

    int ok = 1;
    for (int it = -1; it < iters; ++it) {
        int64_t epoch = it + 2;
        int par = (int)(epoch & 1);
        double t0 = now_us();

        for (int j = 0; j < n; ++j) {
            if (j == my_rank) continue;
            *epoch_send[j] = epoch;
            // Data first (unsignaled), then the 8-byte epoch flag (signaled, inline) on the
            // same QP: RC ordering guarantees the data has landed once the flag is seen.
            rdma_post_write_ex(conns[j], input_reg[j], input_mr[j]->lkey, M * sizeof(float),
                               peer_data[j][par].addr, peer_data[j][par].rkey, 0);
            rdma_post_write(conns[j], epoch_send[j], epoch_mr[j]->lkey, sizeof(int64_t),
                            peer_flag[j].addr, peer_flag[j].rkey);
        }
        for (int j = 0; j < n; ++j) {
            if (j == my_rank) continue;
            volatile int64_t* poll = (volatile int64_t*)flag_recv[j];
            while (*poll < epoch) { /* spin */
            }
        }
        __atomic_thread_fence(__ATOMIC_ACQUIRE);
        for (size_t i = 0; i < M; ++i) {
            float sum = input[i];
            for (int j = 0; j < n; ++j) {
                if (j == my_rank) continue;
                sum += data_recv[j][par][i];
            }
            output[i] = sum;
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
    rdma_print_stats("one-shot LL", lat, iters, n);
    free(lat);

    // Barrier so no rank closes its connections while a peer is still in its last round.
    for (int j = 0; j < n; ++j)
        if (j != my_rank) rdma_barrier(conns[j]);

    rdma_mesh_close(conns, n, my_rank);
    for (int r = 0; r < n; ++r) free(all_vecs[r]);
    free(all_vecs);
    free(ref);
    free(output);
    free(data_recv);
    free(flag_recv);
    free(peer_data);
    free(peer_flag);
    free(input_reg);
    free(input_mr);
    free(epoch_send);
    free(epoch_mr);
    free(ips);
    return ok ? 0 : 1;
}
