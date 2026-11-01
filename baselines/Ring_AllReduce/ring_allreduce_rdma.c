/* Ring AllReduce (reduce-scatter, then allgather) over RDMA SEND/RECV.
 *
 * The textbook ring algorithm with ordinary two-sided messaging, written the way a
 * library does it without the tricks used in paper_algorithms_v2 and _v3: no flags, no
 * epochs, no sentinel, no inline sends, no double buffers. Receives are posted before
 * the connection is established, every send is signaled, and each step waits for its
 * message before the next one starts. This is the reference the other programs are
 * compared against.
 *
 * Usage: ring_allreduce_rdma <my_rank> <num_ranks> <base_port> <ip0> ... <ipN-1>
 *                            [numFloats] [iters]
 *
 * Every rank connects to the next and the previous rank only (two links per rank, one
 * link when there are 2 ranks). The message is split into num_ranks chunks. Steps
 * 0..n-2 add chunks up the ring (reduce-scatter), steps n-1..2n-3 pass the finished
 * chunks around (allgather). Message number k of an iteration always lands in receive
 * slot k, so nothing is overwritten before it is used.
 */
#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <errno.h>
#include <infiniband/verbs.h>
#include <math.h>
#include <netinet/in.h>
#include <rdma/rdma_cma.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define CHECK(cond, msg)                                                                  \
    do {                                                                                  \
        if (!(cond)) {                                                                    \
            fprintf(stderr, "error: %s (%s:%d, errno=%d %s)\n", msg, __FILE__, __LINE__, \
                    errno, strerror(errno));                                              \
            exit(1);                                                                      \
        }                                                                                 \
    } while (0)

#define SEED_BASE 9000
#define QP_DEPTH 128
#define CQ_DEPTH 256
#define MAX_SLOTS 64
#define SEND_ID 0x5E0D0000ULL

typedef struct {
    float* work; /* M floats: the buffer the ring works in, holds the result at the end */
    float* rx;   /* L receive slots of cs floats */
    size_t M, cs;
    int L;
} Buffers;

typedef struct {
    struct rdma_event_channel* ec;
    struct rdma_cm_id* listen_id;
    struct rdma_cm_id* id;
    struct ibv_pd* pd;
    struct ibv_cq* cq;
    struct ibv_qp* qp;
    struct ibv_mr* mr_work;
    struct ibv_mr* mr_rx;
} Link;

typedef struct {
    int ready[MAX_SLOTS]; /* receive slot k has a message waiting */
    int expect;           /* slot the next receive completion must belong to */
    int sends_out;        /* sends posted but not yet completed */
} Progress;

static double now_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e6 + (double)ts.tv_nsec / 1e3;
}

