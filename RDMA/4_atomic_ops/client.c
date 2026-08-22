#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <rdma/rdma_cma.h>
#include <infiniband/verbs.h>

#define SERVER_IP  "127.0.0.1"
#define PORT       20004
#define BUF_SIZE   1024
#define TIMEOUT_MS 2000
#define ADD_AMOUNT 10
#define NEW_VALUE  999

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

    /* Every atomic completion writes the value the counter held
     * IMMEDIATELY BEFORE the operation into this local buffer - that's
     * the whole contract of both FETCH_AND_ADD and COMPARE_AND_SWAP. It
     * needs IBV_ACCESS_LOCAL_WRITE (our own NIC writes into it) and,
     * like the server's counter, must be naturally aligned - a plain
     * uint64_t guarantees that. */
    uint64_t local_result = 0;
    struct ibv_mr *result_mr;

    char signal_buf[BUF_SIZE] = "done";
    struct ibv_mr *signal_mr;

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

    result_mr = ibv_reg_mr(pd, &local_result, sizeof(local_result), IBV_ACCESS_LOCAL_WRITE);
    if (!result_mr) { perror("ibv_reg_mr (result)"); exit(1); }
    signal_mr = ibv_reg_mr(pd, signal_buf, BUF_SIZE, 0);
    if (!signal_mr) { perror("ibv_reg_mr (signal)"); exit(1); }

    struct rdma_conn_param conn_param;
    memset(&conn_param, 0, sizeof(conn_param));
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

    printf("client: connected to %s:%d - counter at %p (rkey 0x%x)\n",
           SERVER_IP, PORT, (void *)(uintptr_t)info.addr, info.rkey);

    struct ibv_sge result_sge = {
        .addr = (uintptr_t)&local_result, .length = sizeof(local_result), .lkey = result_mr->lkey,
    };
    struct ibv_wc wc;
    struct ibv_send_wr *bad_send_wr;

    /* --- IBV_WR_ATOMIC_FETCH_AND_ADD ---
     * Atomically: local_result <- counter's CURRENT value, then
     * counter <- counter + compare_add, all as one indivisible hardware
     * operation the remote CPU never sees or participates in. This is
     * exactly the primitive distributed locks/counters are built from -
     * no other peer touching the same memory (even with its own
     * concurrent atomic ops) can interleave with this and corrupt it. */
    struct ibv_send_wr add_wr;
    memset(&add_wr, 0, sizeof(add_wr));
    add_wr.wr_id = 1;
    add_wr.sg_list = &result_sge;
    add_wr.num_sge = 1;
    add_wr.opcode = IBV_WR_ATOMIC_FETCH_AND_ADD;
    add_wr.send_flags = IBV_SEND_SIGNALED;
    add_wr.wr.atomic.remote_addr = info.addr;
    add_wr.wr.atomic.rkey        = info.rkey;
    add_wr.wr.atomic.compare_add = ADD_AMOUNT; /* the amount to add */

    if (ibv_post_send(id->qp, &add_wr, &bad_send_wr)) { perror("ibv_post_send (fetch_add)"); exit(1); }
    if (poll_cq(cq, &wc)) exit(1);

    uint64_t value_before_add = local_result;
    uint64_t expected_now = value_before_add + ADD_AMOUNT;
    printf("client: FETCH_AND_ADD(+%d) - counter was %lu, should now be %lu\n",
           ADD_AMOUNT, (unsigned long)value_before_add, (unsigned long)expected_now);

    /* --- IBV_WR_ATOMIC_CMP_AND_SWP ---
     * Atomically: if counter == compare_add, set counter <- swap;
     * local_result always gets whatever counter's value WAS at the
     * moment of the attempt, regardless of whether the swap happened -
     * that's how you tell success from failure: compare what came back
     * against what you expected to see. */
    struct ibv_send_wr cas_wr;
    memset(&cas_wr, 0, sizeof(cas_wr));
    cas_wr.wr_id = 2;
    cas_wr.sg_list = &result_sge;
    cas_wr.num_sge = 1;
    cas_wr.opcode = IBV_WR_ATOMIC_CMP_AND_SWP;
    cas_wr.send_flags = IBV_SEND_SIGNALED;
    cas_wr.wr.atomic.remote_addr = info.addr;
    cas_wr.wr.atomic.rkey        = info.rkey;
    cas_wr.wr.atomic.compare_add = expected_now; /* what we expect it to still be */
    cas_wr.wr.atomic.swap        = NEW_VALUE;    /* what we want to set it to */

    if (ibv_post_send(id->qp, &cas_wr, &bad_send_wr)) { perror("ibv_post_send (cas)"); exit(1); }
    if (poll_cq(cq, &wc)) exit(1);

    uint64_t value_before_cas = local_result;
    if (value_before_cas == expected_now) {
        printf("client: COMPARE_AND_SWAP succeeded - counter was %lu as expected, "
               "swapped it to %d\n", (unsigned long)value_before_cas, NEW_VALUE);
    } else {
        printf("client: COMPARE_AND_SWAP did NOT apply - counter was %lu, "
               "not the %lu we expected (someone else must have changed it)\n",
               (unsigned long)value_before_cas, (unsigned long)expected_now);
    }

    struct ibv_sge signal_sge = {
        .addr = (uintptr_t)signal_buf, .length = (uint32_t)(strlen(signal_buf) + 1),
        .lkey = signal_mr->lkey,
    };
    struct ibv_send_wr signal_wr = {
        .wr_id = 3, .sg_list = &signal_sge, .num_sge = 1,
        .opcode = IBV_WR_SEND, .send_flags = IBV_SEND_SIGNALED,
    };
    if (ibv_post_send(id->qp, &signal_wr, &bad_send_wr)) { perror("ibv_post_send (signal)"); exit(1); }
    if (poll_cq(cq, &wc)) exit(1);
    printf("client: sent \"done\" signal\n");

    rdma_disconnect(id);
    if (rdma_get_cm_event(ec, &event)) { perror("rdma_get_cm_event"); exit(1); }
    rdma_ack_cm_event(event);

    rdma_destroy_qp(id);
    ibv_dereg_mr(result_mr);
    ibv_dereg_mr(signal_mr);
    ibv_destroy_cq(cq);
    ibv_dealloc_pd(pd);
    rdma_destroy_id(id);
    rdma_destroy_event_channel(ec);

    return 0;
}
