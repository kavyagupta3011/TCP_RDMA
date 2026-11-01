# Results: v3 (paper_algorithms_v3)

**v3** = `paper_algorithms_v3`: version 2 plus the changes of 2026-10-28: a correctness check in every timed round, a faster adding step and -O3 for Baseline and LL, and the negative test program.

Machines: rank 0 = compute02, rank 1 = compute03, rank 2 = compute04. PBS job 30203.master.local.

How to read: a value is the time of one AllReduce, averaged over the ranks that finished
correctly. If a rank crashed, hung (stopped by the timeout) or got a wrong result, the cell
says so. Sizes: 32 B = 8 floats, 64 B = 16, 128 B = 32, 32 KB = 8,192, 256 KB = 65,536,
32 MB = 8,388,608 floats per rank. 1000 timed rounds, 100 for 256 KB and 32 MB.

## 1. All algorithms, 3 ranks: median

| Algorithm | 32 B | 64 B | 128 B | 32 KB | 256 KB | 32 MB |
|---|---|---|---|---|---|---|
| Baseline (with barrier) | 8.80 µs | 8.76 µs | 8.93 µs | 44.38 µs | 238.8 µs | 36.42 ms |
| LL (one-shot) | 2.14 µs | 2.16 µs | 2.24 µs | 33.78 µs | 230.6 µs | 36.57 ms |
| Sentinel (one-shot) | 1.85 µs | 1.87 µs | 1.93 µs | 42.84 µs | 292.0 µs | 38.41 ms |
| Two-shot LL | 4.24 µs | 4.26 µs | 4.32 µs | 34.40 µs | 229.6 µs | 31.56 ms |
| Two-shot Sentinel | 3.51 µs | 3.59 µs | 3.71 µs | 36.62 µs | 230.8 µs | 32.82 ms |
| LL128 Atomic | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) |
| Bidirectional (2 ranks, 8 chunks per round) | 13.38 µs | 13.74 µs | 13.78 µs | 37.99 µs | 170.0 µs | 19.39 ms |

## 2. All algorithms, 3 ranks: p99 and mean

p99 (99% of the rounds were faster):

| Algorithm | 32 B | 64 B | 128 B | 32 KB | 256 KB | 32 MB |
|---|---|---|---|---|---|---|
| Baseline (with barrier) | 9.65 µs | 9.70 µs | 9.72 µs | 47.57 µs | 244.8 µs | 39.71 ms |
| LL (one-shot) | 2.87 µs | 2.40 µs | 2.95 µs | 37.81 µs | 240.2 µs | 42.05 ms |
| Sentinel (one-shot) | 2.59 µs | 2.13 µs | 2.46 µs | 47.76 µs | 316.1 µs | 43.72 ms |
| Two-shot LL | 5.82 µs | 5.28 µs | 5.78 µs | 40.17 µs | 288.9 µs | 35.16 ms |
| Two-shot Sentinel | 5.06 µs | 4.63 µs | 5.17 µs | 40.92 µs | 245.6 µs | 36.44 ms |
| LL128 Atomic | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) |
| Bidirectional (2 ranks, 8 chunks per round) | 14.70 µs | 16.40 µs | 15.04 µs | 39.56 µs | 174.3 µs | 19.86 ms |

Mean (the only statistic version 1 prints, for comparison with it):

| Algorithm | 32 B | 64 B | 128 B | 32 KB | 256 KB | 32 MB |
|---|---|---|---|---|---|---|
| Baseline (with barrier) | 8.08 µs | 8.02 µs | 8.15 µs | 44.23 µs | 238.9 µs | 36.51 ms |
| LL (one-shot) | 2.20 µs | 2.20 µs | 2.30 µs | 33.99 µs | 230.7 µs | 36.46 ms |
| Sentinel (one-shot) | 1.90 µs | 1.90 µs | 1.97 µs | 42.64 µs | 289.3 µs | 38.68 ms |
| Two-shot LL | 4.30 µs | 4.29 µs | 4.39 µs | 34.67 µs | 235.9 µs | 31.65 ms |
| Two-shot Sentinel | 3.59 µs | 3.65 µs | 3.76 µs | 36.93 µs | 231.6 µs | 32.89 ms |
| LL128 Atomic | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) |
| Bidirectional (2 ranks, 8 chunks per round) | 13.51 µs | 13.82 µs | 13.86 µs | 38.03 µs | 170.2 µs | 19.40 ms |

## 3. Cost of the barrier: Baseline vs LL, 3 ranks (median)

