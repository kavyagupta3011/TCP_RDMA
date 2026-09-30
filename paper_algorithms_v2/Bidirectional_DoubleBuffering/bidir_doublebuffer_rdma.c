// bidir_doublebuffer_rdma.c
//
// Chunked pairwise reduction using Bidirectional Communication & Double
// Buffering, Section IV.B.3 and Fig. 4 of "Every us Matters: Achieving Near
// Speed-of-Light Latency in GPU Collectives" (arXiv:2607.16100), over real
// InfiniBand RDMA verbs. See ../README.md for why this version exists, and
// LL/ll_allreduce_rdma.c for the RC-ordering trick (data WRITE, then a
// trailing flag WRITE, same QP) this file reuses per chunk.
//
// This is the ONE algorithm in this project that stays pairwise even in the
// N-node generalization -- exactly like the original CUDA version, which
// already scoped this one to 2 GPUs on purpose, because the paper's own
// Fig. 4 is inherently a 2-rank teaching example (bidirectional traffic
// between exactly two participants is the whole point of the picture). So
// this program still only ever uses ranks 0 and 1: it accepts the SAME
// command-line shape as every other program in this project (for a
// consistent launch story across the whole set), but requires num_ranks to
// be exactly 2. If your professor wants an N>2 demonstration, use one of
// the other six programs -- they all generalize to N.
//
// Mechanism, per chunk: push this chunk into the peer's current buffer
// (data WRITE + trailing flag WRITE, ordered on the same QP), wait for the
// peer's chunk in this rank's own current buffer (poll the local flag),
// reduce, advance to the next buffer. Two buffers (0 and 1) are ping-ponged
// by chunk parity; because each side only reuses buffer 0 again two chunks
// later, and by then it has necessarily already both sent its own next
// chunk and consumed the peer's previous one, no barrier or reset is ever
// needed between chunks -- "each receive serves as an implicit permission
// for the next send" (paper, Section IV.B.3).
//
// Usage:
//   ./bidir_doublebuffer_rdma <my_rank> 2 <base_port> <ip0> <ip1> [numFloats] [numChunks] [iters]
// No MPI -- launch once per rank by hand:
//   node A: ./bidir_doublebuffer_rdma 0 2 20000 10.1.2.1 10.1.2.2
//   node B: ./bidir_doublebuffer_rdma 1 2 20000 10.1.2.1 10.1.2.2

#include "../common/rdma_common.h"

#define BIDIR_SEED_BASE 3000

static void usage(const char* prog) {
    fprintf(stderr,
            "Usage: %s <my_rank> 2 <base_port> <ip0> <ip1> [numFloats] [numChunks] [iters]\n"
            "This algorithm is pairwise ONLY -- num_ranks must be exactly 2.\n"
            "No MPI -- launch once per rank by hand:\n"
            "  node A: %s 0 2 20000 10.1.2.1 10.1.2.2\n"
            "  node B: %s 1 2 20000 10.1.2.1 10.1.2.2\n",
            prog, prog, prog);
}

