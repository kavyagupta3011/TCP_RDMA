# Results: v1 (paper_algorithms)

**v1** = `paper_algorithms`: the first RDMA version (git commit 0512582, 2026-09-30). Each RDMA write waits for its completion before the next one; one receive buffer reused every round; Sentinel never re-arms its buffers; flag waits use `!=`; prints only the mean; checks the result only in the first round.

Machines: rank 0 = compute02, rank 1 = compute03, rank 2 = compute04. PBS job 30203.master.local.

How to read: a value is the time of one AllReduce, averaged over the ranks that finished
correctly. If a rank crashed, hung (stopped by the timeout) or got a wrong result, the cell
says so. Sizes: 32 B = 8 floats, 64 B = 16, 128 B = 32, 32 KB = 8,192, 256 KB = 65,536,
32 MB = 8,388,608 floats per rank. 1000 timed rounds, 100 for 256 KB and 32 MB.

## 1. All algorithms, 3 ranks: mean

This version prints only the mean (`Average ... latency`), so all its tables use the mean.

| Algorithm | 32 B | 64 B | 128 B | 32 KB | 256 KB | 32 MB |
|---|---|---|---|---|---|---|
| Baseline (with barrier) | 11.38 µs | 11.39 µs | 11.53 µs | 72.92 µs | 439.6 µs | 54.52 ms |
| LL (one-shot) | hung (rank 0/1/2) | hung (rank 0/1/2) | hung (rank 0/1/2) | 58.75 µs | 385.2 µs | 46.86 ms |
| Sentinel (one-shot) | 4.04 µs | 4.10 µs | 4.22 µs | 68.21 µs | 458.1 µs (2 of 3 ranks; rank 1 crashed) | 55.29 ms (2 of 3 ranks; rank 0 crashed) |
| Two-shot LL | 15.94 µs | 15.89 µs | 15.79 µs | 51.60 µs | 272.1 µs | 31.80 ms |
| Two-shot Sentinel | 8.01 µs | 7.99 µs | 8.07 µs | 49.30 µs (2 of 3 ranks; rank 0 crashed) | 312.7 µs | 34.92 ms (1 of 3 ranks; rank 0 crashed, rank 1 crashed) |
| LL128 Atomic | crashed (rank 0/1/2) | crashed (rank 0/1/2) | crashed (rank 0/1/2) | crashed (rank 0/1/2) | crashed (rank 0/1/2) | crashed (rank 0/1/2) |
| Bidirectional (2 ranks, 8 chunks per round) | 31.42 µs | 31.57 µs | 31.33 µs | 55.62 µs | 178.7 µs | 19.60 ms |

## 2. Cost of the barrier: Baseline vs LL, 3 ranks (mean)

|  | 32 B | 64 B | 128 B | 32 KB | 256 KB | 32 MB |
|---|---|---|---|---|---|---|
| Baseline - LL | - | - | - | 14.17 µs | 54.40 µs | 7652.3 µs |
| Baseline / LL | - | - | - | 1.24x | 1.14x | 1.16x |

## 3. 2 ranks vs 3 ranks: Baseline and LL (mean)

|  | 32 B | 64 B | 128 B | 32 KB | 32 MB |
|---|---|---|---|---|---|
| Baseline (with barrier), 2 ranks | 4.79 µs | 1583.2 µs | 1970.1 µs | 33.18 µs | 25.88 ms |
| Baseline (with barrier), 3 ranks | 11.38 µs | 11.39 µs | 11.53 µs | 72.92 µs | 54.52 ms |
| LL (one-shot), 2 ranks | hung (rank 0/1) | hung (rank 0/1) | hung (rank 0/1) | 30.40 µs | 25.69 ms |
| LL (one-shot), 3 ranks | hung (rank 0/1/2) | hung (rank 0/1/2) | hung (rank 0/1/2) | 58.75 µs | 46.86 ms |

## 4. Correctness and errors

| Log | PASSED lines | FAILED lines | ranks ok | crashed | hung | skipped |
|---|---|---|---|---|---|---|
| `1_3ranks_32B_64B_128B_32KB.txt` | 59 | 0 | 58 | 13 | 9 | 0 |
| `2_3ranks_256KB.txt` | 17 | 0 | 16 | 4 | 0 | 0 |
| `3_3ranks_32MB.txt` | 17 | 0 | 14 | 6 | 0 | 0 |
| `4_2ranks_baseline_ll.txt` | 14 | 0 | 14 | 0 | 6 | 0 |

