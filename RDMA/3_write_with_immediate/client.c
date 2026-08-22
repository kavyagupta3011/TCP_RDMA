#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <rdma/rdma_cma.h>
#include <infiniband/verbs.h>

#define SERVER_IP  "127.0.0.1"
#define PORT       20003
#define BUF_SIZE   1024
#define TIMEOUT_MS 2000
#define IMM_VALUE  0xCAFEBABEu

struct mr_info {
    uint64_t addr;
    uint32_t rkey;
} __attribute__((packed));

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

    char write_buf[BUF_SIZE] =
        "this arrived via WRITE_WITH_IMM - one round trip, no extra SEND needed";
    struct ibv_mr *write_mr;

    ec = rdma_create_event_channel();
    if (!ec) { perror("rdma_create_event_channel"); exit(1); }
    if (rdma_create_id(ec, &id, NULL, RDMA_PS_TCP)) { perror("rdma_create_id"); exit(1); }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, SERVER_IP, &addr.sin_addr) <= 0) { perror("inet_pton"); exit(1); }

    if (rdma_resolve_addr(id, NULL, (struct sockaddr *)&addr, TIMEOUT_MS)) {
        perror("rdma_resolve_addr"); exit(1);
    }
    if (rdma_get_cm_event(ec, &event)) { perror("rdma_get_cm_event"); exit(1); }
    if (event->event != RDMA_CM_EVENT_ADDR_RESOLVED) {
        fprintf(stderr, "client: unexpected event %s\n", rdma_event_str(event->event)); exit(1);
    }
    rdma_ack_cm_event(event);

    if (rdma_resolve_route(id, TIMEOUT_MS)) { perror("rdma_resolve_route"); exit(1); }
    if (rdma_get_cm_event(ec, &event)) { perror("rdma_get_cm_event"); exit(1); }
    if (event->event != RDMA_CM_EVENT_ROUTE_RESOLVED) {
        fprintf(stderr, "client: unexpected event %s\n", rdma_event_str(event->event)); exit(1);
    }
    rdma_ack_cm_event(event);

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
    if (rdma_create_qp(id, pd, &qp_attr)) { perror("rdma_create_qp"); exit(1); }

    write_mr = ibv_reg_mr(pd, write_buf, BUF_SIZE, 0);
    if (!write_mr) { perror("ibv_reg_mr (write)"); exit(1); }

    struct rdma_conn_param conn_param;
    memset(&conn_param, 0, sizeof(conn_param));
    if (rdma_connect(id, &conn_param)) { perror("rdma_connect"); exit(1); }

    if (rdma_get_cm_event(ec, &event)) { perror("rdma_get_cm_event"); exit(1); }
    if (event->event != RDMA_CM_EVENT_ESTABLISHED) {
        fprintf(stderr, "client: unexpected event %s\n", rdma_event_str(event->event)); exit(1);
    }
    if (event->param.conn.private_data_len < sizeof(struct mr_info)) {
        fprintf(stderr, "client: server did not send memory info\n"); exit(1);
    }
    struct mr_info info;
    memcpy(&info, event->param.conn.private_data, sizeof(info));
    rdma_ack_cm_event(event);

    printf("client: connected to %s:%d - target %p (rkey 0x%x)\n",
           SERVER_IP, PORT, (void *)(uintptr_t)info.addr, info.rkey);

    /* One operation does what took a WRITE + a separate SEND in
     * 1_rdma_write: the data lands directly in the server's memory AND
     * the server gets a completion (with our chosen 32-bit tag) telling
     * it to look, in a single round trip. imm_data must be set in
     * network byte order - htonl() it just like any value crossing
     * the wire. */
    struct ibv_sge write_sge = {
        .addr = (uintptr_t)write_buf, .length = (uint32_t)(strlen(write_buf) + 1),
        .lkey = write_mr->lkey,
    };
    struct ibv_send_wr write_wr = {
        .wr_id = 1, .sg_list = &write_sge, .num_sge = 1,
        .opcode = IBV_WR_RDMA_WRITE_WITH_IMM, .send_flags = IBV_SEND_SIGNALED,
        .imm_data = htonl(IMM_VALUE),
    };
    write_wr.wr.rdma.remote_addr = info.addr;
    write_wr.wr.rdma.rkey        = info.rkey;

    struct ibv_send_wr *bad_send_wr;
    if (ibv_post_send(id->qp, &write_wr, &bad_send_wr)) {
        perror("ibv_post_send (write_with_imm)"); exit(1);
    }

    struct ibv_wc wc;
    if (poll_cq(cq, &wc)) exit(1);
    printf("client: WRITE_WITH_IMM completed (immediate 0x%08x, %zu bytes)\n",
           IMM_VALUE, strlen(write_buf) + 1);

    rdma_disconnect(id);
    if (rdma_get_cm_event(ec, &event)) { perror("rdma_get_cm_event"); exit(1); }
    rdma_ack_cm_event(event);

    rdma_destroy_qp(id);
    ibv_dereg_mr(write_mr);
    ibv_destroy_cq(cq);
    ibv_dealloc_pd(pd);
    rdma_destroy_id(id);
    rdma_destroy_event_channel(ec);

    return 0;
}
