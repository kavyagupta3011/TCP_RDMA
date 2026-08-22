#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <rdma/rdma_cma.h>
#include <infiniband/verbs.h>

#define PORT     20004
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

    /* Atomic operations work on a single 64-bit value and REQUIRE that
     * value to be 8-byte aligned in memory - the hardware simply won't do
     * an atomic op on a misaligned address. Declaring this as an actual
     * uint64_t (instead of, say, a byte offset into a char array)
     * guarantees the compiler gives it correct alignment automatically -
     * this is a genuinely common bug source if you instead try to point
     * an atomic at an arbitrary offset inside a larger buffer. */
    uint64_t counter = 100;
    struct ibv_mr *counter_mr;

    char notify_buf[BUF_SIZE];
    struct ibv_mr *notify_mr;

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
    printf("server: counter starts at %lu\n", (unsigned long)counter);

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

    /* IBV_ACCESS_REMOTE_ATOMIC follows the same rule as REMOTE_WRITE: the
     * verbs API requires IBV_ACCESS_LOCAL_WRITE alongside it, since a
     * remote atomic physically modifies this memory just like a WRITE
     * would. */
    counter_mr = ibv_reg_mr(pd, &counter, sizeof(counter),
                             IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_ATOMIC);
    if (!counter_mr) { perror("ibv_reg_mr (counter)"); exit(1); }

    notify_mr = ibv_reg_mr(pd, notify_buf, BUF_SIZE, IBV_ACCESS_LOCAL_WRITE);
    if (!notify_mr) { perror("ibv_reg_mr (notify)"); exit(1); }

    struct ibv_sge notify_sge = {
        .addr = (uintptr_t)notify_buf, .length = BUF_SIZE, .lkey = notify_mr->lkey,
    };
    struct ibv_recv_wr notify_wr = { .wr_id = 1, .sg_list = &notify_sge, .num_sge = 1 };
    struct ibv_recv_wr *bad_recv_wr;
    if (ibv_post_recv(conn_id->qp, &notify_wr, &bad_recv_wr)) { perror("ibv_post_recv"); exit(1); }

    struct mr_info info = { .addr = (uint64_t)(uintptr_t)&counter, .rkey = counter_mr->rkey };
    struct rdma_conn_param conn_param;
    memset(&conn_param, 0, sizeof(conn_param));
    conn_param.private_data = &info;
    conn_param.private_data_len = sizeof(info);
    /* Atomics need responder resources too, exactly like RDMA READ - both
     * are "the remote side reaches into my memory and I have to have
     * reserved capacity to service that," unlike SEND/RECV/plain WRITE. */
    conn_param.responder_resources = 1;
    conn_param.initiator_depth = 1;

    if (rdma_accept(conn_id, &conn_param)) { perror("rdma_accept"); exit(1); }

    if (rdma_get_cm_event(ec, &event)) { perror("rdma_get_cm_event"); exit(1); }
    if (event->event != RDMA_CM_EVENT_ESTABLISHED) {
        fprintf(stderr, "server: unexpected event %s\n", rdma_event_str(event->event)); exit(1);
    }
    rdma_ack_cm_event(event);

    printf("server: connection established, exposing counter at %p (rkey 0x%x)\n",
           (void *)&counter, counter_mr->rkey);

    struct ibv_wc wc;
    if (poll_cq(cq, &wc)) exit(1); /* client's "done" signal, after both atomics */

    /* This is the point worth calling out explicitly: reading `counter`
     * here is just... reading a C variable. No RDMA verbs call is needed
     * for the SERVER to access its OWN memory - ibv_post_send/recv,
     * memory registration, all of that machinery exists ONLY to let a
     * REMOTE peer touch this memory. Locally, it was always just a
     * uint64_t sitting in this process's address space. */
    printf("server: client is done - counter is now %lu (direct local read, no RDMA involved)\n",
           (unsigned long)counter);

    if (rdma_get_cm_event(ec, &event)) { perror("rdma_get_cm_event"); exit(1); }
    if (event->event == RDMA_CM_EVENT_DISCONNECTED) {
        printf("server: client disconnected\n");
    }
    rdma_ack_cm_event(event);

    rdma_destroy_qp(conn_id);
    ibv_dereg_mr(counter_mr);
    ibv_dereg_mr(notify_mr);
    ibv_destroy_cq(cq);
    ibv_dealloc_pd(pd);
    rdma_destroy_id(conn_id);
    rdma_destroy_id(listen_id);
    rdma_destroy_event_channel(ec);

    printf("server: done, exiting\n");
    return 0;
}