static void sleep_ms(int ms) {
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

static void make_addr(struct sockaddr_in* a, const char* ip, int port) {
    memset(a, 0, sizeof(*a));
    a->sin_family = AF_INET;
    a->sin_port = htons((uint16_t)port);
    CHECK(inet_pton(AF_INET, ip, &a->sin_addr) == 1, "bad ip address");
}

/* Waits for one CM event. Returns 0 if it is the expected one. The event is acknowledged
 * here; for a connect request the new id is handed back through new_id. */
static int wait_event(struct rdma_event_channel* ec, enum rdma_cm_event_type want,
                      struct rdma_cm_id** new_id) {
    struct rdma_cm_event* ev = NULL;
    if (rdma_get_cm_event(ec, &ev) != 0) return -1;
    int ok = (ev->event == want);
    if (ok && new_id) *new_id = ev->id;
    if (!ok)
        fprintf(stderr, "cm: expected %s, got %s\n", rdma_event_str(want),
                rdma_event_str(ev->event));
    rdma_ack_cm_event(ev);
    return ok ? 0 : -1;
}

static void post_rx(Link* l, const Buffers* b, int slot) {
    struct ibv_sge sge;
    sge.addr = (uintptr_t)(b->rx + (size_t)slot * b->cs);
    sge.length = (uint32_t)(b->cs * sizeof(float));
    sge.lkey = l->mr_rx->lkey;
    struct ibv_recv_wr wr, *bad = NULL;
    memset(&wr, 0, sizeof(wr));
    wr.wr_id = (uint64_t)slot;
    wr.sg_list = &sge;
    wr.num_sge = 1;
    CHECK(ibv_post_recv(l->qp, &wr, &bad) == 0, "ibv_post_recv failed");
}

/* PD, CQ, QP and memory registrations of one link. recv_side: this link carries the
 * messages from the previous rank, so all receive slots are posted now. */
static void setup_link(Link* l, const Buffers* b, int recv_side) {
    struct ibv_context* ctx = l->id->verbs;
    l->pd = ibv_alloc_pd(ctx);
    CHECK(l->pd != NULL, "ibv_alloc_pd failed");
    l->cq = ibv_create_cq(ctx, CQ_DEPTH, NULL, NULL, 0);
    CHECK(l->cq != NULL, "ibv_create_cq failed");
    struct ibv_qp_init_attr qa;
    memset(&qa, 0, sizeof(qa));
    qa.send_cq = l->cq;
    qa.recv_cq = l->cq;
    qa.qp_type = IBV_QPT_RC;
    qa.cap.max_send_wr = QP_DEPTH;
    qa.cap.max_recv_wr = QP_DEPTH;
    qa.cap.max_send_sge = 1;
    qa.cap.max_recv_sge = 1;
    CHECK(rdma_create_qp(l->id, l->pd, &qa) == 0, "rdma_create_qp failed");
    l->qp = l->id->qp;
    l->mr_work = ibv_reg_mr(l->pd, b->work, b->M * sizeof(float), IBV_ACCESS_LOCAL_WRITE);
    l->mr_rx = ibv_reg_mr(l->pd, b->rx, (size_t)b->L * b->cs * sizeof(float),
                          IBV_ACCESS_LOCAL_WRITE);
    CHECK(l->mr_work != NULL && l->mr_rx != NULL, "ibv_reg_mr failed");
    if (recv_side)
        for (int s = 0; s < b->L; ++s) post_rx(l, b, s);
}

static void link_close(Link* l) {
    if (l->id) rdma_disconnect(l->id);
    if (l->qp) rdma_destroy_qp(l->id);
    if (l->mr_work) ibv_dereg_mr(l->mr_work);
    if (l->mr_rx) ibv_dereg_mr(l->mr_rx);
    if (l->cq) ibv_destroy_cq(l->cq);
    if (l->pd) ibv_dealloc_pd(l->pd);
    if (l->id) rdma_destroy_id(l->id);
    if (l->listen_id) rdma_destroy_id(l->listen_id);
    if (l->ec) rdma_destroy_event_channel(l->ec);
    memset(l, 0, sizeof(*l));
}

static void start_listen(Link* l, const char* ip, int port) {
    l->ec = rdma_create_event_channel();
    CHECK(l->ec != NULL, "rdma_create_event_channel failed");
    CHECK(rdma_create_id(l->ec, &l->listen_id, NULL, RDMA_PS_TCP) == 0, "rdma_create_id failed");
    struct sockaddr_in a;
    make_addr(&a, ip, port);
    CHECK(rdma_bind_addr(l->listen_id, (struct sockaddr*)&a) == 0,
          "rdma_bind_addr failed (is the IPoIB address of this node right?)");
    CHECK(rdma_listen(l->listen_id, 4) == 0, "rdma_listen failed");
}

static void accept_link(Link* l, const Buffers* b, int recv_side) {
    struct rdma_cm_id* nid = NULL;
    CHECK(wait_event(l->ec, RDMA_CM_EVENT_CONNECT_REQUEST, &nid) == 0, "no connect request");
    l->id = nid;
    setup_link(l, b, recv_side);
    struct rdma_conn_param cp;
    memset(&cp, 0, sizeof(cp));
    cp.initiator_depth = 1;
    cp.responder_resources = 1;
    cp.rnr_retry_count = 7;
    CHECK(rdma_accept(l->id, &cp) == 0, "rdma_accept failed");
    CHECK(wait_event(l->ec, RDMA_CM_EVENT_ESTABLISHED, NULL) == 0, "connection not established");
}

/* The server may not be listening yet when this rank starts, so a failed attempt is
 * repeated every 100 ms for up to 30 s. */
static void connect_link(Link* l, const Buffers* b, int recv_side, const char* ip, int port) {
    for (int attempt = 0; attempt < 300; ++attempt) {
        memset(l, 0, sizeof(*l));
        l->ec = rdma_create_event_channel();
        CHECK(l->ec != NULL, "rdma_create_event_channel failed");
        CHECK(rdma_create_id(l->ec, &l->id, NULL, RDMA_PS_TCP) == 0, "rdma_create_id failed");
        struct sockaddr_in a;
        make_addr(&a, ip, port);
        if (rdma_resolve_addr(l->id, NULL, (struct sockaddr*)&a, 2000) == 0 &&
            wait_event(l->ec, RDMA_CM_EVENT_ADDR_RESOLVED, NULL) == 0 &&
            rdma_resolve_route(l->id, 2000) == 0 &&
            wait_event(l->ec, RDMA_CM_EVENT_ROUTE_RESOLVED, NULL) == 0) {
            setup_link(l, b, recv_side);
            struct rdma_conn_param cp;
            memset(&cp, 0, sizeof(cp));
            cp.initiator_depth = 1;
            cp.responder_resources = 1;
            cp.retry_count = 7;
            cp.rnr_retry_count = 7;
            if (rdma_connect(l->id, &cp) == 0 &&
                wait_event(l->ec, RDMA_CM_EVENT_ESTABLISHED, NULL) == 0)
                return;
        }
        link_close(l);
        sleep_ms(100);
    }
    CHECK(0, "could not connect");
}

/* Handles the completions that are available on one link. */
static void poll_link(Link* l, Progress* p, int L) {
    struct ibv_wc wc[16];
    int n = ibv_poll_cq(l->cq, 16, wc);
    CHECK(n >= 0, "ibv_poll_cq failed");
    for (int i = 0; i < n; ++i) {
        if (wc[i].status != IBV_WC_SUCCESS) {
            fprintf(stderr, "work completion error: %s (wr_id=%llu)\n",
                    ibv_wc_status_str(wc[i].status), (unsigned long long)wc[i].wr_id);
            exit(1);
        }
        if (wc[i].opcode == IBV_WC_RECV) {
            CHECK((int)wc[i].wr_id == p->expect, "receive completed out of order");
            p->ready[p->expect] = 1;
            p->expect = (p->expect + 1) % L;
        } else {
            p->sends_out--;
        }
    }
}

static void post_send(Link* l, Progress* p, const float* addr, size_t floats) {
    struct ibv_sge sge;
    sge.addr = (uintptr_t)addr;
    sge.length = (uint32_t)(floats * sizeof(float));
    sge.lkey = l->mr_work->lkey;
    struct ibv_send_wr wr, *bad = NULL;
    memset(&wr, 0, sizeof(wr));
    wr.wr_id = SEND_ID;
    wr.sg_list = &sge;
    wr.num_sge = 1;
    wr.opcode = IBV_WR_SEND;
    wr.send_flags = IBV_SEND_SIGNALED;
    CHECK(ibv_post_send(l->qp, &wr, &bad) == 0, "ibv_post_send failed");
    p->sends_out++;
}

static void wait_slot(Link* lp, Link* ln, Progress* p, int L, int slot) {
    while (!p->ready[slot]) {
        poll_link(lp, p, L);
        if (ln != lp) poll_link(ln, p, L);
    }
}

static float* make_vector(size_t n, unsigned seed) {
    float* v = malloc(n * sizeof(float));
    CHECK(v != NULL, "malloc failed");
    unsigned state = seed;
    for (size_t i = 0; i < n; ++i) {
        state = state * 1103515245u + 12345u;
        float u = (float)((state >> 8) & 0xFFFFFF) / (float)0xFFFFFF;
        v[i] = u * 2.0f - 1.0f;
    }
    return v;
}

static int verify(const float* got, const float* ref, size_t n, float rel_tol) {
    size_t bad = 0;
    for (size_t i = 0; i < n; ++i) {
        float diff = fabsf(got[i] - ref[i]);
        float scale = fmaxf(1.0f, fabsf(ref[i]));
        if (diff / scale > rel_tol) {
            if (bad < 5) fprintf(stderr, "  mismatch at %zu: got %f, want %f\n", i, got[i], ref[i]);
            bad++;
        }
    }
    if (bad) fprintf(stderr, "verify: %zu / %zu elements mismatched\n", bad, n);
    return bad == 0;
}

static int cmp_double(const void* a, const void* b) {
    double x = *(const double*)a, y = *(const double*)b;
    return (x > y) - (x < y);
}

static void print_stats(const char* name, double* lat, int iters, int ranks) {
    double sum = 0;
    for (int i = 0; i < iters; ++i) sum += lat[i];
    qsort(lat, (size_t)iters, sizeof(double), cmp_double);
    printf("Average %s AllReduce latency over %d iters (%d ranks): %.3f us\n", name, iters, ranks,
           sum / iters);
    printf("  min=%.3f us  median=%.3f us  p99=%.3f us  max=%.3f us\n", lat[0], lat[iters / 2],
           lat[(int)((iters - 1) * 0.99)], lat[iters - 1]);
}

static void usage(const char* prog) {
    fprintf(stderr,
            "Usage: %s <my_rank> <num_ranks> <base_port> <ip0> <ip1> ... <ip(N-1)> "
            "[numFloats] [iters]\n",
            prog);
}

int main(int argc, char** argv) {
    if (argc < 5) {
        usage(argv[0]);
        return 1;
    }
    int my_rank = atoi(argv[1]);
    int n = atoi(argv[2]);
    int base_port = atoi(argv[3]);
    if (n < 2 || my_rank < 0 || my_rank >= n || 2 * (n - 1) > MAX_SLOTS) {
        fprintf(stderr, "need 2 <= num_ranks <= %d and 0 <= my_rank < num_ranks\n",
                MAX_SLOTS / 2 + 1);
        return 1;
    }
    if (argc < 4 + n) {
        usage(argv[0]);
        return 1;
    }
    const char** ips = malloc((size_t)n * sizeof(char*));
    for (int i = 0; i < n; ++i) ips[i] = argv[4 + i];
    int arg_idx = 4 + n;
    size_t M = (argc > arg_idx) ? (size_t)atol(argv[arg_idx]) : (1u << 16);
    arg_idx++;
    int iters = (argc > arg_idx) ? atoi(argv[arg_idx]) : 100;
    if (iters < 1) iters = 1;
    if (M % (size_t)n != 0) M += (size_t)n - (M % (size_t)n);
    size_t cs = M / (size_t)n;
    int L = 2 * (n - 1);
    printf("Ring AllReduce, standard algorithm (RDMA send/recv, N-node): M=%zu floats, %d chunks of "
           "%zu, rank=%d/%d\n",
           M, n, cs, my_rank, n);

    Buffers b;
    b.M = M;
    b.cs = cs;
    b.L = L;
    void* tmp = NULL;
    CHECK(posix_memalign(&tmp, 64, M * sizeof(float)) == 0, "posix_memalign failed");
    b.work = tmp;
    CHECK(posix_memalign(&tmp, 64, (size_t)L * cs * sizeof(float)) == 0, "posix_memalign failed");
    b.rx = tmp;
    memset(b.work, 0, M * sizeof(float));
    memset(b.rx, 0, (size_t)L * cs * sizeof(float));

    float** all_vecs = malloc((size_t)n * sizeof(float*));
    for (int r = 0; r < n; ++r) all_vecs[r] = make_vector(M, SEED_BASE + (unsigned)r);
    float* input = all_vecs[my_rank];
    float* ref = malloc(M * sizeof(float));
    for (size_t i = 0; i < M; ++i) {
        float s = 0.0f;
        for (int r = 0; r < n; ++r) s += all_vecs[r][i];
        ref[i] = s;
    }

    /* links: ln goes to the next rank (sends), lp comes from the previous rank (receives) */
    Link ln, lp;
    memset(&ln, 0, sizeof(ln));
    memset(&lp, 0, sizeof(lp));
    Link* link_next = &ln;
    Link* link_prev = (n == 2) ? &ln : &lp;
    if (n == 2) {
        if (my_rank == 0) {
            start_listen(&ln, ips[0], base_port);
            accept_link(&ln, &b, 1);
        } else {
            connect_link(&ln, &b, 1, ips[0], base_port);
        }
    } else {
        /* ring edge k joins rank k and rank k+1 (the last edge joins n-1 and 0), port
         * base_port + k. The lower rank of an edge listens. Edges are set up in order of k,
         * so the connections form a chain and nobody waits in a cycle. */
        if (my_rank == 0) {
            start_listen(&ln, ips[0], base_port);
            start_listen(&lp, ips[0], base_port + n - 1);
            accept_link(&ln, &b, 0);
            accept_link(&lp, &b, 1);
        } else if (my_rank < n - 1) {
            start_listen(&ln, ips[my_rank], base_port + my_rank);
            connect_link(&lp, &b, 1, ips[my_rank - 1], base_port + my_rank - 1);
            accept_link(&ln, &b, 0);
        } else {
            connect_link(&lp, &b, 1, ips[n - 2], base_port + n - 2);
            connect_link(&ln, &b, 0, ips[0], base_port + n - 1);
        }
    }
    printf("[ring] rank %d connected (next=%d, prev=%d)\n", my_rank, (my_rank + 1) % n,
           (my_rank + n - 1) % n);

    Progress p;
    memset(&p, 0, sizeof(p));
    double* lat = malloc((size_t)iters * sizeof(double));
    int ok = 1, bad_rounds = 0;
    for (int it = -1; it < iters; ++it) {
        int64_t epoch = it + 2;
        if (it >= 0) {
            float s = (float)(epoch * (my_rank + 1));
            input[0] = input[M - 1] = s;
        }
        double t0 = now_us();
        memcpy(b.work, input, M * sizeof(float));
        int step = 0;
        for (int s = 0; s < n - 1; ++s, ++step) { /* reduce-scatter */
            int send_c = ((my_rank - s) % n + n) % n;
            int recv_c = ((my_rank - s - 1) % n + n) % n;
            post_send(link_next, &p, b.work + (size_t)send_c * cs, cs);
            wait_slot(link_prev, link_next, &p, L, step);
            float* w = b.work + (size_t)recv_c * cs;
            const float* x = b.rx + (size_t)step * cs;
            for (size_t i = 0; i < cs; ++i) w[i] += x[i];
            p.ready[step] = 0;
            post_rx(link_prev, &b, step);
        }
        for (int s = 0; s < n - 1; ++s, ++step) { /* allgather */
            int send_c = ((my_rank + 1 - s) % n + n) % n;
            int recv_c = ((my_rank - s) % n + n) % n;
            post_send(link_next, &p, b.work + (size_t)send_c * cs, cs);
            wait_slot(link_prev, link_next, &p, L, step);
            memcpy(b.work + (size_t)recv_c * cs, b.rx + (size_t)step * cs, cs * sizeof(float));
            p.ready[step] = 0;
            post_rx(link_prev, &b, step);
        }
        while (p.sends_out > 0) {
            poll_link(link_next, &p, L);
            if (link_prev != link_next) poll_link(link_prev, &p, L);
        }
        double elapsed = now_us() - t0;
        if (it == -1) {
            ok = verify(b.work, ref, M, 1e-3f);
            printf("Correctness check vs. CPU reference: %s\n", ok ? "PASSED" : "FAILED");
        } else {
            lat[it] = elapsed;
            float want = (float)(epoch * n * (n + 1) / 2);
            if (!(b.work[0] == want && b.work[M - 1] == want)) {
                fprintf(stderr, "round check: epoch %lld got first=%f last=%f, want %f\n",
                        (long long)epoch, b.work[0], b.work[M - 1], want);
                bad_rounds++;
            }
        }
    }
    printf("Correctness check, every timed round (first/last element): %s\n",
           bad_rounds ? "FAILED" : "PASSED");
    print_stats("ring (reduce-scatter + allgather)", lat, iters, n);
    free(lat);

    /* keep the links open a little so a slower rank can finish before they are torn down */
    sleep_ms(2000);
    link_close(&ln);
    if (n > 2) link_close(&lp);
    for (int r = 0; r < n; ++r) free(all_vecs[r]);
    free(all_vecs);
    free(ref);
    free(b.work);
    free(b.rx);
    free(ips);
    return (ok && !bad_rounds) ? 0 : 1;
}
