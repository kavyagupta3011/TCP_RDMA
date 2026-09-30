// sentinel_allreduce_rdma.c
//
// One-shot, N-NODE AllReduce-sum using the Sentinel synchronization idea,
// Section IV.B.2 of "Every us Matters: Achieving Near Speed-of-Light
// Latency in GPU Collectives" (arXiv:2607.16100), over real InfiniBand RDMA
// verbs. See ../README.md for why this version exists, and
// LL/ll_allreduce_rdma.c for the RC-ordering background and the
// per-peer-registration detail (every connection has its own protection
// domain) this file also relies on.
//
// The idea: write real data directly (full width, no flag) into a buffer
// pre-filled with a reserved sentinel bit pattern; the receiver polls for
// the value to stop matching the sentinel. Single RDMA WRITE per peer per
// round (no trailing flag write).
//
// The catch, exactly as the paper describes: the receiving buffer must be
// reset to the sentinel value before it can be reused, and doing that
// safely across REPEATED rounds between SEPARATE PROCESSES (no shared host
// to sequence resets against sends) needs real coordination -- unless you
// use the paper's own answer to that problem: double buffering (Section
// IV.B.3), applied here across ALLREDUCE CALLS rather than across chunks
// within one call. Every peer-pair gets TWO scratch regions, reset to the
// sentinel exactly ONCE (right after that connection is established, which
// is itself already a natural one-time synchronization point). Round i
// writes into buffer (i % 2); because both sides execute rounds in the same
// strict sequential order, by the time round i+2 could possibly arrive at a
// buffer, its round-i contents have provably already been consumed. No
// per-round reset, no per-round barrier, after the one-time initial reset.
//
// N-node generalization: same full-mesh push as LL/ -- every rank writes
// its entire input to every other rank's per-peer sentinel buffer, and sums
// its own input with all N-1 received vectors once every slot has stopped
// reading as the sentinel.
//
// Usage:
//   ./sentinel_allreduce_rdma <my_rank> <num_ranks> <base_port> <ip0> ... <ip(N-1)> [numFloats] [iters]
// No MPI -- launch once per rank by hand. Example for 3 ranks:
//   node A: ./sentinel_allreduce_rdma 0 3 20000 10.1.2.1 10.1.2.2 10.1.2.3
//   node B: ./sentinel_allreduce_rdma 1 3 20000 10.1.2.1 10.1.2.2 10.1.2.3
//   node C: ./sentinel_allreduce_rdma 2 3 20000 10.1.2.1 10.1.2.2 10.1.2.3

#include "../common/rdma_common.h"

#define SENTINEL_BITS 0x7fdead00u
#define SENTINEL_SEED_BASE 2000

static float sentinel_value(void) {
    unsigned int bits = SENTINEL_BITS;
    float f;
    memcpy(&f, &bits, sizeof(f));
    return f;
}

static unsigned int float_bits(float f) {
    unsigned int bits;
    memcpy(&bits, &f, sizeof(bits));
    return bits;
}

static int is_sentinel(float f) { return float_bits(f) == SENTINEL_BITS; }

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

    printf("Sentinel one-shot AllReduce (RDMA verbs, N-node): M=%zu floats, rank=%d/%d\n", M, my_rank,
           n);

    RdmaConn** conns = rdma_mesh_connect(my_rank, n, ips, base_port);

    float** all_vecs = malloc((size_t)n * sizeof(float*));
    for (int r = 0; r < n; ++r) all_vecs[r] = make_random_vector(M, SENTINEL_SEED_BASE + r);
    float* input = all_vecs[my_rank];
    float* ref = malloc(M * sizeof(float));
    reference_sum_n(all_vecs, n, ref, M);
    float* output = malloc(M * sizeof(float));

    float sv = sentinel_value();

    // Per peer: two sentinel-initialized receive buffers (double buffered
    // across rounds), and -- since every connection has its own protection
    // domain -- a per-peer registered copy of `input` to send from.
    float* (*recv_buf)[2] = calloc((size_t)n, sizeof(*recv_buf));
    RegionInfo (*peer_regions)[2] = calloc((size_t)n, sizeof(*peer_regions));
    float** input_reg = calloc((size_t)n, sizeof(float*));
    struct ibv_mr** input_mr = calloc((size_t)n, sizeof(struct ibv_mr*));

    for (int j = 0; j < n; ++j) {
        if (j == my_rank) continue;
        struct ibv_mr* buf_mr[2];
        for (int b = 0; b < 2; ++b) {
            recv_buf[j][b] = rdma_reg_buffer(conns[j], M * sizeof(float), &buf_mr[b]);
            for (size_t i = 0; i < M; ++i) recv_buf[j][b][i] = sv;  // ONE-TIME reset
        }
        input_reg[j] = rdma_reg_buffer(conns[j], M * sizeof(float), &input_mr[j]);
        memcpy(input_reg[j], input, M * sizeof(float));

        RegionInfo local[2] = {
            {(uint64_t)(uintptr_t)recv_buf[j][0], buf_mr[0]->rkey, (uint32_t)(M * sizeof(float))},
            {(uint64_t)(uintptr_t)recv_buf[j][1], buf_mr[1]->rkey, (uint32_t)(M * sizeof(float))},
        };
        rdma_exchange_regions(conns[j], local, 2, peer_regions[j]);
    }

    int ok = 1;
    double total_us = 0;
    for (int it = -1; it < iters; ++it) {
        int buf_idx = (it + 1) % 2;
        double t0 = now_us();

        for (int j = 0; j < n; ++j) {
            if (j == my_rank) continue;
            rdma_write(conns[j], input_reg[j], input_mr[j]->lkey, M * sizeof(float),
                       peer_regions[j][buf_idx].addr, peer_regions[j][buf_idx].rkey);
        }
        for (size_t i = 0; i < M; ++i) {
            float sum = input[i];
            for (int j = 0; j < n; ++j) {
                if (j == my_rank) continue;
                volatile float* slot = (volatile float*)recv_buf[j][buf_idx];
                while (is_sentinel(slot[i])) { /* spin */
                }
                sum += slot[i];
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
    printf("Average one-shot Sentinel AllReduce latency over %d iters (%d ranks): %.3f us\n", iters, n,
           total_us / iters);
    printf("(No per-round reset needed -- see file header for why double buffering makes that safe.)\n");

    rdma_mesh_close(conns, n, my_rank);
    for (int r = 0; r < n; ++r) free(all_vecs[r]);
    free(all_vecs);
    free(ref);
    free(output);
    free(recv_buf);
    free(peer_regions);
    free(input_reg);
    free(input_mr);
    free(ips);
    return ok ? 0 : 1;
}
