#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <rdma/rdma_cma.h>
#include <infiniband/verbs.h>

#define PORT     20001
#define BUF_SIZE 1024

/* This is the piece of information the client needs before it can WRITE
 * into our memory: WHERE (the virtual address) and permission to do so
 * (the rkey). There is no "discovery" in RDMA - nobody can touch memory
 * they haven't been explicitly told about and handed a key for. We ship
 * this over as private data riding on the RDMA CM accept handshake, so
 * no separate side-channel connection is needed.
 *
 * Note: in a real deployment across different machine architectures you
 * would also need to convert addr/rkey to a fixed byte order (like
 * htons() for ports) before putting them on the wire - skipped here to
 * keep the example focused, since addr/rkey only need to make sense to
 * the SAME NIC/driver that issued them, and we're running same-arch. */
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

    /* target_buf is the memory the CLIENT will write into directly - our
     * own code never explicitly "receives" into it. We only find out
     * something happened via the separate signal_buf/RECV below. */
    char target_buf[BUF_SIZE];
    char signal_buf[BUF_SIZE];
    struct ibv_mr *target_mr, *signal_mr;

    memset(target_buf, 0, sizeof(target_buf));
    strcpy(target_buf, "(nothing written yet)");

    ec = rdma_create_event_channel();
    if (!ec) { perror("rdma_create_event_channel"); exit(1); }
    if (rdma_create_id(ec, &listen_id, NULL, RDMA_PS_TCP)) {
        perror("rdma_create_id"); exit(1);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    if (rdma_bind_addr(listen_id, (struct sockaddr *)&addr)) { perror("rdma_bind_addr"); exit(1); }
    if (rdma_listen(listen_id, 1)) { perror("rdma_listen"); exit(1); }

    printf("server: listening on port %d (RDMA CM)...\n", PORT);

    if (rdma_get_cm_event(ec, &event)) { perror("rdma_get_cm_event"); exit(1); }
    if (event->event != RDMA_CM_EVENT_CONNECT_REQUEST) {
        fprintf(stderr, "server: unexpected event %s\n", rdma_event_str(event->event));
        exit(1);
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

    /* The buffer a REMOTE peer will WRITE into needs IBV_ACCESS_REMOTE_WRITE.
     * The verbs API requires IBV_ACCESS_LOCAL_WRITE to be granted
     * alongside it - the logic being: if a remote peer is allowed to
     * physically overwrite this memory, your own local side implicitly
     * needs the same "can write this memory" permission too. */
    target_mr = ibv_reg_mr(pd, target_buf, BUF_SIZE,
                            IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
    if (!target_mr) { perror("ibv_reg_mr (target)"); exit(1); }

    /* signal_buf/signal_mr exist purely so we have something to
     * ibv_post_recv() against - a plain RDMA WRITE delivers data
     * completely silently on the target side (no completion, no
     * notification at all). The client will WRITE into target_buf, then
     * separately SEND a tiny "done" message into signal_buf so we know
     * when it's safe to look at target_buf. RC's per-QP ordering
     * guarantee is what makes this safe: operations posted to the same
     * queue pair are delivered to the remote side in the order they were
     * posted, so the WRITE is guaranteed to have landed before this SEND
     * arrives. */
    signal_mr = ibv_reg_mr(pd, signal_buf, BUF_SIZE, IBV_ACCESS_LOCAL_WRITE);
    if (!signal_mr) { perror("ibv_reg_mr (signal)"); exit(1); }

    struct ibv_sge signal_sge = {
        .addr = (uintptr_t)signal_buf, .length = BUF_SIZE, .lkey = signal_mr->lkey,
    };
    struct ibv_recv_wr signal_wr = { .wr_id = 1, .sg_list = &signal_sge, .num_sge = 1 };
    struct ibv_recv_wr *bad_recv_wr;
    if (ibv_post_recv(conn_id->qp, &signal_wr, &bad_recv_wr)) {
        perror("ibv_post_recv"); exit(1);
    }

    /* Hand the client target_buf's address + rkey through the accept
     * handshake's private data. */
    struct mr_info info = { .addr = (uint64_t)(uintptr_t)target_buf, .rkey = target_mr->rkey };
    struct rdma_conn_param conn_param;
    memset(&conn_param, 0, sizeof(conn_param));
    conn_param.private_data = &info;
    conn_param.private_data_len = sizeof(info);

    if (rdma_accept(conn_id, &conn_param)) { perror("rdma_accept"); exit(1); }

    if (rdma_get_cm_event(ec, &event)) { perror("rdma_get_cm_event"); exit(1); }
    if (event->event != RDMA_CM_EVENT_ESTABLISHED) {
        fprintf(stderr, "server: unexpected event %s\n", rdma_event_str(event->event));
        exit(1);
    }
    rdma_ack_cm_event(event);

    printf("server: connection established, shared target buffer %p (rkey 0x%x)\n",
           (void *)target_buf, target_mr->rkey);

    /* Wait for the "done" signal - this is the ONLY notification we get
     * that the WRITE happened at all. */
    struct ibv_wc wc;
    if (poll_cq(cq, &wc)) exit(1);

    printf("server: got \"done\" signal - target buffer now contains: \"%s\"\n", target_buf);

    if (rdma_get_cm_event(ec, &event)) { perror("rdma_get_cm_event"); exit(1); }
    if (event->event == RDMA_CM_EVENT_DISCONNECTED) {
        printf("server: client disconnected\n");
    }
    rdma_ack_cm_event(event);

    rdma_destroy_qp(conn_id);
    ibv_dereg_mr(target_mr);
    ibv_dereg_mr(signal_mr);
    ibv_destroy_cq(cq);
    ibv_dealloc_pd(pd);
    rdma_destroy_id(conn_id);
    rdma_destroy_id(listen_id);
    rdma_destroy_event_channel(ec);

    printf("server: done, exiting\n");
    return 0;
}
