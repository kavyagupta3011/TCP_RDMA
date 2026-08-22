#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <rdma/rdma_cma.h>
#include <infiniband/verbs.h>

#define SERVER_IP  "127.0.0.1"
#define PORT       20002
#define BUF_SIZE   1024
#define TIMEOUT_MS 2000

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

    char read_buf[BUF_SIZE];   /* the READ's destination - our own local memory */
    char signal_buf[BUF_SIZE] = "read done";
    struct ibv_mr *read_mr, *signal_mr;

    memset(read_buf, 0, sizeof(read_buf));

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

    /* read_buf is where OUR OWN NIC will write the data it pulls back -
     * so, unlike the WRITE example's source buffer, this DOES need
     * IBV_ACCESS_LOCAL_WRITE (our own hardware is writing into it, just
     * like a RECV buffer would need). signal_buf is send-only, so 0. */
    read_mr = ibv_reg_mr(pd, read_buf, BUF_SIZE, IBV_ACCESS_LOCAL_WRITE);
    if (!read_mr) { perror("ibv_reg_mr (read)"); exit(1); }
    signal_mr = ibv_reg_mr(pd, signal_buf, BUF_SIZE, 0);
    if (!signal_mr) { perror("ibv_reg_mr (signal)"); exit(1); }

    struct rdma_conn_param conn_param;
    memset(&conn_param, 0, sizeof(conn_param));
    /* Mirror image of the server's responder_resources: how many
     * outstanding READ/ATOMIC requests WE intend to issue at once. */
    conn_param.initiator_depth = 1;
    conn_param.responder_resources = 1;

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

    printf("client: connected to %s:%d - server offers %p (rkey 0x%x) for READ\n",
           SERVER_IP, PORT, (void *)(uintptr_t)info.addr, info.rkey);

    /* The one-sided pull: read BUF_SIZE bytes starting at the server's
     * address directly into our local read_buf. The server's CPU does
     * nothing while this happens - our NIC and its NIC handle the whole
     * transfer between themselves. */
    struct ibv_sge read_sge = {
        .addr = (uintptr_t)read_buf, .length = BUF_SIZE, .lkey = read_mr->lkey,
    };
    struct ibv_send_wr read_wr = {
        .wr_id = 1, .sg_list = &read_sge, .num_sge = 1,
        .opcode = IBV_WR_RDMA_READ, .send_flags = IBV_SEND_SIGNALED,
    };
    read_wr.wr.rdma.remote_addr = info.addr;
    read_wr.wr.rdma.rkey        = info.rkey;

    struct ibv_send_wr *bad_send_wr;
    if (ibv_post_send(id->qp, &read_wr, &bad_send_wr)) { perror("ibv_post_send (read)"); exit(1); }

    struct ibv_wc wc;
    /* Unlike a WRITE, a READ's completion on OUR side is meaningful proof
     * the data has actually arrived - by the time this returns, read_buf
     * genuinely holds a copy of the server's memory. */
    if (poll_cq(cq, &wc)) exit(1);
    printf("client: RDMA READ completed - got: \"%s\"\n", read_buf);

    struct ibv_sge signal_sge = {
        .addr = (uintptr_t)signal_buf, .length = (uint32_t)(strlen(signal_buf) + 1),
        .lkey = signal_mr->lkey,
    };
    struct ibv_send_wr signal_wr = {
        .wr_id = 2, .sg_list = &signal_sge, .num_sge = 1,
        .opcode = IBV_WR_SEND, .send_flags = IBV_SEND_SIGNALED,
    };
    if (ibv_post_send(id->qp, &signal_wr, &bad_send_wr)) { perror("ibv_post_send (signal)"); exit(1); }
    if (poll_cq(cq, &wc)) exit(1);
    printf("client: sent confirmation to server\n");

    rdma_disconnect(id);
    if (rdma_get_cm_event(ec, &event)) { perror("rdma_get_cm_event"); exit(1); }
    rdma_ack_cm_event(event);

    rdma_destroy_qp(id);
    ibv_dereg_mr(read_mr);
    ibv_dereg_mr(signal_mr);
    ibv_destroy_cq(cq);
    ibv_dealloc_pd(pd);
    rdma_destroy_id(id);
    rdma_destroy_event_channel(ec);

    return 0;
}
