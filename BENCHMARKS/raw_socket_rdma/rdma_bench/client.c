#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <time.h>
#include <arpa/inet.h>
#include <rdma/rdma_cma.h>
#include <infiniband/verbs.h>

#define SERVER_IP   "127.0.0.1"
#define PORT        20005
#define MAX_SIZE    (4 * 1024 * 1024)
#define NOTIFY_SIZE 64
#define TIMEOUT_MS  2000
#define DONE_MARKER 0xFFFFFFFFu

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

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static int post_recv_notify(struct ibv_qp *qp, char *notify_buf, struct ibv_mr *notify_mr) {
    struct ibv_sge sge = { .addr = (uintptr_t)notify_buf, .length = NOTIFY_SIZE, .lkey = notify_mr->lkey };
    struct ibv_recv_wr wr = { .wr_id = 1, .sg_list = &sge, .num_sge = 1 };
    struct ibv_recv_wr *bad_wr;
    return ibv_post_recv(qp, &wr, &bad_wr);
}

static uint32_t latency_iters(size_t size) {
    if (size <= 8192) return 1000;
    if (size <= 131072) return 300;
    return 50;
}

static uint32_t bandwidth_iters(size_t size) {
    uint32_t iters = (uint32_t)((8UL * 1024 * 1024) / size);
    if (iters < 5) iters = 5;
    if (iters > 2000) iters = 2000;
    return iters;
}

