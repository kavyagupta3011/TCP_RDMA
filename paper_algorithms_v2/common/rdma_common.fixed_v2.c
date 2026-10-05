// rdma_common.c -- see rdma_common.h for the design rationale.

#define _POSIX_C_SOURCE 200809L  // clock_gettime, posix_memalign under -std=c11

#include "rdma_common.h"

#include <errno.h>
#include <math.h>

#define CQ_DEPTH 32
#define QP_MAX_WR 32

static struct rdma_cm_event* wait_event(struct rdma_event_channel* ec,
                                         enum rdma_cm_event_type expected) {
    struct rdma_cm_event* event = NULL;
    RDMA_CHECK(rdma_get_cm_event(ec, &event) == 0, "rdma_get_cm_event failed");
    if (event->event != expected) {
        fprintf(stderr, "RDMA CM: expected event %d, got %d (%s)\n", expected, event->event,
                rdma_event_str(event->event));
        exit(1);
    }
    return event;
}

static void build_qp(RdmaConn* conn) {
    conn->pd = ibv_alloc_pd(conn->cm_id->verbs);
    RDMA_CHECK(conn->pd != NULL, "ibv_alloc_pd failed");

    conn->cq = ibv_create_cq(conn->cm_id->verbs, CQ_DEPTH, NULL, NULL, 0);
    RDMA_CHECK(conn->cq != NULL, "ibv_create_cq failed");

    struct ibv_qp_init_attr qp_attr;
    memset(&qp_attr, 0, sizeof(qp_attr));
    qp_attr.send_cq = conn->cq;
    qp_attr.recv_cq = conn->cq;
    qp_attr.qp_type = IBV_QPT_RC;
    qp_attr.cap.max_send_wr = QP_MAX_WR;
    qp_attr.cap.max_recv_wr = QP_MAX_WR;
    qp_attr.cap.max_send_sge = 1;
    qp_attr.cap.max_recv_sge = 1;
    qp_attr.cap.max_inline_data = 128;  // request; the HCA reports what it really granted below

    RDMA_CHECK(rdma_create_qp(conn->cm_id, conn->pd, &qp_attr) == 0, "rdma_create_qp failed");
    conn->qp = conn->cm_id->qp;
    conn->max_inline = qp_attr.cap.max_inline_data;
}

// Pre-posts a receive for the bootstrap RECV buffer -- required before the
// peer's matching SEND can land, standard RC-QP requirement.
static void post_boot_recv(RdmaConn* conn) {
    struct ibv_sge sge;
    sge.addr = (uintptr_t)conn->boot_recv_buf;
    sge.length = MAX_REGIONS * sizeof(RegionInfo);
    sge.lkey = conn->boot_recv_mr->lkey;

    struct ibv_recv_wr wr, *bad_wr = NULL;
    memset(&wr, 0, sizeof(wr));
    wr.wr_id = 0xB00710;  // "boot"
    wr.sg_list = &sge;
    wr.num_sge = 1;

    RDMA_CHECK(ibv_post_recv(conn->qp, &wr, &bad_wr) == 0, "ibv_post_recv (bootstrap) failed");
}

static void alloc_bootstrap_buffers(RdmaConn* conn) {
    conn->boot_send_buf = calloc(MAX_REGIONS, sizeof(RegionInfo));
    conn->boot_recv_buf = calloc(MAX_REGIONS, sizeof(RegionInfo));
    RDMA_CHECK(conn->boot_send_buf && conn->boot_recv_buf, "calloc (bootstrap buffers) failed");

    conn->boot_send_mr = ibv_reg_mr(conn->pd, conn->boot_send_buf, MAX_REGIONS * sizeof(RegionInfo),
                                     IBV_ACCESS_LOCAL_WRITE);
    conn->boot_recv_mr = ibv_reg_mr(conn->pd, conn->boot_recv_buf, MAX_REGIONS * sizeof(RegionInfo),
                                     IBV_ACCESS_LOCAL_WRITE);
    RDMA_CHECK(conn->boot_send_mr && conn->boot_recv_mr, "ibv_reg_mr (bootstrap buffers) failed");

    conn->atomic_result_buf = aligned_alloc(8, sizeof(int64_t));
    RDMA_CHECK(conn->atomic_result_buf != NULL, "aligned_alloc (atomic result) failed");
    conn->atomic_result_mr =
        ibv_reg_mr(conn->pd, conn->atomic_result_buf, sizeof(int64_t), IBV_ACCESS_LOCAL_WRITE);
    RDMA_CHECK(conn->atomic_result_mr != NULL, "ibv_reg_mr (atomic result) failed");

    post_boot_recv(conn);
}

