# Results: v2 (paper_algorithms_v2)

**v2** = `paper_algorithms_v2`: version 1 plus the fixes of 2026-10-05 (git commit 421f67d): writes posted back to back and small ones sent inline, two receive buffers used in turn, Sentinel re-armed, `<` flag waits, barrier receive fix, LL128 skips cleanly; prints min / median / p99 / max.

Machines: rank 0 = compute02, rank 1 = compute03, rank 2 = compute04. PBS job 30203.master.local.

How to read: a value is the time of one AllReduce, averaged over the ranks that finished
correctly. If a rank crashed, hung (stopped by the timeout) or got a wrong result, the cell
says so. Sizes: 32 B = 8 floats, 64 B = 16, 128 B = 32, 32 KB = 8,192, 256 KB = 65,536,
32 MB = 8,388,608 floats per rank. 1000 timed rounds, 100 for 256 KB and 32 MB.

## 1. All algorithms, 3 ranks: median

| Algorithm | 32 B | 64 B | 128 B | 32 KB | 256 KB | 32 MB |
|---|---|---|---|---|---|---|
| Baseline (with barrier) | 8.79 µs | 8.80 µs | 8.96 µs | 65.38 µs | 435.1 µs | 46.78 ms |
| LL (one-shot) | 2.16 µs | 2.17 µs | 2.23 µs | 45.04 µs | 323.6 µs | 43.85 ms |
| Sentinel (one-shot) | 1.87 µs | 1.87 µs | 1.92 µs | 42.49 µs | 293.6 µs | 38.41 ms |
| Two-shot LL | 4.27 µs | 4.24 µs | 4.28 µs | 34.40 µs | 229.6 µs | 31.63 ms |
| Two-shot Sentinel | 3.52 µs | 3.54 µs | 3.69 µs | 45.22 µs | 285.0 µs | 38.13 ms |
| LL128 Atomic | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) |
| Bidirectional (2 ranks, 8 chunks per round) | 13.67 µs | 13.87 µs | 13.79 µs | 37.97 µs | 172.1 µs | 19.44 ms |

## 2. All algorithms, 3 ranks: p99 and mean

p99 (99% of the rounds were faster):

| Algorithm | 32 B | 64 B | 128 B | 32 KB | 256 KB | 32 MB |
|---|---|---|---|---|---|---|
| Baseline (with barrier) | 9.43 µs | 9.97 µs | 9.75 µs | 80.45 µs | 554.6 µs | 52.42 ms |
| LL (one-shot) | 2.36 µs | 2.38 µs | 2.79 µs | 53.06 µs | 342.7 µs | 47.86 ms |
| Sentinel (one-shot) | 2.03 µs | 2.52 µs | 2.16 µs | 48.87 µs | 309.6 µs | 43.65 ms |
| Two-shot LL | 5.62 µs | 5.82 µs | 5.71 µs | 40.18 µs | 289.5 µs | 34.16 ms |
| Two-shot Sentinel | 4.87 µs | 4.33 µs | 4.96 µs | 48.77 µs | 297.1 µs | 41.29 ms |
| LL128 Atomic | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) |
| Bidirectional (2 ranks, 8 chunks per round) | 15.55 µs | 15.05 µs | 15.44 µs | 39.82 µs | 176.5 µs | 19.77 ms |

Mean (the only statistic version 1 prints, for comparison with it):

| Algorithm | 32 B | 64 B | 128 B | 32 KB | 256 KB | 32 MB |
|---|---|---|---|---|---|---|
| Baseline (with barrier) | 8.03 µs | 8.07 µs | 8.20 µs | 63.25 µs | 405.4 µs | 46.96 ms |
| LL (one-shot) | 2.20 µs | 2.21 µs | 2.29 µs | 45.27 µs | 323.7 µs | 43.91 ms |
| Sentinel (one-shot) | 1.90 µs | 1.92 µs | 1.95 µs | 42.47 µs | 291.1 µs | 38.71 ms |
| Two-shot LL | 4.33 µs | 4.30 µs | 4.34 µs | 35.11 µs | 235.0 µs | 31.76 ms |
| Two-shot Sentinel | 3.58 µs | 3.58 µs | 3.73 µs | 45.18 µs | 285.6 µs | 38.24 ms |
| LL128 Atomic | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) |
| Bidirectional (2 ranks, 8 chunks per round) | 13.77 µs | 13.94 µs | 13.87 µs | 38.04 µs | 172.3 µs | 19.44 ms |

