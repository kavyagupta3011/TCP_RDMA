# Results: v1 (paper_algorithms)

**v1**: first RDMA version: blocking writes, one receive buffer, mean only.

Machines: rank 0 = compute02, rank 1 = compute03, rank 2 = compute04. PBS jobs: 30219.master.local, 30220.master.local.

A value is the time of one AllReduce (median over the timed rounds, averaged over the ranks that
finished correctly). Cells with two numbers are **pass 1 / pass 2**: the same experiment run twice,
the second time with the versions in the opposite order. If a rank crashed, hung (stopped by the
timeout) or got a wrong result, the cell says so. 1000 timed rounds for sizes up to 32 KB, 100
for 256 KB and above.

v1 prints only the mean, so every number for v1 is a mean, not a median.

## 1. Median, 3 ranks (pass 1 / pass 2)

| Algorithm | 32 B | 64 B | 128 B | 32 KB | 256 KB | 1 MB | 32 MB |
|---|---|---|---|---|---|---|---|
| Baseline (with barrier) | 1905.6 µs / 1526.5 µs | 11.42 µs / 1259.7 µs | 11.55 µs / 11.60 µs | 70.55 µs / 74.41 µs | 459.4 µs / 429.3 µs | 1657.6 µs / 1641.5 µs | 54.29 ms / 54.69 ms |
| LL (one-shot) | hung (rank 0/1/2) | hung (rank 0/1/2) | hung (rank 0/1/2) | 59.59 µs / 59.75 µs | 395.4 µs / 377.6 µs | 1526.0 µs / 1521.7 µs | 46.90 ms / 46.91 ms |
| Sentinel (one-shot) | 4.03 µs / 4.04 µs | 4.06 µs / 4.09 µs | 4.23 µs / 4.25 µs | 68.84 µs / 67.37 µs | 421.8 µs / 459.6 µs | 1736.1 µs / 1771.8 µs | 55.32 ms / 55.77 ms |
| Two-shot LL | 15.78 µs / 15.89 µs | 15.88 µs / 15.89 µs | 15.89 µs / 15.96 µs | 50.94 µs / 50.84 µs | 272.7 µs / 275.4 µs | 987.6 µs / 990.0 µs | 33.31 ms / 31.83 ms |
| Two-shot Sentinel | 7.99 µs / 8.00 µs | 8.02 µs / 7.79 µs | 8.06 µs / 8.10 µs | 49.29 µs / 49.21 µs | 314.9 µs / 327.9 µs | 1156.6 µs / 1110.5 µs | 35.41 ms / 35.03 ms |
| LL128 Atomic | crashed (rank 0/1/2) | crashed (rank 0/1/2) | crashed (rank 0/1/2) | crashed (rank 0/1/2) | crashed (rank 0/1/2) | crashed (rank 0/1/2) | crashed (rank 0/1/2) |
| Bidirectional (2 ranks, 8 chunks per round) | 31.43 µs / 31.51 µs | 31.55 µs / 31.40 µs | 31.56 µs / 31.56 µs | 55.62 µs / 55.55 µs | 179.1 µs / 179.2 µs | 565.2 µs / 565.0 µs | 19.65 ms / 19.59 ms |

## 2. p99, 3 ranks (average of the passes)

99% of the rounds were faster than this.

| Algorithm | 32 B | 64 B | 128 B | 32 KB | 256 KB | 1 MB | 32 MB |
|---|---|---|---|---|---|---|---|
| Baseline (with barrier) | ok (rank 0/1/2) | ok (rank 0/1/2) | ok (rank 0/1/2) | ok (rank 0/1/2) | ok (rank 0/1/2) | ok (rank 0/1/2) | ok (rank 0/1/2) |
| LL (one-shot) | hung (rank 0/1/2) | hung (rank 0/1/2) | hung (rank 0/1/2) | ok (rank 0/1/2) | ok (rank 0/1/2) | ok (rank 0/1/2) | ok (rank 0/1/2) |
| Sentinel (one-shot) | ok (rank 0/1/2) | ok (rank 0/1/2) | ok (rank 0/1/2) | ok (rank 0/1/2) | ok (rank 0/1/2) | ok (rank 0/2), crashed (rank 1) | crashed (rank 0/1), ok (rank 2) |
| Two-shot LL | ok (rank 0/1/2) | ok (rank 0/1/2) | ok (rank 0/1/2) | ok (rank 0/1/2) | ok (rank 0/1/2) | ok (rank 0/1/2) | ok (rank 0/1/2) |
| Two-shot Sentinel | ok (rank 0/1/2) | ok (rank 0/1/2) | ok (rank 0/1/2) | ok (rank 0/1/2) | ok (rank 0/1/2) | ok (rank 0/1/2) | crashed (rank 0/1), ok (rank 2) |
| LL128 Atomic | crashed (rank 0/1/2) | crashed (rank 0/1/2) | crashed (rank 0/1/2) | crashed (rank 0/1/2) | crashed (rank 0/1/2) | crashed (rank 0/1/2) | crashed (rank 0/1/2) |
| Bidirectional (2 ranks, 8 chunks per round) | ok (rank 0/1) | ok (rank 0/1) | ok (rank 0/1) | ok (rank 0/1) | ok (rank 0/1) | ok (rank 0/1) | ok (rank 0/1) |

## 3. Cost of the barrier: Baseline vs LL, 3 ranks

|  | 32 B | 64 B | 128 B | 32 KB | 256 KB | 1 MB | 32 MB |
|---|---|---|---|---|---|---|---|
| Baseline - LL | - | - | - | 12.81 µs | 57.85 µs | 125.7 µs | 7585.5 µs |
| Baseline / LL | - | - | - | 1.21x | 1.15x | 1.08x | 1.16x |

## 4. 2 ranks vs 3 ranks

|  | 32 B | 32 KB | 32 MB |
|---|---|---|---|
| Baseline (with barrier), 2 ranks | 1220.5 µs | 32.29 µs | 26.12 ms |
| Baseline (with barrier), 3 ranks | 1716.0 µs | 72.48 µs | 54.49 ms |
| LL (one-shot), 2 ranks | hung (rank 0/1) | 30.52 µs | 26.34 ms |
| LL (one-shot), 3 ranks | hung (rank 0/1/2) | 59.67 µs | 46.90 ms |

## 5. Correctness and errors