int main(void) {
    struct rdma_event_channel *ec;
    struct rdma_cm_id *id;
    struct rdma_cm_event *event;
    struct sockaddr_in addr;

    struct ibv_pd *pd;
    struct ibv_cq *cq;
    struct ibv_qp_init_attr qp_attr;

    char *recv_target_buf, *send_source_buf;
    char notify_buf[NOTIFY_SIZE];
    struct ibv_mr *recv_target_mr, *send_source_mr, *notify_mr;

    size_t sizes[] = {
        1, 2, 4, 8, 16, 32, 64, 128, 256, 512,
        1024, 2048, 4096, 8192, 16384, 32768, 65536,
        131072, 262144, 524288, 1048576, 2097152, 4194304
    };
    int n_sizes = (int)(sizeof(sizes) / sizeof(sizes[0]));

    recv_target_buf = malloc(MAX_SIZE);
    send_source_buf = malloc(MAX_SIZE);
    if (!recv_target_buf || !send_source_buf) { perror("malloc"); exit(1); }
    memset(send_source_buf, 'C', MAX_SIZE);

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
    cq = ibv_create_cq(id->verbs, 256, NULL, NULL, 0);
    if (!cq) { perror("ibv_create_cq"); exit(1); }

    memset(&qp_attr, 0, sizeof(qp_attr));
    qp_attr.send_cq = cq;
    qp_attr.recv_cq = cq;
    qp_attr.qp_type = IBV_QPT_RC;
    qp_attr.cap.max_send_wr = 128;
    qp_attr.cap.max_recv_wr = 4;
    qp_attr.cap.max_send_sge = 1;
    qp_attr.cap.max_recv_sge = 1;
    if (rdma_create_qp(id, pd, &qp_attr)) { perror("rdma_create_qp"); exit(1); }

    recv_target_mr = ibv_reg_mr(pd, recv_target_buf, MAX_SIZE,
                                 IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
    if (!recv_target_mr) { perror("ibv_reg_mr (recv_target)"); exit(1); }
    send_source_mr = ibv_reg_mr(pd, send_source_buf, MAX_SIZE, 0);
    if (!send_source_mr) { perror("ibv_reg_mr (send_source)"); exit(1); }
    notify_mr = ibv_reg_mr(pd, notify_buf, NOTIFY_SIZE, IBV_ACCESS_LOCAL_WRITE);
    if (!notify_mr) { perror("ibv_reg_mr (notify)"); exit(1); }

    struct mr_info my_info = { .addr = (uint64_t)(uintptr_t)recv_target_buf, .rkey = recv_target_mr->rkey };
    struct rdma_conn_param conn_param;
    memset(&conn_param, 0, sizeof(conn_param));
    conn_param.private_data = &my_info;
    conn_param.private_data_len = sizeof(my_info);

    if (rdma_connect(id, &conn_param)) { perror("rdma_connect"); exit(1); }

    if (rdma_get_cm_event(ec, &event)) { perror("rdma_get_cm_event"); exit(1); }
    if (event->event != RDMA_CM_EVENT_ESTABLISHED) {
        fprintf(stderr, "client: unexpected event %s\n", rdma_event_str(event->event)); exit(1);
    }
    if (event->param.conn.private_data_len < sizeof(struct mr_info)) {
        fprintf(stderr, "client: server did not send memory info\n"); exit(1);
    }
    struct mr_info peer_info;
    memcpy(&peer_info, event->param.conn.private_data, sizeof(peer_info));
    rdma_ack_cm_event(event);

    printf("client: connected to %s:%d - server buffer %p (rkey 0x%x)\n\n",
           SERVER_IP, PORT, (void *)(uintptr_t)peer_info.addr, peer_info.rkey);

    struct ibv_wc wc;
    struct ibv_send_wr *bad_send_wr;

    printf("=== Latency (RDMA WRITE_WITH_IMM ping-pong, one-way = RTT / 2) ===\n");
    printf("%12s %18s\n", "Size(B)", "Latency(us)");
    for (int i = 0; i < n_sizes; i++) {
        size_t size = sizes[i];
        uint32_t iters = latency_iters(size);
        uint32_t warmup = 2;
        uint32_t total = iters + warmup;

        double t0 = 0.0;
        for (uint32_t it = 0; it < total; it++) {
            if (it == warmup) t0 = now_sec(); /* start timing right after warmup finishes */

            /* Post our RECV for the pong BEFORE sending the ping - the
             * server could reply before we'd otherwise be ready for it. */
            if (post_recv_notify(id->qp, notify_buf, notify_mr)) { perror("ibv_post_recv"); exit(1); }

            struct ibv_sge ping_sge = {
                .addr = (uintptr_t)send_source_buf, .length = (uint32_t)size, .lkey = send_source_mr->lkey,
            };
            struct ibv_send_wr ping_wr;
            memset(&ping_wr, 0, sizeof(ping_wr));
            ping_wr.wr_id = 1;
            ping_wr.sg_list = &ping_sge;
            ping_wr.num_sge = 1;
            ping_wr.opcode = IBV_WR_RDMA_WRITE_WITH_IMM;
            ping_wr.send_flags = IBV_SEND_SIGNALED; /* this driver needs a completion for every
                                                      * post to reclaim its send-queue slot - an
                                                      * unsignaled post here fails ibv_post_send
                                                      * with ENOMEM on the very first call on this
                                                      * hardware, even with a mostly-empty queue. */
            ping_wr.imm_data = htonl((uint32_t)size);
            ping_wr.wr.rdma.remote_addr = peer_info.addr;
            ping_wr.wr.rdma.rkey        = peer_info.rkey;

            if (ibv_post_send(id->qp, &ping_wr, &bad_send_wr)) { perror("ibv_post_send (ping)"); exit(1); }

            /* With signaled sends we now get two completions per iteration:
             * our own ping's send completion, and the pong actually landing
             * (a RECV completion carrying immediate data). The order between
             * them isn't guaranteed, so keep polling until we see the one
             * that actually marks the round trip as done - the pong. */
            for (;;) {
                if (poll_cq(cq, &wc)) exit(1);
                if (wc.opcode == IBV_WC_RECV_RDMA_WITH_IMM) break;
            }
        }
        double t1 = now_sec();

        double rtt_avg_usec = (t1 - t0) * 1e6 / iters;
        printf("%12zu %18.2f\n", size, rtt_avg_usec / 2.0);
    }

    printf("\n=== Bandwidth (one-sided RDMA WRITE, one poll per write) ===\n");
    printf("%12s %18s\n", "Size(B)", "Bandwidth(MB/s)");
    for (int i = 0; i < n_sizes; i++) {
        size_t size = sizes[i];
        uint32_t iters = bandwidth_iters(size);

        double t0 = now_sec();
        for (uint32_t it = 0; it < iters; it++) {
            struct ibv_sge sge = {
                .addr = (uintptr_t)send_source_buf, .length = (uint32_t)size, .lkey = send_source_mr->lkey,
            };
            struct ibv_send_wr wr;
            memset(&wr, 0, sizeof(wr));
            wr.wr_id = 2;
            wr.sg_list = &sge;
            wr.num_sge = 1;
            wr.opcode = IBV_WR_RDMA_WRITE; /* plain WRITE - no immediate, no completion on the
                                             * server at all, which is the whole point: this
                                             * measures how fast we can push data with ZERO
                                             * server CPU involvement. */
            wr.send_flags = IBV_SEND_SIGNALED; /* this driver needs a completion for every post
                                                 * to reclaim its send-queue slot (see the ping
                                                 * loop above) - originally this only signaled
                                                 * every WINDOW-th write to pipeline the sends,
                                                 * but that relies on the driver being willing to
                                                 * queue up unsignaled ones, which this hardware
                                                 * doesn't tolerate. One poll per write costs some
                                                 * throughput but is what actually works here. */
            wr.wr.rdma.remote_addr = peer_info.addr;
            wr.wr.rdma.rkey        = peer_info.rkey;

            if (ibv_post_send(id->qp, &wr, &bad_send_wr)) { perror("ibv_post_send (write)"); exit(1); }
            if (poll_cq(cq, &wc)) exit(1);
        }
        double t1 = now_sec();

        double total_bytes = (double)size * (double)iters;
        double mb_per_sec = (total_bytes / (t1 - t0)) / (1024.0 * 1024.0);
        printf("%12zu %18.2f\n", size, mb_per_sec);
    }

    /* Tell the server we're done - it's parked in a loop waiting for
     * exactly this sentinel immediate value. */
    struct ibv_sge done_sge = {
        .addr = (uintptr_t)send_source_buf, .length = 1, .lkey = send_source_mr->lkey,
    };
    struct ibv_send_wr done_wr;
    memset(&done_wr, 0, sizeof(done_wr));
    done_wr.wr_id = 3;
    done_wr.sg_list = &done_sge;
    done_wr.num_sge = 1;
    done_wr.opcode = IBV_WR_RDMA_WRITE_WITH_IMM;
    done_wr.send_flags = IBV_SEND_SIGNALED;
    done_wr.imm_data = htonl(DONE_MARKER);
    done_wr.wr.rdma.remote_addr = peer_info.addr;
    done_wr.wr.rdma.rkey        = peer_info.rkey;

    if (ibv_post_send(id->qp, &done_wr, &bad_send_wr)) { perror("ibv_post_send (done)"); exit(1); }
    if (poll_cq(cq, &wc)) exit(1);

    rdma_disconnect(id);
    if (rdma_get_cm_event(ec, &event)) { perror("rdma_get_cm_event"); exit(1); }
    rdma_ack_cm_event(event);

    rdma_destroy_qp(id);
    ibv_dereg_mr(recv_target_mr);
    ibv_dereg_mr(send_source_mr);
    ibv_dereg_mr(notify_mr);
    ibv_destroy_cq(cq);
    ibv_dealloc_pd(pd);
    rdma_destroy_id(id);
    rdma_destroy_event_channel(ec);
    free(recv_target_buf);
    free(send_source_buf);

    return 0;
}
