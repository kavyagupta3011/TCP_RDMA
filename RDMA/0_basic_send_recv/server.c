#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <rdma/rdma_cma.h>
#include <infiniband/verbs.h>

#define PORT     20000
#define BUF_SIZE 1024

/* poll_cq(): busy-wait until exactly one work completion shows up on cq,
 * then return it. Polling (instead of blocking on an event) is the
 * lowest-latency way to consume completions and is what most RDMA code
 * does on the hot path - it burns CPU while waiting, which is a fair
 * trade when microseconds matter. (The alternative - completion
 * channels / ibv_get_cq_event - lets you block without spinning, at the
 * cost of some latency. Out of scope for these first examples.) */
static int poll_cq(struct ibv_cq *cq, struct ibv_wc *wc) {
    int n;
    do {
        n = ibv_poll_cq(cq, 1, wc);
    } while (n == 0);

    if (n < 0) {
        fprintf(stderr, "ibv_poll_cq failed\n");
        return -1;
    }
    if (wc->status != IBV_WC_SUCCESS) {
        fprintf(stderr, "work completion error: %s (opcode %d)\n",
                ibv_wc_status_str(wc->status), wc->opcode);
        return -1;
    }
    return 0;
}

int main(void) {
    struct rdma_event_channel *ec;
    struct rdma_cm_id *listen_id, *conn_id;
    struct rdma_cm_event *event;
    struct sockaddr_in addr;

    struct ibv_pd *pd;
    struct ibv_cq *cq;
    struct ibv_qp_init_attr qp_attr;
    struct ibv_mr *recv_mr, *send_mr;
    char recv_buf[BUF_SIZE];
    char send_buf[BUF_SIZE];

    /* 1. rdma_create_event_channel(): every RDMA CM state change (address
     *    resolved, route resolved, connection request, established,
     *    disconnected...) gets reported here. This is the RDMA CM
     *    equivalent of "the socket itself" in the TCP world for
     *    CONNECTION SETUP - but actual DATA never flows through this
     *    channel. Data transfer is entirely the queue pair's job, and
     *    that's a completely separate path once the connection is up. */
    ec = rdma_create_event_channel();
    if (!ec) { perror("rdma_create_event_channel"); exit(1); }

    /* 2. rdma_create_id(): allocate a connection identifier - the RDMA CM
     *    analogue of socket(). RDMA_PS_TCP just requests TCP-like
     *    (reliable, connection-oriented) semantics for the connection -
     *    no actual TCP is involved in the data path. */
    if (rdma_create_id(ec, &listen_id, NULL, RDMA_PS_TCP)) {
        perror("rdma_create_id"); exit(1);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    /* 3. rdma_bind_addr() + rdma_listen(): the RDMA CM equivalents of
     *    bind() + listen(). */
    if (rdma_bind_addr(listen_id, (struct sockaddr *)&addr)) {
        perror("rdma_bind_addr"); exit(1);
    }
    if (rdma_listen(listen_id, 1)) {
        perror("rdma_listen"); exit(1);
    }

    printf("server: listening on port %d (RDMA CM)...\n", PORT);

    /* 4. Wait for a connection request. Every step of the RDMA CM
     *    handshake shows up as an event pulled off the channel with
     *    rdma_get_cm_event() - it blocks until something happens. */
    if (rdma_get_cm_event(ec, &event)) { perror("rdma_get_cm_event"); exit(1); }
    if (event->event != RDMA_CM_EVENT_CONNECT_REQUEST) {
        fprintf(stderr, "server: unexpected event %s\n", rdma_event_str(event->event));
        exit(1);
    }

    /* event->id is a NEW rdma_cm_id representing this specific incoming
     * connection - distinct from listen_id, which just keeps listening
     * for more. Save it before acking: the `event` struct itself becomes
     * invalid after the ack, but the id it points to stays alive. */
    conn_id = event->id;
    rdma_ack_cm_event(event);

    /* 5. Now that we have a concrete connection, conn_id->verbs is a
     *    valid ibv_context tied to the real RDMA device. Build the
     *    objects a TCP server never needed: a protection domain, a
     *    completion queue, and a queue pair. */
    pd = ibv_alloc_pd(conn_id->verbs);
    if (!pd) { perror("ibv_alloc_pd"); exit(1); }

    cq = ibv_create_cq(conn_id->verbs, 16, NULL, NULL, 0);
    if (!cq) { perror("ibv_create_cq"); exit(1); }

    memset(&qp_attr, 0, sizeof(qp_attr));
    qp_attr.send_cq = cq;
    qp_attr.recv_cq = cq;         /* sharing one CQ for send and recv completions */
    qp_attr.qp_type = IBV_QPT_RC; /* Reliable Connection */
    qp_attr.cap.max_send_wr = 4;
    qp_attr.cap.max_recv_wr = 4;
    qp_attr.cap.max_send_sge = 1;
    qp_attr.cap.max_recv_sge = 1;

    /* rdma_create_qp() creates the actual queue pair and attaches it to
     * conn_id (usable afterward as conn_id->qp) - the RDMA CM convenience
     * wrapper around plain ibv_create_qp() plus the manual QP state
     * transitions (RESET -> INIT -> RTR -> RTS) you'd otherwise have to
     * drive yourself with ibv_modify_qp(). */
    if (rdma_create_qp(conn_id, pd, &qp_attr)) {
        perror("rdma_create_qp"); exit(1);
    }

    /* Register the buffer we'll receive into. Registration pins the
     * memory and gives the NIC permission to touch it; IBV_ACCESS_LOCAL_WRITE
     * is required on any MR our OWN queue pair will write incoming data
     * into (which is exactly what a RECV does). */
    recv_mr = ibv_reg_mr(pd, recv_buf, BUF_SIZE, IBV_ACCESS_LOCAL_WRITE);
    if (!recv_mr) { perror("ibv_reg_mr (recv)"); exit(1); }

    /* 6. Post a RECV work request BEFORE accepting the connection. This
     *    matters: the instant the connection is established, the client
     *    might SEND immediately - and a SEND with no matching posted
     *    RECV on the other side is an error. There's no kernel buffer
     *    quietly absorbing it the way TCP would. */
    struct ibv_sge recv_sge = {
        .addr   = (uintptr_t)recv_buf,
        .length = BUF_SIZE,
        .lkey   = recv_mr->lkey,
    };
    struct ibv_recv_wr recv_wr = {
        .wr_id   = 1,
        .sg_list = &recv_sge,
        .num_sge = 1,
    };
    struct ibv_recv_wr *bad_recv_wr;
    if (ibv_post_recv(conn_id->qp, &recv_wr, &bad_recv_wr)) {
        perror("ibv_post_recv"); exit(1);
    }

    /* 7. rdma_accept(): the RDMA CM equivalent of accept() completing the
     *    handshake. A zeroed conn_param means "default QP settings, no
     *    private data" - later examples (RDMA READ, atomics) will fill
     *    in responder_resources here, since those operations need it. */
    struct rdma_conn_param conn_param;
    memset(&conn_param, 0, sizeof(conn_param));
    if (rdma_accept(conn_id, &conn_param)) {
        perror("rdma_accept"); exit(1);
    }

    /* Wait for confirmation the connection is fully up. */
    if (rdma_get_cm_event(ec, &event)) { perror("rdma_get_cm_event"); exit(1); }
    if (event->event != RDMA_CM_EVENT_ESTABLISHED) {
        fprintf(stderr, "server: unexpected event %s\n", rdma_event_str(event->event));
        exit(1);
    }
    rdma_ack_cm_event(event);

    printf("server: connection established\n");

    /* 8. This is the two-sided SEND/RECV operation - the closest RDMA
     *    equivalent to TCP's recv()/send(). We already posted our RECV
     *    above; now we just poll the completion queue until the client's
     *    SEND actually lands. Notice there's no "read whatever bytes
     *    happen to be available" - the RECV work request pre-declared
     *    exactly where incoming data could go, and the completion tells
     *    us exactly how many bytes actually arrived (wc.byte_len). */
    struct ibv_wc wc;
    if (poll_cq(cq, &wc)) exit(1);

    size_t recv_len = wc.byte_len;
    if (recv_len >= BUF_SIZE) recv_len = BUF_SIZE - 1;
    recv_buf[recv_len] = '\0';
    printf("server: received \"%s\" (%zu bytes)\n", recv_buf, recv_len);

    /* Echo it back with a SEND of our own. */
    memcpy(send_buf, recv_buf, recv_len);
    send_mr = ibv_reg_mr(pd, send_buf, BUF_SIZE, 0); /* source-only: no special access needed */
    if (!send_mr) { perror("ibv_reg_mr (send)"); exit(1); }

    struct ibv_sge send_sge = {
        .addr   = (uintptr_t)send_buf,
        .length = (uint32_t)recv_len,
        .lkey   = send_mr->lkey,
    };
    struct ibv_send_wr send_wr = {
        .wr_id      = 2,
        .sg_list    = &send_sge,
        .num_sge    = 1,
        .opcode     = IBV_WR_SEND,
        .send_flags = IBV_SEND_SIGNALED, /* ask for a completion entry once this SEND lands */
    };
    struct ibv_send_wr *bad_send_wr;
    if (ibv_post_send(conn_id->qp, &send_wr, &bad_send_wr)) {
        perror("ibv_post_send"); exit(1);
    }
    if (poll_cq(cq, &wc)) exit(1);
    printf("server: echoed reply back to client\n");

    /* 9. Wait for the client to disconnect, then tear everything down.
     *    Order matters: destroy the QP (and with it, any references to
     *    the CQ/PD) before freeing the CQ/PD/MRs they depend on. */
    if (rdma_get_cm_event(ec, &event)) { perror("rdma_get_cm_event"); exit(1); }
    if (event->event == RDMA_CM_EVENT_DISCONNECTED) {
        printf("server: client disconnected\n");
    }
    rdma_ack_cm_event(event);

    rdma_destroy_qp(conn_id);
    ibv_dereg_mr(recv_mr);
    ibv_dereg_mr(send_mr);
    ibv_destroy_cq(cq);
    ibv_dealloc_pd(pd);
    rdma_destroy_id(conn_id);
    rdma_destroy_id(listen_id);
    rdma_destroy_event_channel(ec);

    printf("server: done, exiting\n");
    return 0;
}