| Log | PASSED lines | FAILED lines | ranks ok | crashed | hung | wrong result | skipped |
|---|---|---|---|---|---|---|---|
| `pass1_1_3ranks_32B_64B_128B_32KB.txt` | 59 | 0 | 59 | 12 | 9 | 0 | 0 |
| `pass1_2_3ranks_256KB.txt` | 17 | 0 | 17 | 3 | 0 | 0 | 0 |
| `pass1_3_3ranks_1MB.txt` | 17 | 0 | 16 | 4 | 0 | 0 | 0 |
| `pass1_4_3ranks_32MB.txt` | 17 | 0 | 13 | 7 | 0 | 0 | 0 |
| `pass1_5_2ranks.txt` | 10 | 0 | 10 | 0 | 2 | 0 | 0 |
| `pass2_1_3ranks_32B_64B_128B_32KB.txt` | 59 | 0 | 58 | 13 | 9 | 0 | 0 |
| `pass2_2_3ranks_256KB.txt` | 17 | 0 | 16 | 4 | 0 | 0 | 0 |
| `pass2_3_3ranks_1MB.txt` | 17 | 0 | 14 | 6 | 0 | 0 | 0 |
| `pass2_4_3ranks_32MB.txt` | 17 | 0 | 15 | 5 | 0 | 0 | 0 |
| `pass2_5_2ranks.txt` | 10 | 0 | 10 | 0 | 2 | 0 | 0 |

Error messages (first one per rank):

