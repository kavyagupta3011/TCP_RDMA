#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <rdma/rdma_cma.h>
#include <infiniband/verbs.h>

#define PORT     20003
#define BUF_SIZE 1024

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
    struct rdma_cm_id *listen_id, *conn_id;
    struct rdma_cm_event *event;
    struct sockaddr_in addr;

    struct ibv_pd *pd;
    struct ibv_cq *cq;
    struct ibv_qp_init_attr qp_attr;

    char target_buf[BUF_SIZE];
    struct ibv_mr *target_mr;

    /* notify_buf is where we post a RECV, exactly like 1_rdma_write's
     * signal_buf - but the mechanics are different this time, explained
     * below at the poll_cq() call. */
    char notify_buf[BUF_SIZE];
    struct ibv_mr *notify_mr;

    memset(target_buf, 0, sizeof(target_buf));
    strcpy(target_buf, "(nothing written yet)");

    ec = rdma_create_event_channel();
    if (!ec) { perror("rdma_create_event_channel"); exit(1); }
    if (rdma_create_id(ec, &listen_id, NULL, RDMA_PS_TCP)) { perror("rdma_create_id"); exit(1); }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);
    if (rdma_bind_addr(listen_id, (struct sockaddr *)&addr)) { perror("rdma_bind_addr"); exit(1); }
    if (rdma_listen(listen_id, 1)) { perror("rdma_listen"); exit(1); }

    printf("server: listening on port %d (RDMA CM)...\n", PORT);

    if (rdma_get_cm_event(ec, &event)) { perror("rdma_get_cm_event"); exit(1); }
    if (event->event != RDMA_CM_EVENT_CONNECT_REQUEST) {
        fprintf(stderr, "server: unexpected event %s\n", rdma_event_str(event->event)); exit(1);
    }
    conn_id = event->id;
    rdma_ack_cm_event(event);

    pd = ibv_alloc_pd(conn_id->verbs);
    if (!pd) { perror("ibv_alloc_pd"); exit(1); }
    cq = ibv_create_cq(conn_id->verbs, 16, NULL, NULL, 0);
    if (!cq) { perror("ibv_create_cq"); exit(1); }

    memset(&qp_attr, 0, sizeof(qp_attr));
    qp_attr.send_cq = cq;
    qp_attr.recv_cq = cq;
    qp_attr.qp_type = IBV_QPT_RC;
    qp_attr.cap.max_send_wr = 4;
    qp_attr.cap.max_recv_wr = 4;
    qp_attr.cap.max_send_sge = 1;
    qp_attr.cap.max_recv_sge = 1;
    if (rdma_create_qp(conn_id, pd, &qp_attr)) { perror("rdma_create_qp"); exit(1); }

    target_mr = ibv_reg_mr(pd, target_buf, BUF_SIZE,
                            IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
    if (!target_mr) { perror("ibv_reg_mr (target)"); exit(1); }

    notify_mr = ibv_reg_mr(pd, notify_buf, BUF_SIZE, IBV_ACCESS_LOCAL_WRITE);
    if (!notify_mr) { perror("ibv_reg_mr (notify)"); exit(1); }

    /* Post a RECV exactly like the plain-WRITE example did - but this
     * time it's not a stand-in for a separate SEND message. A
     * WRITE_WITH_IMMEDIATE operation, unlike a plain WRITE, DOES consume
     * a posted RECV work request on the target side and DOES generate a
     * completion for it - carrying the sender's 32-bit immediate value
     * along with it. Notably, notify_buf's own contents are never
     * touched by this - the WRITE's actual payload still goes straight
     * to target_buf via remote_addr/rkey, same as always. The posted
     * RECV here is purely a "notification slot," consumed for its
     * completion, not for any data landing in its buffer. */
    struct ibv_sge notify_sge = {
        .addr = (uintptr_t)notify_buf, .length = BUF_SIZE, .lkey = notify_mr->lkey,
    };
    struct ibv_recv_wr notify_wr = { .wr_id = 1, .sg_list = &notify_sge, .num_sge = 1 };
    struct ibv_recv_wr *bad_recv_wr;
    if (ibv_post_recv(conn_id->qp, &notify_wr, &bad_recv_wr)) { perror("ibv_post_recv"); exit(1); }

    struct mr_info info = { .addr = (uint64_t)(uintptr_t)target_buf, .rkey = target_mr->rkey };
    struct rdma_conn_param conn_param;
    memset(&conn_param, 0, sizeof(conn_param));
    conn_param.private_data = &info;
    conn_param.private_data_len = sizeof(info);
    /* WRITE_WITH_IMMEDIATE is still fundamentally a WRITE, not a READ or
     * an atomic, so responder_resources/initiator_depth don't need to be
     * raised above 0 here - only READ and ATOMIC operations need that. */

    if (rdma_accept(conn_id, &conn_param)) { perror("rdma_accept"); exit(1); }

    if (rdma_get_cm_event(ec, &event)) { perror("rdma_get_cm_event"); exit(1); }
    if (event->event != RDMA_CM_EVENT_ESTABLISHED) {
        fprintf(stderr, "server: unexpected event %s\n", rdma_event_str(event->event)); exit(1);
    }
    rdma_ack_cm_event(event);

    printf("server: connection established, offering %p (rkey 0x%x)\n",
           (void *)target_buf, target_mr->rkey);

    struct ibv_wc wc;
    if (poll_cq(cq, &wc)) exit(1);

    /* wc_flags tells us whether immediate data actually rode along with
     * this completion - it's possible (though not here) to receive a
     * completion from a RECV WR that matched a plain SEND instead, which
     * wouldn't carry IBV_WC_WITH_IMM. imm_data is carried in network byte
     * order (it's declared __be32), so ntohl() it back like any other
     * value that crossed the wire. */
    if (wc.wc_flags & IBV_WC_WITH_IMM) {
        uint32_t imm = ntohl(wc.imm_data);
        printf("server: WRITE_WITH_IMM landed - immediate value = 0x%08x, "
               "target buffer now: \"%s\"\n", imm, target_buf);
    } else {
        printf("server: got a completion with no immediate data (unexpected here)\n");
    }

    if (rdma_get_cm_event(ec, &event)) { perror("rdma_get_cm_event"); exit(1); }
    if (event->event == RDMA_CM_EVENT_DISCONNECTED) {
        printf("server: client disconnected\n");
    }
    rdma_ack_cm_event(event);

    rdma_destroy_qp(conn_id);
    ibv_dereg_mr(target_mr);
    ibv_dereg_mr(notify_mr);
    ibv_destroy_cq(cq);
    ibv_dealloc_pd(pd);
    rdma_destroy_id(conn_id);
    rdma_destroy_id(listen_id);
    rdma_destroy_event_channel(ec);

    printf("server: done, exiting\n");
    return 0;
}
