// rdma_common.h
//
// Shared RDMA CM + verbs connection setup for paper_algorithms_v2/.
//
// Why this version exists: paper_algorithms/ (the original delivery) is
// written for GPUs on the same NVLink domain, using CUDA peer-to-peer
// memory as the stand-in for the paper's "symmetric heap." Diagnostics on
// 172.16.201.12 (check_node.sh, run 2026-09-30) confirmed that cluster has
// NO GPU/accelerator resources on any of its 8 compute nodes -- only CPUs
// and a real Mellanox ConnectX-3 InfiniBand fabric (mlx4_0). This version
// reimplements the same four/five synchronization mechanisms using REAL
// RDMA verbs (ibverbs + librdmacm) one-sided operations across two of that
// cluster's compute nodes, so it actually runs on hardware you have access
// to right now, using the InfiniBand fabric you've already benchmarked with
// in rdma_bench/.
//
// Design: every algorithm here is a single program launched once PER NODE,
// N times total for an N-rank run, each copy told its own rank index and
// the full list of every rank's IPoIB address (see rdma_mesh_connect()
// below) -- no MPI, no job launcher, just plain command-line arguments,
// per an explicit "no MPI at all" requirement. Every rank ends up with an
// RdmaConn to every OTHER rank (a full mesh), and the algorithms loop over
// that mesh exactly like the original CUDA version loops over GPUs.
//
// (Bidirectional_DoubleBuffering/ is the one exception, matching the
// original CUDA version's own scope decision: the paper's own Fig. 4 is a
// 2-rank teaching example, so that program still only uses ranks 0 and 1
// out of however many are launched -- see its README.)
//
// Bootstrap pattern (standard for RDMA applications): establish the
// connection via RDMA CM, then swap each side's buffer addr+rkey+size using
// one ordinary two-sided SEND/RECV message pair (rdma_exchange_regions
// below). Only after that does either side ever issue a one-sided RDMA
// WRITE or ATOMIC operation -- exactly mirroring how NCCL's device-side API
// registers symmetric memory once, then uses it for many one-sided ops.

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

// Blocks until a client connects on `port`. `ip` selects which local
// interface to bind (pass your node's IPoIB address, e.g. "10.1.2.1").
// (Single-shot convenience wrapper around listen_only + accept_one below;
// still used directly by the mesh bootstrap for the 2-rank case.)
RdmaConn* rdma_server_accept(const char* ip, int port);

// Blocks until connected to server_ip:port (server_ip = the other node's
// IPoIB address).
RdmaConn* rdma_client_connect(const char* server_ip, int port);

// Split form of rdma_server_accept, used by rdma_mesh_connect() to start
// EVERY listener a rank needs before ANY rank attempts to connect to any
// of them -- avoids a "client tries to connect before the server called
// rdma_listen yet" race across N independently-started processes, without
// needing retry/backoff logic. listen_only() binds and starts listening
// but does not block; accept_one() blocks until one client connects and
// completes that one connection.
RdmaConn* rdma_server_listen_only(const char* ip, int port);
void rdma_server_accept_one(RdmaConn* conn);

// Establishes a full mesh of RDMA connections among `n` ranks, given every
// rank's IPoIB address in `ips[0..n)` (same array, same order, passed to
// every rank's process) and this process's own index `my_rank`. Every rank
// independently walks the SAME deterministic sequence of pairs (0,1),
// (0,2), ..., (0,n-1), (1,2), ..., (n-2,n-1) -- for pair (a,b), rank a
// listens and rank b connects -- so no rank ever needs to be told anything
// beyond the shared address list; there's no ambiguity about which
// incoming connection is from which rank, since only one specific pair is
// ever active at a time. This trades bootstrap parallelism (connections
// are established one pair at a time, not concurrently) for simplicity and
// robustness, which is the right trade for a one-time setup phase with a
// handful of nodes -- it is never part of any timed benchmark loop.
//
// Returns an array of n RdmaConn*; result[my_rank] is NULL (no
// self-connection), result[j] is this rank's connection to rank j for
// every other j.
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

// Posts a SIGNALED RDMA WRITE (local_addr/lkey -> remote_addr/rkey, `len`
// bytes) and blocks until ITS OWN completion is polled -- i.e. until the
// local NIC confirms the write was issued, not until the remote side has
// "seen" it in any application sense (this project's algorithms detect
// that by polling the written memory itself, exactly like the CUDA version
// polls peer-written GPU memory). Signaled + immediately polled, matching
// the fix already established in rdma_bench/client.c and server.c earlier
// in this project (unsignaled sends without polling exhausted queue
// resources there -- ENOMEM).
void rdma_write(RdmaConn* conn, void* local_addr, uint32_t lkey, size_t len,
                 uint64_t remote_addr, uint32_t rkey);

// Split form of rdma_write(): rdma_post_write() posts a SIGNALED RDMA WRITE
// and returns immediately (small payloads go INLINE); rdma_wait_write()
// later blocks for that one write's completion. Lets a caller post to ALL
// peers first, do useful work (e.g. spin for incoming data), and only then
// collect the send completions, instead of paying a full NIC round trip per
// peer one after another. Exactly ONE write may be outstanding per
// connection between a post and its matching wait. The local buffer must
// stay unmodified until rdma_wait_write() returns (unless sent inline).
void rdma_post_write(RdmaConn* conn, void* local_addr, uint32_t lkey, size_t len,
                      uint64_t remote_addr, uint32_t rkey);
void rdma_wait_write(RdmaConn* conn);

// Native InfiniBand atomic fetch-and-add (IBV_WR_ATOMIC_FETCH_AND_ADD) on
// the PEER's 8-byte-aligned remote memory. Real RDMA atomics are 64-bit
// SIGNED INTEGER only -- there is no floating-point remote atomic add on
// InfiniBand (unlike NVLink's cache-line atomics, which is what the
// original CUDA LL128_Atomic implementation relies on for float data). See
// LL128_Atomic/README.md in this folder for exactly how that constraint
// shapes this version's design. *old_val_out receives the pre-add value.
void rdma_atomic_fetch_add(RdmaConn* conn, uint64_t remote_addr, uint32_t rkey,
                            int64_t add_val, int64_t* old_val_out);

// One SEND + one matching RECV in each direction -- a genuine two-sided
// round trip, used ONLY by Baseline_Barrier/ as the explicit "conventional"
// synchronization point every other algorithm here is built to avoid.
void rdma_barrier(RdmaConn* conn);

double now_us(void);

void rdma_close(RdmaConn* conn);

// ---- Shared numeric helpers (every algorithm program uses these for its
// correctness check) -----------------------------------------------------
//
// Every rank generates EVERY rank's local vector locally, from a known
// per-rank seed, purely to compute an independent reference sum to verify
// the network-computed result against -- the actual algorithm only ever
// uses its own local vector plus whatever arrives over the wire.
float* make_random_vector(size_t n, unsigned seed);
void reference_sum(const float* a, const float* b, float* out, size_t n);       // 2-vector convenience
void reference_sum_n(float** vectors, int n_vectors, float* out, size_t n);     // N-vector AllReduce reference
int verify(const float* got, const float* ref, size_t n, float rel_tol);

#endif