- pass1 LL128 Atomic, 32 B, rank 0: `RDMA error: ibv_post_send (ATOMIC_FETCH_AND_ADD) failed -- does this HCA/link support RDMA atomics? (mlx4 does) (../common/rdma_common.c:328, errno=22 Invalid argument)`
- pass1 LL128 Atomic, 32 B, rank 1: `RDMA error: ibv_post_send (ATOMIC_FETCH_AND_ADD) failed -- does this HCA/link support RDMA atomics? (mlx4 does) (../common/rdma_common.c:328, errno=22 Invalid argument)`
- pass1 LL128 Atomic, 32 B, rank 2: `RDMA error: ibv_post_send (ATOMIC_FETCH_AND_ADD) failed -- does this HCA/link support RDMA atomics? (mlx4 does) (../common/rdma_common.c:328, errno=22 Invalid argument)`
- pass1 LL128 Atomic, 64 B, rank 0: `RDMA error: ibv_post_send (ATOMIC_FETCH_AND_ADD) failed -- does this HCA/link support RDMA atomics? (mlx4 does) (../common/rdma_common.c:328, errno=22 Invalid argument)`
- pass1 LL128 Atomic, 64 B, rank 1: `RDMA error: ibv_post_send (ATOMIC_FETCH_AND_ADD) failed -- does this HCA/link support RDMA atomics? (mlx4 does) (../common/rdma_common.c:328, errno=22 Invalid argument)`
- pass1 LL128 Atomic, 64 B, rank 2: `RDMA error: ibv_post_send (ATOMIC_FETCH_AND_ADD) failed -- does this HCA/link support RDMA atomics? (mlx4 does) (../common/rdma_common.c:328, errno=22 Invalid argument)`
- pass1 LL128 Atomic, 128 B, rank 0: `RDMA error: ibv_post_send (ATOMIC_FETCH_AND_ADD) failed -- does this HCA/link support RDMA atomics? (mlx4 does) (../common/rdma_common.c:328, errno=22 Invalid argument)`
- pass1 LL128 Atomic, 128 B, rank 1: `RDMA error: ibv_post_send (ATOMIC_FETCH_AND_ADD) failed -- does this HCA/link support RDMA atomics? (mlx4 does) (../common/rdma_common.c:328, errno=22 Invalid argument)`
- pass1 LL128 Atomic, 128 B, rank 2: `RDMA error: ibv_post_send (ATOMIC_FETCH_AND_ADD) failed -- does this HCA/link support RDMA atomics? (mlx4 does) (../common/rdma_common.c:328, errno=22 Invalid argument)`
- pass1 LL128 Atomic, 32 KB, rank 0: `RDMA error: ibv_post_send (ATOMIC_FETCH_AND_ADD) failed -- does this HCA/link support RDMA atomics? (mlx4 does) (../common/rdma_common.c:328, errno=22 Invalid argument)`
- pass1 LL128 Atomic, 32 KB, rank 1: `RDMA error: ibv_post_send (ATOMIC_FETCH_AND_ADD) failed -- does this HCA/link support RDMA atomics? (mlx4 does) (../common/rdma_common.c:328, errno=22 Invalid argument)`
- pass1 LL128 Atomic, 32 KB, rank 2: `RDMA error: ibv_post_send (ATOMIC_FETCH_AND_ADD) failed -- does this HCA/link support RDMA atomics? (mlx4 does) (../common/rdma_common.c:328, errno=22 Invalid argument)`
- pass1 LL128 Atomic, 256 KB, rank 0: `RDMA error: ibv_post_send (ATOMIC_FETCH_AND_ADD) failed -- does this HCA/link support RDMA atomics? (mlx4 does) (../common/rdma_common.c:328, errno=22 Invalid argument)`
- pass1 LL128 Atomic, 256 KB, rank 1: `RDMA error: ibv_post_send (ATOMIC_FETCH_AND_ADD) failed -- does this HCA/link support RDMA atomics? (mlx4 does) (../common/rdma_common.c:328, errno=22 Invalid argument)`
- pass1 LL128 Atomic, 256 KB, rank 2: `RDMA error: ibv_post_send (ATOMIC_FETCH_AND_ADD) failed -- does this HCA/link support RDMA atomics? (mlx4 does) (../common/rdma_common.c:328, errno=22 Invalid argument)`
- pass1 Sentinel (one-shot), 1 MB, rank 1: `RDMA work completion error: transport retry counter exceeded (wr_id=4126, expected=4126)`
- pass1 LL128 Atomic, 1 MB, rank 0: `RDMA error: ibv_post_send (ATOMIC_FETCH_AND_ADD) failed -- does this HCA/link support RDMA atomics? (mlx4 does) (../common/rdma_common.c:328, errno=22 Invalid argument)`
- pass1 LL128 Atomic, 1 MB, rank 1: `RDMA error: ibv_post_send (ATOMIC_FETCH_AND_ADD) failed -- does this HCA/link support RDMA atomics? (mlx4 does) (../common/rdma_common.c:328, errno=22 Invalid argument)`
- pass1 LL128 Atomic, 1 MB, rank 2: `RDMA error: ibv_post_send (ATOMIC_FETCH_AND_ADD) failed -- does this HCA/link support RDMA atomics? (mlx4 does) (../common/rdma_common.c:328, errno=22 Invalid argument)`
- pass1 Sentinel (one-shot), 32 MB, rank 0: `RDMA work completion error: transport retry counter exceeded (wr_id=4126, expected=4126)`
- pass1 Sentinel (one-shot), 32 MB, rank 1: `RDMA work completion error: transport retry counter exceeded (wr_id=4126, expected=4126)`
- pass1 Two-shot Sentinel, 32 MB, rank 0: `RDMA work completion error: transport retry counter exceeded (wr_id=4126, expected=4126)`
- pass1 Two-shot Sentinel, 32 MB, rank 1: `RDMA work completion error: transport retry counter exceeded (wr_id=4126, expected=4126)`
- pass1 LL128 Atomic, 32 MB, rank 0: `RDMA error: ibv_post_send (ATOMIC_FETCH_AND_ADD) failed -- does this HCA/link support RDMA atomics? (mlx4 does) (../common/rdma_common.c:328, errno=22 Invalid argument)`
- pass1 LL128 Atomic, 32 MB, rank 1: `RDMA work completion error: transport retry counter exceeded (wr_id=4126, expected=4126)`
- pass1 LL128 Atomic, 32 MB, rank 2: `RDMA work completion error: transport retry counter exceeded (wr_id=4126, expected=4126)`
- pass2 LL128 Atomic, 32 B, rank 0: `RDMA error: ibv_post_send (ATOMIC_FETCH_AND_ADD) failed -- does this HCA/link support RDMA atomics? (mlx4 does) (../common/rdma_common.c:328, errno=22 Invalid argument)`
- pass2 LL128 Atomic, 32 B, rank 1: `RDMA error: ibv_post_send (ATOMIC_FETCH_AND_ADD) failed -- does this HCA/link support RDMA atomics? (mlx4 does) (../common/rdma_common.c:328, errno=22 Invalid argument)`
- pass2 LL128 Atomic, 32 B, rank 2: `RDMA error: ibv_post_send (ATOMIC_FETCH_AND_ADD) failed -- does this HCA/link support RDMA atomics? (mlx4 does) (../common/rdma_common.c:328, errno=22 Invalid argument)`
- pass2 Two-shot Sentinel, 64 B, rank 0: `RDMA work completion error: transport retry counter exceeded (wr_id=4126, expected=4126)`
- pass2 LL128 Atomic, 64 B, rank 0: `RDMA error: ibv_post_send (ATOMIC_FETCH_AND_ADD) failed -- does this HCA/link support RDMA atomics? (mlx4 does) (../common/rdma_common.c:328, errno=22 Invalid argument)`
- pass2 LL128 Atomic, 64 B, rank 1: `RDMA error: ibv_post_send (ATOMIC_FETCH_AND_ADD) failed -- does this HCA/link support RDMA atomics? (mlx4 does) (../common/rdma_common.c:328, errno=22 Invalid argument)`
- pass2 LL128 Atomic, 64 B, rank 2: `RDMA error: ibv_post_send (ATOMIC_FETCH_AND_ADD) failed -- does this HCA/link support RDMA atomics? (mlx4 does) (../common/rdma_common.c:328, errno=22 Invalid argument)`
- pass2 LL128 Atomic, 128 B, rank 0: `RDMA error: ibv_post_send (ATOMIC_FETCH_AND_ADD) failed -- does this HCA/link support RDMA atomics? (mlx4 does) (../common/rdma_common.c:328, errno=22 Invalid argument)`
- pass2 LL128 Atomic, 128 B, rank 1: `RDMA error: ibv_post_send (ATOMIC_FETCH_AND_ADD) failed -- does this HCA/link support RDMA atomics? (mlx4 does) (../common/rdma_common.c:328, errno=22 Invalid argument)`
- pass2 LL128 Atomic, 128 B, rank 2: `RDMA error: ibv_post_send (ATOMIC_FETCH_AND_ADD) failed -- does this HCA/link support RDMA atomics? (mlx4 does) (../common/rdma_common.c:328, errno=22 Invalid argument)`
- pass2 LL128 Atomic, 32 KB, rank 0: `RDMA error: ibv_post_send (ATOMIC_FETCH_AND_ADD) failed -- does this HCA/link support RDMA atomics? (mlx4 does) (../common/rdma_common.c:328, errno=22 Invalid argument)`
- pass2 LL128 Atomic, 32 KB, rank 1: `RDMA error: ibv_post_send (ATOMIC_FETCH_AND_ADD) failed -- does this HCA/link support RDMA atomics? (mlx4 does) (../common/rdma_common.c:328, errno=22 Invalid argument)`
- pass2 LL128 Atomic, 32 KB, rank 2: `RDMA error: ibv_post_send (ATOMIC_FETCH_AND_ADD) failed -- does this HCA/link support RDMA atomics? (mlx4 does) (../common/rdma_common.c:328, errno=22 Invalid argument)`
- pass2 Sentinel (one-shot), 256 KB, rank 1: `RDMA work completion error: transport retry counter exceeded (wr_id=4126, expected=4126)`
- pass2 LL128 Atomic, 256 KB, rank 0: `RDMA error: ibv_post_send (ATOMIC_FETCH_AND_ADD) failed -- does this HCA/link support RDMA atomics? (mlx4 does) (../common/rdma_common.c:328, errno=22 Invalid argument)`
- pass2 LL128 Atomic, 256 KB, rank 1: `RDMA error: ibv_post_send (ATOMIC_FETCH_AND_ADD) failed -- does this HCA/link support RDMA atomics? (mlx4 does) (../common/rdma_common.c:328, errno=22 Invalid argument)`
- pass2 LL128 Atomic, 256 KB, rank 2: `RDMA error: ibv_post_send (ATOMIC_FETCH_AND_ADD) failed -- does this HCA/link support RDMA atomics? (mlx4 does) (../common/rdma_common.c:328, errno=22 Invalid argument)`
- pass2 Sentinel (one-shot), 1 MB, rank 1: `RDMA work completion error: transport retry counter exceeded (wr_id=4126, expected=4126)`
- pass2 Two-shot Sentinel, 1 MB, rank 0: `RDMA work completion error: transport retry counter exceeded (wr_id=4126, expected=4126)`
- pass2 Two-shot Sentinel, 1 MB, rank 1: `RDMA work completion error: transport retry counter exceeded (wr_id=4126, expected=4126)`
- pass2 LL128 Atomic, 1 MB, rank 0: `RDMA error: ibv_post_send (ATOMIC_FETCH_AND_ADD) failed -- does this HCA/link support RDMA atomics? (mlx4 does) (../common/rdma_common.c:328, errno=22 Invalid argument)`
- pass2 LL128 Atomic, 1 MB, rank 1: `RDMA error: ibv_post_send (ATOMIC_FETCH_AND_ADD) failed -- does this HCA/link support RDMA atomics? (mlx4 does) (../common/rdma_common.c:328, errno=22 Invalid argument)`
- pass2 LL128 Atomic, 1 MB, rank 2: `RDMA error: ibv_post_send (ATOMIC_FETCH_AND_ADD) failed -- does this HCA/link support RDMA atomics? (mlx4 does) (../common/rdma_common.c:328, errno=22 Invalid argument)`
- pass2 Two-shot Sentinel, 32 MB, rank 0: `RDMA work completion error: transport retry counter exceeded (wr_id=4126, expected=4126)`
- pass2 Two-shot Sentinel, 32 MB, rank 1: `RDMA work completion error: transport retry counter exceeded (wr_id=4126, expected=4126)`
- pass2 LL128 Atomic, 32 MB, rank 0: `RDMA error: ibv_post_send (ATOMIC_FETCH_AND_ADD) failed -- does this HCA/link support RDMA atomics? (mlx4 does) (../common/rdma_common.c:328, errno=22 Invalid argument)`
- pass2 LL128 Atomic, 32 MB, rank 1: `RDMA work completion error: transport retry counter exceeded (wr_id=4126, expected=4126)`
- pass2 LL128 Atomic, 32 MB, rank 2: `RDMA work completion error: transport retry counter exceeded (wr_id=4126, expected=4126)`

