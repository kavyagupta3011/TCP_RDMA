#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <rdma/rdma_cma.h>
#include <infiniband/verbs.h>

#define PORT     20002
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

    /* source_buf is what the client will READ FROM. Our own code never
     * "sends" it - the client's NIC pulls it directly. */
    char source_buf[BUF_SIZE] = "this text lives only in the server's memory until READ pulls it out";
    char signal_buf[BUF_SIZE];
    struct ibv_mr *source_mr, *signal_mr;

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

    /* Unlike the WRITE example, being the target of a remote READ does
     * NOT require IBV_ACCESS_LOCAL_WRITE - reading memory doesn't modify
     * it, so there's no "the local side must also be allowed to write
     * this" requirement the way there was for IBV_ACCESS_REMOTE_WRITE. */
    source_mr = ibv_reg_mr(pd, source_buf, BUF_SIZE, IBV_ACCESS_REMOTE_READ);
    if (!source_mr) { perror("ibv_reg_mr (source)"); exit(1); }

    signal_mr = ibv_reg_mr(pd, signal_buf, BUF_SIZE, IBV_ACCESS_LOCAL_WRITE);
    if (!signal_mr) { perror("ibv_reg_mr (signal)"); exit(1); }

    struct ibv_sge signal_sge = {
        .addr = (uintptr_t)signal_buf, .length = BUF_SIZE, .lkey = signal_mr->lkey,
    };
    struct ibv_recv_wr signal_wr = { .wr_id = 1, .sg_list = &signal_sge, .num_sge = 1 };
    struct ibv_recv_wr *bad_recv_wr;
    if (ibv_post_recv(conn_id->qp, &signal_wr, &bad_recv_wr)) { perror("ibv_post_recv"); exit(1); }

    struct mr_info info = { .addr = (uint64_t)(uintptr_t)source_buf, .rkey = source_mr->rkey };
    struct rdma_conn_param conn_param;
    memset(&conn_param, 0, sizeof(conn_param));
    conn_param.private_data = &info;
    conn_param.private_data_len = sizeof(info);

    /* responder_resources: how many incoming RDMA READ/ATOMIC requests
     * this QP can have in flight against it at once. This is the one
     * genuinely easy-to-miss gotcha with READ/atomics - SEND, RECV, and
     * plain WRITE all work fine with this left at 0, but a READ (or an
     * atomic, in the next example) targeting a QP with responder_resources
     * left at 0 will simply fail. The client sets the mirror-image field
     * (initiator_depth) on its side; RDMA CM negotiates the two down to
     * whatever the smaller side/hardware actually supports. */
    conn_param.responder_resources = 1;
    conn_param.initiator_depth = 1;

    if (rdma_accept(conn_id, &conn_param)) { perror("rdma_accept"); exit(1); }

    if (rdma_get_cm_event(ec, &event)) { perror("rdma_get_cm_event"); exit(1); }
    if (event->event != RDMA_CM_EVENT_ESTABLISHED) {
        fprintf(stderr, "server: unexpected event %s\n", rdma_event_str(event->event)); exit(1);
    }
    rdma_ack_cm_event(event);

    printf("server: connection established, offering %p (rkey 0x%x) for READ\n",
           (void *)source_buf, source_mr->rkey);

    struct ibv_wc wc;
    if (poll_cq(cq, &wc)) exit(1); /* client's "done reading" signal */
    printf("server: client confirmed it read the buffer\n");

    if (rdma_get_cm_event(ec, &event)) { perror("rdma_get_cm_event"); exit(1); }
    if (event->event == RDMA_CM_EVENT_DISCONNECTED) {
        printf("server: client disconnected\n");
    }
    rdma_ack_cm_event(event);

    rdma_destroy_qp(conn_id);
    ibv_dereg_mr(source_mr);
    ibv_dereg_mr(signal_mr);
    ibv_destroy_cq(cq);
    ibv_dealloc_pd(pd);
    rdma_destroy_id(conn_id);
    rdma_destroy_id(listen_id);
    rdma_destroy_event_channel(ec);

    printf("server: done, exiting\n");
    return 0;
}
