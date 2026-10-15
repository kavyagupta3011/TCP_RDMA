// twoshot_sentinel_allreduce_rdma.c
//
// Two-shot AllReduce (sum) for N nodes with Sentinel synchronization in both phases,
// written with InfiniBand RDMA verbs. Same ReduceScatter + AllGather structure as
// Twoshot_LL (rank c owns chunk c), but arrival is detected with the sentinel value
// instead of a flag. Each (phase, peer) pair has two sentinel-filled buffers used
// alternately by round parity, and each slot is re-armed after it is consumed.
//
// Usage:
//   ./twoshot_sentinel_allreduce_rdma <my_rank> <num_ranks> <base_port> <ip0> ... <ip(N-1)> [numFloats] [iters]
// Example for 3 ranks (one process per node):
//   ./twoshot_sentinel_allreduce_rdma 0 3 20000 10.1.2.1 10.1.2.2 10.1.2.3

#include "../common/rdma_common.h"

#define SENTINEL_BITS 0x7fdead00u
#define TWOSHOT_SENTINEL_SEED_BASE 5000

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

// Per-iteration latency summary (average, min, median, p99, max).
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

    if (M % (size_t)n != 0) M += n - (M % n);
    size_t chunk = M / n;

    printf(
        "Two-shot Sentinel AllReduce (RDMA verbs, N-node): M=%zu floats, %d chunks of %zu, "
        "rank=%d/%d\n",
        M, n, chunk, my_rank, n);

    RdmaConn** conns = rdma_mesh_connect(my_rank, n, ips, base_port);

    float** all_vecs = malloc((size_t)n * sizeof(float*));
    for (int r = 0; r < n; ++r) all_vecs[r] = make_random_vector(M, TWOSHOT_SENTINEL_SEED_BASE + r);
    float* input = all_vecs[my_rank];
    float* ref = malloc(M * sizeof(float));
    reference_sum_n(all_vecs, n, ref, M);
    float* output = malloc(M * sizeof(float));

    float sv = sentinel_value();

    // Per peer j: rs_buf[j][0..1] receive j's phase 1 data for my chunk and ag_buf[j][0..1]
    // receive j's finished chunk in phase 2, each double buffered by round parity. There
    // are also per-peer registered copies of my phase 1 slice and my finished chunk.
    float* (*rs_buf)[2] = calloc((size_t)n, sizeof(*rs_buf));
    float* (*ag_buf)[2] = calloc((size_t)n, sizeof(*ag_buf));
    RegionInfo (*peer_rs)[2] = calloc((size_t)n, sizeof(*peer_rs));
    RegionInfo (*peer_ag)[2] = calloc((size_t)n, sizeof(*peer_ag));
    float** phase1_src = calloc((size_t)n, sizeof(float*));
    struct ibv_mr** phase1_src_mr = calloc((size_t)n, sizeof(struct ibv_mr*));
    float** reduced = calloc((size_t)n, sizeof(float*));
    struct ibv_mr** reduced_mr = calloc((size_t)n, sizeof(struct ibv_mr*));

    for (int j = 0; j < n; ++j) {
        if (j == my_rank) continue;
        struct ibv_mr* rs_mr[2];
        struct ibv_mr* ag_mr[2];
        for (int b = 0; b < 2; ++b) {
            rs_buf[j][b] = rdma_reg_buffer(conns[j], chunk * sizeof(float), &rs_mr[b]);
            ag_buf[j][b] = rdma_reg_buffer(conns[j], chunk * sizeof(float), &ag_mr[b]);
            for (size_t i = 0; i < chunk; ++i) rs_buf[j][b][i] = sv;  // ONE-TIME reset
            for (size_t i = 0; i < chunk; ++i) ag_buf[j][b][i] = sv;
        }
        phase1_src[j] = rdma_reg_buffer(conns[j], chunk * sizeof(float), &phase1_src_mr[j]);
        memcpy(phase1_src[j], input + (size_t)j * chunk, chunk * sizeof(float));
        reduced[j] = rdma_reg_buffer(conns[j], chunk * sizeof(float), &reduced_mr[j]);

        RegionInfo local[4] = {
            {(uint64_t)(uintptr_t)rs_buf[j][0], rs_mr[0]->rkey, (uint32_t)(chunk * sizeof(float))},
            {(uint64_t)(uintptr_t)rs_buf[j][1], rs_mr[1]->rkey, (uint32_t)(chunk * sizeof(float))},
            {(uint64_t)(uintptr_t)ag_buf[j][0], ag_mr[0]->rkey, (uint32_t)(chunk * sizeof(float))},
            {(uint64_t)(uintptr_t)ag_buf[j][1], ag_mr[1]->rkey, (uint32_t)(chunk * sizeof(float))},
        };
        RegionInfo remote[4];
        rdma_exchange_regions(conns[j], local, 4, remote);
        peer_rs[j][0] = remote[0];
        peer_rs[j][1] = remote[1];
        peer_ag[j][0] = remote[2];
        peer_ag[j][1] = remote[3];
    }

    double* lat = malloc((size_t)(iters > 0 ? iters : 1) * sizeof(double));

    // Barrier so all ranks start together (keeps connection-setup skew out of the first timed round).
    for (int j = 0; j < n; ++j)
        if (j != my_rank) rdma_barrier(conns[j]);

    int ok = 1;
    for (int it = -1; it < iters; ++it) {
        int parity = (it + 1) % 2;
        double t0 = now_us();

        // Phase 1 (ReduceScatter).
        for (int j = 0; j < n; ++j) {
            if (j == my_rank) continue;
            rdma_post_write(conns[j], phase1_src[j], phase1_src_mr[j]->lkey, chunk * sizeof(float),
                            peer_rs[j][parity].addr, peer_rs[j][parity].rkey);
        }
        for (size_t i = 0; i < chunk; ++i) {
            float sum = input[(size_t)my_rank * chunk + i];
            for (int j = 0; j < n; ++j) {
                if (j == my_rank) continue;
                volatile float* slot = (volatile float*)rs_buf[j][parity];
                while (is_sentinel(slot[i])) { /* spin */
                }
                sum += slot[i];
                slot[i] = sv;  // RE-ARM after consume (see Sentinel/ for why this is safe)
            }
            for (int j = 0; j < n; ++j) {
                if (j == my_rank) continue;
                reduced[j][i] = sum;
            }
            output[(size_t)my_rank * chunk + i] = sum;
        }

        // Phase-1 sends finished long ago (we just received every peer's data); collect them
        // so only one write per connection is ever outstanding.
        for (int j = 0; j < n; ++j)
            if (j != my_rank) rdma_wait_write(conns[j]);

        // Phase 2 (AllGather).
        for (int j = 0; j < n; ++j) {
            if (j == my_rank) continue;
            rdma_post_write(conns[j], reduced[j], reduced_mr[j]->lkey, chunk * sizeof(float),
                            peer_ag[j][parity].addr, peer_ag[j][parity].rkey);
        }
        for (int j = 0; j < n; ++j) {
            if (j == my_rank) continue;
            volatile float* slot = (volatile float*)ag_buf[j][parity];
            for (size_t i = 0; i < chunk; ++i) {
                while (is_sentinel(slot[i])) { /* spin */
                }
                output[(size_t)j * chunk + i] = slot[i];
                slot[i] = sv;  // RE-ARM after consume
            }
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
    print_stats("two-shot Sentinel", lat, iters, n);
    free(lat);

    for (int j = 0; j < n; ++j)
        if (j != my_rank) rdma_barrier(conns[j]);

    rdma_mesh_close(conns, n, my_rank);
    for (int r = 0; r < n; ++r) free(all_vecs[r]);
    free(all_vecs);
    free(ref);
    free(output);
    free(rs_buf);
    free(ag_buf);
    free(peer_rs);
    free(peer_ag);
    free(phase1_src);
    free(phase1_src_mr);
    free(reduced);
    free(reduced_mr);
    free(ips);
    return ok ? 0 : 1;
}
