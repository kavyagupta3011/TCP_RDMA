// sentinel_allreduce_rdma.c
//
// One-shot AllReduce (sum) for N nodes using sentinel values (Section IV.B.2 of the paper,
// arXiv:2607.16100), written with InfiniBand RDMA verbs. Every rank writes its input vector,
// without a flag, into a receive buffer on every other rank. That buffer was filled beforehand
// with a reserved bit pattern (a NaN), and the receiver waits until each element stops matching
// it. One RDMA WRITE per peer per round.
//
// Every peer gets two receive buffers used alternately by round. A slot is set back to the
// sentinel right after it has been read. This is safe because the peer cannot write round i+2
// into a buffer until it has received my round i+1 data, which I only send after finishing
// round i, including the reset.
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

static int cmp_double(const void* a, const void* b) {
    double x = *(const double*)a, y = *(const double*)b;
    return (x > y) - (x < y);
}

// Per-iteration latency summary. The mean alone hides one-off stalls (a late
// rank, a page fault); min/median/p99 show what a typical round really costs.
static void print_stats(const char* name, double* lat, int iters, int ranks) {
    double sum = 0;
    for (int i = 0; i < iters; ++i) sum += lat[i];
    qsort(lat, (size_t)iters, sizeof(double), cmp_double);
    printf("Average %s AllReduce latency over %d iters (%d ranks): %.3f us\n", name, iters, ranks,
           sum / iters);
    printf("  min=%.3f us  median=%.3f us  p99=%.3f us  max=%.3f us\n", lat[0], lat[iters / 2],
           lat[(int)((iters - 1) * 0.99)], lat[iters - 1]);
}

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

    double* lat = malloc((size_t)(iters > 0 ? iters : 1) * sizeof(double));

    // Start all ranks together so the first timed round doesn't absorb
    // connection-setup skew between processes.
    for (int j = 0; j < n; ++j)
        if (j != my_rank) rdma_barrier(conns[j]);

    int ok = 1;
    for (int it = -1; it < iters; ++it) {
        int buf_idx = (it + 1) % 2;
        double t0 = now_us();

        for (int j = 0; j < n; ++j) {
            if (j == my_rank) continue;
            // Post to every peer back to back (small payloads go inline); completions are
            // collected only AFTER the reduce below, overlapping them with the wait for data.
            rdma_post_write(conns[j], input_reg[j], input_mr[j]->lkey, M * sizeof(float),
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
                // RE-ARM: put the sentinel back now that this value is consumed, so the
                // next use of this buffer (round it+2) blocks until fresh data lands.
                slot[i] = sv;
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
    print_stats("one-shot Sentinel", lat, iters, n);
    free(lat);

    // Don't tear down connections while a peer may still be finishing its last round.
    for (int j = 0; j < n; ++j)
        if (j != my_rank) rdma_barrier(conns[j]);

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
