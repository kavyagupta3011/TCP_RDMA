// ll_allreduce_rdma.c
//
// One-shot, N-NODE AllReduce-sum using the LL (low-latency) synchronization
// idea, Section IV.B.1 of "Every us Matters: Achieving Near Speed-of-Light
// Latency in GPU Collectives" (arXiv:2607.16100), reimplemented over REAL
// InfiniBand RDMA verbs instead of CUDA/NVLink -- see ../README.md for why
// this version exists (no GPU on this cluster).
//
// The paper's LL trick is: pack a completion flag INTO the same atomic
// transaction as the data, so a 16-byte aligned store either lands whole or
// not at all as far as any reader is concerned -- no separate barrier
// needed. That exact trick is GPU/NVLink-specific. InfiniBand has no
// equivalent "atomic 16-byte store" primitive -- but Reliable Connection
// (RC) QPs guarantee that messages posted on the same QP are APPLIED TO
// REMOTE MEMORY IN THE ORDER THEY WERE POSTED. So instead of one atomic
// flag+data store, this version posts TWO ordinary RDMA WRITEs, back to
// back, on the same QP: (1) WRITE the full data vector into the peer's
// data_recv region, (2) WRITE a single 8-byte epoch value into the peer's
// flag_recv region. Because #2 is posted after #1 on the same QP, RC
// ordering guarantees #1 has already landed by the time the receiver
// observes #2 -- one busy-polled 8-byte word per peer, no explicit
// synchronization primitive.
//
// N-node generalization: this is now a full mesh push. Every rank writes
// its ENTIRE input vector directly to EVERY other rank (N-1 sends, N-1
// receives), then sums its own input with all N-1 received vectors. No
// reduction tree, no partitioning -- this mirrors exactly what the original
// CUDA one-shot version does across GPUs, now across processes/nodes.
//
// Important RDMA detail this generalization surfaces: every peer connection
// has its OWN protection domain (see rdma_common.c's build_qp -- each
// RdmaConn gets its own ibv_alloc_pd call). A buffer used as the LOCAL side
// of a WRITE on connection j must be registered against connection j's PD
// specifically. So unlike the 2-node version (one peer, one registration),
// this file keeps a PER-PEER registered copy of `input` (input_reg[j]) and
// of the epoch value (epoch_send[j]) -- same bytes, N-1 separate
// registrations, one per outbound connection.
//
// Because the flag is an EPOCH that increments every round (not a fixed
// value), no reset is ever needed between repeated AllReduce calls.
//
// Usage:
//   ./ll_allreduce_rdma <my_rank> <num_ranks> <base_port> <ip0> ... <ip(N-1)> [numFloats] [iters]
// No MPI -- launch this once per rank by hand (e.g. one SSH session per
// compute node). Example for 3 ranks:
//   node A: ./ll_allreduce_rdma 0 3 20000 10.1.2.1 10.1.2.2 10.1.2.3
//   node B: ./ll_allreduce_rdma 1 3 20000 10.1.2.1 10.1.2.2 10.1.2.3
//   node C: ./ll_allreduce_rdma 2 3 20000 10.1.2.1 10.1.2.2 10.1.2.3

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

    // Every rank locally regenerates EVERY rank's input vector (same seed
    // scheme, BASE+rank) purely to compute an independent reference sum --
    // only this rank's own slot (all_vecs[my_rank]) ever goes on the wire.
    float** all_vecs = malloc((size_t)n * sizeof(float*));
    for (int r = 0; r < n; ++r) all_vecs[r] = make_random_vector(M, LL_SEED_BASE + r);
    float* input = all_vecs[my_rank];
    float* ref = malloc(M * sizeof(float));
    reference_sum_n(all_vecs, n, ref, M);
    float* output = malloc(M * sizeof(float));

    float** data_recv = calloc((size_t)n, sizeof(float*));
    int64_t** flag_recv = calloc((size_t)n, sizeof(int64_t*));
    RegionInfo* peer_data = calloc((size_t)n, sizeof(RegionInfo));
    RegionInfo* peer_flag = calloc((size_t)n, sizeof(RegionInfo));
    float** input_reg = calloc((size_t)n, sizeof(float*));
    struct ibv_mr** input_mr = calloc((size_t)n, sizeof(struct ibv_mr*));
    int64_t** epoch_send = calloc((size_t)n, sizeof(int64_t*));
    struct ibv_mr** epoch_mr = calloc((size_t)n, sizeof(struct ibv_mr*));

    for (int j = 0; j < n; ++j) {
        if (j == my_rank) continue;
        struct ibv_mr *data_recv_mr, *flag_recv_mr;
        data_recv[j] = rdma_reg_buffer(conns[j], M * sizeof(float), &data_recv_mr);
        flag_recv[j] = rdma_reg_buffer(conns[j], sizeof(int64_t), &flag_recv_mr);

        input_reg[j] = rdma_reg_buffer(conns[j], M * sizeof(float), &input_mr[j]);
        memcpy(input_reg[j], input, M * sizeof(float));
        epoch_send[j] = rdma_reg_buffer(conns[j], sizeof(int64_t), &epoch_mr[j]);

        RegionInfo local[2] = {
            {(uint64_t)(uintptr_t)data_recv[j], data_recv_mr->rkey, (uint32_t)(M * sizeof(float))},
            {(uint64_t)(uintptr_t)flag_recv[j], flag_recv_mr->rkey, sizeof(int64_t)},
        };
        RegionInfo remote[2];
        rdma_exchange_regions(conns[j], local, 2, remote);
        peer_data[j] = remote[0];
        peer_flag[j] = remote[1];
    }

    int ok = 1;
    double total_us = 0;
    for (int it = -1; it < iters; ++it) {
        int64_t epoch = it + 2;
        double t0 = now_us();

        for (int j = 0; j < n; ++j) {
            if (j == my_rank) continue;
            *epoch_send[j] = epoch;
            rdma_write(conns[j], input_reg[j], input_mr[j]->lkey, M * sizeof(float), peer_data[j].addr,
                       peer_data[j].rkey);
            rdma_write(conns[j], epoch_send[j], epoch_mr[j]->lkey, sizeof(int64_t), peer_flag[j].addr,
                       peer_flag[j].rkey);
        }
        for (int j = 0; j < n; ++j) {
            if (j == my_rank) continue;
            volatile int64_t* poll = (volatile int64_t*)flag_recv[j];
            while (*poll != epoch) { /* spin */
            }
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
    printf("Average one-shot LL AllReduce latency over %d iters (%d ranks): %.3f us\n", iters, n,
           total_us / iters);

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
