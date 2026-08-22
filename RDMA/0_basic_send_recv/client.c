#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <rdma/rdma_cma.h>
#include <infiniband/verbs.h>

/* Replace with the server's real IP address (or an interface name resolves
 * via rdma_resolve_addr - a raw IP is simplest to start with). This must
 * be an address reachable over the RDMA-capable NIC, not just any NIC. */
#define SERVER_IP  "127.0.0.1"
#define PORT       20000
#define BUF_SIZE   1024
#define TIMEOUT_MS 2000

static int poll_cq(struct ibv_cq *cq, struct ibv_wc *wc) {
    int n;
    do {
        n = ibv_poll_cq(cq, 1, wc);
    } while (n == 0);
    if (n < 0) { fprintf(stderr, "ibv_poll_cq failed\n"); return -1; }
    if (wc->status != IBV_WC_SUCCESS) {
        fprintf(stderr, "work completion error: %s (opcode %d)\n",
                ibv_wc_status_str(wc->status), wc->opcode);
        return -1;
    }
    return 0;
}

int main(void) {
    struct rdma_event_channel *ec;
    struct rdma_cm_id *id;
    struct rdma_cm_event *event;
    struct sockaddr_in addr;

    struct ibv_pd *pd;
    struct ibv_cq *cq;
    struct ibv_qp_init_attr qp_attr;
    struct ibv_mr *send_mr, *recv_mr;
    char send_buf[BUF_SIZE] = "hello from client";
    char recv_buf[BUF_SIZE];

    ec = rdma_create_event_channel();
    if (!ec) { perror("rdma_create_event_channel"); exit(1); }

    if (rdma_create_id(ec, &id, NULL, RDMA_PS_TCP)) {
        perror("rdma_create_id"); exit(1);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, SERVER_IP, &addr.sin_addr) <= 0) {
        perror("inet_pton"); exit(1);
    }

    /* 1. rdma_resolve_addr(): the RDMA CM equivalent of the address-lookup
     *    half of connect(). Unlike plain TCP, RDMA needs to work out
     *    which local RDMA device/port can actually reach the destination
     *    BEFORE a queue pair can be created - a queue pair belongs to one
     *    specific device. This call is asynchronous; the result arrives
     *    as an event. */
    if (rdma_resolve_addr(id, NULL, (struct sockaddr *)&addr, TIMEOUT_MS)) {
        perror("rdma_resolve_addr"); exit(1);
    }
    if (rdma_get_cm_event(ec, &event)) { perror("rdma_get_cm_event"); exit(1); }
    if (event->event != RDMA_CM_EVENT_ADDR_RESOLVED) {
        fprintf(stderr, "client: unexpected event %s\n", rdma_event_str(event->event));
        exit(1);
    }
    rdma_ack_cm_event(event);

    /* 2. rdma_resolve_route(): find the actual network path to the
     *    destination over that device (for InfiniBand, the LID/path
     *    record; for RoCE, the Ethernet route). Also asynchronous. */
    if (rdma_resolve_route(id, TIMEOUT_MS)) {
        perror("rdma_resolve_route"); exit(1);
    }
    if (rdma_get_cm_event(ec, &event)) { perror("rdma_get_cm_event"); exit(1); }
    if (event->event != RDMA_CM_EVENT_ROUTE_RESOLVED) {
        fprintf(stderr, "client: unexpected event %s\n", rdma_event_str(event->event));
        exit(1);
    }
    rdma_ack_cm_event(event);

    /* 3. Only now does id->verbs point at a real device context, so only
     *    now can we build the PD/CQ/QP - identical setup to the server's. */
    pd = ibv_alloc_pd(id->verbs);
    if (!pd) { perror("ibv_alloc_pd"); exit(1); }

    cq = ibv_create_cq(id->verbs, 16, NULL, NULL, 0);
    if (!cq) { perror("ibv_create_cq"); exit(1); }

    memset(&qp_attr, 0, sizeof(qp_attr));
    qp_attr.send_cq = cq;
    qp_attr.recv_cq = cq;
    qp_attr.qp_type = IBV_QPT_RC;
    qp_attr.cap.max_send_wr = 4;
    qp_attr.cap.max_recv_wr = 4;
    qp_attr.cap.max_send_sge = 1;
    qp_attr.cap.max_recv_sge = 1;

    if (rdma_create_qp(id, pd, &qp_attr)) {
        perror("rdma_create_qp"); exit(1);
    }

    recv_mr = ibv_reg_mr(pd, recv_buf, BUF_SIZE, IBV_ACCESS_LOCAL_WRITE);
    if (!recv_mr) { perror("ibv_reg_mr (recv)"); exit(1); }
    send_mr = ibv_reg_mr(pd, send_buf, BUF_SIZE, 0);
    if (!send_mr) { perror("ibv_reg_mr (send)"); exit(1); }

    /* Post our RECV (for the echoed reply) before connecting, for the
     * same reason the server posts its RECV before accept()ing - the
     * reply could arrive the instant the connection is up. */
    struct ibv_sge recv_sge = {
        .addr = (uintptr_t)recv_buf, .length = BUF_SIZE, .lkey = recv_mr->lkey,
    };
    struct ibv_recv_wr recv_wr = { .wr_id = 1, .sg_list = &recv_sge, .num_sge = 1 };
    struct ibv_recv_wr *bad_recv_wr;
    if (ibv_post_recv(id->qp, &recv_wr, &bad_recv_wr)) {
        perror("ibv_post_recv"); exit(1);
    }

    /* 4. rdma_connect(): completes the handshake - the RDMA CM equivalent
     *    of TCP's connect(). */
    struct rdma_conn_param conn_param;
    memset(&conn_param, 0, sizeof(conn_param));
    if (rdma_connect(id, &conn_param)) {
        perror("rdma_connect"); exit(1);
    }
    if (rdma_get_cm_event(ec, &event)) { perror("rdma_get_cm_event"); exit(1); }
    if (event->event != RDMA_CM_EVENT_ESTABLISHED) {
        fprintf(stderr, "client: unexpected event %s\n", rdma_event_str(event->event));
        exit(1);
    }
    rdma_ack_cm_event(event);

    printf("client: connected to %s:%d\n", SERVER_IP, PORT);

    /* 5. Post our SEND. */
    size_t msg_len = strlen(send_buf);
    struct ibv_sge send_sge = {
        .addr = (uintptr_t)send_buf, .length = (uint32_t)msg_len, .lkey = send_mr->lkey,
    };
    struct ibv_send_wr send_wr = {
        .wr_id = 2, .sg_list = &send_sge, .num_sge = 1,
        .opcode = IBV_WR_SEND, .send_flags = IBV_SEND_SIGNALED,
    };
    struct ibv_send_wr *bad_send_wr;
    if (ibv_post_send(id->qp, &send_wr, &bad_send_wr)) {
        perror("ibv_post_send"); exit(1);
    }

    struct ibv_wc wc;
    if (poll_cq(cq, &wc)) exit(1); /* our SEND completing */
    printf("client: sent \"%s\" (%zu bytes)\n", send_buf, msg_len);

    if (poll_cq(cq, &wc)) exit(1); /* the server's reply landing in recv_buf */
    size_t reply_len = wc.byte_len;
    if (reply_len >= BUF_SIZE) reply_len = BUF_SIZE - 1;
    recv_buf[reply_len] = '\0';
    printf("client: server replied \"%s\" (%zu bytes)\n", recv_buf, reply_len);

    /* 6. rdma_disconnect(): tear down, mirroring close(). */
    rdma_disconnect(id);
    if (rdma_get_cm_event(ec, &event)) { perror("rdma_get_cm_event"); exit(1); }
    rdma_ack_cm_event(event); /* expect RDMA_CM_EVENT_DISCONNECTED */

    rdma_destroy_qp(id);
    ibv_dereg_mr(send_mr);
    ibv_dereg_mr(recv_mr);
    ibv_destroy_cq(cq);
    ibv_dealloc_pd(pd);
    rdma_destroy_id(id);
    rdma_destroy_event_channel(ec);

    return 0;
}