## 6. Every run, per rank

Times in µs. `-` = not printed by this version or the rank did not finish.

| pass | Program | ranks | size | rank | status | mean | min | median | p99 | max | correctness | exit |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| pass1 | Baseline (with barrier) | 3 | 32 B | 0 | ok | 1,905.59 | - | - | - | - | PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 32 B | 1 | ok | 1,905.59 | - | - | - | - | PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 32 B | 2 | ok | 1,905.59 | - | - | - | - | PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 32 B | 0 | hung | - | - | - | - | - | - | 124 |
| pass1 | LL (one-shot) | 3 | 32 B | 1 | hung | - | - | - | - | - | - | 124 |
| pass1 | LL (one-shot) | 3 | 32 B | 2 | hung | - | - | - | - | - | - | 124 |
| pass1 | Sentinel (one-shot) | 3 | 32 B | 0 | ok | 4.03 | - | - | - | - | PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 32 B | 1 | ok | 4.02 | - | - | - | - | PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 32 B | 2 | ok | 4.05 | - | - | - | - | PASSED | 0 |
| pass1 | Two-shot LL | 3 | 32 B | 0 | ok | 15.78 | - | - | - | - | PASSED | 0 |
| pass1 | Two-shot LL | 3 | 32 B | 1 | ok | 15.78 | - | - | - | - | PASSED | 0 |
| pass1 | Two-shot LL | 3 | 32 B | 2 | ok | 15.78 | - | - | - | - | PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 32 B | 0 | ok | 7.99 | - | - | - | - | PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 32 B | 1 | ok | 7.98 | - | - | - | - | PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 32 B | 2 | ok | 8.00 | - | - | - | - | PASSED | 0 |
| pass1 | LL128 Atomic | 3 | 32 B | 0 | crashed | - | - | - | - | - | - | 1 |
| pass1 | LL128 Atomic | 3 | 32 B | 1 | crashed | - | - | - | - | - | - | 1 |
| pass1 | LL128 Atomic | 3 | 32 B | 2 | crashed | - | - | - | - | - | - | 1 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 B | 0 | ok | 31.43 | - | - | - | - | PASSED | 0 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 B | 1 | ok | 31.43 | - | - | - | - | PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 64 B | 0 | ok | 11.42 | - | - | - | - | PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 64 B | 1 | ok | 11.41 | - | - | - | - | PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 64 B | 2 | ok | 11.41 | - | - | - | - | PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 64 B | 0 | hung | - | - | - | - | - | - | 124 |
| pass1 | LL (one-shot) | 3 | 64 B | 1 | hung | - | - | - | - | - | - | 124 |
| pass1 | LL (one-shot) | 3 | 64 B | 2 | hung | - | - | - | - | - | - | 124 |
| pass1 | Sentinel (one-shot) | 3 | 64 B | 0 | ok | 4.06 | - | - | - | - | PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 64 B | 1 | ok | 4.06 | - | - | - | - | PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 64 B | 2 | ok | 4.07 | - | - | - | - | PASSED | 0 |
| pass1 | Two-shot LL | 3 | 64 B | 0 | ok | 15.88 | - | - | - | - | PASSED | 0 |
| pass1 | Two-shot LL | 3 | 64 B | 1 | ok | 15.88 | - | - | - | - | PASSED | 0 |
| pass1 | Two-shot LL | 3 | 64 B | 2 | ok | 15.88 | - | - | - | - | PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 64 B | 0 | ok | 8.02 | - | - | - | - | PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 64 B | 1 | ok | 8.01 | - | - | - | - | PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 64 B | 2 | ok | 8.02 | - | - | - | - | PASSED | 0 |
| pass1 | LL128 Atomic | 3 | 64 B | 0 | crashed | - | - | - | - | - | - | 1 |
| pass1 | LL128 Atomic | 3 | 64 B | 1 | crashed | - | - | - | - | - | - | 1 |
| pass1 | LL128 Atomic | 3 | 64 B | 2 | crashed | - | - | - | - | - | - | 1 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 64 B | 0 | ok | 31.55 | - | - | - | - | PASSED | 0 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 64 B | 1 | ok | 31.55 | - | - | - | - | PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 128 B | 0 | ok | 11.55 | - | - | - | - | PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 128 B | 1 | ok | 11.54 | - | - | - | - | PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 128 B | 2 | ok | 11.54 | - | - | - | - | PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 128 B | 0 | hung | - | - | - | - | - | - | 124 |
| pass1 | LL (one-shot) | 3 | 128 B | 1 | hung | - | - | - | - | - | - | 124 |
| pass1 | LL (one-shot) | 3 | 128 B | 2 | hung | - | - | - | - | - | - | 124 |
| pass1 | Sentinel (one-shot) | 3 | 128 B | 0 | ok | 4.24 | - | - | - | - | PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 128 B | 1 | ok | 4.23 | - | - | - | - | PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 128 B | 2 | ok | 4.24 | - | - | - | - | PASSED | 0 |
| pass1 | Two-shot LL | 3 | 128 B | 0 | ok | 15.89 | - | - | - | - | PASSED | 0 |
| pass1 | Two-shot LL | 3 | 128 B | 1 | ok | 15.89 | - | - | - | - | PASSED | 0 |
| pass1 | Two-shot LL | 3 | 128 B | 2 | ok | 15.89 | - | - | - | - | PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 128 B | 0 | ok | 8.05 | - | - | - | - | PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 128 B | 1 | ok | 8.07 | - | - | - | - | PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 128 B | 2 | ok | 8.06 | - | - | - | - | PASSED | 0 |
| pass1 | LL128 Atomic | 3 | 128 B | 0 | crashed | - | - | - | - | - | - | 1 |
| pass1 | LL128 Atomic | 3 | 128 B | 1 | crashed | - | - | - | - | - | - | 1 |
| pass1 | LL128 Atomic | 3 | 128 B | 2 | crashed | - | - | - | - | - | - | 1 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 128 B | 0 | ok | 31.56 | - | - | - | - | PASSED | 0 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 128 B | 1 | ok | 31.56 | - | - | - | - | PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 32 KB | 0 | ok | 70.56 | - | - | - | - | PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 32 KB | 1 | ok | 70.55 | - | - | - | - | PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 32 KB | 2 | ok | 70.55 | - | - | - | - | PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 32 KB | 0 | ok | 59.59 | - | - | - | - | PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 32 KB | 1 | ok | 59.59 | - | - | - | - | PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 32 KB | 2 | ok | 59.59 | - | - | - | - | PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 32 KB | 0 | ok | 68.81 | - | - | - | - | PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 32 KB | 1 | ok | 68.85 | - | - | - | - | PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 32 KB | 2 | ok | 68.86 | - | - | - | - | PASSED | 0 |
| pass1 | Two-shot LL | 3 | 32 KB | 0 | ok | 50.94 | - | - | - | - | PASSED | 0 |
| pass1 | Two-shot LL | 3 | 32 KB | 1 | ok | 50.94 | - | - | - | - | PASSED | 0 |
| pass1 | Two-shot LL | 3 | 32 KB | 2 | ok | 50.94 | - | - | - | - | PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 32 KB | 0 | ok | 49.30 | - | - | - | - | PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 32 KB | 1 | ok | 49.34 | - | - | - | - | PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 32 KB | 2 | ok | 49.24 | - | - | - | - | PASSED | 0 |
| pass1 | LL128 Atomic | 3 | 32 KB | 0 | crashed | - | - | - | - | - | - | 1 |
| pass1 | LL128 Atomic | 3 | 32 KB | 1 | crashed | - | - | - | - | - | - | 1 |
| pass1 | LL128 Atomic | 3 | 32 KB | 2 | crashed | - | - | - | - | - | - | 1 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 KB | 0 | ok | 55.62 | - | - | - | - | PASSED | 0 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 KB | 1 | ok | 55.62 | - | - | - | - | PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 256 KB | 0 | ok | 460.14 | - | - | - | - | PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 256 KB | 1 | ok | 458.97 | - | - | - | - | PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 256 KB | 2 | ok | 459.05 | - | - | - | - | PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 256 KB | 0 | ok | 395.56 | - | - | - | - | PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 256 KB | 1 | ok | 395.71 | - | - | - | - | PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 256 KB | 2 | ok | 394.90 | - | - | - | - | PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 256 KB | 0 | ok | 420.65 | - | - | - | - | PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 256 KB | 1 | ok | 422.26 | - | - | - | - | PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 256 KB | 2 | ok | 422.52 | - | - | - | - | PASSED | 0 |
| pass1 | Two-shot LL | 3 | 256 KB | 0 | ok | 272.67 | - | - | - | - | PASSED | 0 |
| pass1 | Two-shot LL | 3 | 256 KB | 1 | ok | 272.69 | - | - | - | - | PASSED | 0 |
| pass1 | Two-shot LL | 3 | 256 KB | 2 | ok | 272.64 | - | - | - | - | PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 256 KB | 0 | ok | 315.05 | - | - | - | - | PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 256 KB | 1 | ok | 314.79 | - | - | - | - | PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 256 KB | 2 | ok | 314.73 | - | - | - | - | PASSED | 0 |
| pass1 | LL128 Atomic | 3 | 256 KB | 0 | crashed | - | - | - | - | - | - | 1 |
| pass1 | LL128 Atomic | 3 | 256 KB | 1 | crashed | - | - | - | - | - | - | 1 |
| pass1 | LL128 Atomic | 3 | 256 KB | 2 | crashed | - | - | - | - | - | - | 1 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 256 KB | 0 | ok | 179.10 | - | - | - | - | PASSED | 0 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 256 KB | 1 | ok | 179.10 | - | - | - | - | PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 1 MB | 0 | ok | 1,658.72 | - | - | - | - | PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 1 MB | 1 | ok | 1,656.79 | - | - | - | - | PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 1 MB | 2 | ok | 1,657.28 | - | - | - | - | PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 1 MB | 0 | ok | 1,526.67 | - | - | - | - | PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 1 MB | 1 | ok | 1,525.66 | - | - | - | - | PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 1 MB | 2 | ok | 1,525.68 | - | - | - | - | PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 1 MB | 0 | ok | 1,726.71 | - | - | - | - | PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 1 MB | 1 | crashed | - | - | - | - | - | PASSED | 1 |
| pass1 | Sentinel (one-shot) | 3 | 1 MB | 2 | ok | 1,745.48 | - | - | - | - | PASSED | 0 |
| pass1 | Two-shot LL | 3 | 1 MB | 0 | ok | 987.34 | - | - | - | - | PASSED | 0 |
| pass1 | Two-shot LL | 3 | 1 MB | 1 | ok | 988.14 | - | - | - | - | PASSED | 0 |
| pass1 | Two-shot LL | 3 | 1 MB | 2 | ok | 987.32 | - | - | - | - | PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 1 MB | 0 | ok | 1,156.90 | - | - | - | - | PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 1 MB | 1 | ok | 1,158.64 | - | - | - | - | PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 1 MB | 2 | ok | 1,154.13 | - | - | - | - | PASSED | 0 |
| pass1 | LL128 Atomic | 3 | 1 MB | 0 | crashed | - | - | - | - | - | - | 1 |
| pass1 | LL128 Atomic | 3 | 1 MB | 1 | crashed | - | - | - | - | - | - | 1 |
| pass1 | LL128 Atomic | 3 | 1 MB | 2 | crashed | - | - | - | - | - | - | 1 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 1 MB | 0 | ok | 565.18 | - | - | - | - | PASSED | 0 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 1 MB | 1 | ok | 565.28 | - | - | - | - | PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 32 MB | 0 | ok | 54,349.84 | - | - | - | - | PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 32 MB | 1 | ok | 54,273.45 | - | - | - | - | PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 32 MB | 2 | ok | 54,235.73 | - | - | - | - | PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 32 MB | 0 | ok | 46,909.67 | - | - | - | - | PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 32 MB | 1 | ok | 46,921.96 | - | - | - | - | PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 32 MB | 2 | ok | 46,876.97 | - | - | - | - | PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 32 MB | 0 | crashed | - | - | - | - | - | PASSED | 1 |
| pass1 | Sentinel (one-shot) | 3 | 32 MB | 1 | crashed | - | - | - | - | - | PASSED | 1 |
| pass1 | Sentinel (one-shot) | 3 | 32 MB | 2 | ok | 55,323.56 | - | - | - | - | PASSED | 0 |
| pass1 | Two-shot LL | 3 | 32 MB | 0 | ok | 33,312.45 | - | - | - | - | PASSED | 0 |
| pass1 | Two-shot LL | 3 | 32 MB | 1 | ok | 33,322.17 | - | - | - | - | PASSED | 0 |
| pass1 | Two-shot LL | 3 | 32 MB | 2 | ok | 33,290.00 | - | - | - | - | PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 32 MB | 0 | crashed | - | - | - | - | - | PASSED | 1 |
| pass1 | Two-shot Sentinel | 3 | 32 MB | 1 | crashed | - | - | - | - | - | PASSED | 1 |
| pass1 | Two-shot Sentinel | 3 | 32 MB | 2 | ok | 35,408.76 | - | - | - | - | PASSED | 0 |
| pass1 | LL128 Atomic | 3 | 32 MB | 0 | crashed | - | - | - | - | - | - | 1 |
| pass1 | LL128 Atomic | 3 | 32 MB | 1 | crashed | - | - | - | - | - | - | 1 |
| pass1 | LL128 Atomic | 3 | 32 MB | 2 | crashed | - | - | - | - | - | - | 1 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 MB | 0 | ok | 19,651.46 | - | - | - | - | PASSED | 0 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 MB | 1 | ok | 19,651.93 | - | - | - | - | PASSED | 0 |
| pass1 | Baseline (with barrier) | 2 | 32 B | 0 | ok | 1,320.96 | - | - | - | - | PASSED | 0 |
| pass1 | Baseline (with barrier) | 2 | 32 B | 1 | ok | 1,320.94 | - | - | - | - | PASSED | 0 |
| pass1 | LL (one-shot) | 2 | 32 B | 0 | hung | - | - | - | - | - | - | 124 |
| pass1 | LL (one-shot) | 2 | 32 B | 1 | hung | - | - | - | - | - | - | 124 |
| pass1 | Baseline (with barrier) | 2 | 32 KB | 0 | ok | 32.27 | - | - | - | - | PASSED | 0 |
| pass1 | Baseline (with barrier) | 2 | 32 KB | 1 | ok | 32.27 | - | - | - | - | PASSED | 0 |
| pass1 | LL (one-shot) | 2 | 32 KB | 0 | ok | 30.41 | - | - | - | - | PASSED | 0 |
| pass1 | LL (one-shot) | 2 | 32 KB | 1 | ok | 30.41 | - | - | - | - | PASSED | 0 |
| pass1 | Baseline (with barrier) | 2 | 32 MB | 0 | ok | 25,999.38 | - | - | - | - | PASSED | 0 |
| pass1 | Baseline (with barrier) | 2 | 32 MB | 1 | ok | 25,996.25 | - | - | - | - | PASSED | 0 |
| pass1 | LL (one-shot) | 2 | 32 MB | 0 | ok | 26,599.88 | - | - | - | - | PASSED | 0 |
| pass1 | LL (one-shot) | 2 | 32 MB | 1 | ok | 26,580.88 | - | - | - | - | PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 32 B | 0 | ok | 1,526.48 | - | - | - | - | PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 32 B | 1 | ok | 1,526.48 | - | - | - | - | PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 32 B | 2 | ok | 1,526.48 | - | - | - | - | PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 32 B | 0 | hung | - | - | - | - | - | - | 124 |
| pass2 | LL (one-shot) | 3 | 32 B | 1 | hung | - | - | - | - | - | - | 124 |
| pass2 | LL (one-shot) | 3 | 32 B | 2 | hung | - | - | - | - | - | - | 124 |
| pass2 | Sentinel (one-shot) | 3 | 32 B | 0 | ok | 4.03 | - | - | - | - | PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 32 B | 1 | ok | 4.04 | - | - | - | - | PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 32 B | 2 | ok | 4.05 | - | - | - | - | PASSED | 0 |
| pass2 | Two-shot LL | 3 | 32 B | 0 | ok | 15.89 | - | - | - | - | PASSED | 0 |
| pass2 | Two-shot LL | 3 | 32 B | 1 | ok | 15.89 | - | - | - | - | PASSED | 0 |
| pass2 | Two-shot LL | 3 | 32 B | 2 | ok | 15.89 | - | - | - | - | PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 32 B | 0 | ok | 8.01 | - | - | - | - | PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 32 B | 1 | ok | 7.98 | - | - | - | - | PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 32 B | 2 | ok | 8.01 | - | - | - | - | PASSED | 0 |
| pass2 | LL128 Atomic | 3 | 32 B | 0 | crashed | - | - | - | - | - | - | 1 |
| pass2 | LL128 Atomic | 3 | 32 B | 1 | crashed | - | - | - | - | - | - | 1 |
| pass2 | LL128 Atomic | 3 | 32 B | 2 | crashed | - | - | - | - | - | - | 1 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 B | 0 | ok | 31.51 | - | - | - | - | PASSED | 0 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 B | 1 | ok | 31.51 | - | - | - | - | PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 64 B | 0 | ok | 1,259.70 | - | - | - | - | PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 64 B | 1 | ok | 1,259.70 | - | - | - | - | PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 64 B | 2 | ok | 1,259.69 | - | - | - | - | PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 64 B | 0 | hung | - | - | - | - | - | - | 124 |
| pass2 | LL (one-shot) | 3 | 64 B | 1 | hung | - | - | - | - | - | - | 124 |
| pass2 | LL (one-shot) | 3 | 64 B | 2 | hung | - | - | - | - | - | - | 124 |
| pass2 | Sentinel (one-shot) | 3 | 64 B | 0 | ok | 4.08 | - | - | - | - | PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 64 B | 1 | ok | 4.11 | - | - | - | - | PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 64 B | 2 | ok | 4.08 | - | - | - | - | PASSED | 0 |
| pass2 | Two-shot LL | 3 | 64 B | 0 | ok | 15.89 | - | - | - | - | PASSED | 0 |
| pass2 | Two-shot LL | 3 | 64 B | 1 | ok | 15.89 | - | - | - | - | PASSED | 0 |
| pass2 | Two-shot LL | 3 | 64 B | 2 | ok | 15.88 | - | - | - | - | PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 64 B | 0 | crashed | - | - | - | - | - | PASSED | 1 |
| pass2 | Two-shot Sentinel | 3 | 64 B | 1 | ok | 8.04 | - | - | - | - | PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 64 B | 2 | ok | 7.54 | - | - | - | - | PASSED | 0 |
| pass2 | LL128 Atomic | 3 | 64 B | 0 | crashed | - | - | - | - | - | - | 1 |
| pass2 | LL128 Atomic | 3 | 64 B | 1 | crashed | - | - | - | - | - | - | 1 |
| pass2 | LL128 Atomic | 3 | 64 B | 2 | crashed | - | - | - | - | - | - | 1 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 64 B | 0 | ok | 31.39 | - | - | - | - | PASSED | 0 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 64 B | 1 | ok | 31.41 | - | - | - | - | PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 128 B | 0 | ok | 11.60 | - | - | - | - | PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 128 B | 1 | ok | 11.60 | - | - | - | - | PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 128 B | 2 | ok | 11.60 | - | - | - | - | PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 128 B | 0 | hung | - | - | - | - | - | - | 124 |
| pass2 | LL (one-shot) | 3 | 128 B | 1 | hung | - | - | - | - | - | - | 124 |
| pass2 | LL (one-shot) | 3 | 128 B | 2 | hung | - | - | - | - | - | - | 124 |
| pass2 | Sentinel (one-shot) | 3 | 128 B | 0 | ok | 4.24 | - | - | - | - | PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 128 B | 1 | ok | 4.26 | - | - | - | - | PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 128 B | 2 | ok | 4.24 | - | - | - | - | PASSED | 0 |
| pass2 | Two-shot LL | 3 | 128 B | 0 | ok | 15.96 | - | - | - | - | PASSED | 0 |
| pass2 | Two-shot LL | 3 | 128 B | 1 | ok | 15.96 | - | - | - | - | PASSED | 0 |
| pass2 | Two-shot LL | 3 | 128 B | 2 | ok | 15.95 | - | - | - | - | PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 128 B | 0 | ok | 8.13 | - | - | - | - | PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 128 B | 1 | ok | 8.08 | - | - | - | - | PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 128 B | 2 | ok | 8.09 | - | - | - | - | PASSED | 0 |
| pass2 | LL128 Atomic | 3 | 128 B | 0 | crashed | - | - | - | - | - | - | 1 |
| pass2 | LL128 Atomic | 3 | 128 B | 1 | crashed | - | - | - | - | - | - | 1 |
| pass2 | LL128 Atomic | 3 | 128 B | 2 | crashed | - | - | - | - | - | - | 1 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 128 B | 0 | ok | 31.56 | - | - | - | - | PASSED | 0 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 128 B | 1 | ok | 31.56 | - | - | - | - | PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 32 KB | 0 | ok | 74.40 | - | - | - | - | PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 32 KB | 1 | ok | 74.40 | - | - | - | - | PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 32 KB | 2 | ok | 74.41 | - | - | - | - | PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 32 KB | 0 | ok | 59.76 | - | - | - | - | PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 32 KB | 1 | ok | 59.74 | - | - | - | - | PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 32 KB | 2 | ok | 59.74 | - | - | - | - | PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 32 KB | 0 | ok | 67.39 | - | - | - | - | PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 32 KB | 1 | ok | 67.35 | - | - | - | - | PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 32 KB | 2 | ok | 67.37 | - | - | - | - | PASSED | 0 |
| pass2 | Two-shot LL | 3 | 32 KB | 0 | ok | 50.84 | - | - | - | - | PASSED | 0 |
| pass2 | Two-shot LL | 3 | 32 KB | 1 | ok | 50.84 | - | - | - | - | PASSED | 0 |
| pass2 | Two-shot LL | 3 | 32 KB | 2 | ok | 50.84 | - | - | - | - | PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 32 KB | 0 | ok | 49.21 | - | - | - | - | PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 32 KB | 1 | ok | 49.35 | - | - | - | - | PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 32 KB | 2 | ok | 49.06 | - | - | - | - | PASSED | 0 |
| pass2 | LL128 Atomic | 3 | 32 KB | 0 | crashed | - | - | - | - | - | - | 1 |
| pass2 | LL128 Atomic | 3 | 32 KB | 1 | crashed | - | - | - | - | - | - | 1 |
| pass2 | LL128 Atomic | 3 | 32 KB | 2 | crashed | - | - | - | - | - | - | 1 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 KB | 0 | ok | 55.55 | - | - | - | - | PASSED | 0 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 KB | 1 | ok | 55.55 | - | - | - | - | PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 256 KB | 0 | ok | 429.28 | - | - | - | - | PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 256 KB | 1 | ok | 429.28 | - | - | - | - | PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 256 KB | 2 | ok | 429.28 | - | - | - | - | PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 256 KB | 0 | ok | 378.82 | - | - | - | - | PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 256 KB | 1 | ok | 377.00 | - | - | - | - | PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 256 KB | 2 | ok | 376.90 | - | - | - | - | PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 256 KB | 0 | ok | 451.76 | - | - | - | - | PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 256 KB | 1 | crashed | - | - | - | - | - | PASSED | 1 |
| pass2 | Sentinel (one-shot) | 3 | 256 KB | 2 | ok | 467.51 | - | - | - | - | PASSED | 0 |
| pass2 | Two-shot LL | 3 | 256 KB | 0 | ok | 275.33 | - | - | - | - | PASSED | 0 |
| pass2 | Two-shot LL | 3 | 256 KB | 1 | ok | 275.51 | - | - | - | - | PASSED | 0 |
| pass2 | Two-shot LL | 3 | 256 KB | 2 | ok | 275.35 | - | - | - | - | PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 256 KB | 0 | ok | 326.25 | - | - | - | - | PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 256 KB | 1 | ok | 328.79 | - | - | - | - | PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 256 KB | 2 | ok | 328.70 | - | - | - | - | PASSED | 0 |
| pass2 | LL128 Atomic | 3 | 256 KB | 0 | crashed | - | - | - | - | - | - | 1 |
| pass2 | LL128 Atomic | 3 | 256 KB | 1 | crashed | - | - | - | - | - | - | 1 |
| pass2 | LL128 Atomic | 3 | 256 KB | 2 | crashed | - | - | - | - | - | - | 1 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 256 KB | 0 | ok | 179.19 | - | - | - | - | PASSED | 0 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 256 KB | 1 | ok | 179.21 | - | - | - | - | PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 1 MB | 0 | ok | 1,645.22 | - | - | - | - | PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 1 MB | 1 | ok | 1,639.66 | - | - | - | - | PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 1 MB | 2 | ok | 1,639.75 | - | - | - | - | PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 1 MB | 0 | ok | 1,522.06 | - | - | - | - | PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 1 MB | 1 | ok | 1,521.05 | - | - | - | - | PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 1 MB | 2 | ok | 1,522.00 | - | - | - | - | PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 1 MB | 0 | ok | 1,751.05 | - | - | - | - | PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 1 MB | 1 | crashed | - | - | - | - | - | PASSED | 1 |
| pass2 | Sentinel (one-shot) | 3 | 1 MB | 2 | ok | 1,792.59 | - | - | - | - | PASSED | 0 |
| pass2 | Two-shot LL | 3 | 1 MB | 0 | ok | 989.30 | - | - | - | - | PASSED | 0 |
| pass2 | Two-shot LL | 3 | 1 MB | 1 | ok | 990.15 | - | - | - | - | PASSED | 0 |
| pass2 | Two-shot LL | 3 | 1 MB | 2 | ok | 990.54 | - | - | - | - | PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 1 MB | 0 | crashed | - | - | - | - | - | PASSED | 1 |
| pass2 | Two-shot Sentinel | 3 | 1 MB | 1 | crashed | - | - | - | - | - | PASSED | 1 |
| pass2 | Two-shot Sentinel | 3 | 1 MB | 2 | ok | 1,110.52 | - | - | - | - | PASSED | 0 |
| pass2 | LL128 Atomic | 3 | 1 MB | 0 | crashed | - | - | - | - | - | - | 1 |
| pass2 | LL128 Atomic | 3 | 1 MB | 1 | crashed | - | - | - | - | - | - | 1 |
| pass2 | LL128 Atomic | 3 | 1 MB | 2 | crashed | - | - | - | - | - | - | 1 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 1 MB | 0 | ok | 564.99 | - | - | - | - | PASSED | 0 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 1 MB | 1 | ok | 564.96 | - | - | - | - | PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 32 MB | 0 | ok | 54,752.96 | - | - | - | - | PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 32 MB | 1 | ok | 54,683.92 | - | - | - | - | PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 32 MB | 2 | ok | 54,643.79 | - | - | - | - | PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 32 MB | 0 | ok | 46,923.91 | - | - | - | - | PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 32 MB | 1 | ok | 46,920.73 | - | - | - | - | PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 32 MB | 2 | ok | 46,873.66 | - | - | - | - | PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 32 MB | 0 | ok | 55,738.46 | - | - | - | - | PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 32 MB | 1 | ok | 55,799.96 | - | - | - | - | PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 32 MB | 2 | ok | 55,769.58 | - | - | - | - | PASSED | 0 |
| pass2 | Two-shot LL | 3 | 32 MB | 0 | ok | 31,843.15 | - | - | - | - | PASSED | 0 |
| pass2 | Two-shot LL | 3 | 32 MB | 1 | ok | 31,838.54 | - | - | - | - | PASSED | 0 |
| pass2 | Two-shot LL | 3 | 32 MB | 2 | ok | 31,811.02 | - | - | - | - | PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 32 MB | 0 | crashed | - | - | - | - | - | PASSED | 1 |
| pass2 | Two-shot Sentinel | 3 | 32 MB | 1 | crashed | - | - | - | - | - | PASSED | 1 |
| pass2 | Two-shot Sentinel | 3 | 32 MB | 2 | ok | 35,025.01 | - | - | - | - | PASSED | 0 |
| pass2 | LL128 Atomic | 3 | 32 MB | 0 | crashed | - | - | - | - | - | - | 1 |
| pass2 | LL128 Atomic | 3 | 32 MB | 1 | crashed | - | - | - | - | - | - | 1 |
| pass2 | LL128 Atomic | 3 | 32 MB | 2 | crashed | - | - | - | - | - | - | 1 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 MB | 0 | ok | 19,586.12 | - | - | - | - | PASSED | 0 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 MB | 1 | ok | 19,585.50 | - | - | - | - | PASSED | 0 |
| pass2 | Baseline (with barrier) | 2 | 32 B | 0 | ok | 1,119.96 | - | - | - | - | PASSED | 0 |
| pass2 | Baseline (with barrier) | 2 | 32 B | 1 | ok | 1,119.96 | - | - | - | - | PASSED | 0 |
| pass2 | LL (one-shot) | 2 | 32 B | 0 | hung | - | - | - | - | - | - | 124 |
| pass2 | LL (one-shot) | 2 | 32 B | 1 | hung | - | - | - | - | - | - | 124 |
| pass2 | Baseline (with barrier) | 2 | 32 KB | 0 | ok | 32.32 | - | - | - | - | PASSED | 0 |
| pass2 | Baseline (with barrier) | 2 | 32 KB | 1 | ok | 32.30 | - | - | - | - | PASSED | 0 |
| pass2 | LL (one-shot) | 2 | 32 KB | 0 | ok | 30.64 | - | - | - | - | PASSED | 0 |
| pass2 | LL (one-shot) | 2 | 32 KB | 1 | ok | 30.64 | - | - | - | - | PASSED | 0 |
| pass2 | Baseline (with barrier) | 2 | 32 MB | 0 | ok | 26,241.34 | - | - | - | - | PASSED | 0 |
| pass2 | Baseline (with barrier) | 2 | 32 MB | 1 | ok | 26,235.00 | - | - | - | - | PASSED | 0 |
| pass2 | LL (one-shot) | 2 | 32 MB | 0 | ok | 26,080.19 | - | - | - | - | PASSED | 0 |
| pass2 | LL (one-shot) | 2 | 32 MB | 1 | ok | 26,083.71 | - | - | - | - | PASSED | 0 |

