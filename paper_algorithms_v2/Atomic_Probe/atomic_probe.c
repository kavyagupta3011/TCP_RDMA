// atomic_probe.c
//
// Diagnostic only -- NOT an AllReduce algorithm. LL128_Atomic fails on this
// cluster with ibv_post_send(ATOMIC_FETCH_AND_ADD) returning EINVAL (22). This
// program isolates why by (1) printing what the HCA/QP report about atomics and
// (2) trying several variants of an atomic post on one connection, printing the
// raw return code of each instead of exiting on the first failure.
//
// Usage (launch once per rank, same arguments as the algorithms):
//   ./atomic_probe <my_rank> <num_ranks> <base_port> <ip0> ... <ip(N-1)> [ignored...]

#include "../common/rdma_common.h"

#define PROBE(...)                \
    do {                          \
        printf("PROBE " __VA_ARGS__); \
        fflush(stdout);           \
    } while (0)

// Posts one atomic with full control over the knobs, polls its completion, and
// returns the post_send return code (0 = posted), or -1 if the completion
// failed / timed out. Never exits.
static int try_atomic(RdmaConn* conn, enum ibv_wr_opcode op, uint64_t remote_addr, uint32_t rkey,
                      void* local_buf, uint32_t lkey, int send_flags, const char* name) {
    struct ibv_sge sge;
    sge.addr = (uintptr_t)local_buf;
    sge.length = sizeof(int64_t);
    sge.lkey = lkey;

    struct ibv_send_wr wr, *bad_wr = NULL;
    memset(&wr, 0, sizeof(wr));
    wr.wr_id = 0xA70A1C;
    wr.sg_list = &sge;
    wr.num_sge = 1;
    wr.opcode = op;
    wr.send_flags = send_flags;
    wr.wr.atomic.remote_addr = remote_addr;
    wr.wr.atomic.rkey = rkey;
    wr.wr.atomic.compare_add = 1;
    wr.wr.atomic.swap = 0;

    int rc = ibv_post_send(conn->qp, &wr, &bad_wr);
    if (rc != 0) {
        PROBE("%-48s post_send rc=%d (%s)\n", name, rc, strerror(rc));
        return rc;
    }
    struct ibv_wc wc;
    double t0 = now_us();
    int n;
    while ((n = ibv_poll_cq(conn->cq, 1, &wc)) == 0) {
        if (now_us() - t0 > 2e6) {
            PROBE("%-48s posted OK but NO completion after 2 s\n", name);
            return -1;
        }
    }
    if (n < 0 || wc.status != IBV_WC_SUCCESS) {
        PROBE("%-48s posted OK but completion status=%d (%s)\n", name, n < 0 ? -1 : (int)wc.status,
              n < 0 ? "poll error" : ibv_wc_status_str(wc.status));
        return -1;
    }
    PROBE("%-48s OK\n", name);
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 5) {
        fprintf(stderr, "Usage: %s <my_rank> <num_ranks> <base_port> <ip0> ... <ip(N-1)>\n", argv[0]);
        return 1;
    }
    int my_rank = atoi(argv[1]);
    int n = atoi(argv[2]);
    int base_port = atoi(argv[3]);
    if (n < 2 || my_rank < 0 || my_rank >= n || argc < 4 + n) {
        fprintf(stderr, "bad arguments\n");
        return 1;
    }
    const char** ips = malloc((size_t)n * sizeof(char*));
    for (int i = 0; i < n; ++i) ips[i] = argv[4 + i];

    RdmaConn** conns = rdma_mesh_connect(my_rank, n, ips, base_port);

    // Everything below uses just ONE peer connection (the lowest-numbered peer).
    int peer = (my_rank == 0) ? 1 : 0;
    RdmaConn* c = conns[peer];

    // ---- what does the device / QP say about atomics? ----
    struct ibv_device_attr dattr;
    memset(&dattr, 0, sizeof(dattr));
    if (ibv_query_device(c->cm_id->verbs, &dattr) == 0) {
        PROBE("device atomic_cap=%d (0=NONE 1=HCA 2=GLOB) max_qp_rd_atom=%d max_qp_init_rd_atom=%d\n",
              (int)dattr.atomic_cap, dattr.max_qp_rd_atom, dattr.max_qp_init_rd_atom);
    } else {
        PROBE("ibv_query_device failed\n");
    }
    struct ibv_qp_attr qattr;
    struct ibv_qp_init_attr qinit;
    memset(&qattr, 0, sizeof(qattr));
    if (ibv_query_qp(c->qp, &qattr,
                     IBV_QP_STATE | IBV_QP_ACCESS_FLAGS | IBV_QP_MAX_QP_RD_ATOMIC |
                         IBV_QP_MAX_DEST_RD_ATOMIC | IBV_QP_CAP,
                     &qinit) == 0) {
        PROBE("QP state=%d (3=RTS) access_flags=0x%x (0x8=REMOTE_ATOMIC) max_rd_atomic=%d "
              "max_dest_rd_atomic=%d max_inline=%u max_send_sge=%u\n",
              (int)qattr.qp_state, (unsigned)qattr.qp_access_flags, (int)qattr.max_rd_atomic,
              (int)qattr.max_dest_rd_atomic, (unsigned)qinit.cap.max_inline_data,
              (unsigned)qinit.cap.max_send_sge);
    } else {
        PROBE("ibv_query_qp failed\n");
    }

    // ---- remote counter + an extra local result buffer, exchanged over the connection ----
    struct ibv_mr *ctr_mr, *res_mr, *data_mr;
    int64_t* ctr = rdma_reg_buffer(c, sizeof(int64_t), &ctr_mr);
    int64_t* res = rdma_reg_buffer(c, sizeof(int64_t), &res_mr);  // fresh 64B-aligned result buffer
    char* data = rdma_reg_buffer(c, 64, &data_mr);
    RegionInfo local[2] = {
        {(uint64_t)(uintptr_t)ctr, ctr_mr->rkey, sizeof(int64_t)},
        {(uint64_t)(uintptr_t)data, data_mr->rkey, 64},
    };
    RegionInfo remote[2];
    rdma_exchange_regions(c, local, 2, remote);
    rdma_barrier(c);

    // Only the lower rank of the pair issues the tests; the other just hosts the counter.
    if (my_rank < peer) {
        PROBE("rank %d -> peer %d, remote counter addr=%llx rkey=%u\n", my_rank, peer,
              (unsigned long long)remote[0].addr, remote[0].rkey);

        try_atomic(c, IBV_WR_ATOMIC_FETCH_AND_ADD, remote[0].addr, remote[0].rkey,
                   c->atomic_result_buf, c->atomic_result_mr->lkey, IBV_SEND_SIGNALED,
                   "1 FETCH_AND_ADD, library result buf");
        try_atomic(c, IBV_WR_ATOMIC_FETCH_AND_ADD, remote[0].addr, remote[0].rkey, res,
                   res_mr->lkey, IBV_SEND_SIGNALED, "2 FETCH_AND_ADD, fresh 64B-aligned result buf");
        try_atomic(c, IBV_WR_ATOMIC_CMP_AND_SWP, remote[0].addr, remote[0].rkey, res, res_mr->lkey,
                   IBV_SEND_SIGNALED, "3 CMP_AND_SWP");

        // Does a preceding unsignaled INLINE write (what LL128_Atomic does) matter?
        rdma_post_write_ex(c, data, data_mr->lkey, 44, remote[1].addr, remote[1].rkey, 0);
        try_atomic(c, IBV_WR_ATOMIC_FETCH_AND_ADD, remote[0].addr, remote[0].rkey,
                   c->atomic_result_buf, c->atomic_result_mr->lkey, IBV_SEND_SIGNALED,
                   "4 FETCH_AND_ADD after unsignaled inline WRITE");

        PROBE("remote counter should now equal the number of OK atomics above; reading it needs the "
              "peer (see its line)\n");
    }
    rdma_barrier(c);
    if (my_rank > peer) PROBE("rank %d (counter host) final counter value = %lld\n", my_rank, (long long)*ctr);
    rdma_barrier(c);

    rdma_mesh_close(conns, n, my_rank);
    free(ips);
    return 0;
}
