// ll128_atomic_allreduce_rdma.c
//
// Two-shot, N-NODE AllReduce-sum using a genuine hardware atomic as the
// completion signal -- the actual core idea of the paper's LL128 Atomic
// algorithm (Section IV.B.4, Fig. 5) -- over real InfiniBand RDMA verbs.
// See ../README.md for why this version exists, and Twoshot_LL/'s header
// for the N-ary ReduceScatter+AllGather chunk-ownership convention this
// file also uses.
//
// IMPORTANT hardware-honesty note, please read before comparing this to the
// original CUDA LL128_Atomic implementation or to the paper itself: on
// NVLink, the paper's design gets TWO things from one hardware primitive --
// (a) multiple GPUs' FLOATING-POINT data contributions are summed directly
// by the memory fabric via a cache-line atomic add, AND (b) a per-group
// counter, packed into the same atomic-add stream, signals "everyone has
// contributed" once it reaches N, with no separate flag. InfiniBand's
// native RDMA atomic (IBV_WR_ATOMIC_FETCH_AND_ADD) is a real, genuine
// hardware atomic -- but it operates on a 64-bit SIGNED INTEGER, not a
// float; there is no remote floating-point atomic add on InfiniBand. So
// this version keeps (b) -- the atomic really does serve as the completion
// signal, with no separate flag write anywhere -- but cannot keep (a): the
// actual floating-point DATA still arrives via an ordinary RDMA WRITE into
// a per-sender slot (exactly like Twoshot_LL/Sentinel) and is summed
// locally by the CPU once the atomic counter says it's ready.
//
// Because each counter is incremented by a real atomic add rather than
// overwritten by a plain WRITE, it's naturally monotonic across the whole
// program's lifetime (never needs resetting) -- exactly like LL's epoch
// flag, just advanced by hardware ADD instead of overwritten by hand. In
// the N-node generalization, EVERY one of my N-1 peers issues one atomic
// +1 to my counter each round (one per phase), so my counter's value after
// round `it` (counting the correctness round as it=-1) is
// (it+2)*(num_ranks-1) -- that is this file's `expect` value below. At N=2
// this collapses to exactly the original formula (expect = it+2).
// WRITE(data) is posted before ATOMIC_FETCH_AND_ADD(counter, +1) on the
// same QP each round/peer, so RC ordering guarantees the data has landed by
// the time the counter reflects it.
//
// Usage:
//   ./ll128_atomic_allreduce_rdma <my_rank> <num_ranks> <base_port> <ip0> ... <ip(N-1)> [numFloats] [iters]
// No MPI -- launch once per rank by hand. Example for 3 ranks:
//   node A: ./ll128_atomic_allreduce_rdma 0 3 20000 10.1.2.1 10.1.2.2 10.1.2.3
//   node B: ./ll128_atomic_allreduce_rdma 1 3 20000 10.1.2.1 10.1.2.2 10.1.2.3
//   node C: ./ll128_atomic_allreduce_rdma 2 3 20000 10.1.2.1 10.1.2.2 10.1.2.3

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

    float** all_vecs = malloc((size_t)n * sizeof(float*));
    for (int r = 0; r < n; ++r) all_vecs[r] = make_random_vector(M, LL128_SEED_BASE + r);
    float* input = all_vecs[my_rank];
    float* ref = malloc(M * sizeof(float));
    reference_sum_n(all_vecs, n, ref, M);
    float* output = malloc(M * sizeof(float));

    float** rs_recv = calloc((size_t)n, sizeof(float*));
    int64_t** rs_ctr = calloc((size_t)n, sizeof(int64_t*));  // starts at 0, monotonic forever
    float** ag_recv = calloc((size_t)n, sizeof(float*));
    int64_t** ag_ctr = calloc((size_t)n, sizeof(int64_t*));
    RegionInfo* peer_rs = calloc((size_t)n, sizeof(RegionInfo));
    RegionInfo* peer_rsc = calloc((size_t)n, sizeof(RegionInfo));
    RegionInfo* peer_ag = calloc((size_t)n, sizeof(RegionInfo));
    RegionInfo* peer_agc = calloc((size_t)n, sizeof(RegionInfo));

    float** phase1_src = calloc((size_t)n, sizeof(float*));
    struct ibv_mr** phase1_src_mr = calloc((size_t)n, sizeof(struct ibv_mr*));
    float** reduced = calloc((size_t)n, sizeof(float*));
    struct ibv_mr** reduced_mr = calloc((size_t)n, sizeof(struct ibv_mr*));

    for (int j = 0; j < n; ++j) {
        if (j == my_rank) continue;
        struct ibv_mr *rs_mr, *rsc_mr, *ag_mr, *agc_mr;
        rs_recv[j] = rdma_reg_buffer(conns[j], chunk * sizeof(float), &rs_mr);
        rs_ctr[j] = rdma_reg_buffer(conns[j], sizeof(int64_t), &rsc_mr);
        ag_recv[j] = rdma_reg_buffer(conns[j], chunk * sizeof(float), &ag_mr);
        ag_ctr[j] = rdma_reg_buffer(conns[j], sizeof(int64_t), &agc_mr);

        phase1_src[j] = rdma_reg_buffer(conns[j], chunk * sizeof(float), &phase1_src_mr[j]);
        memcpy(phase1_src[j], input + (size_t)j * chunk, chunk * sizeof(float));
        reduced[j] = rdma_reg_buffer(conns[j], chunk * sizeof(float), &reduced_mr[j]);

        RegionInfo local[4] = {
            {(uint64_t)(uintptr_t)rs_recv[j], rs_mr->rkey, (uint32_t)(chunk * sizeof(float))},
            {(uint64_t)(uintptr_t)rs_ctr[j], rsc_mr->rkey, sizeof(int64_t)},
            {(uint64_t)(uintptr_t)ag_recv[j], ag_mr->rkey, (uint32_t)(chunk * sizeof(float))},
            {(uint64_t)(uintptr_t)ag_ctr[j], agc_mr->rkey, sizeof(int64_t)},
        };
        RegionInfo remote[4];
        rdma_exchange_regions(conns[j], local, 4, remote);
        peer_rs[j] = remote[0];
        peer_rsc[j] = remote[1];
        peer_ag[j] = remote[2];
        peer_agc[j] = remote[3];
    }

    int ok = 1;
    double total_us = 0;
    for (int it = -1; it < iters; ++it) {
        int64_t expect = (int64_t)(it + 2) * (n - 1);
        double t0 = now_us();
        int64_t old_val;

        // Phase 1 (ReduceScatter): data WRITE then atomic +1, no flag.
        for (int j = 0; j < n; ++j) {
            if (j == my_rank) continue;
            rdma_write(conns[j], phase1_src[j], phase1_src_mr[j]->lkey, chunk * sizeof(float),
                       peer_rs[j].addr, peer_rs[j].rkey);
            rdma_atomic_fetch_add(conns[j], peer_rsc[j].addr, peer_rsc[j].rkey, 1, &old_val);
        }
        for (int j = 0; j < n; ++j) {
            if (j == my_rank) continue;
            volatile int64_t* poll = (volatile int64_t*)rs_ctr[j];
            while (*poll != expect) { /* spin */
            }
        }
        for (size_t i = 0; i < chunk; ++i) {
            float sum = input[(size_t)my_rank * chunk + i];
            for (int j = 0; j < n; ++j) {
                if (j == my_rank) continue;
                sum += rs_recv[j][i];
            }
            for (int j = 0; j < n; ++j) {
                if (j == my_rank) continue;
                reduced[j][i] = sum;
            }
            output[(size_t)my_rank * chunk + i] = sum;
        }

        // Phase 2 (AllGather): same pattern, broadcasting the completed chunk.
        for (int j = 0; j < n; ++j) {
            if (j == my_rank) continue;
            rdma_write(conns[j], reduced[j], reduced_mr[j]->lkey, chunk * sizeof(float),
                       peer_ag[j].addr, peer_ag[j].rkey);
            rdma_atomic_fetch_add(conns[j], peer_agc[j].addr, peer_agc[j].rkey, 1, &old_val);
        }
        for (int j = 0; j < n; ++j) {
            if (j == my_rank) continue;
            volatile int64_t* poll = (volatile int64_t*)ag_ctr[j];
            while (*poll != expect) { /* spin */
            }
            memcpy(output + (size_t)j * chunk, ag_recv[j], chunk * sizeof(float));
        }

        double elapsed = now_us() - t0;
        if (it == -1) {
            ok = verify(output, ref, M, 1e-3f);
            printf("Correctness check vs. CPU reference: %s\n", ok ? "PASSED" : "FAILED");
        } else {
            total_us += elapsed;
        }
    }
    printf("Average two-shot LL128 Atomic AllReduce latency over %d iters (%d ranks): %.3f us\n", iters,
           n, total_us / iters);

    rdma_mesh_close(conns, n, my_rank);
    for (int r = 0; r < n; ++r) free(all_vecs[r]);
    free(all_vecs);
    free(ref);
    free(output);
    free(rs_recv);
    free(rs_ctr);
    free(ag_recv);
    free(ag_ctr);
    free(peer_rs);
    free(peer_rsc);
    free(peer_ag);
    free(peer_agc);
    free(phase1_src);
    free(phase1_src_mr);
    free(reduced);
    free(reduced_mr);
    free(ips);
    return ok ? 0 : 1;
}