RdmaConn* rdma_server_listen_only(const char* ip, int port) {
    RdmaConn* conn = calloc(1, sizeof(RdmaConn));
    conn->is_server = 1;

    conn->ec = rdma_create_event_channel();
    RDMA_CHECK(conn->ec != NULL, "rdma_create_event_channel failed");
    RDMA_CHECK(rdma_create_id(conn->ec, &conn->listen_id, NULL, RDMA_PS_TCP) == 0,
               "rdma_create_id (listen) failed");

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    RDMA_CHECK(inet_pton(AF_INET, ip, &addr.sin_addr) == 1, "inet_pton (server ip) failed");

    RDMA_CHECK(rdma_bind_addr(conn->listen_id, (struct sockaddr*)&addr) == 0,
               "rdma_bind_addr failed -- is this node's IPoIB address correct?");
    RDMA_CHECK(rdma_listen(conn->listen_id, 1) == 0, "rdma_listen failed");
    printf("[server] listening on %s:%d ...\n", ip, port);
    return conn;
}

void rdma_server_accept_one(RdmaConn* conn) {
    struct rdma_cm_event* ev = wait_event(conn->ec, RDMA_CM_EVENT_CONNECT_REQUEST);
    conn->cm_id = ev->id;
    rdma_ack_cm_event(ev);

    build_qp(conn);
    alloc_bootstrap_buffers(conn);

    struct rdma_conn_param cp;
    memset(&cp, 0, sizeof(cp));
    cp.initiator_depth = 1;  // needed for this side to ISSUE atomics/RDMA reads
    cp.responder_resources = 1;  // needed for this side to be the TARGET of atomics/reads
    cp.rnr_retry_count = 7;
    RDMA_CHECK(rdma_accept(conn->cm_id, &cp) == 0, "rdma_accept failed");

    ev = wait_event(conn->ec, RDMA_CM_EVENT_ESTABLISHED);
    rdma_ack_cm_event(ev);
    printf("[server] connection established.\n");
}

RdmaConn* rdma_server_accept(const char* ip, int port) {
    RdmaConn* conn = rdma_server_listen_only(ip, port);
    rdma_server_accept_one(conn);
    return conn;
}

RdmaConn* rdma_client_connect(const char* server_ip, int port) {
    RdmaConn* conn = calloc(1, sizeof(RdmaConn));
    conn->is_server = 0;

    conn->ec = rdma_create_event_channel();
    RDMA_CHECK(conn->ec != NULL, "rdma_create_event_channel failed");
    RDMA_CHECK(rdma_create_id(conn->ec, &conn->cm_id, NULL, RDMA_PS_TCP) == 0,
               "rdma_create_id (client) failed");

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    RDMA_CHECK(inet_pton(AF_INET, server_ip, &addr.sin_addr) == 1,
               "inet_pton (server ip) failed");

    RDMA_CHECK(rdma_resolve_addr(conn->cm_id, NULL, (struct sockaddr*)&addr, 2000) == 0,
               "rdma_resolve_addr failed");
    struct rdma_cm_event* ev = wait_event(conn->ec, RDMA_CM_EVENT_ADDR_RESOLVED);
    rdma_ack_cm_event(ev);

    RDMA_CHECK(rdma_resolve_route(conn->cm_id, 2000) == 0, "rdma_resolve_route failed");
    ev = wait_event(conn->ec, RDMA_CM_EVENT_ROUTE_RESOLVED);
    rdma_ack_cm_event(ev);

    build_qp(conn);
    alloc_bootstrap_buffers(conn);

    struct rdma_conn_param cp;
    memset(&cp, 0, sizeof(cp));
    cp.initiator_depth = 1;
    cp.responder_resources = 1;
    cp.retry_count = 7;
    cp.rnr_retry_count = 7;
    printf("[client] connecting to %s:%d ...\n", server_ip, port);
    RDMA_CHECK(rdma_connect(conn->cm_id, &cp) == 0, "rdma_connect failed");

    ev = wait_event(conn->ec, RDMA_CM_EVENT_ESTABLISHED);
    rdma_ack_cm_event(ev);
    printf("[client] connection established.\n");
    return conn;
}