## 3. Cost of the barrier: Baseline vs LL, 3 ranks (median)

|  | 32 B | 64 B | 128 B | 32 KB | 256 KB | 32 MB |
|---|---|---|---|---|---|---|
| Baseline - LL | 6.62 µs | 6.63 µs | 6.73 µs | 20.33 µs | 111.5 µs | 2931.9 µs |
| Baseline / LL | 4.06x | 4.06x | 4.01x | 1.45x | 1.34x | 1.07x |

## 4. 2 ranks vs 3 ranks: Baseline and LL (median)

|  | 32 B | 64 B | 128 B | 32 KB | 32 MB |
|---|---|---|---|---|---|
| Baseline (with barrier), 2 ranks | 4.44 µs | 4.44 µs | 4.53 µs | 34.84 µs | 33.17 ms |
| Baseline (with barrier), 3 ranks | 8.79 µs | 8.80 µs | 8.96 µs | 65.38 µs | 46.78 ms |
| LL (one-shot), 2 ranks | 1.81 µs | 1.78 µs | 1.79 µs | 32.86 µs | 29.19 ms |
| LL (one-shot), 3 ranks | 2.16 µs | 2.17 µs | 2.23 µs | 45.04 µs | 43.85 ms |

## 5. Atomics probe and negative test

Atomic_Probe (`logs/5_atomic_probe.txt`):

```
PROBE device atomic_cap=0 (0=NONE 1=HCA 2=GLOB) max_qp_rd_atom=16 max_qp_init_rd_atom=16
PROBE QP state=3 (3=RTS) access_flags=0xe (0x8=REMOTE_ATOMIC) max_rd_atomic=1 max_dest_rd_atomic=1 max_inline=188 max_send_sge=12
PROBE rank 0 -> peer 1, remote counter addr=141fdc0 rkey=855117
PROBE 1 FETCH_AND_ADD, library result buf              post_send rc=22 (Invalid argument)
PROBE 2 FETCH_AND_ADD, fresh 64B-aligned result buf    post_send rc=22 (Invalid argument)
PROBE 3 CMP_AND_SWP                                    post_send rc=22 (Invalid argument)
PROBE 4 FETCH_AND_ADD after unsignaled inline WRITE    post_send rc=22 (Invalid argument)
PROBE remote counter should now equal the number of OK atomics above; reading it needs the peer (see its line)
PROBE device atomic_cap=0 (0=NONE 1=HCA 2=GLOB) max_qp_rd_atom=16 max_qp_init_rd_atom=16
PROBE QP state=3 (3=RTS) access_flags=0xe (0x8=REMOTE_ATOMIC) max_rd_atomic=1 max_dest_rd_atomic=1 max_inline=188 max_send_sge=12
PROBE rank 1 (counter host) final counter value = 0
```

## 6. Correctness and errors

| Log | PASSED lines | FAILED lines | ranks ok | crashed | hung | skipped |
|---|---|---|---|---|---|---|
| `1_3ranks_32B_64B_128B_32KB.txt` | 68 | 0 | 68 | 0 | 0 | 12 |
| `2_3ranks_256KB.txt` | 17 | 0 | 17 | 0 | 0 | 3 |
| `3_3ranks_32MB.txt` | 17 | 0 | 17 | 0 | 0 | 3 |
| `4_2ranks_baseline_ll.txt` | 20 | 0 | 20 | 0 | 0 | 0 |
| `5_atomic_probe.txt` | 0 | 0 | 0 | 0 | 0 | 0 |

## 7. Every run, per rank

Times in µs. `-` = not printed by this version or the rank did not finish.