|  | 32 B | 64 B | 128 B | 32 KB | 256 KB | 32 MB |
|---|---|---|---|---|---|---|
| Baseline - LL | 6.66 µs | 6.60 µs | 6.69 µs | 10.60 µs | 8.19 µs | -150.19 µs |
| Baseline / LL | 4.11x | 4.05x | 3.98x | 1.31x | 1.04x | 1.00x |

## 4. 2 ranks vs 3 ranks: Baseline and LL (median)

|  | 32 B | 64 B | 128 B | 32 KB | 32 MB |
|---|---|---|---|---|---|
| Baseline (with barrier), 2 ranks | 4.38 µs | 4.38 µs | 4.43 µs | 22.69 µs | 22.92 ms |
| Baseline (with barrier), 3 ranks | 8.80 µs | 8.76 µs | 8.93 µs | 44.38 µs | 36.42 ms |
| LL (one-shot), 2 ranks | 1.76 µs | 1.75 µs | 1.80 µs | 22.68 µs | 23.70 ms |
| LL (one-shot), 3 ranks | 2.14 µs | 2.16 µs | 2.24 µs | 33.78 µs | 36.57 ms |

## 5. Atomics probe and negative test

Atomic_Probe (`logs/5_atomic_probe.txt`):

```
PROBE device atomic_cap=0 (0=NONE 1=HCA 2=GLOB) max_qp_rd_atom=16 max_qp_init_rd_atom=16
PROBE QP state=3 (3=RTS) access_flags=0xe (0x8=REMOTE_ATOMIC) max_rd_atomic=1 max_dest_rd_atomic=1 max_inline=188 max_send_sge=12
PROBE rank 0 -> peer 1, remote counter addr=120cdc0 rkey=1569536
PROBE 1 FETCH_AND_ADD, library result buf              post_send rc=22 (Invalid argument)
PROBE 2 FETCH_AND_ADD, fresh 64B-aligned result buf    post_send rc=22 (Invalid argument)
PROBE 3 CMP_AND_SWP                                    post_send rc=22 (Invalid argument)
PROBE 4 FETCH_AND_ADD after unsignaled inline WRITE    post_send rc=22 (Invalid argument)
PROBE remote counter should now equal the number of OK atomics above; reading it needs the peer (see its line)
PROBE device atomic_cap=0 (0=NONE 1=HCA 2=GLOB) max_qp_rd_atom=16 max_qp_init_rd_atom=16
PROBE QP state=3 (3=RTS) access_flags=0xe (0x8=REMOTE_ATOMIC) max_rd_atomic=1 max_dest_rd_atomic=1 max_inline=188 max_send_sge=12
PROBE rank 1 (counter host) final counter value = 0
```

LL without the wait (negative test) at 32 B (`logs/6_negative_test.txt`): 5 FAILED and 1 PASSED correctness lines. It must fail: this shows the check catches a missing wait.

LL without the wait (negative test) at 32 KB (`logs/6_negative_test.txt`): 6 FAILED and 0 PASSED correctness lines. It must fail: this shows the check catches a missing wait.

## 6. Correctness and errors

| Log | PASSED lines | FAILED lines | ranks ok | crashed | hung | skipped |
|---|---|---|---|---|---|---|
| `1_3ranks_32B_64B_128B_32KB.txt` | 128 | 0 | 68 | 0 | 0 | 12 |
| `2_3ranks_256KB.txt` | 32 | 0 | 17 | 0 | 0 | 3 |
| `3_3ranks_32MB.txt` | 32 | 0 | 17 | 0 | 0 | 3 |
| `4_2ranks_baseline_ll.txt` | 40 | 0 | 20 | 0 | 0 | 0 |
| `5_atomic_probe.txt` | 0 | 0 | 0 | 0 | 0 | 0 |
| `6_negative_test.txt` | 1 | 11 | 0 | 0 | 0 | 0 |

Error messages (first one per rank):

- LL without the wait (negative test), 32 B, rank 0: `verify: 8 / 8 elements mismatched`
- LL without the wait (negative test), 32 B, rank 1: `verify: 8 / 8 elements mismatched`
- LL without the wait (negative test), 32 KB, rank 0: `verify: 2931 / 8192 elements mismatched`
- LL without the wait (negative test), 32 KB, rank 1: `verify: 3312 / 8192 elements mismatched`
- LL without the wait (negative test), 32 KB, rank 2: `verify: 2206 / 8192 elements mismatched`

## 7. Every run, per rank

Times in µs. `-` = not printed by this version or the rank did not finish.

