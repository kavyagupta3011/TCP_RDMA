# Results: v2 (paper_algorithms_v2)

**v2**: writes posted together, two receive buffers, min/median/p99/max, start and end barrier.

Machines: rank 0 = compute02, rank 1 = compute03, rank 2 = compute04. PBS jobs: 30219.master.local, 30220.master.local.

A value is the time of one AllReduce (median over the timed rounds, averaged over the ranks that
finished correctly). Cells with two numbers are **pass 1 / pass 2**: the same experiment run twice,
the second time with the versions in the opposite order. If a rank crashed, hung (stopped by the
timeout) or got a wrong result, the cell says so. 1000 timed rounds for sizes up to 32 KB, 100
for 256 KB and above.

## 1. Median, 3 ranks (pass 1 / pass 2)

| Algorithm | 32 B | 64 B | 128 B | 32 KB | 256 KB | 1 MB | 32 MB |
|---|---|---|---|---|---|---|---|
| Baseline (with barrier) | 8.46 µs / 8.85 µs | 8.79 µs / 8.82 µs | 8.98 µs / 8.97 µs | 65.00 µs / 64.96 µs | 441.8 µs / 437.4 µs | 1755.9 µs / 1680.1 µs | 45.85 ms / 45.63 ms |
| LL (one-shot) | 2.16 µs / 2.18 µs | 2.20 µs / 2.18 µs | 2.23 µs / 2.24 µs | 45.20 µs / 45.14 µs | 322.5 µs / 323.8 µs | 1263.1 µs / 1263.7 µs | 43.88 ms / 43.94 ms |
| Sentinel (one-shot) | 1.86 µs / 1.86 µs | 1.88 µs / 1.87 µs | 1.93 µs / 1.94 µs | 42.97 µs / 43.04 µs | 290.1 µs / 288.4 µs | 1119.5 µs / 1106.8 µs | 38.36 ms / 38.54 ms |
| Two-shot LL | 4.27 µs / 4.26 µs | 4.29 µs / 4.28 µs | 4.30 µs / 4.30 µs | 34.45 µs / 34.39 µs | 229.2 µs / 229.5 µs | 902.4 µs / 902.0 µs | 31.65 ms / 31.64 ms |
| Two-shot Sentinel | 3.58 µs / 3.51 µs | 3.57 µs / 3.57 µs | 3.69 µs / 3.69 µs | 44.89 µs / 44.89 µs | 284.2 µs / 287.5 µs | 1098.8 µs / 1109.8 µs | 38.19 ms / 38.24 ms |
| LL128 Atomic | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) |
| Bidirectional (2 ranks, 8 chunks per round) | 13.64 µs / 13.78 µs | 13.63 µs / 13.71 µs | 13.62 µs / 13.82 µs | 37.99 µs / 37.96 µs | 172.0 µs / 171.7 µs | 541.7 µs / 541.6 µs | 19.45 ms / 19.39 ms |

## 2. p99, 3 ranks (average of the passes)

99% of the rounds were faster than this.

| Algorithm | 32 B | 64 B | 128 B | 32 KB | 256 KB | 1 MB | 32 MB |
|---|---|---|---|---|---|---|---|
| Baseline (with barrier) | 9.92 µs | 9.80 µs | 10.32 µs | 81.75 µs | 569.0 µs | 2213.3 µs | 50.90 ms |
| LL (one-shot) | 2.67 µs | 2.51 µs | 2.51 µs | 52.69 µs | 340.1 µs | 1527.6 µs | 48.25 ms |
| Sentinel (one-shot) | 2.52 µs | 2.05 µs | 2.28 µs | 50.02 µs | 302.8 µs | 1128.9 µs | 43.34 ms |
| Two-shot LL | 5.52 µs | 5.79 µs | 5.66 µs | 40.22 µs | 289.2 µs | 1019.8 µs | 36.09 ms |
| Two-shot Sentinel | 4.68 µs | 4.71 µs | 5.21 µs | 48.73 µs | 298.0 µs | 1131.6 µs | 41.26 ms |
| LL128 Atomic | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) |
| Bidirectional (2 ranks, 8 chunks per round) | 15.13 µs | 15.06 µs | 15.18 µs | 39.60 µs | 175.9 µs | 546.8 µs | 19.77 ms |

## 3. Cost of the barrier: Baseline vs LL, 3 ranks

|  | 32 B | 64 B | 128 B | 32 KB | 256 KB | 1 MB | 32 MB |
|---|---|---|---|---|---|---|---|
| Baseline - LL | 6.48 µs | 6.62 µs | 6.74 µs | 19.81 µs | 116.5 µs | 454.6 µs | 1833.6 µs |
| Baseline / LL | 3.99x | 4.02x | 4.01x | 1.44x | 1.36x | 1.36x | 1.04x |

## 4. 2 ranks vs 3 ranks

|  | 32 B | 32 KB | 32 MB |
|---|---|---|---|
| Baseline (with barrier), 2 ranks | 4.38 µs | 34.91 µs | 29.08 ms |
| Baseline (with barrier), 3 ranks | 8.65 µs | 64.98 µs | 45.74 ms |
| LL (one-shot), 2 ranks | 1.78 µs | 32.93 µs | 29.97 ms |
| LL (one-shot), 3 ranks | 2.17 µs | 45.17 µs | 43.91 ms |

## 5. Atomics probe and negative test

Atomic_Probe (`logs/pass1_6_atomic_probe.txt`):

```
PROBE device atomic_cap=0 (0=NONE 1=HCA 2=GLOB) max_qp_rd_atom=16 max_qp_init_rd_atom=16
PROBE QP state=3 (3=RTS) access_flags=0xe (0x8=REMOTE_ATOMIC) max_rd_atomic=1 max_dest_rd_atomic=1 max_inline=188 max_send_sge=12
PROBE rank 0 -> peer 1, remote counter addr=207ddc0 rkey=1305078
PROBE 1 FETCH_AND_ADD, library result buf              post_send rc=22 (Invalid argument)
PROBE 2 FETCH_AND_ADD, fresh 64B-aligned result buf    post_send rc=22 (Invalid argument)
PROBE 3 CMP_AND_SWP                                    post_send rc=22 (Invalid argument)
PROBE 4 FETCH_AND_ADD after unsignaled inline WRITE    post_send rc=22 (Invalid argument)
PROBE remote counter should now equal the number of OK atomics above; reading it needs the peer (see its line)
PROBE device atomic_cap=0 (0=NONE 1=HCA 2=GLOB) max_qp_rd_atom=16 max_qp_init_rd_atom=16
PROBE QP state=3 (3=RTS) access_flags=0xe (0x8=REMOTE_ATOMIC) max_rd_atomic=1 max_dest_rd_atomic=1 max_inline=188 max_send_sge=12
PROBE rank 1 (counter host) final counter value = 0
```