// Deterministic, collision-free port for pair (a,b), a<b, computed
// identically by every rank from n and base_port alone -- no coordination
// needed to agree who listens where.
static int mesh_port(int a, int b, int n, int base_port) {
    return base_port + a * n + b;
}

RdmaConn** rdma_mesh_connect(int my_rank, int n, const char** ips, int base_port) {
    RDMA_CHECK(my_rank >= 0 && my_rank < n, "rdma_mesh_connect: my_rank out of range");
    RdmaConn** result = calloc((size_t)n, sizeof(RdmaConn*));
    RDMA_CHECK(result != NULL, "calloc (mesh result) failed");

    // Phase A: start every listener this rank will ever need -- one per
    // pair (my_rank, j) with j > my_rank -- BEFORE any rank tries to
    // connect to any of them. This is what makes the "no MPI, just start
    // every process by hand, in any order" launch model safe: there is no
    // window where a client could reach a server that hasn't called
    // rdma_listen() yet.
    for (int j = my_rank + 1; j < n; ++j) {
        result[j] = rdma_server_listen_only(ips[my_rank], mesh_port(my_rank, j, n, base_port));
    }

    // Phase B: every rank walks the exact same pair sequence (0,1),(0,2),
    // ...,(0,n-1),(1,2),...,(n-2,n-1) and only acts on the pairs it's a
    // member of -- so "which connection is this" is never ambiguous, with
    // zero messages exchanged to agree on it.
    for (int a = 0; a < n; ++a) {
        for (int b = a + 1; b < n; ++b) {
            if (my_rank == a) {
                printf("[mesh] accepting connection from rank %d ...\n", b);
                rdma_server_accept_one(result[b]);
            } else if (my_rank == b) {
                printf("[mesh] connecting to rank %d (%s) ...\n", a, ips[a]);
                result[a] = rdma_client_connect(ips[a], mesh_port(a, b, n, base_port));
            }
            // else: pair (a,b) doesn't involve my_rank -- both actual
            // members handle it independently; this rank just moves on to
            // the next pair in the shared sequence.
        }
    }

    printf("[mesh] rank %d: full mesh established with %d peer(s).\n", my_rank, n - 1);
    return result;
}

void rdma_mesh_close(RdmaConn** conns, int n, int my_rank) {
    for (int j = 0; j < n; ++j) {
        if (j == my_rank) continue;
        if (conns[j]) rdma_close(conns[j]);
    }
    free(conns);
}

void* rdma_reg_buffer(RdmaConn* conn, size_t size, struct ibv_mr** mr_out) {
    void* buf = NULL;
    RDMA_CHECK(posix_memalign(&buf, 64, size) == 0, "posix_memalign failed");
    memset(buf, 0, size);
    int access = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ |
                 IBV_ACCESS_REMOTE_ATOMIC;
    struct ibv_mr* mr = ibv_reg_mr(conn->pd, buf, size, access);
    RDMA_CHECK(mr != NULL, "ibv_reg_mr failed");
    *mr_out = mr;
    return buf;
}

static void poll_one(struct ibv_cq* cq, uint64_t expect_wr_id) {
    struct ibv_wc wc;
    int n;
    do {
        n = ibv_poll_cq(cq, 1, &wc);
    } while (n == 0);
    RDMA_CHECK(n >= 0, "ibv_poll_cq failed");
    if (wc.status != IBV_WC_SUCCESS) {
        fprintf(stderr, "RDMA work completion error: %s (wr_id=%llu, expected=%llu)\n",
                ibv_wc_status_str(wc.status), (unsigned long long)wc.wr_id,
                (unsigned long long)expect_wr_id);
        exit(1);
    }
}