int main(int argc, char** argv) {
    if (argc < 6) {
        usage(argv[0]);
        return 1;
    }
    int my_rank = atoi(argv[1]);
    int n = atoi(argv[2]);
    int base_port = atoi(argv[3]);
    if (n != 2 || my_rank < 0 || my_rank >= 2) {
        fprintf(stderr, "num_ranks must be exactly 2 for this algorithm; my_rank must be 0 or 1.\n");
        return 1;
    }
    const char* ips[2] = {argv[4], argv[5]};
    int arg_idx = 6;
    size_t M = (argc > arg_idx) ? (size_t)atol(argv[arg_idx]) : (1u << 18);
    arg_idx++;
    int num_chunks = (argc > arg_idx) ? atoi(argv[arg_idx]) : 8;
    arg_idx++;
    int iters = (argc > arg_idx) ? atoi(argv[arg_idx]) : 100;

    if (M % (size_t)num_chunks != 0) M += num_chunks - (M % num_chunks);
    size_t chunk_elems = M / num_chunks;
    int peer_rank = 1 - my_rank;

    printf(
        "Bidirectional Communication & Double Buffering (RDMA verbs, pairwise): "
        "M=%zu floats, %d chunks of %zu floats, rank=%d/2\n",
        M, num_chunks, chunk_elems, my_rank);

    RdmaConn** conns = rdma_mesh_connect(my_rank, 2, ips, base_port);
    RdmaConn* conn = conns[peer_rank];

    float* input = make_random_vector(M, BIDIR_SEED_BASE + my_rank);
    float* peer_input_ref = make_random_vector(M, BIDIR_SEED_BASE + peer_rank);
    float* ref = malloc(M * sizeof(float));
    reference_sum(input, peer_input_ref, ref, M);
    float* output = malloc(M * sizeof(float));

    struct ibv_mr *data_mr[2], *flag_mr[2];
    float* data_buf[2];
    int64_t* flag_buf[2];
    for (int b = 0; b < 2; ++b) {
        data_buf[b] = rdma_reg_buffer(conn, chunk_elems * sizeof(float), &data_mr[b]);
        flag_buf[b] = rdma_reg_buffer(conn, sizeof(int64_t), &flag_mr[b]);
    }

    struct ibv_mr* input_mr;
    float* input_reg = rdma_reg_buffer(conn, M * sizeof(float), &input_mr);
    memcpy(input_reg, input, M * sizeof(float));

    RegionInfo local_regions[4] = {
        {(uint64_t)(uintptr_t)data_buf[0], data_mr[0]->rkey, (uint32_t)(chunk_elems * sizeof(float))},
        {(uint64_t)(uintptr_t)flag_buf[0], flag_mr[0]->rkey, sizeof(int64_t)},
        {(uint64_t)(uintptr_t)data_buf[1], data_mr[1]->rkey, (uint32_t)(chunk_elems * sizeof(float))},
        {(uint64_t)(uintptr_t)flag_buf[1], flag_mr[1]->rkey, sizeof(int64_t)},
    };
    RegionInfo peer_regions[4];
    rdma_exchange_regions(conn, local_regions, 4, peer_regions);
    RegionInfo peer_data[2] = {peer_regions[0], peer_regions[2]};
    RegionInfo peer_flag[2] = {peer_regions[1], peer_regions[3]};

    volatile int64_t* flag_poll[2] = {(volatile int64_t*)flag_buf[0], (volatile int64_t*)flag_buf[1]};

    struct ibv_mr* epoch_mr;
    int64_t* epoch_send = rdma_reg_buffer(conn, sizeof(int64_t), &epoch_mr);

    // One full pass = correctness check.
    for (int c = 0; c < num_chunks; ++c) {
        int buf = c % 2;
        int64_t flag_val = c + 1;
        size_t base = (size_t)c * chunk_elems;

        *epoch_send = flag_val;
        rdma_write(conn, input_reg + base, input_mr->lkey, chunk_elems * sizeof(float),
                   peer_data[buf].addr, peer_data[buf].rkey);
        rdma_write(conn, epoch_send, epoch_mr->lkey, sizeof(int64_t), peer_flag[buf].addr,
                   peer_flag[buf].rkey);
        while (flag_poll[buf][0] != flag_val) { /* spin */
        }
        for (size_t i = 0; i < chunk_elems; ++i) output[base + i] = input[base + i] + data_buf[buf][i];
    }
    int ok = verify(output, ref, M, 1e-3f);
    printf("Correctness check vs. CPU reference: %s\n", ok ? "PASSED" : "FAILED");

    int64_t global_flag_counter = num_chunks + 1;  // continue past the correctness-check pass above
    double total_us = 0;
    for (int it = 0; it < iters; ++it) {
        double t0 = now_us();
        for (int c = 0; c < num_chunks; ++c) {
            int buf = c % 2;
            int64_t flag_val = ++global_flag_counter;  // simple monotonic counter, unique forever
            size_t base = (size_t)c * chunk_elems;

            *epoch_send = flag_val;
            rdma_write(conn, input_reg + base, input_mr->lkey, chunk_elems * sizeof(float),
                       peer_data[buf].addr, peer_data[buf].rkey);
            rdma_write(conn, epoch_send, epoch_mr->lkey, sizeof(int64_t), peer_flag[buf].addr,
                       peer_flag[buf].rkey);
            while (flag_poll[buf][0] != flag_val) { /* spin */
            }
            for (size_t i = 0; i < chunk_elems; ++i)
                output[base + i] = input[base + i] + data_buf[buf][i];
        }
        total_us += now_us() - t0;
    }
    printf(
        "Average bidirectional double-buffered reduction latency over %d iters "
        "(%d chunks/iter): %.3f us\n",
        iters, num_chunks, total_us / iters);

    rdma_mesh_close(conns, 2, my_rank);
    free(input);
    free(peer_input_ref);
    free(ref);
    free(output);
    return ok ? 0 : 1;
}