| Program | ranks | size | rank | status | mean | min | median | p99 | max | correctness | exit | log |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Baseline (with barrier) | 3 | 32 B | 0 | ok | 8.08 | 6.96 | 8.31 | 9.43 | 19.15 | PASSED/PASSED | 0 | 1 |
| Baseline (with barrier) | 3 | 32 B | 1 | ok | 8.08 | 6.73 | 9.09 | 9.76 | 20.55 | PASSED/PASSED | 0 | 1 |
| Baseline (with barrier) | 3 | 32 B | 2 | ok | 8.07 | 6.27 | 9.02 | 9.77 | 20.98 | PASSED/PASSED | 0 | 1 |
| LL (one-shot) | 3 | 32 B | 0 | ok | 2.20 | 2.08 | 2.14 | 2.88 | 18.45 | PASSED/PASSED | 0 | 1 |
| LL (one-shot) | 3 | 32 B | 1 | ok | 2.20 | 2.07 | 2.14 | 2.86 | 18.25 | PASSED/PASSED | 0 | 1 |
| LL (one-shot) | 3 | 32 B | 2 | ok | 2.20 | 2.08 | 2.14 | 2.87 | 18.11 | PASSED/PASSED | 0 | 1 |
| Sentinel (one-shot) | 3 | 32 B | 0 | ok | 1.90 | 1.80 | 1.84 | 2.69 | 18.94 | PASSED/PASSED | 0 | 1 |
| Sentinel (one-shot) | 3 | 32 B | 1 | ok | 1.89 | 1.79 | 1.84 | 2.20 | 12.39 | PASSED/PASSED | 0 | 1 |
| Sentinel (one-shot) | 3 | 32 B | 2 | ok | 1.90 | 1.78 | 1.85 | 2.89 | 18.67 | PASSED/PASSED | 0 | 1 |
| Two-shot LL | 3 | 32 B | 0 | ok | 4.30 | 4.11 | 4.24 | 5.61 | 16.41 | PASSED/PASSED | 0 | 1 |
| Two-shot LL | 3 | 32 B | 1 | ok | 4.30 | 4.06 | 4.24 | 6.07 | 17.07 | PASSED/PASSED | 0 | 1 |
| Two-shot LL | 3 | 32 B | 2 | ok | 4.29 | 4.12 | 4.24 | 5.79 | 17.15 | PASSED/PASSED | 0 | 1 |
| Two-shot Sentinel | 3 | 32 B | 0 | ok | 3.59 | 3.45 | 3.52 | 4.91 | 20.83 | PASSED/PASSED | 0 | 1 |
| Two-shot Sentinel | 3 | 32 B | 1 | ok | 3.58 | 3.44 | 3.51 | 5.01 | 20.80 | PASSED/PASSED | 0 | 1 |
| Two-shot Sentinel | 3 | 32 B | 2 | ok | 3.59 | 3.42 | 3.51 | 5.28 | 21.04 | PASSED/PASSED | 0 | 1 |
| LL128 Atomic | 3 | 32 B | 0 | skipped | - | - | - | - | - | - | 2 | 1 |
| LL128 Atomic | 3 | 32 B | 1 | skipped | - | - | - | - | - | - | 2 | 1 |
| LL128 Atomic | 3 | 32 B | 2 | skipped | - | - | - | - | - | - | 2 | 1 |
| Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 B | 0 | ok | 13.51 | 13.24 | 13.37 | 14.82 | 23.57 | PASSED | 0 | 1 |
| Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 B | 1 | ok | 13.51 | 13.21 | 13.38 | 14.58 | 22.89 | PASSED | 0 | 1 |
| Baseline (with barrier) | 3 | 64 B | 0 | ok | 8.02 | 6.89 | 8.24 | 9.26 | 27.82 | PASSED/PASSED | 0 | 1 |
| Baseline (with barrier) | 3 | 64 B | 1 | ok | 8.00 | 6.65 | 9.04 | 9.77 | 14.71 | PASSED/PASSED | 0 | 1 |
| Baseline (with barrier) | 3 | 64 B | 2 | ok | 8.02 | 5.54 | 9.00 | 10.07 | 28.50 | PASSED/PASSED | 0 | 1 |
| LL (one-shot) | 3 | 64 B | 0 | ok | 2.20 | 2.09 | 2.17 | 2.41 | 13.14 | PASSED/PASSED | 0 | 1 |
| LL (one-shot) | 3 | 64 B | 1 | ok | 2.21 | 2.10 | 2.16 | 2.40 | 13.36 | PASSED/PASSED | 0 | 1 |
| LL (one-shot) | 3 | 64 B | 2 | ok | 2.20 | 2.10 | 2.16 | 2.38 | 12.80 | PASSED/PASSED | 0 | 1 |
| Sentinel (one-shot) | 3 | 64 B | 0 | ok | 1.90 | 1.81 | 1.87 | 2.16 | 10.36 | PASSED/PASSED | 0 | 1 |
| Sentinel (one-shot) | 3 | 64 B | 1 | ok | 1.90 | 1.81 | 1.87 | 2.12 | 10.51 | PASSED/PASSED | 0 | 1 |
| Sentinel (one-shot) | 3 | 64 B | 2 | ok | 1.90 | 1.81 | 1.87 | 2.12 | 10.24 | PASSED/PASSED | 0 | 1 |
| Two-shot LL | 3 | 64 B | 0 | ok | 4.29 | 4.12 | 4.26 | 5.72 | 10.23 | PASSED/PASSED | 0 | 1 |
| Two-shot LL | 3 | 64 B | 1 | ok | 4.29 | 4.11 | 4.26 | 4.92 | 11.44 | PASSED/PASSED | 0 | 1 |
| Two-shot LL | 3 | 64 B | 2 | ok | 4.29 | 4.12 | 4.26 | 5.20 | 10.82 | PASSED/PASSED | 0 | 1 |
| Two-shot Sentinel | 3 | 64 B | 0 | ok | 3.65 | 3.51 | 3.59 | 4.39 | 19.05 | PASSED/PASSED | 0 | 1 |
| Two-shot Sentinel | 3 | 64 B | 1 | ok | 3.65 | 3.50 | 3.58 | 4.72 | 19.66 | PASSED/PASSED | 0 | 1 |
| Two-shot Sentinel | 3 | 64 B | 2 | ok | 3.65 | 3.49 | 3.59 | 4.78 | 18.90 | PASSED/PASSED | 0 | 1 |
| LL128 Atomic | 3 | 64 B | 0 | skipped | - | - | - | - | - | - | 2 | 1 |
| LL128 Atomic | 3 | 64 B | 1 | skipped | - | - | - | - | - | - | 2 | 1 |
| LL128 Atomic | 3 | 64 B | 2 | skipped | - | - | - | - | - | - | 2 | 1 |
| Bidirectional (2 ranks, 8 chunks per round) | 2 | 64 B | 0 | ok | 13.81 | 13.47 | 13.74 | 16.11 | 29.41 | PASSED | 0 | 1 |
| Bidirectional (2 ranks, 8 chunks per round) | 2 | 64 B | 1 | ok | 13.82 | 13.36 | 13.75 | 16.70 | 29.18 | PASSED | 0 | 1 |
| Baseline (with barrier) | 3 | 128 B | 0 | ok | 8.15 | 7.60 | 8.38 | 9.19 | 18.91 | PASSED/PASSED | 0 | 1 |
| Baseline (with barrier) | 3 | 128 B | 1 | ok | 8.15 | 6.76 | 9.23 | 9.98 | 18.13 | PASSED/PASSED | 0 | 1 |
| Baseline (with barrier) | 3 | 128 B | 2 | ok | 8.15 | 6.61 | 9.18 | 10.00 | 18.18 | PASSED/PASSED | 0 | 1 |
| LL (one-shot) | 3 | 128 B | 0 | ok | 2.31 | 2.14 | 2.25 | 2.82 | 35.49 | PASSED/PASSED | 0 | 1 |
| LL (one-shot) | 3 | 128 B | 1 | ok | 2.30 | 2.10 | 2.24 | 2.73 | 34.57 | PASSED/PASSED | 0 | 1 |
| LL (one-shot) | 3 | 128 B | 2 | ok | 2.31 | 2.14 | 2.23 | 3.30 | 32.77 | PASSED/PASSED | 0 | 1 |
| Sentinel (one-shot) | 3 | 128 B | 0 | ok | 1.97 | 1.88 | 1.93 | 2.42 | 11.81 | PASSED/PASSED | 0 | 1 |
| Sentinel (one-shot) | 3 | 128 B | 1 | ok | 1.97 | 1.86 | 1.93 | 2.53 | 11.92 | PASSED/PASSED | 0 | 1 |
| Sentinel (one-shot) | 3 | 128 B | 2 | ok | 1.96 | 1.86 | 1.93 | 2.44 | 11.77 | PASSED/PASSED | 0 | 1 |
| Two-shot LL | 3 | 128 B | 0 | ok | 4.39 | 4.24 | 4.33 | 5.56 | 14.06 | PASSED/PASSED | 0 | 1 |
| Two-shot LL | 3 | 128 B | 1 | ok | 4.39 | 4.19 | 4.31 | 5.97 | 15.15 | PASSED/PASSED | 0 | 1 |
| Two-shot LL | 3 | 128 B | 2 | ok | 4.39 | 4.22 | 4.32 | 5.82 | 14.63 | PASSED/PASSED | 0 | 1 |
| Two-shot Sentinel | 3 | 128 B | 0 | ok | 3.76 | 3.63 | 3.72 | 4.88 | 13.52 | PASSED/PASSED | 0 | 1 |
| Two-shot Sentinel | 3 | 128 B | 1 | ok | 3.76 | 3.61 | 3.71 | 5.29 | 14.35 | PASSED/PASSED | 0 | 1 |
| Two-shot Sentinel | 3 | 128 B | 2 | ok | 3.76 | 3.61 | 3.71 | 5.35 | 13.87 | PASSED/PASSED | 0 | 1 |
| LL128 Atomic | 3 | 128 B | 0 | skipped | - | - | - | - | - | - | 2 | 1 |
| LL128 Atomic | 3 | 128 B | 1 | skipped | - | - | - | - | - | - | 2 | 1 |
| LL128 Atomic | 3 | 128 B | 2 | skipped | - | - | - | - | - | - | 2 | 1 |
| Bidirectional (2 ranks, 8 chunks per round) | 2 | 128 B | 0 | ok | 13.86 | 13.56 | 13.78 | 14.88 | 24.33 | PASSED | 0 | 1 |
| Bidirectional (2 ranks, 8 chunks per round) | 2 | 128 B | 1 | ok | 13.86 | 13.55 | 13.78 | 15.19 | 24.06 | PASSED | 0 | 1 |
| Baseline (with barrier) | 3 | 32 KB | 0 | ok | 44.23 | 38.99 | 44.33 | 46.91 | 55.94 | PASSED/PASSED | 0 | 1 |
| Baseline (with barrier) | 3 | 32 KB | 1 | ok | 44.23 | 38.61 | 44.30 | 47.80 | 56.87 | PASSED/PASSED | 0 | 1 |
| Baseline (with barrier) | 3 | 32 KB | 2 | ok | 44.23 | 37.62 | 44.51 | 47.99 | 59.36 | PASSED/PASSED | 0 | 1 |
| LL (one-shot) | 3 | 32 KB | 0 | ok | 33.99 | 27.29 | 33.77 | 37.48 | 52.14 | PASSED/PASSED | 0 | 1 |
| LL (one-shot) | 3 | 32 KB | 1 | ok | 34.00 | 25.46 | 33.81 | 37.90 | 50.73 | PASSED/PASSED | 0 | 1 |
| LL (one-shot) | 3 | 32 KB | 2 | ok | 33.99 | 21.30 | 33.76 | 38.07 | 50.32 | PASSED/PASSED | 0 | 1 |
| Sentinel (one-shot) | 3 | 32 KB | 0 | ok | 42.64 | 35.45 | 43.26 | 47.66 | 59.26 | PASSED/PASSED | 0 | 1 |
| Sentinel (one-shot) | 3 | 32 KB | 1 | ok | 42.63 | 34.35 | 42.82 | 48.11 | 56.30 | PASSED/PASSED | 0 | 1 |
| Sentinel (one-shot) | 3 | 32 KB | 2 | ok | 42.65 | 34.27 | 42.43 | 47.50 | 59.02 | PASSED/PASSED | 0 | 1 |
| Two-shot LL | 3 | 32 KB | 0 | ok | 34.67 | 32.21 | 34.58 | 40.34 | 48.60 | PASSED/PASSED | 0 | 1 |
| Two-shot LL | 3 | 32 KB | 1 | ok | 34.67 | 31.60 | 34.25 | 40.09 | 50.11 | PASSED/PASSED | 0 | 1 |
| Two-shot LL | 3 | 32 KB | 2 | ok | 34.67 | 32.38 | 34.36 | 40.07 | 50.88 | PASSED/PASSED | 0 | 1 |
| Two-shot Sentinel | 3 | 32 KB | 0 | ok | 36.92 | 34.66 | 36.41 | 41.19 | 49.72 | PASSED/PASSED | 0 | 1 |
| Two-shot Sentinel | 3 | 32 KB | 1 | ok | 36.93 | 33.33 | 36.76 | 40.78 | 55.89 | PASSED/PASSED | 0 | 1 |
| Two-shot Sentinel | 3 | 32 KB | 2 | ok | 36.93 | 35.20 | 36.70 | 40.79 | 54.02 | PASSED/PASSED | 0 | 1 |
| LL128 Atomic | 3 | 32 KB | 0 | skipped | - | - | - | - | - | - | 2 | 1 |
| LL128 Atomic | 3 | 32 KB | 1 | skipped | - | - | - | - | - | - | 2 | 1 |
| LL128 Atomic | 3 | 32 KB | 2 | skipped | - | - | - | - | - | - | 2 | 1 |
| Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 KB | 0 | ok | 38.03 | 37.69 | 37.99 | 39.51 | 43.88 | PASSED | 0 | 1 |
| Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 KB | 1 | ok | 38.03 | 37.58 | 37.98 | 39.61 | 43.84 | PASSED | 0 | 1 |
| Baseline (with barrier) | 3 | 256 KB | 0 | ok | 238.69 | 234.54 | 238.64 | 243.88 | 244.61 | PASSED/PASSED | 0 | 2 |
| Baseline (with barrier) | 3 | 256 KB | 1 | ok | 238.90 | 234.60 | 238.71 | 247.58 | 251.08 | PASSED/PASSED | 0 | 2 |
| Baseline (with barrier) | 3 | 256 KB | 2 | ok | 239.04 | 234.12 | 238.99 | 242.95 | 271.67 | PASSED/PASSED | 0 | 2 |
| LL (one-shot) | 3 | 256 KB | 0 | ok | 230.48 | 202.65 | 230.86 | 238.73 | 254.94 | PASSED/PASSED | 0 | 2 |
| LL (one-shot) | 3 | 256 KB | 1 | ok | 230.97 | 223.26 | 230.74 | 240.83 | 270.60 | PASSED/PASSED | 0 | 2 |
| LL (one-shot) | 3 | 256 KB | 2 | ok | 230.78 | 219.11 | 230.19 | 241.03 | 254.23 | PASSED/PASSED | 0 | 2 |
| Sentinel (one-shot) | 3 | 256 KB | 0 | ok | 289.32 | 273.33 | 289.07 | 310.77 | 316.54 | PASSED/PASSED | 0 | 2 |
| Sentinel (one-shot) | 3 | 256 KB | 1 | ok | 289.36 | 273.42 | 290.09 | 315.25 | 326.75 | PASSED/PASSED | 0 | 2 |
| Sentinel (one-shot) | 3 | 256 KB | 2 | ok | 289.33 | 272.70 | 296.86 | 322.24 | 323.26 | PASSED/PASSED | 0 | 2 |
| Two-shot LL | 3 | 256 KB | 0 | ok | 235.92 | 224.95 | 229.40 | 291.22 | 300.68 | PASSED/PASSED | 0 | 2 |
| Two-shot LL | 3 | 256 KB | 1 | ok | 235.79 | 218.90 | 230.10 | 285.86 | 292.13 | PASSED/PASSED | 0 | 2 |
| Two-shot LL | 3 | 256 KB | 2 | ok | 236.00 | 225.13 | 229.36 | 289.55 | 296.30 | PASSED/PASSED | 0 | 2 |
| Two-shot Sentinel | 3 | 256 KB | 0 | ok | 231.60 | 227.43 | 230.56 | 240.15 | 245.78 | PASSED/PASSED | 0 | 2 |
| Two-shot Sentinel | 3 | 256 KB | 1 | ok | 231.30 | 221.29 | 231.32 | 242.65 | 242.68 | PASSED/PASSED | 0 | 2 |
| Two-shot Sentinel | 3 | 256 KB | 2 | ok | 231.81 | 222.54 | 230.59 | 254.05 | 277.02 | PASSED/PASSED | 0 | 2 |
| LL128 Atomic | 3 | 256 KB | 0 | skipped | - | - | - | - | - | - | 2 | 2 |
| LL128 Atomic | 3 | 256 KB | 1 | skipped | - | - | - | - | - | - | 2 | 2 |
| LL128 Atomic | 3 | 256 KB | 2 | skipped | - | - | - | - | - | - | 2 | 2 |
| Bidirectional (2 ranks, 8 chunks per round) | 2 | 256 KB | 0 | ok | 170.16 | 163.31 | 170.02 | 174.18 | 174.40 | PASSED | 0 | 2 |
| Bidirectional (2 ranks, 8 chunks per round) | 2 | 256 KB | 1 | ok | 170.23 | 169.01 | 170.07 | 174.37 | 174.43 | PASSED | 0 | 2 |
| Baseline (with barrier) | 3 | 32 MB | 0 | ok | 36,518.86 | 36,065.59 | 36,448.92 | 38,056.53 | 38,610.81 | PASSED/PASSED | 0 | 3 |
| Baseline (with barrier) | 3 | 32 MB | 1 | ok | 36,521.88 | 35,831.10 | 36,425.16 | 38,295.94 | 38,314.40 | PASSED/PASSED | 0 | 3 |
| Baseline (with barrier) | 3 | 32 MB | 2 | ok | 36,482.32 | 32,094.80 | 36,397.31 | 42,771.78 | 43,113.99 | PASSED/PASSED | 0 | 3 |
| LL (one-shot) | 3 | 32 MB | 0 | ok | 36,469.43 | 31,981.41 | 36,591.94 | 42,284.37 | 42,518.22 | PASSED/PASSED | 0 | 3 |
| LL (one-shot) | 3 | 32 MB | 1 | ok | 36,471.08 | 31,577.12 | 36,712.95 | 41,596.19 | 41,771.58 | PASSED/PASSED | 0 | 3 |
| LL (one-shot) | 3 | 32 MB | 2 | ok | 36,429.22 | 31,448.22 | 36,417.08 | 42,261.83 | 42,433.38 | PASSED/PASSED | 0 | 3 |
| Sentinel (one-shot) | 3 | 32 MB | 0 | ok | 38,703.76 | 38,036.38 | 38,379.02 | 43,248.88 | 43,471.18 | PASSED/PASSED | 0 | 3 |
| Sentinel (one-shot) | 3 | 32 MB | 1 | ok | 38,698.90 | 37,879.87 | 38,415.04 | 43,294.77 | 43,456.08 | PASSED/PASSED | 0 | 3 |
| Sentinel (one-shot) | 3 | 32 MB | 2 | ok | 38,652.05 | 36,620.31 | 38,422.28 | 44,629.10 | 44,861.82 | PASSED/PASSED | 0 | 3 |
| Two-shot LL | 3 | 32 MB | 0 | ok | 31,675.37 | 31,250.27 | 31,526.35 | 34,181.25 | 36,762.47 | PASSED/PASSED | 0 | 3 |
| Two-shot LL | 3 | 32 MB | 1 | ok | 31,655.40 | 30,941.70 | 31,574.73 | 34,969.95 | 38,234.64 | PASSED/PASSED | 0 | 3 |
| Two-shot LL | 3 | 32 MB | 2 | ok | 31,629.88 | 30,104.35 | 31,590.06 | 36,318.09 | 37,355.95 | PASSED/PASSED | 0 | 3 |
| Two-shot Sentinel | 3 | 32 MB | 0 | ok | 32,886.60 | 31,906.32 | 32,828.22 | 35,890.29 | 38,149.19 | PASSED/PASSED | 0 | 3 |
| Two-shot Sentinel | 3 | 32 MB | 1 | ok | 32,890.65 | 32,011.61 | 32,809.76 | 36,363.09 | 36,971.59 | PASSED/PASSED | 0 | 3 |
| Two-shot Sentinel | 3 | 32 MB | 2 | ok | 32,878.33 | 31,872.60 | 32,821.34 | 37,054.51 | 37,742.28 | PASSED/PASSED | 0 | 3 |
| LL128 Atomic | 3 | 32 MB | 0 | skipped | - | - | - | - | - | - | 2 | 3 |
| LL128 Atomic | 3 | 32 MB | 1 | skipped | - | - | - | - | - | - | 2 | 3 |
| LL128 Atomic | 3 | 32 MB | 2 | skipped | - | - | - | - | - | - | 2 | 3 |
| Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 MB | 0 | ok | 19,400.51 | 19,090.72 | 19,393.57 | 20,037.71 | 20,107.56 | PASSED | 0 | 3 |
| Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 MB | 1 | ok | 19,397.79 | 19,113.00 | 19,395.18 | 19,691.58 | 20,384.77 | PASSED | 0 | 3 |
| Baseline (with barrier) | 2 | 32 B | 0 | ok | 4.08 | 3.60 | 4.38 | 4.55 | 15.26 | PASSED/PASSED | 0 | 4 |
| Baseline (with barrier) | 2 | 32 B | 1 | ok | 4.08 | 2.31 | 4.37 | 4.52 | 15.22 | PASSED/PASSED | 0 | 4 |
| LL (one-shot) | 2 | 32 B | 0 | ok | 1.81 | 1.68 | 1.76 | 2.06 | 16.85 | PASSED/PASSED | 0 | 4 |
| LL (one-shot) | 2 | 32 B | 1 | ok | 1.80 | 1.67 | 1.76 | 2.02 | 16.78 | PASSED/PASSED | 0 | 4 |
| Baseline (with barrier) | 2 | 64 B | 0 | ok | 4.18 | 2.75 | 4.37 | 4.56 | 19.46 | PASSED/PASSED | 0 | 4 |
| Baseline (with barrier) | 2 | 64 B | 1 | ok | 4.17 | 2.69 | 4.38 | 4.55 | 13.88 | PASSED/PASSED | 0 | 4 |
| LL (one-shot) | 2 | 64 B | 0 | ok | 1.76 | 1.68 | 1.75 | 1.82 | 6.37 | PASSED/PASSED | 0 | 4 |
| LL (one-shot) | 2 | 64 B | 1 | ok | 1.76 | 1.68 | 1.75 | 1.82 | 7.17 | PASSED/PASSED | 0 | 4 |
| Baseline (with barrier) | 2 | 128 B | 0 | ok | 4.21 | 2.35 | 4.44 | 4.92 | 20.02 | PASSED/PASSED | 0 | 4 |
| Baseline (with barrier) | 2 | 128 B | 1 | ok | 4.21 | 2.69 | 4.43 | 4.91 | 20.60 | PASSED/PASSED | 0 | 4 |
| LL (one-shot) | 2 | 128 B | 0 | ok | 1.83 | 1.69 | 1.79 | 1.94 | 11.97 | PASSED/PASSED | 0 | 4 |
| LL (one-shot) | 2 | 128 B | 1 | ok | 1.83 | 1.70 | 1.82 | 1.88 | 11.94 | PASSED/PASSED | 0 | 4 |
| Baseline (with barrier) | 2 | 32 KB | 0 | ok | 22.58 | 19.95 | 22.55 | 25.27 | 34.26 | PASSED/PASSED | 0 | 4 |
| Baseline (with barrier) | 2 | 32 KB | 1 | ok | 22.59 | 21.97 | 22.83 | 24.28 | 32.34 | PASSED/PASSED | 0 | 4 |
| LL (one-shot) | 2 | 32 KB | 0 | ok | 19.48 | 12.61 | 22.62 | 25.94 | 30.49 | PASSED/PASSED | 0 | 4 |
| LL (one-shot) | 2 | 32 KB | 1 | ok | 19.51 | 12.90 | 22.74 | 25.74 | 40.93 | PASSED/PASSED | 0 | 4 |
| Baseline (with barrier) | 2 | 32 MB | 0 | ok | 22,943.16 | 22,501.07 | 22,953.88 | 23,473.50 | 23,914.11 | PASSED/PASSED | 0 | 4 |
| Baseline (with barrier) | 2 | 32 MB | 1 | ok | 22,940.09 | 22,481.84 | 22,889.42 | 23,665.93 | 23,850.13 | PASSED/PASSED | 0 | 4 |
| LL (one-shot) | 2 | 32 MB | 0 | ok | 22,998.53 | 13,485.47 | 23,718.98 | 32,621.58 | 34,207.70 | PASSED/PASSED | 0 | 4 |
| LL (one-shot) | 2 | 32 MB | 1 | ok | 22,912.42 | 13,534.95 | 23,686.66 | 32,570.91 | 32,652.40 | PASSED/PASSED | 0 | 4 |
| Atomic_Probe | 2 | 32 B | 0 | no result | - | - | - | - | - | - | 0 | 5 |
| Atomic_Probe | 2 | 32 B | 1 | no result | - | - | - | - | - | - | 0 | 5 |
| LL without the wait (negative test) | 3 | 32 B | 0 | wrong result | 2.22 | 2.15 | 2.21 | 2.41 | 4.04 | FAILED/FAILED | 1 | 6 |
| LL without the wait (negative test) | 3 | 32 B | 1 | wrong result | 2.22 | 2.14 | 2.20 | 2.37 | 12.49 | FAILED/FAILED | 1 | 6 |
| LL without the wait (negative test) | 3 | 32 B | 2 | wrong result | 2.23 | 2.15 | 2.21 | 2.37 | 6.56 | PASSED/FAILED | 0 | 6 |
| LL without the wait (negative test) | 3 | 32 KB | 0 | wrong result | 24.77 | 23.53 | 24.28 | 29.54 | 36.23 | FAILED/FAILED | 1 | 6 |
| LL without the wait (negative test) | 3 | 32 KB | 1 | wrong result | 27.43 | 23.33 | 27.20 | 36.67 | 38.74 | FAILED/FAILED | 1 | 6 |
| LL without the wait (negative test) | 3 | 32 KB | 2 | wrong result | 24.91 | 23.31 | 24.52 | 29.88 | 42.07 | FAILED/FAILED | 1 | 6 |

`log` = the number at the start of the file name in `logs/`:

- `1_3ranks_32B_64B_128B_32KB.txt`: 3 ranks, all algorithms, 32 B / 64 B / 128 B / 32 KB, 1000 rounds
- `2_3ranks_256KB.txt`: 3 ranks, all algorithms, 256 KB, 100 rounds
- `3_3ranks_32MB.txt`: 3 ranks, all algorithms, 32 MB, 100 rounds
- `4_2ranks_baseline_ll.txt`: 2 ranks, Baseline and LL, 32 B / 64 B / 128 B / 32 KB (1000 rounds) and 32 MB (100 rounds)
- `5_atomic_probe.txt`: Atomic_Probe: does the network card support RDMA atomics (needed by LL128)
- `6_negative_test.txt`: negative test: LL with the wait removed, must print FAILED