| Program | ranks | size | rank | status | mean | min | median | p99 | max | correctness | exit | log |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Baseline (with barrier) | 3 | 32 B | 0 | ok | 8.03 | 5.95 | 8.27 | 9.26 | 13.24 | PASSED | 0 | 1 |
| Baseline (with barrier) | 3 | 32 B | 1 | ok | 8.03 | 6.68 | 9.06 | 9.52 | 13.15 | PASSED | 0 | 1 |
| Baseline (with barrier) | 3 | 32 B | 2 | ok | 8.03 | 5.30 | 9.02 | 9.50 | 13.41 | PASSED | 0 | 1 |
| LL (one-shot) | 3 | 32 B | 0 | ok | 2.20 | 2.10 | 2.17 | 2.38 | 11.13 | PASSED | 0 | 1 |
| LL (one-shot) | 3 | 32 B | 1 | ok | 2.20 | 2.08 | 2.16 | 2.36 | 11.43 | PASSED | 0 | 1 |
| LL (one-shot) | 3 | 32 B | 2 | ok | 2.20 | 2.10 | 2.16 | 2.33 | 10.80 | PASSED | 0 | 1 |
| Sentinel (one-shot) | 3 | 32 B | 0 | ok | 1.90 | 1.82 | 1.87 | 2.01 | 11.66 | PASSED | 0 | 1 |
| Sentinel (one-shot) | 3 | 32 B | 1 | ok | 1.90 | 1.81 | 1.87 | 2.08 | 11.84 | PASSED | 0 | 1 |
| Sentinel (one-shot) | 3 | 32 B | 2 | ok | 1.90 | 1.81 | 1.87 | 2.00 | 11.83 | PASSED | 0 | 1 |
| Two-shot LL | 3 | 32 B | 0 | ok | 4.33 | 4.18 | 4.29 | 5.40 | 14.43 | PASSED | 0 | 1 |
| Two-shot LL | 3 | 32 B | 1 | ok | 4.33 | 4.08 | 4.26 | 5.75 | 14.95 | PASSED | 0 | 1 |
| Two-shot LL | 3 | 32 B | 2 | ok | 4.33 | 4.16 | 4.26 | 5.71 | 14.21 | PASSED | 0 | 1 |
| Two-shot Sentinel | 3 | 32 B | 0 | ok | 3.58 | 3.44 | 3.52 | 4.99 | 13.62 | PASSED | 0 | 1 |
| Two-shot Sentinel | 3 | 32 B | 1 | ok | 3.58 | 3.42 | 3.51 | 5.00 | 14.15 | PASSED | 0 | 1 |
| Two-shot Sentinel | 3 | 32 B | 2 | ok | 3.58 | 3.40 | 3.52 | 4.61 | 13.49 | PASSED | 0 | 1 |
| LL128 Atomic | 3 | 32 B | 0 | skipped | - | - | - | - | - | - | 2 | 1 |
| LL128 Atomic | 3 | 32 B | 1 | skipped | - | - | - | - | - | - | 2 | 1 |
| LL128 Atomic | 3 | 32 B | 2 | skipped | - | - | - | - | - | - | 2 | 1 |
| Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 B | 0 | ok | 13.77 | 13.39 | 13.66 | 15.41 | 31.73 | PASSED | 0 | 1 |
| Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 B | 1 | ok | 13.77 | 13.36 | 13.69 | 15.69 | 32.50 | PASSED | 0 | 1 |
| Baseline (with barrier) | 3 | 64 B | 0 | ok | 8.07 | 6.08 | 8.25 | 9.81 | 23.83 | PASSED | 0 | 1 |
| Baseline (with barrier) | 3 | 64 B | 1 | ok | 8.07 | 6.70 | 9.08 | 9.86 | 25.10 | PASSED | 0 | 1 |
| Baseline (with barrier) | 3 | 64 B | 2 | ok | 8.06 | 6.08 | 9.06 | 10.23 | 25.35 | PASSED | 0 | 1 |
| LL (one-shot) | 3 | 64 B | 0 | ok | 2.21 | 2.09 | 2.17 | 2.41 | 11.71 | PASSED | 0 | 1 |
| LL (one-shot) | 3 | 64 B | 1 | ok | 2.21 | 2.09 | 2.16 | 2.39 | 11.86 | PASSED | 0 | 1 |
| LL (one-shot) | 3 | 64 B | 2 | ok | 2.20 | 2.09 | 2.17 | 2.34 | 11.54 | PASSED | 0 | 1 |
| Sentinel (one-shot) | 3 | 64 B | 0 | ok | 1.92 | 1.81 | 1.87 | 2.59 | 17.13 | PASSED | 0 | 1 |
| Sentinel (one-shot) | 3 | 64 B | 1 | ok | 1.92 | 1.80 | 1.87 | 2.27 | 17.07 | PASSED | 0 | 1 |
| Sentinel (one-shot) | 3 | 64 B | 2 | ok | 1.92 | 1.81 | 1.87 | 2.72 | 16.95 | PASSED | 0 | 1 |
| Two-shot LL | 3 | 64 B | 0 | ok | 4.30 | 4.14 | 4.24 | 5.74 | 14.18 | PASSED | 0 | 1 |
| Two-shot LL | 3 | 64 B | 1 | ok | 4.30 | 4.12 | 4.25 | 5.79 | 15.27 | PASSED | 0 | 1 |
| Two-shot LL | 3 | 64 B | 2 | ok | 4.30 | 4.14 | 4.24 | 5.93 | 14.62 | PASSED | 0 | 1 |
| Two-shot Sentinel | 3 | 64 B | 0 | ok | 3.58 | 3.46 | 3.54 | 4.43 | 8.17 | PASSED | 0 | 1 |
| Two-shot Sentinel | 3 | 64 B | 1 | ok | 3.58 | 3.46 | 3.54 | 4.67 | 8.43 | PASSED | 0 | 1 |
| Two-shot Sentinel | 3 | 64 B | 2 | ok | 3.58 | 3.48 | 3.54 | 3.90 | 8.43 | PASSED | 0 | 1 |
| LL128 Atomic | 3 | 64 B | 0 | skipped | - | - | - | - | - | - | 2 | 1 |
| LL128 Atomic | 3 | 64 B | 1 | skipped | - | - | - | - | - | - | 2 | 1 |
| LL128 Atomic | 3 | 64 B | 2 | skipped | - | - | - | - | - | - | 2 | 1 |
| Bidirectional (2 ranks, 8 chunks per round) | 2 | 64 B | 0 | ok | 13.94 | 13.58 | 13.87 | 14.98 | 23.16 | PASSED | 0 | 1 |
| Bidirectional (2 ranks, 8 chunks per round) | 2 | 64 B | 1 | ok | 13.94 | 13.58 | 13.86 | 15.11 | 23.11 | PASSED | 0 | 1 |
| Baseline (with barrier) | 3 | 128 B | 0 | ok | 8.20 | 6.41 | 8.40 | 9.22 | 21.30 | PASSED | 0 | 1 |
| Baseline (with barrier) | 3 | 128 B | 1 | ok | 8.20 | 6.84 | 9.25 | 10.01 | 23.02 | PASSED | 0 | 1 |
| Baseline (with barrier) | 3 | 128 B | 2 | ok | 8.19 | 6.73 | 9.22 | 10.02 | 23.25 | PASSED | 0 | 1 |
| LL (one-shot) | 3 | 128 B | 0 | ok | 2.29 | 2.13 | 2.23 | 2.94 | 18.70 | PASSED | 0 | 1 |
| LL (one-shot) | 3 | 128 B | 1 | ok | 2.29 | 2.13 | 2.23 | 2.74 | 18.33 | PASSED | 0 | 1 |
| LL (one-shot) | 3 | 128 B | 2 | ok | 2.29 | 2.14 | 2.23 | 2.68 | 19.16 | PASSED | 0 | 1 |
| Sentinel (one-shot) | 3 | 128 B | 0 | ok | 1.95 | 1.85 | 1.92 | 2.15 | 9.14 | PASSED | 0 | 1 |
| Sentinel (one-shot) | 3 | 128 B | 1 | ok | 1.95 | 1.86 | 1.92 | 2.20 | 9.31 | PASSED | 0 | 1 |
| Sentinel (one-shot) | 3 | 128 B | 2 | ok | 1.95 | 1.85 | 1.92 | 2.13 | 9.31 | PASSED | 0 | 1 |
| Two-shot LL | 3 | 128 B | 0 | ok | 4.34 | 4.20 | 4.29 | 5.64 | 12.81 | PASSED | 0 | 1 |
| Two-shot LL | 3 | 128 B | 1 | ok | 4.33 | 4.17 | 4.28 | 5.83 | 13.31 | PASSED | 0 | 1 |
| Two-shot LL | 3 | 128 B | 2 | ok | 4.34 | 4.16 | 4.29 | 5.66 | 12.68 | PASSED | 0 | 1 |
| Two-shot Sentinel | 3 | 128 B | 0 | ok | 3.73 | 3.58 | 3.69 | 4.94 | 8.61 | PASSED | 0 | 1 |
| Two-shot Sentinel | 3 | 128 B | 1 | ok | 3.73 | 3.60 | 3.69 | 5.01 | 9.39 | PASSED | 0 | 1 |
| Two-shot Sentinel | 3 | 128 B | 2 | ok | 3.73 | 3.58 | 3.69 | 4.92 | 9.11 | PASSED | 0 | 1 |
| LL128 Atomic | 3 | 128 B | 0 | skipped | - | - | - | - | - | - | 2 | 1 |
| LL128 Atomic | 3 | 128 B | 1 | skipped | - | - | - | - | - | - | 2 | 1 |
| LL128 Atomic | 3 | 128 B | 2 | skipped | - | - | - | - | - | - | 2 | 1 |
| Bidirectional (2 ranks, 8 chunks per round) | 2 | 128 B | 0 | ok | 13.87 | 13.39 | 13.79 | 15.49 | 32.26 | PASSED | 0 | 1 |
| Bidirectional (2 ranks, 8 chunks per round) | 2 | 128 B | 1 | ok | 13.87 | 13.43 | 13.79 | 15.39 | 32.15 | PASSED | 0 | 1 |
| Baseline (with barrier) | 3 | 32 KB | 0 | ok | 63.25 | 47.14 | 64.95 | 79.93 | 83.12 | PASSED | 0 | 1 |
| Baseline (with barrier) | 3 | 32 KB | 1 | ok | 63.25 | 48.41 | 65.96 | 80.47 | 86.07 | PASSED | 0 | 1 |
| Baseline (with barrier) | 3 | 32 KB | 2 | ok | 63.25 | 47.06 | 65.23 | 80.95 | 90.04 | PASSED | 0 | 1 |
| LL (one-shot) | 3 | 32 KB | 0 | ok | 45.28 | 40.21 | 45.05 | 50.69 | 61.47 | PASSED | 0 | 1 |
| LL (one-shot) | 3 | 32 KB | 1 | ok | 45.26 | 31.11 | 45.13 | 53.79 | 62.04 | PASSED | 0 | 1 |
| LL (one-shot) | 3 | 32 KB | 2 | ok | 45.27 | 27.41 | 44.95 | 54.72 | 64.16 | PASSED | 0 | 1 |
| Sentinel (one-shot) | 3 | 32 KB | 0 | ok | 42.48 | 35.09 | 42.80 | 48.94 | 57.74 | PASSED | 0 | 1 |
| Sentinel (one-shot) | 3 | 32 KB | 1 | ok | 42.46 | 34.56 | 42.42 | 49.31 | 57.17 | PASSED | 0 | 1 |
| Sentinel (one-shot) | 3 | 32 KB | 2 | ok | 42.47 | 34.30 | 42.27 | 48.38 | 59.81 | PASSED | 0 | 1 |
| Two-shot LL | 3 | 32 KB | 0 | ok | 35.11 | 32.01 | 34.53 | 40.33 | 47.85 | PASSED | 0 | 1 |
| Two-shot LL | 3 | 32 KB | 1 | ok | 35.12 | 33.07 | 34.29 | 40.08 | 50.07 | PASSED | 0 | 1 |
| Two-shot LL | 3 | 32 KB | 2 | ok | 35.11 | 32.53 | 34.39 | 40.13 | 48.51 | PASSED | 0 | 1 |
| Two-shot Sentinel | 3 | 32 KB | 0 | ok | 45.17 | 42.32 | 45.19 | 47.92 | 58.80 | PASSED | 0 | 1 |
| Two-shot Sentinel | 3 | 32 KB | 1 | ok | 45.18 | 41.15 | 45.27 | 49.56 | 61.57 | PASSED | 0 | 1 |
| Two-shot Sentinel | 3 | 32 KB | 2 | ok | 45.18 | 42.40 | 45.21 | 48.83 | 63.19 | PASSED | 0 | 1 |
| LL128 Atomic | 3 | 32 KB | 0 | skipped | - | - | - | - | - | - | 2 | 1 |
| LL128 Atomic | 3 | 32 KB | 1 | skipped | - | - | - | - | - | - | 2 | 1 |
| LL128 Atomic | 3 | 32 KB | 2 | skipped | - | - | - | - | - | - | 2 | 1 |
| Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 KB | 0 | ok | 38.04 | 37.70 | 37.97 | 39.77 | 46.38 | PASSED | 0 | 1 |
| Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 KB | 1 | ok | 38.04 | 37.67 | 37.96 | 39.87 | 46.35 | PASSED | 0 | 1 |
| Baseline (with barrier) | 3 | 256 KB | 0 | ok | 404.92 | 320.64 | 434.56 | 538.36 | 554.50 | PASSED | 0 | 2 |
| Baseline (with barrier) | 3 | 256 KB | 1 | ok | 405.59 | 325.29 | 434.25 | 559.97 | 586.62 | PASSED | 0 | 2 |
| Baseline (with barrier) | 3 | 256 KB | 2 | ok | 405.56 | 320.58 | 436.49 | 565.61 | 587.23 | PASSED | 0 | 2 |
| LL (one-shot) | 3 | 256 KB | 0 | ok | 323.33 | 276.45 | 323.48 | 341.60 | 360.70 | PASSED | 0 | 2 |
| LL (one-shot) | 3 | 256 KB | 1 | ok | 323.82 | 309.18 | 323.89 | 343.82 | 364.99 | PASSED | 0 | 2 |
| LL (one-shot) | 3 | 256 KB | 2 | ok | 323.93 | 305.54 | 323.52 | 342.62 | 385.02 | PASSED | 0 | 2 |
| Sentinel (one-shot) | 3 | 256 KB | 0 | ok | 291.08 | 282.36 | 290.48 | 298.09 | 304.94 | PASSED | 0 | 2 |
| Sentinel (one-shot) | 3 | 256 KB | 1 | ok | 291.05 | 272.40 | 299.70 | 317.40 | 322.29 | PASSED | 0 | 2 |
| Sentinel (one-shot) | 3 | 256 KB | 2 | ok | 291.04 | 272.68 | 290.65 | 313.20 | 325.87 | PASSED | 0 | 2 |
| Two-shot LL | 3 | 256 KB | 0 | ok | 235.06 | 225.89 | 229.51 | 290.42 | 290.55 | PASSED | 0 | 2 |
| Two-shot LL | 3 | 256 KB | 1 | ok | 234.87 | 221.11 | 229.98 | 288.01 | 289.96 | PASSED | 0 | 2 |
| Two-shot LL | 3 | 256 KB | 2 | ok | 234.97 | 224.17 | 229.35 | 290.07 | 290.74 | PASSED | 0 | 2 |
| Two-shot Sentinel | 3 | 256 KB | 0 | ok | 285.51 | 277.14 | 285.41 | 293.18 | 297.29 | PASSED | 0 | 2 |
| Two-shot Sentinel | 3 | 256 KB | 1 | ok | 285.59 | 275.82 | 284.99 | 298.09 | 300.73 | PASSED | 0 | 2 |
| Two-shot Sentinel | 3 | 256 KB | 2 | ok | 285.58 | 275.18 | 284.68 | 299.97 | 309.46 | PASSED | 0 | 2 |
| LL128 Atomic | 3 | 256 KB | 0 | skipped | - | - | - | - | - | - | 2 | 2 |
| LL128 Atomic | 3 | 256 KB | 1 | skipped | - | - | - | - | - | - | 2 | 2 |
| LL128 Atomic | 3 | 256 KB | 2 | skipped | - | - | - | - | - | - | 2 | 2 |
| Bidirectional (2 ranks, 8 chunks per round) | 2 | 256 KB | 0 | ok | 172.29 | 164.13 | 172.10 | 176.57 | 179.25 | PASSED | 0 | 2 |
| Bidirectional (2 ranks, 8 chunks per round) | 2 | 256 KB | 1 | ok | 172.36 | 170.21 | 172.05 | 176.49 | 179.49 | PASSED | 0 | 2 |
| Baseline (with barrier) | 3 | 32 MB | 0 | ok | 46,976.20 | 43,843.74 | 46,836.40 | 51,764.50 | 54,917.32 | PASSED | 0 | 3 |
| Baseline (with barrier) | 3 | 32 MB | 1 | ok | 46,969.58 | 43,836.18 | 46,762.60 | 51,109.71 | 51,777.23 | PASSED | 0 | 3 |
| Baseline (with barrier) | 3 | 32 MB | 2 | ok | 46,921.47 | 41,276.40 | 46,746.03 | 54,397.84 | 54,474.80 | PASSED | 0 | 3 |
| LL (one-shot) | 3 | 32 MB | 0 | ok | 43,923.62 | 43,030.54 | 43,894.94 | 47,102.06 | 48,065.28 | PASSED | 0 | 3 |
| LL (one-shot) | 3 | 32 MB | 1 | ok | 43,909.40 | 42,915.34 | 43,898.81 | 46,855.83 | 47,578.45 | PASSED | 0 | 3 |
| LL (one-shot) | 3 | 32 MB | 2 | ok | 43,882.36 | 39,488.74 | 43,755.66 | 49,621.69 | 50,638.84 | PASSED | 0 | 3 |
| Sentinel (one-shot) | 3 | 32 MB | 0 | ok | 38,724.21 | 38,121.45 | 38,398.82 | 42,988.50 | 43,704.86 | PASSED | 0 | 3 |
| Sentinel (one-shot) | 3 | 32 MB | 1 | ok | 38,729.38 | 37,918.86 | 38,427.48 | 43,346.94 | 43,442.50 | PASSED | 0 | 3 |
| Sentinel (one-shot) | 3 | 32 MB | 2 | ok | 38,676.51 | 36,577.49 | 38,396.00 | 44,609.66 | 44,618.93 | PASSED | 0 | 3 |
| Two-shot LL | 3 | 32 MB | 0 | ok | 31,769.15 | 31,145.36 | 31,584.62 | 34,235.28 | 34,383.96 | PASSED | 0 | 3 |
| Two-shot LL | 3 | 32 MB | 1 | ok | 31,780.19 | 30,851.25 | 31,657.12 | 34,118.85 | 34,672.82 | PASSED | 0 | 3 |
| Two-shot LL | 3 | 32 MB | 2 | ok | 31,741.22 | 30,686.09 | 31,656.66 | 34,112.26 | 35,679.55 | PASSED | 0 | 3 |
| Two-shot Sentinel | 3 | 32 MB | 0 | ok | 38,250.03 | 37,971.57 | 38,135.94 | 40,223.35 | 41,357.23 | PASSED | 0 | 3 |
| Two-shot Sentinel | 3 | 32 MB | 1 | ok | 38,244.82 | 37,884.32 | 38,136.83 | 40,882.50 | 40,918.70 | PASSED | 0 | 3 |
| Two-shot Sentinel | 3 | 32 MB | 2 | ok | 38,226.57 | 35,373.26 | 38,117.51 | 42,758.47 | 44,667.33 | PASSED | 0 | 3 |
| LL128 Atomic | 3 | 32 MB | 0 | skipped | - | - | - | - | - | - | 2 | 3 |
| LL128 Atomic | 3 | 32 MB | 1 | skipped | - | - | - | - | - | - | 2 | 3 |
| LL128 Atomic | 3 | 32 MB | 2 | skipped | - | - | - | - | - | - | 2 | 3 |
| Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 MB | 0 | ok | 19,434.11 | 19,118.41 | 19,443.11 | 19,647.26 | 20,251.53 | PASSED | 0 | 3 |
| Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 MB | 1 | ok | 19,438.12 | 19,194.26 | 19,432.85 | 19,892.20 | 20,457.79 | PASSED | 0 | 3 |
| Baseline (with barrier) | 2 | 32 B | 0 | ok | 4.17 | 3.67 | 4.43 | 4.90 | 30.97 | PASSED | 0 | 4 |
| Baseline (with barrier) | 2 | 32 B | 1 | ok | 4.17 | 2.33 | 4.44 | 4.88 | 31.10 | PASSED | 0 | 4 |
| LL (one-shot) | 2 | 32 B | 0 | ok | 1.88 | 1.74 | 1.81 | 2.06 | 17.00 | PASSED | 0 | 4 |
| LL (one-shot) | 2 | 32 B | 1 | ok | 1.88 | 1.72 | 1.81 | 2.11 | 17.34 | PASSED | 0 | 4 |
| Baseline (with barrier) | 2 | 64 B | 0 | ok | 4.30 | 2.79 | 4.44 | 4.98 | 20.33 | PASSED | 0 | 4 |
| Baseline (with barrier) | 2 | 64 B | 1 | ok | 4.30 | 2.45 | 4.44 | 5.12 | 19.65 | PASSED | 0 | 4 |
| LL (one-shot) | 2 | 64 B | 0 | ok | 1.82 | 1.72 | 1.78 | 1.95 | 12.33 | PASSED | 0 | 4 |
| LL (one-shot) | 2 | 64 B | 1 | ok | 1.82 | 1.73 | 1.77 | 1.94 | 12.36 | PASSED | 0 | 4 |
| Baseline (with barrier) | 2 | 128 B | 0 | ok | 4.31 | 2.85 | 4.52 | 4.72 | 12.12 | PASSED | 0 | 4 |
| Baseline (with barrier) | 2 | 128 B | 1 | ok | 4.31 | 3.20 | 4.54 | 4.79 | 11.24 | PASSED | 0 | 4 |
| LL (one-shot) | 2 | 128 B | 0 | ok | 1.87 | 1.73 | 1.79 | 2.15 | 17.85 | PASSED | 0 | 4 |
| LL (one-shot) | 2 | 128 B | 1 | ok | 1.88 | 1.72 | 1.79 | 2.18 | 19.14 | PASSED | 0 | 4 |
| Baseline (with barrier) | 2 | 32 KB | 0 | ok | 35.06 | 30.25 | 34.79 | 44.23 | 55.76 | PASSED | 0 | 4 |
| Baseline (with barrier) | 2 | 32 KB | 1 | ok | 35.08 | 31.67 | 34.88 | 47.03 | 49.72 | PASSED | 0 | 4 |
| LL (one-shot) | 2 | 32 KB | 0 | ok | 30.39 | 17.31 | 33.01 | 43.06 | 53.89 | PASSED | 0 | 4 |
| LL (one-shot) | 2 | 32 KB | 1 | ok | 30.38 | 19.11 | 32.70 | 42.90 | 53.71 | PASSED | 0 | 4 |
| Baseline (with barrier) | 2 | 32 MB | 0 | ok | 33,341.21 | 27,228.72 | 33,267.38 | 38,230.49 | 40,526.46 | PASSED | 0 | 4 |
| Baseline (with barrier) | 2 | 32 MB | 1 | ok | 33,355.66 | 29,300.41 | 33,076.83 | 37,404.18 | 37,601.19 | PASSED | 0 | 4 |
| LL (one-shot) | 2 | 32 MB | 0 | ok | 28,882.89 | 19,986.90 | 29,224.11 | 37,338.53 | 38,851.35 | PASSED | 0 | 4 |
| LL (one-shot) | 2 | 32 MB | 1 | ok | 28,933.70 | 20,572.06 | 29,163.95 | 37,421.02 | 38,162.53 | PASSED | 0 | 4 |
| Atomic_Probe | 2 | 32 B | 0 | no result | - | - | - | - | - | - | 0 | 5 |
| Atomic_Probe | 2 | 32 B | 1 | no result | - | - | - | - | - | - | 0 | 5 |

`log` = the number at the start of the file name in `logs/`:

- `1_3ranks_32B_64B_128B_32KB.txt`: 3 ranks, all algorithms, 32 B / 64 B / 128 B / 32 KB, 1000 rounds
- `2_3ranks_256KB.txt`: 3 ranks, all algorithms, 256 KB, 100 rounds
- `3_3ranks_32MB.txt`: 3 ranks, all algorithms, 32 MB, 100 rounds
- `4_2ranks_baseline_ll.txt`: 2 ranks, Baseline and LL, 32 B / 64 B / 128 B / 32 KB (1000 rounds) and 32 MB (100 rounds)
- `5_atomic_probe.txt`: Atomic_Probe: does the network card support RDMA atomics (needed by LL128)

