#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <rdma/rdma_cma.h>
#include <infiniband/verbs.h>

#define PORT        20005
#define MAX_SIZE    (4 * 1024 * 1024)  /* largest message size in the sweep */
#define NOTIFY_SIZE 64
#define DONE_MARKER 0xFFFFFFFFu        /* sentinel imm_data value meaning "benchmark finished" */

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

/* notify_buf's contents are never touched by an incoming WRITE_WITH_IMM -
 * exactly like 3_write_with_immediate, this RECV exists purely as a
 * "notification slot" to be consumed for its completion. */
static int post_recv_notify(struct ibv_qp *qp, char *notify_buf, struct ibv_mr *notify_mr) {
    struct ibv_sge sge = { .addr = (uintptr_t)notify_buf, .length = NOTIFY_SIZE, .lkey = notify_mr->lkey };
    struct ibv_recv_wr wr = { .wr_id = 1, .sg_list = &sge, .num_sge = 1 };
    struct ibv_recv_wr *bad_wr;
    return ibv_post_recv(qp, &wr, &bad_wr);
}

int main(void) {
    struct rdma_event_channel *ec;
    struct rdma_cm_id *listen_id, *conn_id;
    struct rdma_cm_event *event;
    struct sockaddr_in addr;

    struct ibv_pd *pd;
    struct ibv_cq *cq;
    struct ibv_qp_init_attr qp_attr;

    char *recv_target_buf;  /* what the CLIENT's RDMA WRITEs land in */
    char *send_source_buf;  /* what WE write out of, when replying to a ping */
    char notify_buf[NOTIFY_SIZE];
    struct ibv_mr *recv_target_mr, *send_source_mr, *notify_mr;

    recv_target_buf = malloc(MAX_SIZE);
    send_source_buf = malloc(MAX_SIZE);
    if (!recv_target_buf || !send_source_buf) { perror("malloc"); exit(1); }
    memset(send_source_buf, 'S', MAX_SIZE);

    ec = rdma_create_event_channel();
    if (!ec) { perror("rdma_create_event_channel"); exit(1); }
    if (rdma_create_id(ec, &listen_id, NULL, RDMA_PS_TCP)) { perror("rdma_create_id"); exit(1); }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);
    if (rdma_bind_addr(listen_id, (struct sockaddr *)&addr)) { perror("rdma_bind_addr"); exit(1); }
    if (rdma_listen(listen_id, 1)) { perror("rdma_listen"); exit(1); }

    printf("server: listening on port %d (RDMA bandwidth/latency benchmark)...\n", PORT);

    if (rdma_get_cm_event(ec, &event)) { perror("rdma_get_cm_event"); exit(1); }
    if (event->event != RDMA_CM_EVENT_CONNECT_REQUEST) {
        fprintf(stderr, "server: unexpected event %s\n", rdma_event_str(event->event)); exit(1);
    }
    conn_id = event->id;

    /* Pull the client's recv_target_buf address+rkey out of the connect
     * request's private data now - the event, and this pointer, become
     * invalid the moment we ack it below. */
    struct mr_info peer_info;
    if (event->param.conn.private_data_len < sizeof(struct mr_info)) {
        fprintf(stderr, "server: client did not send memory info\n"); exit(1);
    }
    memcpy(&peer_info, event->param.conn.private_data, sizeof(peer_info));
    rdma_ack_cm_event(event);

    pd = ibv_alloc_pd(conn_id->verbs);
    if (!pd) { perror("ibv_alloc_pd"); exit(1); }
    cq = ibv_create_cq(conn_id->verbs, 256, NULL, NULL, 0);
    if (!cq) { perror("ibv_create_cq"); exit(1); }

    memset(&qp_attr, 0, sizeof(qp_attr));
    qp_attr.send_cq = cq;
    qp_attr.recv_cq = cq;
    qp_attr.qp_type = IBV_QPT_RC;
    qp_attr.cap.max_send_wr = 128;  /* headroom for the bandwidth phase's windowed writes */
    qp_attr.cap.max_recv_wr = 4;
    qp_attr.cap.max_send_sge = 1;
    qp_attr.cap.max_recv_sge = 1;
    if (rdma_create_qp(conn_id, pd, &qp_attr)) { perror("rdma_create_qp"); exit(1); }

    recv_target_mr = ibv_reg_mr(pd, recv_target_buf, MAX_SIZE,
                                 IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
    if (!recv_target_mr) { perror("ibv_reg_mr (recv_target)"); exit(1); }

    send_source_mr = ibv_reg_mr(pd, send_source_buf, MAX_SIZE, 0);
    if (!send_source_mr) { perror("ibv_reg_mr (send_source)"); exit(1); }

    notify_mr = ibv_reg_mr(pd, notify_buf, NOTIFY_SIZE, IBV_ACCESS_LOCAL_WRITE);
    if (!notify_mr) { perror("ibv_reg_mr (notify)"); exit(1); }

    /* Post the very first RECV before accepting - the client could ping
     * us the instant the connection goes live. */
    if (post_recv_notify(conn_id->qp, notify_buf, notify_mr)) { perror("ibv_post_recv"); exit(1); }

    struct mr_info my_info = { .addr = (uint64_t)(uintptr_t)recv_target_buf, .rkey = recv_target_mr->rkey };
    struct rdma_conn_param conn_param;
    memset(&conn_param, 0, sizeof(conn_param));
    conn_param.private_data = &my_info;
    conn_param.private_data_len = sizeof(my_info);
    /* Only WRITE / WRITE_WITH_IMM are used here, never READ or ATOMIC, so
     * responder_resources/initiator_depth stay at 0 - see 3_write_with_immediate. */

    if (rdma_accept(conn_id, &conn_param)) { perror("rdma_accept"); exit(1); }

    if (rdma_get_cm_event(ec, &event)) { perror("rdma_get_cm_event"); exit(1); }
    if (event->event != RDMA_CM_EVENT_ESTABLISHED) {
        fprintf(stderr, "server: unexpected event %s\n", rdma_event_str(event->event)); exit(1);
    }
    rdma_ack_cm_event(event);

    printf("server: connection established, ready for ping-pong (latency) and streaming (bandwidth)\n");

    struct ibv_wc wc;
    struct ibv_send_wr *bad_send_wr;

    /* Main reactive loop. During the LATENCY phase, every ping the client
     * writes shows up here as a RECV completion carrying an immediate
     * value (the ping's size in bytes); we bounce a reply of the same
     * size straight back into the client's buffer. During the BANDWIDTH
     * phase the client's plain RDMA_WRITEs (no immediate) never touch
     * this loop at all - the server just busy-polls with nothing to do,
     * which is exactly the point: zero CPU involvement in a one-sided
     * transfer. There's no iteration count negotiated anywhere - we just
     * keep reacting to whatever arrives until we see DONE_MARKER. */
    for (;;) {
        if (poll_cq(cq, &wc)) exit(1);
        if (!(wc.wc_flags & IBV_WC_WITH_IMM)) {
            fprintf(stderr, "server: completion with no immediate data (unexpected)\n");
            continue;
        }
        uint32_t imm = ntohl(wc.imm_data);
        if (imm == DONE_MARKER) {
            printf("server: client signaled done\n");
            break;
        }

        uint32_t size = imm;
        /* Re-post a fresh RECV before replying - we need to be ready for
         * the NEXT ping the instant our reply lands. */
        if (post_recv_notify(conn_id->qp, notify_buf, notify_mr)) { perror("ibv_post_recv"); exit(1); }

        struct ibv_sge reply_sge = {
            .addr = (uintptr_t)send_source_buf, .length = size, .lkey = send_source_mr->lkey,
        };
        struct ibv_send_wr reply_wr;
        memset(&reply_wr, 0, sizeof(reply_wr));
        reply_wr.wr_id = 2;
        reply_wr.sg_list = &reply_sge;
        reply_wr.num_sge = 1;
        reply_wr.opcode = IBV_WR_RDMA_WRITE_WITH_IMM;
        reply_wr.send_flags = IBV_SEND_SIGNALED; /* this driver needs a completion for every
                                                   * post to reclaim its send-queue slot - an
                                                   * unsignaled post here made the CLIENT's very
                                                   * first ibv_post_send fail with ENOMEM on real
                                                   * hardware, even with a mostly-empty queue. */
        reply_wr.imm_data = htonl(size);
        reply_wr.wr.rdma.remote_addr = peer_info.addr;
        reply_wr.wr.rdma.rkey        = peer_info.rkey;

        if (ibv_post_send(conn_id->qp, &reply_wr, &bad_send_wr)) { perror("ibv_post_send (reply)"); exit(1); }

        /* Consume our own reply's completion right now, so the poll at the
         * top of this loop is always specifically waiting for the NEXT
         * incoming ping, rather than potentially catching this send's own
         * completion first. */
        if (poll_cq(cq, &wc)) exit(1);
    }

    if (rdma_get_cm_event(ec, &event)) { perror("rdma_get_cm_event"); exit(1); }
    if (event->event == RDMA_CM_EVENT_DISCONNECTED) {
        printf("server: client disconnected\n");
    }
    rdma_ack_cm_event(event);

    rdma_destroy_qp(conn_id);
    ibv_dereg_mr(recv_target_mr);
    ibv_dereg_mr(send_source_mr);
    ibv_dereg_mr(notify_mr);
    ibv_destroy_cq(cq);
    ibv_dealloc_pd(pd);
    rdma_destroy_id(conn_id);
    rdma_destroy_id(listen_id);
    rdma_destroy_event_channel(ec);
    free(recv_target_buf);
    free(send_source_buf);

    printf("server: done, exiting\n");
    return 0;
}
