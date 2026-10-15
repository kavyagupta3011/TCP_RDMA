// rdma_common.h
//
// Shared RDMA CM + verbs connection setup used by all the programs in paper_algorithms_v2/.
//
// Each program is started once per node with its own rank and the list of every rank's
// IPoIB address (no MPI). Every rank connects to every other rank (full mesh), then the
// buffer addresses and keys are swapped with an ordinary SEND/RECV pair
// (rdma_exchange_regions). After that the algorithm data moves only with one-sided RDMA
// WRITE / ATOMIC operations.

#ifndef RDMA_COMMON_H
#define RDMA_COMMON_H

#include <arpa/inet.h>
#include <infiniband/verbs.h>
#include <netinet/in.h>
#include <rdma/rdma_cma.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define RDMA_CHECK(cond, msg)                                                     \
    do {                                                                          \
        if (!(cond)) {                                                            \
            fprintf(stderr, "RDMA error: %s (%s:%d, errno=%d %s)\n", msg,         \
                    __FILE__, __LINE__, errno, strerror(errno));                  \
            exit(1);                                                              \
        }                                                                         \
    } while (0)

#define MAX_REGIONS 8

// What gets exchanged during bootstrap: enough for the peer to issue a
// one-sided RDMA WRITE/ATOMIC directly into this buffer.
typedef struct {
    uint64_t addr;
    uint32_t rkey;
    uint32_t size;
} RegionInfo;

typedef struct {
    struct rdma_event_channel* ec;
    struct rdma_cm_id* listen_id;  // server side only
    struct rdma_cm_id* cm_id;      // the established connection, both sides
    struct ibv_pd* pd;
    struct ibv_cq* cq;
    struct ibv_qp* qp;
    int is_server;

    // Largest payload (bytes) the QP can send INLINE (copied into the work
    // request itself, no DMA read of the source buffer). Set in build_qp()
    // from what the HCA actually granted; 0 if inline is unavailable.
    uint32_t max_inline;

    // Which of the two bootstrap receive slots the NEXT incoming SEND lands in.
    int boot_slot;

    // Small fixed bootstrap buffers used only for rdma_exchange_regions()
    // and rdma_barrier() -- ordinary two-sided SEND/RECV, not part of any
    // algorithm's data path.
    RegionInfo* boot_send_buf;
    RegionInfo* boot_recv_buf;
    struct ibv_mr* boot_send_mr;
    struct ibv_mr* boot_recv_mr;

    // Tiny 8-byte-aligned scratch used as the completion target for
    // rdma_atomic_fetch_add()'s "old value" result.
    int64_t* atomic_result_buf;
    struct ibv_mr* atomic_result_mr;
} RdmaConn;

// Blocks until a client connects on `port`. `ip` is the local interface to bind (the
// node's IPoIB address). Wrapper around listen_only + accept_one below.
RdmaConn* rdma_server_accept(const char* ip, int port);

// Blocks until connected to server_ip:port (server_ip = the other node's
// IPoIB address).
RdmaConn* rdma_client_connect(const char* server_ip, int port);

// Split form of rdma_server_accept. listen_only() binds and starts listening without
// blocking; accept_one() blocks until one client connects. rdma_mesh_connect() uses these
// so every listener is up before any rank tries to connect.
RdmaConn* rdma_server_listen_only(const char* ip, int port);
void rdma_server_accept_one(RdmaConn* conn);

// Connects all `n` ranks in a full mesh, given every rank's IPoIB address in ips[0..n)
// (same array on every rank) and this process's index `my_rank`. Every rank walks the same
// sequence of pairs (0,1),(0,2),...,(n-2,n-1); for pair (a,b) rank a listens and rank b
// connects. Connections are made one pair at a time, which is fine for a one-time setup
// with a few nodes.
//
// Returns an array of n RdmaConn*; result[my_rank] is NULL and result[j] is the connection
// to rank j.
RdmaConn** rdma_mesh_connect(int my_rank, int n, const char** ips, int base_port);
void rdma_mesh_close(RdmaConn** conns, int n, int my_rank);