void rdma_exchange_regions(RdmaConn* conn, RegionInfo* local, int n, RegionInfo* remote_out) {
    RDMA_CHECK(n <= MAX_REGIONS, "rdma_exchange_regions: n exceeds MAX_REGIONS");
    memcpy(conn->boot_send_buf, local, n * sizeof(RegionInfo));

    struct ibv_sge sge;
    sge.addr = (uintptr_t)conn->boot_send_buf;
    sge.length = n * sizeof(RegionInfo);
    sge.lkey = conn->boot_send_mr->lkey;

    struct ibv_send_wr wr, *bad_wr = NULL;
    memset(&wr, 0, sizeof(wr));
    wr.wr_id = 0x5E0D;  // "send"
    wr.sg_list = &sge;
    wr.num_sge = 1;
    wr.opcode = IBV_WR_SEND;
    wr.send_flags = IBV_SEND_SIGNALED;

    RDMA_CHECK(ibv_post_send(conn->qp, &wr, &bad_wr) == 0,
               "ibv_post_send (region exchange) failed");

    // The RECV was already posted (post_boot_recv, called from
    // alloc_bootstrap_buffers / after each prior exchange) -- wait for
    // both: our SEND completing, and the peer's SEND landing in our RECV.
    poll_one(conn->cq, 0x5E0D);
    poll_one(conn->cq, 0xB00710);

    memcpy(remote_out, conn->boot_recv_buf, n * sizeof(RegionInfo));
    post_boot_recv(conn);  // re-arm for the next exchange/barrier call
}

void rdma_write(RdmaConn* conn, void* local_addr, uint32_t lkey, size_t len,
                 uint64_t remote_addr, uint32_t rkey) {
    struct ibv_sge sge;
    sge.addr = (uintptr_t)local_addr;
    sge.length = (uint32_t)len;
    sge.lkey = lkey;

    struct ibv_send_wr wr, *bad_wr = NULL;
    memset(&wr, 0, sizeof(wr));
    wr.wr_id = 0x101E;  // "write"
    wr.sg_list = &sge;
    wr.num_sge = 1;
    wr.opcode = IBV_WR_RDMA_WRITE;
    wr.send_flags = IBV_SEND_SIGNALED;
    wr.wr.rdma.remote_addr = remote_addr;
    wr.wr.rdma.rkey = rkey;

    RDMA_CHECK(ibv_post_send(conn->qp, &wr, &bad_wr) == 0, "ibv_post_send (RDMA WRITE) failed");
    poll_one(conn->cq, 0x101E);
}

void rdma_post_write(RdmaConn* conn, void* local_addr, uint32_t lkey, size_t len,
                      uint64_t remote_addr, uint32_t rkey) {
    struct ibv_sge sge;
    sge.addr = (uintptr_t)local_addr;
    sge.length = (uint32_t)len;
    sge.lkey = lkey;

    struct ibv_send_wr wr, *bad_wr = NULL;
    memset(&wr, 0, sizeof(wr));
    wr.wr_id = 0x101E;  // same id as rdma_write(): both are "a write completed"
    wr.sg_list = &sge;
    wr.num_sge = 1;
    wr.opcode = IBV_WR_RDMA_WRITE;
    wr.send_flags = IBV_SEND_SIGNALED;
    if (len <= conn->max_inline) wr.send_flags |= IBV_SEND_INLINE;
    wr.wr.rdma.remote_addr = remote_addr;
    wr.wr.rdma.rkey = rkey;

    RDMA_CHECK(ibv_post_send(conn->qp, &wr, &bad_wr) == 0, "ibv_post_send (RDMA WRITE) failed");
}

void rdma_wait_write(RdmaConn* conn) { poll_one(conn->cq, 0x101E); }