Logs and what each ran:

- `pass1_1_3ranks_32B_64B_128B_32KB.txt`: 3 ranks, 32 B / 64 B / 128 B / 32 KB, 1000 rounds
- `pass1_2_3ranks_256KB.txt`: 3 ranks, 256 KB, 100 rounds
- `pass1_3_3ranks_1MB.txt`: 3 ranks, 1 MB, 100 rounds
- `pass1_4_3ranks_32MB.txt`: 3 ranks, 32 MB, 100 rounds
- `pass1_5_2ranks.txt`: 2 ranks, Baseline and LL (ring for the baselines), 32 B / 32 KB (1000 rounds) and 32 MB (100 rounds)
- `pass2_1_3ranks_32B_64B_128B_32KB.txt`: 3 ranks, 32 B / 64 B / 128 B / 32 KB, 1000 rounds
- `pass2_2_3ranks_256KB.txt`: 3 ranks, 256 KB, 100 rounds
- `pass2_3_3ranks_1MB.txt`: 3 ranks, 1 MB, 100 rounds
- `pass2_4_3ranks_32MB.txt`: 3 ranks, 32 MB, 100 rounds
- `pass2_5_2ranks.txt`: 2 ranks, Baseline and LL (ring for the baselines), 32 B / 32 KB (1000 rounds) and 32 MB (100 rounds)

