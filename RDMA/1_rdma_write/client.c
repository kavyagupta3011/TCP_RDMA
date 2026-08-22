#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <rdma/rdma_cma.h>
#include <infiniband/verbs.h>

#define SERVER_IP  "127.0.0.1"
#define PORT       20001
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

    char write_buf[BUF_SIZE] = "hello, this arrived via RDMA WRITE - no CPU work on the server side!";
    char signal_buf[BUF_SIZE] = "done";
    struct ibv_mr *write_mr, *signal_mr;

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

    /* Both of these are SOURCE buffers for our own outgoing operations
     * (a WRITE reads from write_buf, a SEND reads from signal_buf) - the
     * local HCA only ever reads from them, never writes into them, so
     * access = 0 (no special permission needed) is correct, exactly like
     * a plain SEND buffer in the 0_basic_send_recv example. */
    write_mr = ibv_reg_mr(pd, write_buf, BUF_SIZE, 0);
    if (!write_mr) { perror("ibv_reg_mr (write)"); exit(1); }
    signal_mr = ibv_reg_mr(pd, signal_buf, BUF_SIZE, 0);
    if (!signal_mr) { perror("ibv_reg_mr (signal)"); exit(1); }

    struct rdma_conn_param conn_param;
    memset(&conn_param, 0, sizeof(conn_param));
    if (rdma_connect(id, &conn_param)) { perror("rdma_connect"); exit(1); }

    if (rdma_get_cm_event(ec, &event)) { perror("rdma_get_cm_event"); exit(1); }
    if (event->event != RDMA_CM_EVENT_ESTABLISHED) {
        fprintf(stderr, "client: unexpected event %s\n", rdma_event_str(event->event)); exit(1);
    }

    /* The server's rdma_accept() attached target_buf's address + rkey as
     * private data - this is how we learn WHERE we're allowed to write
     * and WITH WHAT PERMISSION, since nothing about RDMA lets us discover
     * that on our own. */
    if (event->param.conn.private_data_len < sizeof(struct mr_info)) {
        fprintf(stderr, "client: server did not send memory info\n"); exit(1);
    }
    struct mr_info info;
    memcpy(&info, event->param.conn.private_data, sizeof(info));
    rdma_ack_cm_event(event);

    printf("client: connected to %s:%d - server's target buffer is %p (rkey 0x%x)\n",
           SERVER_IP, PORT, (void *)(uintptr_t)info.addr, info.rkey);

    /* The one-sided operation itself: WRITE our local buffer's contents
     * directly into the server's remote memory. Notice the server's CPU
     * plays no part in this at all - its NIC's DMA engine places the
     * bytes there entirely on its own. */
    struct ibv_sge write_sge = {
        .addr = (uintptr_t)write_buf, .length = (uint32_t)(strlen(write_buf) + 1),
        .lkey = write_mr->lkey,
    };
    struct ibv_send_wr write_wr = {
        .wr_id = 1, .sg_list = &write_sge, .num_sge = 1,
        .opcode = IBV_WR_RDMA_WRITE, .send_flags = IBV_SEND_SIGNALED,
    };
    write_wr.wr.rdma.remote_addr = info.addr;
    write_wr.wr.rdma.rkey        = info.rkey;

    struct ibv_send_wr *bad_send_wr;
    if (ibv_post_send(id->qp, &write_wr, &bad_send_wr)) { perror("ibv_post_send (write)"); exit(1); }

    struct ibv_wc wc;
    if (poll_cq(cq, &wc)) exit(1); /* WRITE completing means the data has left OUR machine - not
                                     * necessarily proof it has already landed on the server's side,
                                     * though on the same fabric this is effectively instant */
    printf("client: RDMA WRITE completed (%zu bytes written into remote memory)\n", strlen(write_buf) + 1);

    /* Now the SEND that tells the server "the write is done, go look."
     * Because this SEND is posted on the SAME queue pair AFTER the
     * WRITE, RC's ordering guarantee ensures the server sees the WRITE's
     * data before this SEND is delivered to it. */
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
    printf("client: sent \"done\" signal\n");

    rdma_disconnect(id);
    if (rdma_get_cm_event(ec, &event)) { perror("rdma_get_cm_event"); exit(1); }
    rdma_ack_cm_event(event);

    rdma_destroy_qp(id);
    ibv_dereg_mr(write_mr);
    ibv_dereg_mr(signal_mr);
    ibv_destroy_cq(cq);
    ibv_dealloc_pd(pd);
    rdma_destroy_id(id);
    rdma_destroy_event_channel(ec);

    return 0;
}