Error messages (first one per rank):

- LL128 Atomic, 32 B, rank 0: `RDMA error: ibv_post_send (ATOMIC_FETCH_AND_ADD) failed -- does this HCA/link support RDMA atomics? (mlx4 does) (../common/rdma_common.c:328, errno=22 Invalid argument)`
- LL128 Atomic, 32 B, rank 1: `RDMA error: ibv_post_send (ATOMIC_FETCH_AND_ADD) failed -- does this HCA/link support RDMA atomics? (mlx4 does) (../common/rdma_common.c:328, errno=22 Invalid argument)`
- LL128 Atomic, 32 B, rank 2: `RDMA error: ibv_post_send (ATOMIC_FETCH_AND_ADD) failed -- does this HCA/link support RDMA atomics? (mlx4 does) (../common/rdma_common.c:328, errno=22 Invalid argument)`
- LL128 Atomic, 64 B, rank 0: `RDMA error: ibv_post_send (ATOMIC_FETCH_AND_ADD) failed -- does this HCA/link support RDMA atomics? (mlx4 does) (../common/rdma_common.c:328, errno=22 Invalid argument)`
- LL128 Atomic, 64 B, rank 1: `RDMA error: ibv_post_send (ATOMIC_FETCH_AND_ADD) failed -- does this HCA/link support RDMA atomics? (mlx4 does) (../common/rdma_common.c:328, errno=22 Invalid argument)`
- LL128 Atomic, 64 B, rank 2: `RDMA error: ibv_post_send (ATOMIC_FETCH_AND_ADD) failed -- does this HCA/link support RDMA atomics? (mlx4 does) (../common/rdma_common.c:328, errno=22 Invalid argument)`
- LL128 Atomic, 128 B, rank 0: `RDMA error: ibv_post_send (ATOMIC_FETCH_AND_ADD) failed -- does this HCA/link support RDMA atomics? (mlx4 does) (../common/rdma_common.c:328, errno=22 Invalid argument)`
- LL128 Atomic, 128 B, rank 1: `RDMA error: ibv_post_send (ATOMIC_FETCH_AND_ADD) failed -- does this HCA/link support RDMA atomics? (mlx4 does) (../common/rdma_common.c:328, errno=22 Invalid argument)`
- LL128 Atomic, 128 B, rank 2: `RDMA error: ibv_post_send (ATOMIC_FETCH_AND_ADD) failed -- does this HCA/link support RDMA atomics? (mlx4 does) (../common/rdma_common.c:328, errno=22 Invalid argument)`
- Two-shot Sentinel, 32 KB, rank 0: `RDMA work completion error: transport retry counter exceeded (wr_id=4126, expected=4126)`
- LL128 Atomic, 32 KB, rank 0: `RDMA error: ibv_post_send (ATOMIC_FETCH_AND_ADD) failed -- does this HCA/link support RDMA atomics? (mlx4 does) (../common/rdma_common.c:328, errno=22 Invalid argument)`
- LL128 Atomic, 32 KB, rank 1: `RDMA error: ibv_post_send (ATOMIC_FETCH_AND_ADD) failed -- does this HCA/link support RDMA atomics? (mlx4 does) (../common/rdma_common.c:328, errno=22 Invalid argument)`
- LL128 Atomic, 32 KB, rank 2: `RDMA error: ibv_post_send (ATOMIC_FETCH_AND_ADD) failed -- does this HCA/link support RDMA atomics? (mlx4 does) (../common/rdma_common.c:328, errno=22 Invalid argument)`
- Sentinel (one-shot), 256 KB, rank 1: `RDMA work completion error: transport retry counter exceeded (wr_id=4126, expected=4126)`
- LL128 Atomic, 256 KB, rank 0: `RDMA error: ibv_post_send (ATOMIC_FETCH_AND_ADD) failed -- does this HCA/link support RDMA atomics? (mlx4 does) (../common/rdma_common.c:328, errno=22 Invalid argument)`
- LL128 Atomic, 256 KB, rank 1: `RDMA error: ibv_post_send (ATOMIC_FETCH_AND_ADD) failed -- does this HCA/link support RDMA atomics? (mlx4 does) (../common/rdma_common.c:328, errno=22 Invalid argument)`
- LL128 Atomic, 256 KB, rank 2: `RDMA error: ibv_post_send (ATOMIC_FETCH_AND_ADD) failed -- does this HCA/link support RDMA atomics? (mlx4 does) (../common/rdma_common.c:328, errno=22 Invalid argument)`
- Sentinel (one-shot), 32 MB, rank 0: `RDMA work completion error: transport retry counter exceeded (wr_id=4126, expected=4126)`
- Two-shot Sentinel, 32 MB, rank 0: `RDMA work completion error: transport retry counter exceeded (wr_id=4126, expected=4126)`
- Two-shot Sentinel, 32 MB, rank 1: `RDMA work completion error: transport retry counter exceeded (wr_id=4126, expected=4126)`
- LL128 Atomic, 32 MB, rank 0: `RDMA error: ibv_post_send (ATOMIC_FETCH_AND_ADD) failed -- does this HCA/link support RDMA atomics? (mlx4 does) (../common/rdma_common.c:328, errno=22 Invalid argument)`
- LL128 Atomic, 32 MB, rank 1: `RDMA error: ibv_post_send (ATOMIC_FETCH_AND_ADD) failed -- does this HCA/link support RDMA atomics? (mlx4 does) (../common/rdma_common.c:328, errno=22 Invalid argument)`
- LL128 Atomic, 32 MB, rank 2: `RDMA error: ibv_post_send (ATOMIC_FETCH_AND_ADD) failed -- does this HCA/link support RDMA atomics? (mlx4 does) (../common/rdma_common.c:328, errno=22 Invalid argument)`