// malloc()s `size` bytes and registers them for local + remote read/write +
// atomic access. Returns the buffer; *mr_out receives the memory region
// (needed for its ->lkey when posting local-side work requests).
void* rdma_reg_buffer(RdmaConn* conn, size_t size, struct ibv_mr** mr_out);

// Swaps `n` RegionInfo entries with the peer: sends `local[0..n)`, receives
// into `remote_out[0..n)`. Both sides must call this with the same `n`, in
// the same order relative to their own rdma_reg_buffer() calls, so index i
// means the same logical buffer on both ends.
void rdma_exchange_regions(RdmaConn* conn, RegionInfo* local, int n, RegionInfo* remote_out);

// Posts a signaled RDMA WRITE (local_addr/lkey -> remote_addr/rkey, `len` bytes) and blocks
// until its own completion is polled, i.e. until the local NIC has finished the write. The
// receiver detects arrival by polling the written memory.
void rdma_write(RdmaConn* conn, void* local_addr, uint32_t lkey, size_t len,
                 uint64_t remote_addr, uint32_t rkey);

// Split form of rdma_write(). rdma_post_write() posts a signaled RDMA WRITE and returns
// immediately (small payloads go inline); rdma_wait_write() later waits for that write's
// completion. Only one write may be outstanding per connection between a post and its
// wait, and the local buffer must not change until the wait returns (unless sent inline).
void rdma_post_write(RdmaConn* conn, void* local_addr, uint32_t lkey, size_t len,
                      uint64_t remote_addr, uint32_t rkey);
void rdma_wait_write(RdmaConn* conn);

// Same as rdma_post_write() but with `signaled` = 0 the write has no completion of its own.
// Used for a data write followed by a signaled flag write / atomic on the same QP: RC
// completes in order, so the later completion also covers the earlier write. The caller
// still calls rdma_wait_write()/rdma_wait_atomic() once for the signaled one.
void rdma_post_write_ex(RdmaConn* conn, void* local_addr, uint32_t lkey, size_t len,
                         uint64_t remote_addr, uint32_t rkey, int signaled);

// Non-blocking split of rdma_atomic_fetch_add() (old value is discarded --
// the callers here only use the atomic as a completion counter). Exactly one
// atomic may be outstanding per connection between post and wait.
void rdma_post_atomic_add(RdmaConn* conn, uint64_t remote_addr, uint32_t rkey, int64_t add_val);
void rdma_wait_atomic(RdmaConn* conn);

// Per-iteration latency summary: prints "Average <name> AllReduce latency over
// <iters> iters (<ranks> ranks): X us" plus min/median/p99/max. Sorts `lat`.
void rdma_print_stats(const char* name, double* lat, int iters, int ranks);

// Native InfiniBand atomic fetch-and-add (IBV_WR_ATOMIC_FETCH_AND_ADD) on the peer's
// 8-byte-aligned memory. RDMA atomics work on 64-bit integers only (there is no
// floating-point atomic add). *old_val_out receives the value before the add.
void rdma_atomic_fetch_add(RdmaConn* conn, uint64_t remote_addr, uint32_t rkey,
                            int64_t add_val, int64_t* old_val_out);

// One SEND + one RECV in each direction (a two-sided round trip). Used only by
// Baseline_Barrier as its explicit barrier.
void rdma_barrier(RdmaConn* conn);

double now_us(void);

void rdma_close(RdmaConn* conn);

// ---- Shared helpers used by every program for the correctness check ----
//
// Every rank generates every rank's vector locally from a known seed to compute a
// reference sum; only the rank's own vector is sent over the network.
float* make_random_vector(size_t n, unsigned seed);
void reference_sum(const float* a, const float* b, float* out, size_t n);       // 2-vector convenience
void reference_sum_n(float** vectors, int n_vectors, float* out, size_t n);     // N-vector AllReduce reference
int verify(const float* got, const float* ref, size_t n, float rel_tol);

#endif