Atomic_Probe (`logs/pass2_6_atomic_probe.txt`):

```
PROBE device atomic_cap=0 (0=NONE 1=HCA 2=GLOB) max_qp_rd_atom=16 max_qp_init_rd_atom=16
PROBE QP state=3 (3=RTS) access_flags=0xe (0x8=REMOTE_ATOMIC) max_rd_atomic=1 max_dest_rd_atomic=1 max_inline=188 max_send_sge=12
PROBE rank 0 -> peer 1, remote counter addr=2372dc0 rkey=637317
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

| Log | PASSED lines | FAILED lines | ranks ok | crashed | hung | wrong result | skipped |
|---|---|---|---|---|---|---|---|
| `pass1_1_3ranks_32B_64B_128B_32KB.txt` | 68 | 0 | 68 | 0 | 0 | 0 | 12 |
| `pass1_2_3ranks_256KB.txt` | 17 | 0 | 17 | 0 | 0 | 0 | 3 |
| `pass1_3_3ranks_1MB.txt` | 17 | 0 | 17 | 0 | 0 | 0 | 3 |
| `pass1_4_3ranks_32MB.txt` | 17 | 0 | 17 | 0 | 0 | 0 | 3 |
| `pass1_5_2ranks.txt` | 12 | 0 | 12 | 0 | 0 | 0 | 0 |
| `pass1_6_atomic_probe.txt` | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| `pass2_1_3ranks_32B_64B_128B_32KB.txt` | 68 | 0 | 68 | 0 | 0 | 0 | 12 |
| `pass2_2_3ranks_256KB.txt` | 17 | 0 | 17 | 0 | 0 | 0 | 3 |
| `pass2_3_3ranks_1MB.txt` | 17 | 0 | 17 | 0 | 0 | 0 | 3 |
| `pass2_4_3ranks_32MB.txt` | 17 | 0 | 17 | 0 | 0 | 0 | 3 |
| `pass2_5_2ranks.txt` | 12 | 0 | 12 | 0 | 0 | 0 | 0 |
| `pass2_6_atomic_probe.txt` | 0 | 0 | 0 | 0 | 0 | 0 | 0 |

## 7. Every run, per rank

Times in µs. `-` = not printed by this version or the rank did not finish.

| pass | Program | ranks | size | rank | status | mean | min | median | p99 | max | correctness | exit |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| pass1 | Baseline (with barrier) | 3 | 32 B | 0 | ok | 8.01 | 7.46 | 8.21 | 9.40 | 20.92 | PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 32 B | 1 | ok | 8.01 | 6.65 | 9.05 | 9.69 | 20.61 | PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 32 B | 2 | ok | 8.01 | 4.71 | 8.12 | 9.96 | 25.33 | PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 32 B | 0 | ok | 2.24 | 2.09 | 2.16 | 3.02 | 18.41 | PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 32 B | 1 | ok | 2.23 | 2.08 | 2.15 | 2.81 | 18.48 | PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 32 B | 2 | ok | 2.24 | 2.08 | 2.16 | 3.00 | 18.28 | PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 32 B | 0 | ok | 1.89 | 1.80 | 1.86 | 2.07 | 8.27 | PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 32 B | 1 | ok | 1.89 | 1.79 | 1.86 | 2.07 | 7.71 | PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 32 B | 2 | ok | 1.89 | 1.79 | 1.86 | 2.08 | 8.05 | PASSED | 0 |
| pass1 | Two-shot LL | 3 | 32 B | 0 | ok | 4.32 | 4.19 | 4.28 | 5.15 | 9.50 | PASSED | 0 |
| pass1 | Two-shot LL | 3 | 32 B | 1 | ok | 4.32 | 4.09 | 4.26 | 5.61 | 10.00 | PASSED | 0 |
| pass1 | Two-shot LL | 3 | 32 B | 2 | ok | 4.32 | 4.15 | 4.26 | 5.43 | 10.07 | PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 32 B | 0 | ok | 3.64 | 3.48 | 3.58 | 4.74 | 18.80 | PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 32 B | 1 | ok | 3.64 | 3.45 | 3.58 | 5.22 | 18.84 | PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 32 B | 2 | ok | 3.65 | 3.35 | 3.58 | 5.22 | 18.66 | PASSED | 0 |
| pass1 | LL128 Atomic | 3 | 32 B | 0 | skipped | - | - | - | - | - | - | 2 |
| pass1 | LL128 Atomic | 3 | 32 B | 1 | skipped | - | - | - | - | - | - | 2 |
| pass1 | LL128 Atomic | 3 | 32 B | 2 | skipped | - | - | - | - | - | - | 2 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 B | 0 | ok | 13.71 | 13.43 | 13.61 | 15.04 | 24.59 | PASSED | 0 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 B | 1 | ok | 13.71 | 13.38 | 13.66 | 15.31 | 23.79 | PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 64 B | 0 | ok | 8.02 | 6.06 | 8.25 | 9.20 | 17.14 | PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 64 B | 1 | ok | 8.02 | 6.69 | 9.07 | 9.63 | 18.68 | PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 64 B | 2 | ok | 8.02 | 5.99 | 9.04 | 10.05 | 16.37 | PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 64 B | 0 | ok | 2.23 | 2.12 | 2.20 | 2.47 | 10.65 | PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 64 B | 1 | ok | 2.23 | 2.08 | 2.19 | 2.47 | 11.85 | PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 64 B | 2 | ok | 2.22 | 2.03 | 2.20 | 2.47 | 11.34 | PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 64 B | 0 | ok | 1.91 | 1.83 | 1.88 | 2.02 | 11.01 | PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 64 B | 1 | ok | 1.91 | 1.81 | 1.87 | 2.06 | 11.82 | PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 64 B | 2 | ok | 1.91 | 1.81 | 1.88 | 2.03 | 11.32 | PASSED | 0 |
| pass1 | Two-shot LL | 3 | 64 B | 0 | ok | 4.34 | 4.16 | 4.28 | 6.08 | 15.56 | PASSED | 0 |
| pass1 | Two-shot LL | 3 | 64 B | 1 | ok | 4.34 | 4.14 | 4.30 | 6.01 | 15.49 | PASSED | 0 |
| pass1 | Two-shot LL | 3 | 64 B | 2 | ok | 4.35 | 4.19 | 4.29 | 6.02 | 15.51 | PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 64 B | 0 | ok | 3.62 | 3.51 | 3.57 | 4.64 | 20.21 | PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 64 B | 1 | ok | 3.62 | 3.49 | 3.57 | 4.94 | 19.10 | PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 64 B | 2 | ok | 3.62 | 3.48 | 3.57 | 4.83 | 20.15 | PASSED | 0 |
| pass1 | LL128 Atomic | 3 | 64 B | 0 | skipped | - | - | - | - | - | - | 2 |
| pass1 | LL128 Atomic | 3 | 64 B | 1 | skipped | - | - | - | - | - | - | 2 |
| pass1 | LL128 Atomic | 3 | 64 B | 2 | skipped | - | - | - | - | - | - | 2 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 64 B | 0 | ok | 13.69 | 13.40 | 13.66 | 15.24 | 21.74 | PASSED | 0 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 64 B | 1 | ok | 13.70 | 13.44 | 13.61 | 14.98 | 21.48 | PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 128 B | 0 | ok | 8.24 | 4.83 | 8.40 | 10.34 | 32.52 | PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 128 B | 1 | ok | 8.24 | 6.83 | 9.28 | 10.19 | 33.40 | PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 128 B | 2 | ok | 8.25 | 6.27 | 9.25 | 11.15 | 33.68 | PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 128 B | 0 | ok | 2.26 | 2.14 | 2.24 | 2.48 | 11.83 | PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 128 B | 1 | ok | 2.26 | 2.13 | 2.23 | 2.53 | 12.72 | PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 128 B | 2 | ok | 2.26 | 2.14 | 2.23 | 2.51 | 12.32 | PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 128 B | 0 | ok | 1.96 | 1.86 | 1.93 | 2.34 | 7.37 | PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 128 B | 1 | ok | 1.96 | 1.86 | 1.93 | 2.41 | 7.45 | PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 128 B | 2 | ok | 1.96 | 1.86 | 1.93 | 2.46 | 7.44 | PASSED | 0 |
| pass1 | Two-shot LL | 3 | 128 B | 0 | ok | 4.36 | 4.20 | 4.30 | 5.81 | 12.07 | PASSED | 0 |
| pass1 | Two-shot LL | 3 | 128 B | 1 | ok | 4.36 | 4.21 | 4.30 | 5.80 | 13.17 | PASSED | 0 |
| pass1 | Two-shot LL | 3 | 128 B | 2 | ok | 4.36 | 4.22 | 4.30 | 5.25 | 12.57 | PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 128 B | 0 | ok | 3.74 | 3.60 | 3.69 | 5.20 | 19.86 | PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 128 B | 1 | ok | 3.74 | 3.60 | 3.69 | 5.22 | 21.25 | PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 128 B | 2 | ok | 3.75 | 3.61 | 3.69 | 5.19 | 19.47 | PASSED | 0 |
| pass1 | LL128 Atomic | 3 | 128 B | 0 | skipped | - | - | - | - | - | - | 2 |
| pass1 | LL128 Atomic | 3 | 128 B | 1 | skipped | - | - | - | - | - | - | 2 |
| pass1 | LL128 Atomic | 3 | 128 B | 2 | skipped | - | - | - | - | - | - | 2 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 128 B | 0 | ok | 13.70 | 13.36 | 13.64 | 15.07 | 24.16 | PASSED | 0 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 128 B | 1 | ok | 13.70 | 13.43 | 13.61 | 15.02 | 24.21 | PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 32 KB | 0 | ok | 63.16 | 45.40 | 64.89 | 79.49 | 82.27 | PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 32 KB | 1 | ok | 63.18 | 48.37 | 65.65 | 82.38 | 84.78 | PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 32 KB | 2 | ok | 63.16 | 45.58 | 64.47 | 84.35 | 98.01 | PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 32 KB | 0 | ok | 45.25 | 39.99 | 45.29 | 49.82 | 60.40 | PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 32 KB | 1 | ok | 45.26 | 33.98 | 45.09 | 49.05 | 62.08 | PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 32 KB | 2 | ok | 45.25 | 29.36 | 45.22 | 50.30 | 63.85 | PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 32 KB | 0 | ok | 43.05 | 34.40 | 43.41 | 50.13 | 114.88 | PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 32 KB | 1 | ok | 43.06 | 34.72 | 42.52 | 50.20 | 107.32 | PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 32 KB | 2 | ok | 43.06 | 34.18 | 42.98 | 49.68 | 115.76 | PASSED | 0 |
| pass1 | Two-shot LL | 3 | 32 KB | 0 | ok | 34.67 | 32.16 | 34.68 | 40.39 | 43.65 | PASSED | 0 |
| pass1 | Two-shot LL | 3 | 32 KB | 1 | ok | 34.69 | 33.47 | 34.24 | 40.17 | 45.81 | PASSED | 0 |
| pass1 | Two-shot LL | 3 | 32 KB | 2 | ok | 34.67 | 32.41 | 34.45 | 40.08 | 44.26 | PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 32 KB | 0 | ok | 44.90 | 41.68 | 44.88 | 48.60 | 58.43 | PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 32 KB | 1 | ok | 44.90 | 41.17 | 44.91 | 49.06 | 62.00 | PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 32 KB | 2 | ok | 44.90 | 41.92 | 44.87 | 48.23 | 60.71 | PASSED | 0 |
| pass1 | LL128 Atomic | 3 | 32 KB | 0 | skipped | - | - | - | - | - | - | 2 |
| pass1 | LL128 Atomic | 3 | 32 KB | 1 | skipped | - | - | - | - | - | - | 2 |
| pass1 | LL128 Atomic | 3 | 32 KB | 2 | skipped | - | - | - | - | - | - | 2 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 KB | 0 | ok | 38.05 | 37.68 | 37.99 | 39.61 | 45.94 | PASSED | 0 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 KB | 1 | ok | 38.06 | 37.53 | 37.98 | 39.80 | 47.22 | PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 256 KB | 0 | ok | 423.27 | 319.86 | 441.16 | 537.78 | 540.88 | PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 256 KB | 1 | ok | 424.01 | 322.84 | 443.77 | 582.50 | 597.33 | PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 256 KB | 2 | ok | 423.30 | 321.55 | 440.44 | 578.55 | 590.50 | PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 256 KB | 0 | ok | 322.42 | 261.82 | 322.15 | 329.37 | 376.50 | PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 256 KB | 1 | ok | 323.22 | 312.87 | 322.62 | 337.40 | 393.25 | PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 256 KB | 2 | ok | 323.02 | 306.90 | 322.64 | 337.87 | 378.24 | PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 256 KB | 0 | ok | 287.45 | 273.38 | 292.02 | 303.06 | 316.67 | PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 256 KB | 1 | ok | 287.37 | 273.00 | 291.85 | 305.03 | 306.20 | PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 256 KB | 2 | ok | 287.25 | 272.31 | 286.57 | 299.69 | 317.14 | PASSED | 0 |
| pass1 | Two-shot LL | 3 | 256 KB | 0 | ok | 234.32 | 226.23 | 229.27 | 288.14 | 289.84 | PASSED | 0 |
| pass1 | Two-shot LL | 3 | 256 KB | 1 | ok | 234.38 | 215.19 | 229.50 | 287.21 | 288.31 | PASSED | 0 |
| pass1 | Two-shot LL | 3 | 256 KB | 2 | ok | 234.36 | 224.21 | 228.82 | 288.37 | 289.99 | PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 256 KB | 0 | ok | 284.64 | 277.62 | 283.63 | 295.28 | 297.95 | PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 256 KB | 1 | ok | 284.60 | 269.19 | 285.29 | 295.32 | 295.78 | PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 256 KB | 2 | ok | 284.85 | 272.80 | 283.79 | 298.24 | 303.94 | PASSED | 0 |
| pass1 | LL128 Atomic | 3 | 256 KB | 0 | skipped | - | - | - | - | - | - | 2 |
| pass1 | LL128 Atomic | 3 | 256 KB | 1 | skipped | - | - | - | - | - | - | 2 |
| pass1 | LL128 Atomic | 3 | 256 KB | 2 | skipped | - | - | - | - | - | - | 2 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 256 KB | 0 | ok | 172.23 | 166.10 | 172.03 | 176.69 | 177.62 | PASSED | 0 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 256 KB | 1 | ok | 172.31 | 170.85 | 172.05 | 176.57 | 176.68 | PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 1 MB | 0 | ok | 1,698.54 | 1,266.39 | 1,755.69 | 2,171.20 | 2,225.91 | PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 1 MB | 1 | ok | 1,698.26 | 1,269.47 | 1,763.47 | 2,250.31 | 2,356.59 | PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 1 MB | 2 | ok | 1,698.95 | 1,256.42 | 1,748.59 | 2,237.63 | 2,238.69 | PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 1 MB | 0 | ok | 1,268.34 | 1,197.50 | 1,263.38 | 1,327.93 | 1,736.70 | PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 1 MB | 1 | ok | 1,268.89 | 787.62 | 1,262.94 | 1,729.06 | 1,736.61 | PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 1 MB | 2 | ok | 1,269.26 | 1,232.86 | 1,263.07 | 1,368.22 | 1,726.80 | PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 1 MB | 0 | ok | 1,115.16 | 1,104.85 | 1,119.16 | 1,128.42 | 1,129.44 | PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 1 MB | 1 | ok | 1,115.67 | 1,097.01 | 1,123.98 | 1,138.77 | 1,188.17 | PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 1 MB | 2 | ok | 1,116.05 | 1,093.11 | 1,115.47 | 1,131.89 | 1,208.86 | PASSED | 0 |
| pass1 | Two-shot LL | 3 | 1 MB | 0 | ok | 910.50 | 887.44 | 901.61 | 1,096.98 | 1,138.08 | PASSED | 0 |
| pass1 | Two-shot LL | 3 | 1 MB | 1 | ok | 910.76 | 886.03 | 903.06 | 1,081.75 | 1,126.51 | PASSED | 0 |
| pass1 | Two-shot LL | 3 | 1 MB | 2 | ok | 911.31 | 889.95 | 902.54 | 1,099.22 | 1,120.52 | PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 1 MB | 0 | ok | 1,099.48 | 1,088.45 | 1,098.39 | 1,122.45 | 1,124.27 | PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 1 MB | 1 | ok | 1,099.75 | 1,075.31 | 1,099.06 | 1,134.07 | 1,140.20 | PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 1 MB | 2 | ok | 1,100.16 | 1,061.19 | 1,098.97 | 1,136.67 | 1,155.80 | PASSED | 0 |
| pass1 | LL128 Atomic | 3 | 1 MB | 0 | skipped | - | - | - | - | - | - | 2 |
| pass1 | LL128 Atomic | 3 | 1 MB | 1 | skipped | - | - | - | - | - | - | 2 |
| pass1 | LL128 Atomic | 3 | 1 MB | 2 | skipped | - | - | - | - | - | - | 2 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 1 MB | 0 | ok | 541.79 | 537.75 | 541.84 | 546.87 | 547.56 | PASSED | 0 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 1 MB | 1 | ok | 541.98 | 538.60 | 541.56 | 547.41 | 548.09 | PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 32 MB | 0 | ok | 46,181.09 | 44,013.63 | 45,825.79 | 49,851.75 | 51,457.12 | PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 32 MB | 1 | ok | 46,116.44 | 43,835.16 | 45,874.61 | 50,871.07 | 50,919.96 | PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 32 MB | 2 | ok | 46,120.44 | 41,261.89 | 45,852.19 | 53,249.09 | 53,897.92 | PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 32 MB | 0 | ok | 43,992.95 | 43,226.84 | 43,895.74 | 47,421.53 | 47,735.64 | PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 32 MB | 1 | ok | 43,988.46 | 43,026.10 | 43,911.19 | 47,844.91 | 48,196.87 | PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 32 MB | 2 | ok | 43,954.29 | 39,313.06 | 43,821.90 | 49,929.15 | 49,987.21 | PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 32 MB | 0 | ok | 38,623.80 | 38,102.44 | 38,343.81 | 43,048.91 | 43,485.15 | PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 32 MB | 1 | ok | 38,628.57 | 38,007.15 | 38,356.42 | 43,127.56 | 43,801.34 | PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 32 MB | 2 | ok | 38,578.18 | 36,346.41 | 38,371.98 | 43,233.08 | 45,572.64 | PASSED | 0 |
| pass1 | Two-shot LL | 3 | 32 MB | 0 | ok | 31,811.96 | 31,187.51 | 31,645.08 | 37,809.44 | 38,793.15 | PASSED | 0 |
| pass1 | Two-shot LL | 3 | 32 MB | 1 | ok | 31,812.28 | 30,941.08 | 31,661.16 | 35,321.62 | 36,666.74 | PASSED | 0 |
| pass1 | Two-shot LL | 3 | 32 MB | 2 | ok | 31,774.22 | 30,146.91 | 31,642.98 | 38,081.50 | 38,374.24 | PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 32 MB | 0 | ok | 38,294.41 | 37,964.26 | 38,175.27 | 40,348.42 | 40,457.47 | PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 32 MB | 1 | ok | 38,294.71 | 37,841.58 | 38,209.78 | 41,190.77 | 41,397.72 | PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 32 MB | 2 | ok | 38,271.77 | 35,239.57 | 38,183.19 | 43,087.98 | 43,135.62 | PASSED | 0 |
| pass1 | LL128 Atomic | 3 | 32 MB | 0 | skipped | - | - | - | - | - | - | 2 |
| pass1 | LL128 Atomic | 3 | 32 MB | 1 | skipped | - | - | - | - | - | - | 2 |
| pass1 | LL128 Atomic | 3 | 32 MB | 2 | skipped | - | - | - | - | - | - | 2 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 MB | 0 | ok | 19,455.17 | 19,131.74 | 19,448.92 | 19,769.88 | 20,452.41 | PASSED | 0 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 MB | 1 | ok | 19,451.74 | 19,164.99 | 19,450.47 | 19,895.05 | 20,112.58 | PASSED | 0 |
| pass1 | Baseline (with barrier) | 2 | 32 B | 0 | ok | 4.12 | 3.65 | 4.40 | 4.56 | 12.75 | PASSED | 0 |
| pass1 | Baseline (with barrier) | 2 | 32 B | 1 | ok | 4.12 | 2.37 | 4.39 | 4.67 | 14.15 | PASSED | 0 |
| pass1 | LL (one-shot) | 2 | 32 B | 0 | ok | 1.83 | 1.73 | 1.80 | 1.95 | 12.62 | PASSED | 0 |
| pass1 | LL (one-shot) | 2 | 32 B | 1 | ok | 1.83 | 1.71 | 1.80 | 1.93 | 12.74 | PASSED | 0 |
| pass1 | Baseline (with barrier) | 2 | 32 KB | 0 | ok | 35.29 | 30.61 | 34.94 | 44.00 | 49.69 | PASSED | 0 |
| pass1 | Baseline (with barrier) | 2 | 32 KB | 1 | ok | 35.27 | 31.66 | 34.95 | 46.28 | 48.47 | PASSED | 0 |
| pass1 | LL (one-shot) | 2 | 32 KB | 0 | ok | 30.29 | 17.30 | 31.37 | 42.85 | 44.85 | PASSED | 0 |
| pass1 | LL (one-shot) | 2 | 32 KB | 1 | ok | 30.31 | 19.14 | 31.66 | 42.74 | 46.06 | PASSED | 0 |
| pass1 | Baseline (with barrier) | 2 | 32 MB | 0 | ok | 29,144.19 | 26,250.20 | 29,132.49 | 31,412.19 | 31,799.37 | PASSED | 0 |
| pass1 | Baseline (with barrier) | 2 | 32 MB | 1 | ok | 29,107.78 | 28,230.70 | 28,978.15 | 30,794.10 | 31,231.95 | PASSED | 0 |
| pass1 | LL (one-shot) | 2 | 32 MB | 0 | ok | 29,180.43 | 20,340.56 | 29,100.18 | 37,966.74 | 38,465.46 | PASSED | 0 |
| pass1 | LL (one-shot) | 2 | 32 MB | 1 | ok | 29,142.08 | 20,349.09 | 29,064.94 | 37,482.33 | 37,930.94 | PASSED | 0 |
| pass1 | Atomic_Probe | 2 | 32 B | 0 | no result | - | - | - | - | - | - | 0 |
| pass1 | Atomic_Probe | 2 | 32 B | 1 | no result | - | - | - | - | - | - | 0 |
| pass2 | Baseline (with barrier) | 3 | 32 B | 0 | ok | 8.07 | 6.23 | 8.30 | 10.14 | 24.62 | PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 32 B | 1 | ok | 8.07 | 6.69 | 9.13 | 9.99 | 25.67 | PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 32 B | 2 | ok | 8.07 | 4.76 | 9.12 | 10.36 | 24.09 | PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 32 B | 0 | ok | 2.22 | 2.10 | 2.19 | 2.38 | 7.51 | PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 32 B | 1 | ok | 2.22 | 2.10 | 2.17 | 2.41 | 7.91 | PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 32 B | 2 | ok | 2.22 | 2.10 | 2.19 | 2.40 | 7.35 | PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 32 B | 0 | ok | 1.93 | 1.81 | 1.86 | 3.25 | 18.26 | PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 32 B | 1 | ok | 1.93 | 1.81 | 1.86 | 2.65 | 17.35 | PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 32 B | 2 | ok | 1.93 | 1.80 | 1.86 | 2.98 | 17.78 | PASSED | 0 |
| pass2 | Two-shot LL | 3 | 32 B | 0 | ok | 4.32 | 4.13 | 4.28 | 5.35 | 14.05 | PASSED | 0 |
| pass2 | Two-shot LL | 3 | 32 B | 1 | ok | 4.32 | 4.10 | 4.26 | 5.86 | 13.94 | PASSED | 0 |
| pass2 | Two-shot LL | 3 | 32 B | 2 | ok | 4.32 | 4.16 | 4.26 | 5.72 | 13.38 | PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 32 B | 0 | ok | 3.56 | 3.45 | 3.52 | 4.16 | 13.11 | PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 32 B | 1 | ok | 3.56 | 3.43 | 3.51 | 4.46 | 13.21 | PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 32 B | 2 | ok | 3.56 | 3.41 | 3.51 | 4.29 | 13.09 | PASSED | 0 |
| pass2 | LL128 Atomic | 3 | 32 B | 0 | skipped | - | - | - | - | - | - | 2 |
| pass2 | LL128 Atomic | 3 | 32 B | 1 | skipped | - | - | - | - | - | - | 2 |
| pass2 | LL128 Atomic | 3 | 32 B | 2 | skipped | - | - | - | - | - | - | 2 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 B | 0 | ok | 13.83 | 13.25 | 13.78 | 15.10 | 24.14 | PASSED | 0 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 B | 1 | ok | 13.83 | 13.28 | 13.78 | 15.08 | 24.19 | PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 64 B | 0 | ok | 8.08 | 7.11 | 8.29 | 9.59 | 30.15 | PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 64 B | 1 | ok | 8.08 | 6.73 | 9.10 | 9.89 | 31.78 | PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 64 B | 2 | ok | 8.08 | 6.59 | 9.08 | 10.46 | 31.28 | PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 64 B | 0 | ok | 2.21 | 2.11 | 2.18 | 2.70 | 12.68 | PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 64 B | 1 | ok | 2.21 | 2.12 | 2.18 | 2.52 | 12.90 | PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 64 B | 2 | ok | 2.21 | 2.11 | 2.18 | 2.45 | 12.46 | PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 64 B | 0 | ok | 1.91 | 1.81 | 1.87 | 2.03 | 7.54 | PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 64 B | 1 | ok | 1.91 | 1.82 | 1.88 | 2.08 | 7.22 | PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 64 B | 2 | ok | 1.91 | 1.81 | 1.88 | 2.06 | 7.32 | PASSED | 0 |
| pass2 | Two-shot LL | 3 | 64 B | 0 | ok | 4.32 | 4.13 | 4.28 | 5.77 | 9.91 | PASSED | 0 |
| pass2 | Two-shot LL | 3 | 64 B | 1 | ok | 4.32 | 4.15 | 4.29 | 5.51 | 9.84 | PASSED | 0 |
| pass2 | Two-shot LL | 3 | 64 B | 2 | ok | 4.32 | 4.16 | 4.28 | 5.34 | 9.88 | PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 64 B | 0 | ok | 3.61 | 3.49 | 3.57 | 4.60 | 13.22 | PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 64 B | 1 | ok | 3.61 | 3.50 | 3.57 | 4.90 | 13.51 | PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 64 B | 2 | ok | 3.60 | 3.49 | 3.57 | 4.33 | 13.01 | PASSED | 0 |
| pass2 | LL128 Atomic | 3 | 64 B | 0 | skipped | - | - | - | - | - | - | 2 |
| pass2 | LL128 Atomic | 3 | 64 B | 1 | skipped | - | - | - | - | - | - | 2 |
| pass2 | LL128 Atomic | 3 | 64 B | 2 | skipped | - | - | - | - | - | - | 2 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 64 B | 0 | ok | 13.76 | 13.40 | 13.72 | 15.17 | 24.20 | PASSED | 0 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 64 B | 1 | ok | 13.76 | 13.46 | 13.70 | 14.84 | 23.77 | PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 128 B | 0 | ok | 8.23 | 6.92 | 8.44 | 9.53 | 18.53 | PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 128 B | 1 | ok | 8.23 | 6.88 | 9.29 | 10.32 | 16.95 | PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 128 B | 2 | ok | 8.23 | 6.23 | 9.19 | 10.42 | 16.02 | PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 128 B | 0 | ok | 2.28 | 2.13 | 2.25 | 2.48 | 11.14 | PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 128 B | 1 | ok | 2.28 | 2.13 | 2.24 | 2.53 | 11.45 | PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 128 B | 2 | ok | 2.27 | 2.15 | 2.24 | 2.53 | 11.46 | PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 128 B | 0 | ok | 1.97 | 1.87 | 1.94 | 2.17 | 14.29 | PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 128 B | 1 | ok | 1.97 | 1.88 | 1.94 | 2.16 | 14.57 | PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 128 B | 2 | ok | 1.97 | 1.88 | 1.94 | 2.14 | 14.52 | PASSED | 0 |
| pass2 | Two-shot LL | 3 | 128 B | 0 | ok | 4.36 | 4.21 | 4.31 | 5.88 | 14.68 | PASSED | 0 |
| pass2 | Two-shot LL | 3 | 128 B | 1 | ok | 4.36 | 4.21 | 4.30 | 5.77 | 15.09 | PASSED | 0 |
| pass2 | Two-shot LL | 3 | 128 B | 2 | ok | 4.36 | 4.20 | 4.30 | 5.44 | 14.43 | PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 128 B | 0 | ok | 3.73 | 3.61 | 3.69 | 5.08 | 9.24 | PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 128 B | 1 | ok | 3.73 | 3.61 | 3.69 | 5.19 | 9.27 | PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 128 B | 2 | ok | 3.73 | 3.60 | 3.69 | 5.41 | 9.43 | PASSED | 0 |
| pass2 | LL128 Atomic | 3 | 128 B | 0 | skipped | - | - | - | - | - | - | 2 |
| pass2 | LL128 Atomic | 3 | 128 B | 1 | skipped | - | - | - | - | - | - | 2 |
| pass2 | LL128 Atomic | 3 | 128 B | 2 | skipped | - | - | - | - | - | - | 2 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 128 B | 0 | ok | 13.89 | 13.58 | 13.82 | 15.39 | 23.80 | PASSED | 0 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 128 B | 1 | ok | 13.89 | 13.55 | 13.83 | 15.22 | 23.98 | PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 32 KB | 0 | ok | 62.80 | 47.50 | 64.74 | 79.49 | 80.59 | PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 32 KB | 1 | ok | 62.81 | 47.55 | 65.68 | 80.64 | 84.36 | PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 32 KB | 2 | ok | 62.79 | 45.48 | 64.46 | 84.12 | 96.15 | PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 32 KB | 0 | ok | 45.26 | 41.10 | 45.24 | 51.78 | 61.86 | PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 32 KB | 1 | ok | 45.27 | 27.75 | 45.08 | 57.43 | 66.32 | PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 32 KB | 2 | ok | 45.26 | 27.33 | 45.11 | 57.77 | 64.34 | PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 32 KB | 0 | ok | 42.92 | 35.06 | 43.46 | 49.43 | 57.20 | PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 32 KB | 1 | ok | 42.91 | 34.26 | 42.72 | 50.73 | 56.95 | PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 32 KB | 2 | ok | 42.91 | 34.52 | 42.94 | 49.96 | 58.17 | PASSED | 0 |
| pass2 | Two-shot LL | 3 | 32 KB | 0 | ok | 34.82 | 29.82 | 34.48 | 40.23 | 48.98 | PASSED | 0 |
| pass2 | Two-shot LL | 3 | 32 KB | 1 | ok | 34.84 | 31.88 | 34.28 | 40.37 | 49.53 | PASSED | 0 |
| pass2 | Two-shot LL | 3 | 32 KB | 2 | ok | 34.84 | 32.54 | 34.41 | 40.07 | 51.11 | PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 32 KB | 0 | ok | 44.89 | 41.23 | 44.87 | 49.08 | 65.05 | PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 32 KB | 1 | ok | 44.89 | 41.10 | 44.91 | 49.38 | 62.79 | PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 32 KB | 2 | ok | 44.90 | 41.98 | 44.90 | 48.01 | 63.10 | PASSED | 0 |
| pass2 | LL128 Atomic | 3 | 32 KB | 0 | skipped | - | - | - | - | - | - | 2 |
| pass2 | LL128 Atomic | 3 | 32 KB | 1 | skipped | - | - | - | - | - | - | 2 |
| pass2 | LL128 Atomic | 3 | 32 KB | 2 | skipped | - | - | - | - | - | - | 2 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 KB | 0 | ok | 38.01 | 37.65 | 37.96 | 39.47 | 45.59 | PASSED | 0 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 KB | 1 | ok | 38.01 | 37.56 | 37.95 | 39.51 | 49.76 | PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 256 KB | 0 | ok | 413.15 | 320.26 | 434.84 | 550.60 | 564.35 | PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 256 KB | 1 | ok | 412.31 | 324.85 | 437.46 | 584.83 | 585.32 | PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 256 KB | 2 | ok | 412.34 | 319.15 | 439.76 | 580.01 | 590.65 | PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 256 KB | 0 | ok | 323.81 | 302.03 | 323.91 | 338.17 | 348.26 | PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 256 KB | 1 | ok | 324.31 | 286.96 | 323.78 | 358.35 | 360.89 | PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 256 KB | 2 | ok | 323.94 | 308.06 | 323.59 | 339.70 | 355.69 | PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 256 KB | 0 | ok | 287.23 | 272.80 | 290.46 | 303.21 | 304.69 | PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 256 KB | 1 | ok | 287.91 | 280.27 | 287.00 | 300.22 | 352.96 | PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 256 KB | 2 | ok | 287.75 | 276.63 | 287.74 | 305.69 | 350.62 | PASSED | 0 |
| pass2 | Two-shot LL | 3 | 256 KB | 0 | ok | 236.74 | 225.77 | 229.43 | 291.67 | 295.15 | PASSED | 0 |
| pass2 | Two-shot LL | 3 | 256 KB | 1 | ok | 236.76 | 223.11 | 229.80 | 289.26 | 291.09 | PASSED | 0 |
| pass2 | Two-shot LL | 3 | 256 KB | 2 | ok | 236.75 | 221.29 | 229.17 | 290.69 | 296.97 | PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 256 KB | 0 | ok | 287.58 | 276.13 | 288.27 | 295.94 | 297.10 | PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 256 KB | 1 | ok | 287.64 | 276.66 | 287.66 | 302.09 | 303.01 | PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 256 KB | 2 | ok | 287.62 | 275.00 | 286.63 | 300.88 | 302.54 | PASSED | 0 |
| pass2 | LL128 Atomic | 3 | 256 KB | 0 | skipped | - | - | - | - | - | - | 2 |
| pass2 | LL128 Atomic | 3 | 256 KB | 1 | skipped | - | - | - | - | - | - | 2 |
| pass2 | LL128 Atomic | 3 | 256 KB | 2 | skipped | - | - | - | - | - | - | 2 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 256 KB | 0 | ok | 171.78 | 164.44 | 171.76 | 175.23 | 175.96 | PASSED | 0 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 256 KB | 1 | ok | 171.84 | 170.65 | 171.69 | 175.04 | 176.01 | PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 1 MB | 0 | ok | 1,614.00 | 1,262.67 | 1,654.60 | 2,124.30 | 2,168.62 | PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 1 MB | 1 | ok | 1,620.02 | 1,267.68 | 1,737.24 | 2,255.61 | 2,257.02 | PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 1 MB | 2 | ok | 1,615.79 | 1,256.89 | 1,648.60 | 2,240.56 | 2,240.64 | PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 1 MB | 0 | ok | 1,277.20 | 1,249.52 | 1,264.15 | 1,552.81 | 1,619.93 | PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 1 MB | 1 | ok | 1,277.10 | 892.30 | 1,262.99 | 1,622.28 | 1,630.72 | PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 1 MB | 2 | ok | 1,277.16 | 1,237.28 | 1,263.81 | 1,565.06 | 1,625.64 | PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 1 MB | 0 | ok | 1,105.14 | 1,093.04 | 1,106.29 | 1,118.70 | 1,126.20 | PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 1 MB | 1 | ok | 1,106.08 | 1,091.94 | 1,108.82 | 1,126.51 | 1,188.25 | PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 1 MB | 2 | ok | 1,106.36 | 1,087.48 | 1,105.17 | 1,129.29 | 1,222.94 | PASSED | 0 |
| pass2 | Two-shot LL | 3 | 1 MB | 0 | ok | 906.13 | 890.05 | 901.60 | 946.06 | 954.52 | PASSED | 0 |
| pass2 | Two-shot LL | 3 | 1 MB | 1 | ok | 906.25 | 883.47 | 903.14 | 941.01 | 941.74 | PASSED | 0 |
| pass2 | Two-shot LL | 3 | 1 MB | 2 | ok | 906.83 | 887.31 | 901.28 | 953.89 | 973.52 | PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 1 MB | 0 | ok | 1,109.85 | 1,087.17 | 1,110.95 | 1,126.77 | 1,127.80 | PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 1 MB | 1 | ok | 1,110.05 | 1,089.57 | 1,108.93 | 1,133.15 | 1,134.52 | PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 1 MB | 2 | ok | 1,110.59 | 1,078.26 | 1,109.51 | 1,136.72 | 1,176.09 | PASSED | 0 |
| pass2 | LL128 Atomic | 3 | 1 MB | 0 | skipped | - | - | - | - | - | - | 2 |
| pass2 | LL128 Atomic | 3 | 1 MB | 1 | skipped | - | - | - | - | - | - | 2 |
| pass2 | LL128 Atomic | 3 | 1 MB | 2 | skipped | - | - | - | - | - | - | 2 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 1 MB | 0 | ok | 542.07 | 539.38 | 541.73 | 547.37 | 547.37 | PASSED | 0 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 1 MB | 1 | ok | 541.89 | 538.97 | 541.55 | 545.43 | 549.05 | PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 32 MB | 0 | ok | 45,924.74 | 44,099.38 | 45,634.11 | 49,010.27 | 50,319.91 | PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 32 MB | 1 | ok | 45,934.13 | 44,046.17 | 45,630.79 | 49,702.21 | 50,033.50 | PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 32 MB | 2 | ok | 45,897.51 | 41,161.79 | 45,636.67 | 52,700.71 | 53,024.19 | PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 32 MB | 0 | ok | 44,014.32 | 41,723.79 | 43,979.45 | 47,178.79 | 47,454.47 | PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 32 MB | 1 | ok | 44,024.76 | 41,578.66 | 43,942.47 | 47,467.94 | 47,558.71 | PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 32 MB | 2 | ok | 43,976.30 | 39,455.05 | 43,901.98 | 49,682.03 | 49,731.38 | PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 32 MB | 0 | ok | 38,798.45 | 38,130.64 | 38,499.29 | 43,351.75 | 43,790.21 | PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 32 MB | 1 | ok | 38,802.54 | 38,193.10 | 38,517.48 | 43,620.73 | 44,248.13 | PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 32 MB | 2 | ok | 38,758.54 | 36,501.05 | 38,604.84 | 43,660.51 | 45,941.14 | PASSED | 0 |
| pass2 | Two-shot LL | 3 | 32 MB | 0 | ok | 31,747.57 | 31,224.30 | 31,617.21 | 34,329.12 | 35,055.93 | PASSED | 0 |
| pass2 | Two-shot LL | 3 | 32 MB | 1 | ok | 31,750.28 | 30,980.58 | 31,657.39 | 34,818.29 | 37,553.74 | PASSED | 0 |
| pass2 | Two-shot LL | 3 | 32 MB | 2 | ok | 31,722.71 | 30,164.26 | 31,643.66 | 36,182.47 | 36,535.70 | PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 32 MB | 0 | ok | 38,330.70 | 38,032.43 | 38,236.89 | 40,657.21 | 40,977.84 | PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 32 MB | 1 | ok | 38,326.25 | 37,940.44 | 38,232.12 | 40,432.92 | 40,496.56 | PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 32 MB | 2 | ok | 38,314.56 | 35,369.06 | 38,237.81 | 41,837.94 | 42,415.75 | PASSED | 0 |
| pass2 | LL128 Atomic | 3 | 32 MB | 0 | skipped | - | - | - | - | - | - | 2 |
| pass2 | LL128 Atomic | 3 | 32 MB | 1 | skipped | - | - | - | - | - | - | 2 |
| pass2 | LL128 Atomic | 3 | 32 MB | 2 | skipped | - | - | - | - | - | - | 2 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 MB | 0 | ok | 19,390.35 | 19,085.65 | 19,387.77 | 19,685.63 | 20,392.15 | PASSED | 0 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 MB | 1 | ok | 19,389.97 | 19,016.48 | 19,383.84 | 19,738.76 | 20,225.65 | PASSED | 0 |
| pass2 | Baseline (with barrier) | 2 | 32 B | 0 | ok | 4.17 | 3.10 | 4.37 | 4.59 | 13.93 | PASSED | 0 |
| pass2 | Baseline (with barrier) | 2 | 32 B | 1 | ok | 4.17 | 2.27 | 4.38 | 4.58 | 13.68 | PASSED | 0 |
| pass2 | LL (one-shot) | 2 | 32 B | 0 | ok | 1.81 | 1.68 | 1.77 | 2.06 | 17.17 | PASSED | 0 |
| pass2 | LL (one-shot) | 2 | 32 B | 1 | ok | 1.82 | 1.68 | 1.77 | 2.08 | 17.69 | PASSED | 0 |
| pass2 | Baseline (with barrier) | 2 | 32 KB | 0 | ok | 34.98 | 30.88 | 34.87 | 43.78 | 45.70 | PASSED | 0 |
| pass2 | Baseline (with barrier) | 2 | 32 KB | 1 | ok | 34.98 | 31.70 | 34.89 | 46.08 | 53.24 | PASSED | 0 |
| pass2 | LL (one-shot) | 2 | 32 KB | 0 | ok | 30.66 | 17.30 | 34.67 | 43.31 | 54.38 | PASSED | 0 |
| pass2 | LL (one-shot) | 2 | 32 KB | 1 | ok | 30.67 | 19.14 | 34.03 | 42.88 | 55.12 | PASSED | 0 |
| pass2 | Baseline (with barrier) | 2 | 32 MB | 0 | ok | 29,182.78 | 26,303.79 | 29,210.78 | 32,033.49 | 32,394.56 | PASSED | 0 |
| pass2 | Baseline (with barrier) | 2 | 32 MB | 1 | ok | 29,156.28 | 28,077.15 | 29,008.19 | 31,002.72 | 31,386.94 | PASSED | 0 |
| pass2 | LL (one-shot) | 2 | 32 MB | 0 | ok | 27,742.79 | 19,204.92 | 30,430.56 | 36,475.02 | 36,497.76 | PASSED | 0 |
| pass2 | LL (one-shot) | 2 | 32 MB | 1 | ok | 27,761.12 | 19,842.37 | 31,278.94 | 35,836.69 | 36,500.43 | PASSED | 0 |
| pass2 | Atomic_Probe | 2 | 32 B | 0 | no result | - | - | - | - | - | - | 0 |
| pass2 | Atomic_Probe | 2 | 32 B | 1 | no result | - | - | - | - | - | - | 0 |

Logs and what each ran:

- `pass1_1_3ranks_32B_64B_128B_32KB.txt`: 3 ranks, 32 B / 64 B / 128 B / 32 KB, 1000 rounds
- `pass1_2_3ranks_256KB.txt`: 3 ranks, 256 KB, 100 rounds
- `pass1_3_3ranks_1MB.txt`: 3 ranks, 1 MB, 100 rounds
- `pass1_4_3ranks_32MB.txt`: 3 ranks, 32 MB, 100 rounds
- `pass1_5_2ranks.txt`: 2 ranks, Baseline and LL (ring for the baselines), 32 B / 32 KB (1000 rounds) and 32 MB (100 rounds)
- `pass1_6_atomic_probe.txt`: Atomic_Probe: does the network card support RDMA atomics (needed by LL128)
- `pass2_1_3ranks_32B_64B_128B_32KB.txt`: 3 ranks, 32 B / 64 B / 128 B / 32 KB, 1000 rounds
- `pass2_2_3ranks_256KB.txt`: 3 ranks, 256 KB, 100 rounds
- `pass2_3_3ranks_1MB.txt`: 3 ranks, 1 MB, 100 rounds
- `pass2_4_3ranks_32MB.txt`: 3 ranks, 32 MB, 100 rounds
- `pass2_5_2ranks.txt`: 2 ranks, Baseline and LL (ring for the baselines), 32 B / 32 KB (1000 rounds) and 32 MB (100 rounds)
- `pass2_6_atomic_probe.txt`: Atomic_Probe: does the network card support RDMA atomics (needed by LL128)