## 5. Every run, per rank

Times in µs. `-` = not printed by this version or the rank did not finish.

| Program | ranks | size | rank | status | mean | min | median | p99 | max | correctness | exit | log |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Baseline (with barrier) | 3 | 32 B | 0 | ok | 11.38 | - | - | - | - | PASSED | 0 | 1 |
| Baseline (with barrier) | 3 | 32 B | 1 | ok | 11.38 | - | - | - | - | PASSED | 0 | 1 |
| Baseline (with barrier) | 3 | 32 B | 2 | ok | 11.38 | - | - | - | - | PASSED | 0 | 1 |
| LL (one-shot) | 3 | 32 B | 0 | hung | - | - | - | - | - | - | 124 | 1 |
| LL (one-shot) | 3 | 32 B | 1 | hung | - | - | - | - | - | - | 124 | 1 |
| LL (one-shot) | 3 | 32 B | 2 | hung | - | - | - | - | - | - | 124 | 1 |
| Sentinel (one-shot) | 3 | 32 B | 0 | ok | 4.04 | - | - | - | - | PASSED | 0 | 1 |
| Sentinel (one-shot) | 3 | 32 B | 1 | ok | 4.05 | - | - | - | - | PASSED | 0 | 1 |
| Sentinel (one-shot) | 3 | 32 B | 2 | ok | 4.05 | - | - | - | - | PASSED | 0 | 1 |
| Two-shot LL | 3 | 32 B | 0 | ok | 15.95 | - | - | - | - | PASSED | 0 | 1 |
| Two-shot LL | 3 | 32 B | 1 | ok | 15.94 | - | - | - | - | PASSED | 0 | 1 |
| Two-shot LL | 3 | 32 B | 2 | ok | 15.94 | - | - | - | - | PASSED | 0 | 1 |
| Two-shot Sentinel | 3 | 32 B | 0 | ok | 8.00 | - | - | - | - | PASSED | 0 | 1 |
| Two-shot Sentinel | 3 | 32 B | 1 | ok | 8.01 | - | - | - | - | PASSED | 0 | 1 |
| Two-shot Sentinel | 3 | 32 B | 2 | ok | 8.02 | - | - | - | - | PASSED | 0 | 1 |
| LL128 Atomic | 3 | 32 B | 0 | crashed | - | - | - | - | - | - | 1 | 1 |
| LL128 Atomic | 3 | 32 B | 1 | crashed | - | - | - | - | - | - | 1 | 1 |
| LL128 Atomic | 3 | 32 B | 2 | crashed | - | - | - | - | - | - | 1 | 1 |
| Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 B | 0 | ok | 31.42 | - | - | - | - | PASSED | 0 | 1 |
| Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 B | 1 | ok | 31.41 | - | - | - | - | PASSED | 0 | 1 |
| Baseline (with barrier) | 3 | 64 B | 0 | ok | 11.39 | - | - | - | - | PASSED | 0 | 1 |
| Baseline (with barrier) | 3 | 64 B | 1 | ok | 11.38 | - | - | - | - | PASSED | 0 | 1 |
| Baseline (with barrier) | 3 | 64 B | 2 | ok | 11.39 | - | - | - | - | PASSED | 0 | 1 |
| LL (one-shot) | 3 | 64 B | 0 | hung | - | - | - | - | - | - | 124 | 1 |
| LL (one-shot) | 3 | 64 B | 1 | hung | - | - | - | - | - | - | 124 | 1 |
| LL (one-shot) | 3 | 64 B | 2 | hung | - | - | - | - | - | - | 124 | 1 |
| Sentinel (one-shot) | 3 | 64 B | 0 | ok | 4.09 | - | - | - | - | PASSED | 0 | 1 |
| Sentinel (one-shot) | 3 | 64 B | 1 | ok | 4.12 | - | - | - | - | PASSED | 0 | 1 |
| Sentinel (one-shot) | 3 | 64 B | 2 | ok | 4.10 | - | - | - | - | PASSED | 0 | 1 |
| Two-shot LL | 3 | 64 B | 0 | ok | 15.89 | - | - | - | - | PASSED | 0 | 1 |
| Two-shot LL | 3 | 64 B | 1 | ok | 15.88 | - | - | - | - | PASSED | 0 | 1 |
| Two-shot LL | 3 | 64 B | 2 | ok | 15.88 | - | - | - | - | PASSED | 0 | 1 |
| Two-shot Sentinel | 3 | 64 B | 0 | ok | 7.99 | - | - | - | - | PASSED | 0 | 1 |
| Two-shot Sentinel | 3 | 64 B | 1 | ok | 8.00 | - | - | - | - | PASSED | 0 | 1 |
| Two-shot Sentinel | 3 | 64 B | 2 | ok | 7.99 | - | - | - | - | PASSED | 0 | 1 |
| LL128 Atomic | 3 | 64 B | 0 | crashed | - | - | - | - | - | - | 1 | 1 |
| LL128 Atomic | 3 | 64 B | 1 | crashed | - | - | - | - | - | - | 1 | 1 |
| LL128 Atomic | 3 | 64 B | 2 | crashed | - | - | - | - | - | - | 1 | 1 |
| Bidirectional (2 ranks, 8 chunks per round) | 2 | 64 B | 0 | ok | 31.57 | - | - | - | - | PASSED | 0 | 1 |
| Bidirectional (2 ranks, 8 chunks per round) | 2 | 64 B | 1 | ok | 31.57 | - | - | - | - | PASSED | 0 | 1 |
| Baseline (with barrier) | 3 | 128 B | 0 | ok | 11.54 | - | - | - | - | PASSED | 0 | 1 |
| Baseline (with barrier) | 3 | 128 B | 1 | ok | 11.53 | - | - | - | - | PASSED | 0 | 1 |
| Baseline (with barrier) | 3 | 128 B | 2 | ok | 11.53 | - | - | - | - | PASSED | 0 | 1 |
| LL (one-shot) | 3 | 128 B | 0 | hung | - | - | - | - | - | - | 124 | 1 |
| LL (one-shot) | 3 | 128 B | 1 | hung | - | - | - | - | - | - | 124 | 1 |
| LL (one-shot) | 3 | 128 B | 2 | hung | - | - | - | - | - | - | 124 | 1 |
| Sentinel (one-shot) | 3 | 128 B | 0 | ok | 4.25 | - | - | - | - | PASSED | 0 | 1 |
| Sentinel (one-shot) | 3 | 128 B | 1 | ok | 4.26 | - | - | - | - | PASSED | 0 | 1 |
| Sentinel (one-shot) | 3 | 128 B | 2 | ok | 4.15 | - | - | - | - | PASSED | 0 | 1 |
| Two-shot LL | 3 | 128 B | 0 | ok | 15.79 | - | - | - | - | PASSED | 0 | 1 |
| Two-shot LL | 3 | 128 B | 1 | ok | 15.79 | - | - | - | - | PASSED | 0 | 1 |
| Two-shot LL | 3 | 128 B | 2 | ok | 15.79 | - | - | - | - | PASSED | 0 | 1 |
| Two-shot Sentinel | 3 | 128 B | 0 | ok | 8.05 | - | - | - | - | PASSED | 0 | 1 |
| Two-shot Sentinel | 3 | 128 B | 1 | ok | 8.11 | - | - | - | - | PASSED | 0 | 1 |
| Two-shot Sentinel | 3 | 128 B | 2 | ok | 8.05 | - | - | - | - | PASSED | 0 | 1 |
| LL128 Atomic | 3 | 128 B | 0 | crashed | - | - | - | - | - | - | 1 | 1 |
| LL128 Atomic | 3 | 128 B | 1 | crashed | - | - | - | - | - | - | 1 | 1 |
| LL128 Atomic | 3 | 128 B | 2 | crashed | - | - | - | - | - | - | 1 | 1 |
| Bidirectional (2 ranks, 8 chunks per round) | 2 | 128 B | 0 | ok | 31.33 | - | - | - | - | PASSED | 0 | 1 |
| Bidirectional (2 ranks, 8 chunks per round) | 2 | 128 B | 1 | ok | 31.33 | - | - | - | - | PASSED | 0 | 1 |
| Baseline (with barrier) | 3 | 32 KB | 0 | ok | 72.92 | - | - | - | - | PASSED | 0 | 1 |
| Baseline (with barrier) | 3 | 32 KB | 1 | ok | 72.92 | - | - | - | - | PASSED | 0 | 1 |
| Baseline (with barrier) | 3 | 32 KB | 2 | ok | 72.91 | - | - | - | - | PASSED | 0 | 1 |
| LL (one-shot) | 3 | 32 KB | 0 | ok | 58.77 | - | - | - | - | PASSED | 0 | 1 |
| LL (one-shot) | 3 | 32 KB | 1 | ok | 58.73 | - | - | - | - | PASSED | 0 | 1 |
| LL (one-shot) | 3 | 32 KB | 2 | ok | 58.74 | - | - | - | - | PASSED | 0 | 1 |
| Sentinel (one-shot) | 3 | 32 KB | 0 | ok | 68.24 | - | - | - | - | PASSED | 0 | 1 |
| Sentinel (one-shot) | 3 | 32 KB | 1 | ok | 68.20 | - | - | - | - | PASSED | 0 | 1 |
| Sentinel (one-shot) | 3 | 32 KB | 2 | ok | 68.19 | - | - | - | - | PASSED | 0 | 1 |
| Two-shot LL | 3 | 32 KB | 0 | ok | 51.60 | - | - | - | - | PASSED | 0 | 1 |
| Two-shot LL | 3 | 32 KB | 1 | ok | 51.59 | - | - | - | - | PASSED | 0 | 1 |
| Two-shot LL | 3 | 32 KB | 2 | ok | 51.61 | - | - | - | - | PASSED | 0 | 1 |
| Two-shot Sentinel | 3 | 32 KB | 0 | crashed | - | - | - | - | - | PASSED | 1 | 1 |
| Two-shot Sentinel | 3 | 32 KB | 1 | ok | 49.56 | - | - | - | - | PASSED | 0 | 1 |
| Two-shot Sentinel | 3 | 32 KB | 2 | ok | 49.03 | - | - | - | - | PASSED | 0 | 1 |
| LL128 Atomic | 3 | 32 KB | 0 | crashed | - | - | - | - | - | - | 1 | 1 |
| LL128 Atomic | 3 | 32 KB | 1 | crashed | - | - | - | - | - | - | 1 | 1 |
| LL128 Atomic | 3 | 32 KB | 2 | crashed | - | - | - | - | - | - | 1 | 1 |
| Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 KB | 0 | ok | 55.62 | - | - | - | - | PASSED | 0 | 1 |
| Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 KB | 1 | ok | 55.62 | - | - | - | - | PASSED | 0 | 1 |
| Baseline (with barrier) | 3 | 256 KB | 0 | ok | 440.56 | - | - | - | - | PASSED | 0 | 2 |
| Baseline (with barrier) | 3 | 256 KB | 1 | ok | 438.95 | - | - | - | - | PASSED | 0 | 2 |
| Baseline (with barrier) | 3 | 256 KB | 2 | ok | 439.14 | - | - | - | - | PASSED | 0 | 2 |
| LL (one-shot) | 3 | 256 KB | 0 | ok | 385.33 | - | - | - | - | PASSED | 0 | 2 |
| LL (one-shot) | 3 | 256 KB | 1 | ok | 384.76 | - | - | - | - | PASSED | 0 | 2 |
| LL (one-shot) | 3 | 256 KB | 2 | ok | 385.36 | - | - | - | - | PASSED | 0 | 2 |
| Sentinel (one-shot) | 3 | 256 KB | 0 | ok | 450.90 | - | - | - | - | PASSED | 0 | 2 |
| Sentinel (one-shot) | 3 | 256 KB | 1 | crashed | - | - | - | - | - | PASSED | 1 | 2 |
| Sentinel (one-shot) | 3 | 256 KB | 2 | ok | 465.37 | - | - | - | - | PASSED | 0 | 2 |
| Two-shot LL | 3 | 256 KB | 0 | ok | 272.07 | - | - | - | - | PASSED | 0 | 2 |
| Two-shot LL | 3 | 256 KB | 1 | ok | 271.96 | - | - | - | - | PASSED | 0 | 2 |
| Two-shot LL | 3 | 256 KB | 2 | ok | 272.14 | - | - | - | - | PASSED | 0 | 2 |
| Two-shot Sentinel | 3 | 256 KB | 0 | ok | 312.80 | - | - | - | - | PASSED | 0 | 2 |
| Two-shot Sentinel | 3 | 256 KB | 1 | ok | 312.50 | - | - | - | - | PASSED | 0 | 2 |
| Two-shot Sentinel | 3 | 256 KB | 2 | ok | 312.71 | - | - | - | - | PASSED | 0 | 2 |
| LL128 Atomic | 3 | 256 KB | 0 | crashed | - | - | - | - | - | - | 1 | 2 |
| LL128 Atomic | 3 | 256 KB | 1 | crashed | - | - | - | - | - | - | 1 | 2 |
| LL128 Atomic | 3 | 256 KB | 2 | crashed | - | - | - | - | - | - | 1 | 2 |
| Bidirectional (2 ranks, 8 chunks per round) | 2 | 256 KB | 0 | ok | 178.65 | - | - | - | - | PASSED | 0 | 2 |
| Bidirectional (2 ranks, 8 chunks per round) | 2 | 256 KB | 1 | ok | 178.77 | - | - | - | - | PASSED | 0 | 2 |
| Baseline (with barrier) | 3 | 32 MB | 0 | ok | 54,604.96 | - | - | - | - | PASSED | 0 | 3 |
| Baseline (with barrier) | 3 | 32 MB | 1 | ok | 54,491.84 | - | - | - | - | PASSED | 0 | 3 |
| Baseline (with barrier) | 3 | 32 MB | 2 | ok | 54,449.86 | - | - | - | - | PASSED | 0 | 3 |
| LL (one-shot) | 3 | 32 MB | 0 | ok | 46,889.83 | - | - | - | - | PASSED | 0 | 3 |
| LL (one-shot) | 3 | 32 MB | 1 | ok | 46,875.79 | - | - | - | - | PASSED | 0 | 3 |
| LL (one-shot) | 3 | 32 MB | 2 | ok | 46,823.99 | - | - | - | - | PASSED | 0 | 3 |
| Sentinel (one-shot) | 3 | 32 MB | 0 | crashed | - | - | - | - | - | PASSED | 1 | 3 |
| Sentinel (one-shot) | 3 | 32 MB | 1 | ok | 55,216.64 | - | - | - | - | PASSED | 0 | 3 |
| Sentinel (one-shot) | 3 | 32 MB | 2 | ok | 55,356.81 | - | - | - | - | PASSED | 0 | 3 |
| Two-shot LL | 3 | 32 MB | 0 | ok | 31,810.88 | - | - | - | - | PASSED | 0 | 3 |
| Two-shot LL | 3 | 32 MB | 1 | ok | 31,812.05 | - | - | - | - | PASSED | 0 | 3 |
| Two-shot LL | 3 | 32 MB | 2 | ok | 31,781.17 | - | - | - | - | PASSED | 0 | 3 |
| Two-shot Sentinel | 3 | 32 MB | 0 | crashed | - | - | - | - | - | PASSED | 1 | 3 |
| Two-shot Sentinel | 3 | 32 MB | 1 | crashed | - | - | - | - | - | PASSED | 1 | 3 |
| Two-shot Sentinel | 3 | 32 MB | 2 | ok | 34,917.92 | - | - | - | - | PASSED | 0 | 3 |
| LL128 Atomic | 3 | 32 MB | 0 | crashed | - | - | - | - | - | - | 1 | 3 |
| LL128 Atomic | 3 | 32 MB | 1 | crashed | - | - | - | - | - | - | 1 | 3 |
| LL128 Atomic | 3 | 32 MB | 2 | crashed | - | - | - | - | - | - | 1 | 3 |
| Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 MB | 0 | ok | 19,598.39 | - | - | - | - | PASSED | 0 | 3 |
| Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 MB | 1 | ok | 19,597.53 | - | - | - | - | PASSED | 0 | 3 |
| Baseline (with barrier) | 2 | 32 B | 0 | ok | 4.79 | - | - | - | - | PASSED | 0 | 4 |
| Baseline (with barrier) | 2 | 32 B | 1 | ok | 4.79 | - | - | - | - | PASSED | 0 | 4 |
| LL (one-shot) | 2 | 32 B | 0 | hung | - | - | - | - | - | - | 124 | 4 |
| LL (one-shot) | 2 | 32 B | 1 | hung | - | - | - | - | - | - | 124 | 4 |
| Baseline (with barrier) | 2 | 64 B | 0 | ok | 1,583.21 | - | - | - | - | PASSED | 0 | 4 |
| Baseline (with barrier) | 2 | 64 B | 1 | ok | 1,583.21 | - | - | - | - | PASSED | 0 | 4 |
| LL (one-shot) | 2 | 64 B | 0 | hung | - | - | - | - | - | - | 124 | 4 |
| LL (one-shot) | 2 | 64 B | 1 | hung | - | - | - | - | - | - | 124 | 4 |
| Baseline (with barrier) | 2 | 128 B | 0 | ok | 1,970.11 | - | - | - | - | PASSED | 0 | 4 |
| Baseline (with barrier) | 2 | 128 B | 1 | ok | 1,970.11 | - | - | - | - | PASSED | 0 | 4 |
| LL (one-shot) | 2 | 128 B | 0 | hung | - | - | - | - | - | - | 124 | 4 |
| LL (one-shot) | 2 | 128 B | 1 | hung | - | - | - | - | - | - | 124 | 4 |
| Baseline (with barrier) | 2 | 32 KB | 0 | ok | 33.18 | - | - | - | - | PASSED | 0 | 4 |
| Baseline (with barrier) | 2 | 32 KB | 1 | ok | 33.18 | - | - | - | - | PASSED | 0 | 4 |
| LL (one-shot) | 2 | 32 KB | 0 | ok | 30.39 | - | - | - | - | PASSED | 0 | 4 |
| LL (one-shot) | 2 | 32 KB | 1 | ok | 30.40 | - | - | - | - | PASSED | 0 | 4 |
| Baseline (with barrier) | 2 | 32 MB | 0 | ok | 25,878.93 | - | - | - | - | PASSED | 0 | 4 |
| Baseline (with barrier) | 2 | 32 MB | 1 | ok | 25,890.84 | - | - | - | - | PASSED | 0 | 4 |
| LL (one-shot) | 2 | 32 MB | 0 | ok | 25,691.61 | - | - | - | - | PASSED | 0 | 4 |
| LL (one-shot) | 2 | 32 MB | 1 | ok | 25,688.85 | - | - | - | - | PASSED | 0 | 4 |

`log` = the number at the start of the file name in `logs/`:

- `1_3ranks_32B_64B_128B_32KB.txt`: 3 ranks, all algorithms, 32 B / 64 B / 128 B / 32 KB, 1000 rounds
- `2_3ranks_256KB.txt`: 3 ranks, all algorithms, 256 KB, 100 rounds
- `3_3ranks_32MB.txt`: 3 ranks, all algorithms, 32 MB, 100 rounds
- `4_2ranks_baseline_ll.txt`: 2 ranks, Baseline and LL, 32 B / 64 B / 128 B / 32 KB (1000 rounds) and 32 MB (100 rounds)