void rdma_atomic_fetch_add(RdmaConn* conn, uint64_t remote_addr, uint32_t rkey, int64_t add_val,
                            int64_t* old_val_out) {
    RDMA_CHECK(remote_addr % 8 == 0, "rdma_atomic_fetch_add: remote_addr must be 8-byte aligned");

    struct ibv_sge sge;
    sge.addr = (uintptr_t)conn->atomic_result_buf;
    sge.length = sizeof(int64_t);
    sge.lkey = conn->atomic_result_mr->lkey;

    struct ibv_send_wr wr, *bad_wr = NULL;
    memset(&wr, 0, sizeof(wr));
    wr.wr_id = 0xA70A1C;  // "atomic"
    wr.sg_list = &sge;
    wr.num_sge = 1;
    wr.opcode = IBV_WR_ATOMIC_FETCH_AND_ADD;
    wr.send_flags = IBV_SEND_SIGNALED;
    wr.wr.atomic.remote_addr = remote_addr;
    wr.wr.atomic.rkey = rkey;
    wr.wr.atomic.compare_add = add_val;

    RDMA_CHECK(ibv_post_send(conn->qp, &wr, &bad_wr) == 0,
               "ibv_post_send (ATOMIC_FETCH_AND_ADD) failed -- does this HCA/link support RDMA "
               "atomics? (mlx4 does)");
    poll_one(conn->cq, 0xA70A1C);
    *old_val_out = *conn->atomic_result_buf;
}

void rdma_barrier(RdmaConn* conn) {
    RegionInfo dummy_local[1] = {{0, 0, 0}};
    RegionInfo dummy_remote[1];
    rdma_exchange_regions(conn, dummy_local, 1, dummy_remote);
}

double now_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e6 + (double)ts.tv_nsec / 1e3;
}

float* make_random_vector(size_t n, unsigned seed) {
    float* v = malloc(n * sizeof(float));
    RDMA_CHECK(v != NULL, "malloc (random vector) failed");
    unsigned state = seed;
    for (size_t i = 0; i < n; ++i) {
        // Simple xorshift-ish LCG, portable and deterministic across runs
        // for a given seed -- good enough for a reference-vector generator.
        state = state * 1103515245u + 12345u;
        float u = (float)((state >> 8) & 0xFFFFFF) / (float)0xFFFFFF;  // [0,1)
        v[i] = u * 2.0f - 1.0f;                                        // [-1,1)
    }
    return v;
}

void reference_sum(const float* a, const float* b, float* out, size_t n) {
    for (size_t i = 0; i < n; ++i) out[i] = a[i] + b[i];
}

void reference_sum_n(float** vectors, int n_vectors, float* out, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        float s = 0.0f;
        for (int v = 0; v < n_vectors; ++v) s += vectors[v][i];
        out[i] = s;
    }
}

int verify(const float* got, const float* ref, size_t n, float rel_tol) {
    size_t bad = 0;
    for (size_t i = 0; i < n; ++i) {
        float diff = fabsf(got[i] - ref[i]);
        float scale = fmaxf(1.0f, fabsf(ref[i]));
        if (diff / scale > rel_tol) {
            if (bad < 5) fprintf(stderr, "  mismatch at %zu: got %f, want %f\n", i, got[i], ref[i]);
            bad++;
        }
    }
    if (bad) fprintf(stderr, "verify: %zu / %zu elements mismatched\n", bad, n);
    return bad == 0;
}

void rdma_close(RdmaConn* conn) {
    if (!conn) return;
    if (conn->cm_id) rdma_disconnect(conn->cm_id);
    if (conn->qp) rdma_destroy_qp(conn->cm_id);
    if (conn->cq) ibv_destroy_cq(conn->cq);
    if (conn->boot_send_mr) ibv_dereg_mr(conn->boot_send_mr);
    if (conn->boot_recv_mr) ibv_dereg_mr(conn->boot_recv_mr);
    if (conn->atomic_result_mr) ibv_dereg_mr(conn->atomic_result_mr);
    if (conn->pd) ibv_dealloc_pd(conn->pd);
    if (conn->cm_id) rdma_destroy_id(conn->cm_id);
    if (conn->listen_id) rdma_destroy_id(conn->listen_id);
    if (conn->ec) rdma_destroy_event_channel(conn->ec);
    free(conn->boot_send_buf);
    free(conn->boot_recv_buf);
    free(conn->atomic_result_buf);
    free(conn);
}
