// ll128_atomic_allreduce_rdma.c
//
// Two-shot AllReduce (sum) for N nodes where completion is signalled by an RDMA atomic
// fetch-and-add on a remote counter (LL128 Atomic, Section IV.B.4 of the paper), written
// with InfiniBand RDMA verbs. Chunk ownership is the same as Twoshot_LL (rank c owns
// chunk c).
//
// InfiniBand atomics work on 64-bit integers only, so the float data is sent with a normal
// RDMA write and summed by the CPU; the atomic counter only tells the receiver that the
// data is ready. The data write is posted first and the atomic second on the same QP, so
// RC ordering guarantees the data has landed once the counter changes. The counter only
// increases, so it never needs resetting.
//
// Usage:
//   ./ll128_atomic_allreduce_rdma <my_rank> <num_ranks> <base_port> <ip0> ... <ip(N-1)> [numFloats] [iters]
// Example for 3 ranks (one process per node):
//   ./ll128_atomic_allreduce_rdma 0 3 20000 10.1.2.1 10.1.2.2 10.1.2.3

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

    // This algorithm needs hardware RDMA atomics. Some adapters (including the ConnectX-3 on
    // this cluster, checked with Atomic_Probe/) report atomic_cap = NONE and every atomic post
    // then fails with EINVAL. All ranks have the same adapter, so they all take this branch.
    {
        struct ibv_device_attr dattr;
        memset(&dattr, 0, sizeof(dattr));
        int peer0 = (my_rank == 0) ? 1 : 0;
        if (ibv_query_device(conns[peer0]->cm_id->verbs, &dattr) == 0 &&
            dattr.atomic_cap == IBV_ATOMIC_NONE) {
            printf(
                "SKIPPED: this adapter reports atomic_cap=NONE (no RDMA atomics); LL128_Atomic "
                "cannot run here. See results/ and Atomic_Probe/.\n");
            rdma_mesh_close(conns, n, my_rank);
            free(ips);
            return 2;
        }
    }

    float** all_vecs = malloc((size_t)n * sizeof(float*));
    for (int r = 0; r < n; ++r) all_vecs[r] = make_random_vector(M, LL128_SEED_BASE + r);
    float* input = all_vecs[my_rank];
    float* ref = malloc(M * sizeof(float));
    reference_sum_n(all_vecs, n, ref, M);
    float* output = malloc(M * sizeof(float));

    float* (*rs_recv)[2] = calloc((size_t)n, sizeof(*rs_recv));  // [peer][round parity]
    float* (*ag_recv)[2] = calloc((size_t)n, sizeof(*ag_recv));
    int64_t** rs_ctr = calloc((size_t)n, sizeof(int64_t*));  // starts at 0, monotonic forever
    int64_t** ag_ctr = calloc((size_t)n, sizeof(int64_t*));
    RegionInfo (*peer_rs)[2] = calloc((size_t)n, sizeof(*peer_rs));
    RegionInfo (*peer_ag)[2] = calloc((size_t)n, sizeof(*peer_ag));
    RegionInfo* peer_rsc = calloc((size_t)n, sizeof(RegionInfo));
    RegionInfo* peer_agc = calloc((size_t)n, sizeof(RegionInfo));

    float** phase1_src = calloc((size_t)n, sizeof(float*));
    struct ibv_mr** phase1_src_mr = calloc((size_t)n, sizeof(struct ibv_mr*));
    float** reduced = calloc((size_t)n, sizeof(float*));
    struct ibv_mr** reduced_mr = calloc((size_t)n, sizeof(struct ibv_mr*));

    for (int j = 0; j < n; ++j) {
        if (j == my_rank) continue;
        struct ibv_mr *rs_mr[2], *ag_mr[2], *rsc_mr, *agc_mr;
        for (int b = 0; b < 2; ++b) {
            rs_recv[j][b] = rdma_reg_buffer(conns[j], chunk * sizeof(float), &rs_mr[b]);
            ag_recv[j][b] = rdma_reg_buffer(conns[j], chunk * sizeof(float), &ag_mr[b]);
        }
        rs_ctr[j] = rdma_reg_buffer(conns[j], sizeof(int64_t), &rsc_mr);
        ag_ctr[j] = rdma_reg_buffer(conns[j], sizeof(int64_t), &agc_mr);

        phase1_src[j] = rdma_reg_buffer(conns[j], chunk * sizeof(float), &phase1_src_mr[j]);
        memcpy(phase1_src[j], input + (size_t)j * chunk, chunk * sizeof(float));
        reduced[j] = rdma_reg_buffer(conns[j], chunk * sizeof(float), &reduced_mr[j]);

        uint32_t csz = (uint32_t)(chunk * sizeof(float));
        RegionInfo local[6] = {
            {(uint64_t)(uintptr_t)rs_recv[j][0], rs_mr[0]->rkey, csz},
            {(uint64_t)(uintptr_t)rs_recv[j][1], rs_mr[1]->rkey, csz},
            {(uint64_t)(uintptr_t)rs_ctr[j], rsc_mr->rkey, sizeof(int64_t)},
            {(uint64_t)(uintptr_t)ag_recv[j][0], ag_mr[0]->rkey, csz},
            {(uint64_t)(uintptr_t)ag_recv[j][1], ag_mr[1]->rkey, csz},
            {(uint64_t)(uintptr_t)ag_ctr[j], agc_mr->rkey, sizeof(int64_t)},
        };
        RegionInfo remote[6];
        rdma_exchange_regions(conns[j], local, 6, remote);
        peer_rs[j][0] = remote[0];
        peer_rs[j][1] = remote[1];
        peer_rsc[j] = remote[2];
        peer_ag[j][0] = remote[3];
        peer_ag[j][1] = remote[4];
        peer_agc[j] = remote[5];
    }

    double* lat = malloc((size_t)(iters > 0 ? iters : 1) * sizeof(double));

    // Barrier so all ranks start together (keeps connection-setup skew out of the first timed round).
    for (int j = 0; j < n; ++j)
        if (j != my_rank) rdma_barrier(conns[j]);

    int ok = 1;
    for (int it = -1; it < iters; ++it) {
        // Each peer adds 1 to its own per-connection counter (rs_ctr[j]) every round, so after
        // round `it` that counter equals it+2.
        int64_t expect = it + 2;
        int par = (int)(expect & 1);
        double t0 = now_us();

        // Phase 1 (ReduceScatter): data WRITE (unsignaled) then atomic +1 (signaled), no flag.
        for (int j = 0; j < n; ++j) {
            if (j == my_rank) continue;
            rdma_post_write_ex(conns[j], phase1_src[j], phase1_src_mr[j]->lkey, chunk * sizeof(float),
                               peer_rs[j][par].addr, peer_rs[j][par].rkey, 0);
            rdma_post_atomic_add(conns[j], peer_rsc[j].addr, peer_rsc[j].rkey, 1);
        }
        for (int j = 0; j < n; ++j) {
            if (j == my_rank) continue;
            volatile int64_t* poll = (volatile int64_t*)rs_ctr[j];
            while (*poll < expect) { /* spin */
            }
        }
        __atomic_thread_fence(__ATOMIC_ACQUIRE);
        for (size_t i = 0; i < chunk; ++i) {
            float sum = input[(size_t)my_rank * chunk + i];
            for (int j = 0; j < n; ++j) {
                if (j == my_rank) continue;
                sum += rs_recv[j][par][i];
            }
            for (int j = 0; j < n; ++j) {
                if (j == my_rank) continue;
                reduced[j][i] = sum;
            }
            output[(size_t)my_rank * chunk + i] = sum;
        }
        for (int j = 0; j < n; ++j)
            if (j != my_rank) rdma_wait_atomic(conns[j]);

        // Phase 2 (AllGather): same pattern, broadcasting the completed chunk.
        for (int j = 0; j < n; ++j) {
            if (j == my_rank) continue;
            rdma_post_write_ex(conns[j], reduced[j], reduced_mr[j]->lkey, chunk * sizeof(float),
                               peer_ag[j][par].addr, peer_ag[j][par].rkey, 0);
            rdma_post_atomic_add(conns[j], peer_agc[j].addr, peer_agc[j].rkey, 1);
        }
        for (int j = 0; j < n; ++j) {
            if (j == my_rank) continue;
            volatile int64_t* poll = (volatile int64_t*)ag_ctr[j];
            while (*poll < expect) { /* spin */
            }
            __atomic_thread_fence(__ATOMIC_ACQUIRE);
            memcpy(output + (size_t)j * chunk, ag_recv[j][par], chunk * sizeof(float));
        }
        for (int j = 0; j < n; ++j)
            if (j != my_rank) rdma_wait_atomic(conns[j]);

        double elapsed = now_us() - t0;
        if (it == -1) {
            ok = verify(output, ref, M, 1e-3f);
            printf("Correctness check vs. CPU reference: %s\n", ok ? "PASSED" : "FAILED");
        } else {
            lat[it] = elapsed;
        }
    }
    rdma_print_stats("two-shot LL128 Atomic", lat, iters, n);
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
    free(ag_recv);
    free(rs_ctr);
    free(ag_ctr);
    free(peer_rs);
    free(peer_ag);
    free(peer_rsc);
    free(peer_agc);
    free(phase1_src);
    free(phase1_src_mr);
    free(reduced);
    free(reduced_mr);
    free(ips);
    return ok ? 0 : 1;
}
