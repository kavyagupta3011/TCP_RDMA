# Results: v4 (paper_algorithms_v4)

**v4**: v3 + -O3 and blocked add with copy-out for Two-shot.

Machines: rank 0 = compute02, rank 1 = compute03, rank 2 = compute04. PBS jobs: 30219.master.local, 30220.master.local.

A value is the time of one AllReduce (median over the timed rounds, averaged over the ranks that
finished correctly). Cells with two numbers are **pass 1 / pass 2**: the same experiment run twice,
the second time with the versions in the opposite order. If a rank crashed, hung (stopped by the
timeout) or got a wrong result, the cell says so. 1000 timed rounds for sizes up to 32 KB, 100
for 256 KB and above.

## 1. Median, 3 ranks (pass 1 / pass 2)

| Algorithm | 32 B | 64 B | 128 B | 32 KB | 256 KB | 1 MB | 32 MB |
|---|---|---|---|---|---|---|---|
| Baseline (with barrier) | 8.49 µs / 8.74 µs | 8.77 µs / 8.76 µs | 8.63 µs / 8.90 µs | 44.65 µs / 44.70 µs | 237.8 µs / 238.0 µs | 912.2 µs / 911.3 µs | 36.55 ms / 36.53 ms |
| LL (one-shot) | 2.17 µs / 2.17 µs | 2.17 µs / 2.18 µs | 2.24 µs / 2.24 µs | 33.83 µs / 33.76 µs | 230.1 µs / 230.0 µs | 901.1 µs / 899.9 µs | 36.53 ms / 36.46 ms |
| Sentinel (one-shot) | 1.86 µs / 1.86 µs | 1.86 µs / 1.87 µs | 1.93 µs / 1.93 µs | 42.48 µs / 42.73 µs | 289.4 µs / 285.4 µs | 1115.6 µs / 1123.8 µs | 38.45 ms / 38.51 ms |
| Two-shot LL | 4.31 µs / 4.30 µs | 4.25 µs / 4.28 µs | 4.32 µs / 4.32 µs | 26.31 µs / 26.28 µs | 168.6 µs / 167.5 µs | 658.4 µs / 657.3 µs | 25.74 ms / 25.65 ms |
| Two-shot Sentinel | 3.51 µs / 3.54 µs | 3.59 µs / 3.58 µs | 3.72 µs / 3.70 µs | 32.81 µs / 32.82 µs | 202.8 µs / 202.3 µs | 783.7 µs / 784.2 µs | 28.60 ms / 28.61 ms |
| LL128 Atomic | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) |
| Bidirectional (2 ranks, 8 chunks per round) | 13.58 µs / 13.75 µs | 13.77 µs / 13.74 µs | 13.71 µs / 13.39 µs | 37.95 µs / 37.95 µs | 171.1 µs / 170.9 µs | 541.0 µs / 542.4 µs | 19.47 ms / 19.39 ms |

## 2. p99, 3 ranks (average of the passes)

99% of the rounds were faster than this.

| Algorithm | 32 B | 64 B | 128 B | 32 KB | 256 KB | 1 MB | 32 MB |
|---|---|---|---|---|---|---|---|
| Baseline (with barrier) | 9.78 µs | 9.46 µs | 9.34 µs | 47.75 µs | 246.4 µs | 922.0 µs | 39.71 ms |
| LL (one-shot) | 2.38 µs | 2.50 µs | 2.76 µs | 38.07 µs | 239.4 µs | 920.0 µs | 41.73 ms |
| Sentinel (one-shot) | 2.31 µs | 2.02 µs | 2.64 µs | 48.54 µs | 303.8 µs | 1141.2 µs | 42.91 ms |
| Two-shot LL | 5.82 µs | 5.73 µs | 5.30 µs | 27.39 µs | 173.5 µs | 668.1 µs | 28.64 ms |
| Two-shot Sentinel | 4.63 µs | 4.65 µs | 5.16 µs | 37.54 µs | 212.0 µs | 801.1 µs | 32.43 ms |
| LL128 Atomic | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) |
| Bidirectional (2 ranks, 8 chunks per round) | 15.46 µs | 15.23 µs | 15.97 µs | 39.54 µs | 176.1 µs | 546.8 µs | 19.87 ms |

## 3. Cost of the barrier: Baseline vs LL, 3 ranks

|  | 32 B | 64 B | 128 B | 32 KB | 256 KB | 1 MB | 32 MB |
|---|---|---|---|---|---|---|---|
| Baseline - LL | 6.45 µs | 6.59 µs | 6.52 µs | 10.88 µs | 7.83 µs | 11.26 µs | 45.09 µs |
| Baseline / LL | 3.97x | 4.03x | 3.91x | 1.32x | 1.03x | 1.01x | 1.00x |

## 4. 2 ranks vs 3 ranks

|  | 32 B | 32 KB | 32 MB |
|---|---|---|---|
| Baseline (with barrier), 2 ranks | 4.41 µs | 22.71 µs | 23.01 ms |
| Baseline (with barrier), 3 ranks | 8.61 µs | 44.67 µs | 36.54 ms |
| LL (one-shot), 2 ranks | 1.79 µs | 20.33 µs | 23.13 ms |
| LL (one-shot), 3 ranks | 2.17 µs | 33.80 µs | 36.49 ms |

## 5. Atomics probe and negative test

Atomic_Probe (`logs/pass1_6_atomic_probe.txt`):

```
PROBE device atomic_cap=0 (0=NONE 1=HCA 2=GLOB) max_qp_rd_atom=16 max_qp_init_rd_atom=16
PROBE QP state=3 (3=RTS) access_flags=0xe (0x8=REMOTE_ATOMIC) max_rd_atomic=1 max_dest_rd_atomic=1 max_inline=188 max_send_sge=12
PROBE rank 0 -> peer 1, remote counter addr=20e7dc0 rkey=827472
PROBE 1 FETCH_AND_ADD, library result buf              post_send rc=22 (Invalid argument)
PROBE 2 FETCH_AND_ADD, fresh 64B-aligned result buf    post_send rc=22 (Invalid argument)
PROBE 3 CMP_AND_SWP                                    post_send rc=22 (Invalid argument)
PROBE 4 FETCH_AND_ADD after unsignaled inline WRITE    post_send rc=22 (Invalid argument)
PROBE remote counter should now equal the number of OK atomics above; reading it needs the peer (see its line)
PROBE device atomic_cap=0 (0=NONE 1=HCA 2=GLOB) max_qp_rd_atom=16 max_qp_init_rd_atom=16
PROBE QP state=3 (3=RTS) access_flags=0xe (0x8=REMOTE_ATOMIC) max_rd_atomic=1 max_dest_rd_atomic=1 max_inline=188 max_send_sge=12
PROBE rank 1 (counter host) final counter value = 0
```

LL without the wait (negative test) at 32 B (`logs/pass1_7_negative_test.txt`): 6 FAILED and 0 PASSED correctness lines. It must fail: this shows the check notices a missing wait.

LL without the wait (negative test) at 32 KB (`logs/pass1_7_negative_test.txt`): 6 FAILED and 0 PASSED correctness lines. It must fail: this shows the check notices a missing wait.

Atomic_Probe (`logs/pass2_6_atomic_probe.txt`):

```
PROBE device atomic_cap=0 (0=NONE 1=HCA 2=GLOB) max_qp_rd_atom=16 max_qp_init_rd_atom=16
PROBE QP state=3 (3=RTS) access_flags=0xe (0x8=REMOTE_ATOMIC) max_rd_atomic=1 max_dest_rd_atomic=1 max_inline=188 max_send_sge=12
PROBE rank 0 -> peer 1, remote counter addr=b72dc0 rkey=720160
PROBE 1 FETCH_AND_ADD, library result buf              post_send rc=22 (Invalid argument)
PROBE 2 FETCH_AND_ADD, fresh 64B-aligned result buf    post_send rc=22 (Invalid argument)
PROBE 3 CMP_AND_SWP                                    post_send rc=22 (Invalid argument)
PROBE 4 FETCH_AND_ADD after unsignaled inline WRITE    post_send rc=22 (Invalid argument)
PROBE remote counter should now equal the number of OK atomics above; reading it needs the peer (see its line)
PROBE device atomic_cap=0 (0=NONE 1=HCA 2=GLOB) max_qp_rd_atom=16 max_qp_init_rd_atom=16
PROBE QP state=3 (3=RTS) access_flags=0xe (0x8=REMOTE_ATOMIC) max_rd_atomic=1 max_dest_rd_atomic=1 max_inline=188 max_send_sge=12
PROBE rank 1 (counter host) final counter value = 0
```

LL without the wait (negative test) at 32 B (`logs/pass2_7_negative_test.txt`): 5 FAILED and 1 PASSED correctness lines. It must fail: this shows the check notices a missing wait.

LL without the wait (negative test) at 32 KB (`logs/pass2_7_negative_test.txt`): 6 FAILED and 0 PASSED correctness lines. It must fail: this shows the check notices a missing wait.

## 6. Correctness and errors

| Log | PASSED lines | FAILED lines | ranks ok | crashed | hung | wrong result | skipped |
|---|---|---|---|---|---|---|---|
| `pass1_1_3ranks_32B_64B_128B_32KB.txt` | 128 | 0 | 68 | 0 | 0 | 0 | 12 |
| `pass1_2_3ranks_256KB.txt` | 32 | 0 | 17 | 0 | 0 | 0 | 3 |
| `pass1_3_3ranks_1MB.txt` | 32 | 0 | 17 | 0 | 0 | 0 | 3 |
| `pass1_4_3ranks_32MB.txt` | 32 | 0 | 17 | 0 | 0 | 0 | 3 |
| `pass1_5_2ranks.txt` | 24 | 0 | 12 | 0 | 0 | 0 | 0 |
| `pass1_6_atomic_probe.txt` | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| `pass1_7_negative_test.txt` | 0 | 12 | 0 | 0 | 0 | 6 | 0 |
| `pass2_1_3ranks_32B_64B_128B_32KB.txt` | 128 | 0 | 68 | 0 | 0 | 0 | 12 |
| `pass2_2_3ranks_256KB.txt` | 32 | 0 | 17 | 0 | 0 | 0 | 3 |
| `pass2_3_3ranks_1MB.txt` | 32 | 0 | 17 | 0 | 0 | 0 | 3 |
| `pass2_4_3ranks_32MB.txt` | 32 | 0 | 17 | 0 | 0 | 0 | 3 |
| `pass2_5_2ranks.txt` | 24 | 0 | 12 | 0 | 0 | 0 | 0 |
| `pass2_6_atomic_probe.txt` | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| `pass2_7_negative_test.txt` | 1 | 11 | 0 | 0 | 0 | 6 | 0 |

Error messages (first one per rank):

- pass1 LL without the wait (negative test), 32 B, rank 0: `verify: 8 / 8 elements mismatched`
- pass1 LL without the wait (negative test), 32 B, rank 1: `verify: 8 / 8 elements mismatched`
- pass1 LL without the wait (negative test), 32 B, rank 2: `verify: 8 / 8 elements mismatched`
- pass1 LL without the wait (negative test), 32 KB, rank 0: `verify: 2271 / 8192 elements mismatched`
- pass1 LL without the wait (negative test), 32 KB, rank 1: `verify: 3314 / 8192 elements mismatched`
- pass1 LL without the wait (negative test), 32 KB, rank 2: `verify: 2867 / 8192 elements mismatched`
- pass2 LL without the wait (negative test), 32 B, rank 0: `verify: 8 / 8 elements mismatched`
- pass2 LL without the wait (negative test), 32 B, rank 1: `verify: 8 / 8 elements mismatched`
- pass2 LL without the wait (negative test), 32 KB, rank 0: `verify: 2371 / 8192 elements mismatched`
- pass2 LL without the wait (negative test), 32 KB, rank 1: `verify: 3688 / 8192 elements mismatched`
- pass2 LL without the wait (negative test), 32 KB, rank 2: `verify: 3484 / 8192 elements mismatched`

## 7. Every run, per rank

Times in µs. `-` = not printed by this version or the rank did not finish.

| pass | Program | ranks | size | rank | status | mean | min | median | p99 | max | correctness | exit |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| pass1 | Baseline (with barrier) | 3 | 32 B | 0 | ok | 8.05 | 7.38 | 8.27 | 8.86 | 14.01 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 32 B | 1 | ok | 8.05 | 6.66 | 9.12 | 9.73 | 13.04 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 32 B | 2 | ok | 8.05 | 6.68 | 8.07 | 9.55 | 12.94 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 32 B | 0 | ok | 2.20 | 2.09 | 2.16 | 2.37 | 8.29 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 32 B | 1 | ok | 2.20 | 2.11 | 2.17 | 2.38 | 8.41 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 32 B | 2 | ok | 2.20 | 2.11 | 2.17 | 2.38 | 8.44 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 32 B | 0 | ok | 1.92 | 1.81 | 1.86 | 2.77 | 17.94 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 32 B | 1 | ok | 1.91 | 1.81 | 1.86 | 2.63 | 17.69 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 32 B | 2 | ok | 1.91 | 1.80 | 1.86 | 2.40 | 17.44 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 32 B | 0 | ok | 4.35 | 4.13 | 4.30 | 5.85 | 9.12 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 32 B | 1 | ok | 4.35 | 4.13 | 4.30 | 5.20 | 9.18 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 32 B | 2 | ok | 4.35 | 4.17 | 4.31 | 5.82 | 9.71 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 32 B | 0 | ok | 3.58 | 3.44 | 3.52 | 4.69 | 22.60 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 32 B | 1 | ok | 3.58 | 3.44 | 3.51 | 4.79 | 22.64 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 32 B | 2 | ok | 3.56 | 3.42 | 3.51 | 4.21 | 8.56 | PASSED/PASSED | 0 |
| pass1 | LL128 Atomic | 3 | 32 B | 0 | skipped | - | - | - | - | - | - | 2 |
| pass1 | LL128 Atomic | 3 | 32 B | 1 | skipped | - | - | - | - | - | - | 2 |
| pass1 | LL128 Atomic | 3 | 32 B | 2 | skipped | - | - | - | - | - | - | 2 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 B | 0 | ok | 13.69 | 13.42 | 13.58 | 15.34 | 23.04 | PASSED | 0 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 B | 1 | ok | 13.69 | 13.41 | 13.59 | 15.23 | 23.24 | PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 64 B | 0 | ok | 8.04 | 6.04 | 8.24 | 9.35 | 16.70 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 64 B | 1 | ok | 8.04 | 6.69 | 9.06 | 9.60 | 14.98 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 64 B | 2 | ok | 8.03 | 6.40 | 9.02 | 10.17 | 14.10 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 64 B | 0 | ok | 2.21 | 2.10 | 2.18 | 2.42 | 12.04 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 64 B | 1 | ok | 2.21 | 2.10 | 2.17 | 2.46 | 12.46 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 64 B | 2 | ok | 2.21 | 2.11 | 2.17 | 2.44 | 11.82 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 64 B | 0 | ok | 1.90 | 1.80 | 1.86 | 2.00 | 12.46 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 64 B | 1 | ok | 1.89 | 1.80 | 1.86 | 2.02 | 12.90 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 64 B | 2 | ok | 1.89 | 1.80 | 1.86 | 1.99 | 12.48 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 64 B | 0 | ok | 4.31 | 4.13 | 4.26 | 5.70 | 15.95 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 64 B | 1 | ok | 4.31 | 4.11 | 4.25 | 5.93 | 16.82 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 64 B | 2 | ok | 4.31 | 4.15 | 4.25 | 5.96 | 16.85 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 64 B | 0 | ok | 3.63 | 3.50 | 3.59 | 4.33 | 12.50 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 64 B | 1 | ok | 3.64 | 3.51 | 3.59 | 4.88 | 12.91 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 64 B | 2 | ok | 3.63 | 3.47 | 3.59 | 4.40 | 12.59 | PASSED/PASSED | 0 |
| pass1 | LL128 Atomic | 3 | 64 B | 0 | skipped | - | - | - | - | - | - | 2 |
| pass1 | LL128 Atomic | 3 | 64 B | 1 | skipped | - | - | - | - | - | - | 2 |
| pass1 | LL128 Atomic | 3 | 64 B | 2 | skipped | - | - | - | - | - | - | 2 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 64 B | 0 | ok | 13.83 | 13.47 | 13.76 | 15.11 | 24.69 | PASSED | 0 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 64 B | 1 | ok | 13.83 | 13.37 | 13.77 | 15.10 | 23.91 | PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 128 B | 0 | ok | 8.07 | 7.43 | 8.28 | 9.05 | 17.73 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 128 B | 1 | ok | 8.09 | 6.74 | 9.15 | 9.49 | 16.35 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 128 B | 2 | ok | 8.09 | 6.20 | 8.45 | 9.50 | 18.74 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 128 B | 0 | ok | 2.27 | 2.14 | 2.25 | 2.76 | 7.94 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 128 B | 1 | ok | 2.27 | 2.11 | 2.24 | 2.62 | 8.91 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 128 B | 2 | ok | 2.27 | 2.13 | 2.23 | 2.54 | 8.91 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 128 B | 0 | ok | 1.97 | 1.88 | 1.93 | 2.15 | 12.90 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 128 B | 1 | ok | 1.97 | 1.86 | 1.93 | 2.44 | 13.05 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 128 B | 2 | ok | 1.97 | 1.86 | 1.93 | 2.27 | 12.93 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 128 B | 0 | ok | 4.36 | 4.23 | 4.32 | 4.85 | 8.89 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 128 B | 1 | ok | 4.36 | 4.15 | 4.33 | 5.31 | 9.89 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 128 B | 2 | ok | 4.36 | 4.19 | 4.31 | 5.25 | 9.40 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 128 B | 0 | ok | 3.76 | 3.62 | 3.72 | 4.91 | 13.60 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 128 B | 1 | ok | 3.76 | 3.61 | 3.71 | 5.01 | 14.06 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 128 B | 2 | ok | 3.76 | 3.60 | 3.71 | 4.91 | 13.97 | PASSED/PASSED | 0 |
| pass1 | LL128 Atomic | 3 | 128 B | 0 | skipped | - | - | - | - | - | - | 2 |
| pass1 | LL128 Atomic | 3 | 128 B | 1 | skipped | - | - | - | - | - | - | 2 |
| pass1 | LL128 Atomic | 3 | 128 B | 2 | skipped | - | - | - | - | - | - | 2 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 128 B | 0 | ok | 13.80 | 13.46 | 13.70 | 15.15 | 31.99 | PASSED | 0 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 128 B | 1 | ok | 13.81 | 13.33 | 13.73 | 15.25 | 32.10 | PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 32 KB | 0 | ok | 44.31 | 37.20 | 44.69 | 47.24 | 56.25 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 32 KB | 1 | ok | 44.31 | 37.83 | 44.73 | 48.29 | 58.35 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 32 KB | 2 | ok | 44.30 | 36.00 | 44.52 | 48.25 | 62.39 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 32 KB | 0 | ok | 34.02 | 27.31 | 33.83 | 37.21 | 52.34 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 32 KB | 1 | ok | 34.00 | 26.92 | 33.83 | 37.77 | 51.48 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 32 KB | 2 | ok | 34.01 | 21.33 | 33.83 | 37.25 | 50.97 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 32 KB | 0 | ok | 42.51 | 36.17 | 42.48 | 48.31 | 61.17 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 32 KB | 1 | ok | 42.51 | 34.96 | 42.51 | 47.71 | 65.21 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 32 KB | 2 | ok | 42.50 | 34.43 | 42.45 | 48.11 | 63.90 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 32 KB | 0 | ok | 26.27 | 22.67 | 26.21 | 27.58 | 37.23 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 32 KB | 1 | ok | 26.29 | 23.65 | 26.23 | 27.54 | 38.34 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 32 KB | 2 | ok | 26.29 | 24.04 | 26.48 | 27.84 | 37.17 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 32 KB | 0 | ok | 33.17 | 30.32 | 32.88 | 38.99 | 51.35 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 32 KB | 1 | ok | 33.18 | 30.19 | 32.80 | 37.41 | 49.35 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 32 KB | 2 | ok | 33.19 | 31.34 | 32.75 | 37.54 | 50.83 | PASSED/PASSED | 0 |
| pass1 | LL128 Atomic | 3 | 32 KB | 0 | skipped | - | - | - | - | - | - | 2 |
| pass1 | LL128 Atomic | 3 | 32 KB | 1 | skipped | - | - | - | - | - | - | 2 |
| pass1 | LL128 Atomic | 3 | 32 KB | 2 | skipped | - | - | - | - | - | - | 2 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 KB | 0 | ok | 38.01 | 37.61 | 37.94 | 39.56 | 44.18 | PASSED | 0 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 KB | 1 | ok | 38.02 | 37.63 | 37.95 | 39.52 | 47.15 | PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 256 KB | 0 | ok | 237.95 | 233.73 | 237.94 | 243.36 | 246.80 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 256 KB | 1 | ok | 238.35 | 234.03 | 237.68 | 244.72 | 277.59 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 256 KB | 2 | ok | 238.27 | 229.86 | 237.71 | 255.06 | 268.35 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 256 KB | 0 | ok | 230.17 | 223.74 | 230.25 | 236.21 | 241.66 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 256 KB | 1 | ok | 230.93 | 225.11 | 230.29 | 242.40 | 301.97 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 256 KB | 2 | ok | 230.20 | 223.84 | 229.83 | 237.96 | 239.35 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 256 KB | 0 | ok | 288.92 | 280.15 | 289.21 | 303.52 | 309.58 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 256 KB | 1 | ok | 288.85 | 272.90 | 290.69 | 309.95 | 323.59 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 256 KB | 2 | ok | 288.94 | 279.33 | 288.39 | 307.03 | 321.21 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 256 KB | 0 | ok | 168.84 | 165.85 | 168.48 | 175.09 | 175.80 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 256 KB | 1 | ok | 168.85 | 160.08 | 168.71 | 176.19 | 185.12 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 256 KB | 2 | ok | 168.92 | 164.79 | 168.60 | 173.92 | 176.08 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 256 KB | 0 | ok | 203.21 | 198.76 | 202.66 | 210.22 | 211.72 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 256 KB | 1 | ok | 203.25 | 196.97 | 203.15 | 210.67 | 217.00 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 256 KB | 2 | ok | 203.41 | 198.86 | 202.60 | 214.68 | 221.96 | PASSED/PASSED | 0 |
| pass1 | LL128 Atomic | 3 | 256 KB | 0 | skipped | - | - | - | - | - | - | 2 |
| pass1 | LL128 Atomic | 3 | 256 KB | 1 | skipped | - | - | - | - | - | - | 2 |
| pass1 | LL128 Atomic | 3 | 256 KB | 2 | skipped | - | - | - | - | - | - | 2 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 256 KB | 0 | ok | 171.32 | 168.77 | 171.07 | 177.45 | 178.68 | PASSED | 0 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 256 KB | 1 | ok | 171.40 | 169.04 | 171.08 | 177.47 | 186.27 | PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 1 MB | 0 | ok | 912.20 | 900.35 | 912.55 | 920.47 | 923.19 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 1 MB | 1 | ok | 913.66 | 905.84 | 912.01 | 922.95 | 1,044.05 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 1 MB | 2 | ok | 913.57 | 897.75 | 912.06 | 923.71 | 1,058.05 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 1 MB | 0 | ok | 900.87 | 858.30 | 901.20 | 920.76 | 933.05 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 1 MB | 1 | ok | 901.18 | 883.15 | 900.70 | 918.46 | 928.05 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 1 MB | 2 | ok | 901.38 | 874.76 | 901.28 | 922.52 | 959.59 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 1 MB | 0 | ok | 1,112.87 | 1,097.30 | 1,115.95 | 1,132.00 | 1,137.48 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 1 MB | 1 | ok | 1,113.77 | 1,099.97 | 1,115.70 | 1,136.95 | 1,196.95 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 1 MB | 2 | ok | 1,114.14 | 1,086.81 | 1,115.01 | 1,138.82 | 1,250.46 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 1 MB | 0 | ok | 657.56 | 646.84 | 658.74 | 663.82 | 667.26 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 1 MB | 1 | ok | 657.49 | 640.52 | 658.13 | 670.87 | 677.85 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 1 MB | 2 | ok | 658.10 | 640.45 | 658.21 | 670.11 | 709.42 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 1 MB | 0 | ok | 783.47 | 775.12 | 783.33 | 791.51 | 795.35 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 1 MB | 1 | ok | 783.37 | 761.05 | 783.65 | 796.58 | 798.22 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 1 MB | 2 | ok | 784.21 | 762.82 | 784.23 | 799.93 | 850.63 | PASSED/PASSED | 0 |
| pass1 | LL128 Atomic | 3 | 1 MB | 0 | skipped | - | - | - | - | - | - | 2 |
| pass1 | LL128 Atomic | 3 | 1 MB | 1 | skipped | - | - | - | - | - | - | 2 |
| pass1 | LL128 Atomic | 3 | 1 MB | 2 | skipped | - | - | - | - | - | - | 2 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 1 MB | 0 | ok | 541.21 | 538.25 | 540.97 | 546.88 | 547.31 | PASSED | 0 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 1 MB | 1 | ok | 541.33 | 538.06 | 541.01 | 547.81 | 552.92 | PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 32 MB | 0 | ok | 36,651.82 | 36,206.71 | 36,559.02 | 38,168.46 | 38,390.34 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 32 MB | 1 | ok | 36,642.73 | 36,148.70 | 36,545.19 | 38,179.75 | 38,312.68 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 32 MB | 2 | ok | 36,605.31 | 32,102.26 | 36,541.97 | 43,041.69 | 43,402.22 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 32 MB | 0 | ok | 36,511.93 | 31,854.07 | 36,528.03 | 41,517.89 | 41,644.55 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 32 MB | 1 | ok | 36,509.18 | 31,545.13 | 36,613.86 | 42,012.38 | 42,020.21 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 32 MB | 2 | ok | 36,467.43 | 31,714.04 | 36,446.36 | 42,095.88 | 42,330.27 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 32 MB | 0 | ok | 38,692.01 | 38,099.75 | 38,437.68 | 42,619.59 | 43,565.87 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 32 MB | 1 | ok | 38,697.00 | 38,044.45 | 38,445.21 | 42,761.76 | 43,991.04 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 32 MB | 2 | ok | 38,653.24 | 36,247.58 | 38,462.78 | 43,452.88 | 45,784.90 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 32 MB | 0 | ok | 25,872.17 | 25,284.68 | 25,794.99 | 27,859.64 | 29,731.55 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 32 MB | 1 | ok | 25,840.21 | 25,223.77 | 25,682.57 | 29,677.10 | 30,135.50 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 32 MB | 2 | ok | 25,826.69 | 24,622.57 | 25,747.41 | 29,216.37 | 32,174.69 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 32 MB | 0 | ok | 28,753.48 | 28,234.26 | 28,630.73 | 32,701.22 | 33,210.62 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 32 MB | 1 | ok | 28,751.27 | 28,326.34 | 28,599.61 | 32,036.17 | 33,723.74 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 32 MB | 2 | ok | 28,743.50 | 27,869.14 | 28,579.55 | 33,632.88 | 34,830.80 | PASSED/PASSED | 0 |
| pass1 | LL128 Atomic | 3 | 32 MB | 0 | skipped | - | - | - | - | - | - | 2 |
| pass1 | LL128 Atomic | 3 | 32 MB | 1 | skipped | - | - | - | - | - | - | 2 |
| pass1 | LL128 Atomic | 3 | 32 MB | 2 | skipped | - | - | - | - | - | - | 2 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 MB | 0 | ok | 19,471.96 | 19,172.88 | 19,476.13 | 20,112.79 | 20,431.65 | PASSED | 0 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 MB | 1 | ok | 19,465.74 | 19,233.45 | 19,464.46 | 19,855.28 | 20,204.08 | PASSED | 0 |
| pass1 | Baseline (with barrier) | 2 | 32 B | 0 | ok | 4.14 | 2.40 | 4.40 | 4.61 | 26.28 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 2 | 32 B | 1 | ok | 4.12 | 2.38 | 4.42 | 4.65 | 14.67 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 2 | 32 B | 0 | ok | 1.88 | 1.73 | 1.81 | 2.15 | 22.20 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 2 | 32 B | 1 | ok | 1.88 | 1.72 | 1.81 | 2.06 | 22.24 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 2 | 32 KB | 0 | ok | 22.58 | 21.72 | 22.56 | 24.61 | 28.86 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 2 | 32 KB | 1 | ok | 22.56 | 21.93 | 22.80 | 24.09 | 28.57 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 2 | 32 KB | 0 | ok | 19.46 | 12.65 | 19.38 | 26.36 | 27.98 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 2 | 32 KB | 1 | ok | 19.47 | 12.85 | 19.36 | 26.44 | 28.35 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 2 | 32 MB | 0 | ok | 23,119.91 | 22,606.39 | 23,096.80 | 23,661.58 | 24,115.83 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 2 | 32 MB | 1 | ok | 23,117.97 | 22,487.88 | 23,082.76 | 23,934.65 | 24,068.51 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 2 | 32 MB | 0 | ok | 22,870.35 | 13,446.04 | 23,415.94 | 32,556.56 | 33,047.03 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 2 | 32 MB | 1 | ok | 22,785.37 | 13,343.01 | 23,222.53 | 32,726.51 | 32,813.06 | PASSED/PASSED | 0 |
| pass1 | Atomic_Probe | 2 | 32 B | 0 | no result | - | - | - | - | - | - | 0 |
| pass1 | Atomic_Probe | 2 | 32 B | 1 | no result | - | - | - | - | - | - | 0 |
| pass1 | LL without the wait (negative test) | 3 | 32 B | 0 | wrong result | 2.20 | 2.13 | 2.19 | 2.30 | 6.13 | FAILED/FAILED | 1 |
| pass1 | LL without the wait (negative test) | 3 | 32 B | 1 | wrong result | 2.21 | 2.14 | 2.20 | 2.35 | 6.41 | FAILED/FAILED | 1 |
| pass1 | LL without the wait (negative test) | 3 | 32 B | 2 | wrong result | 2.22 | 2.13 | 2.20 | 2.36 | 5.93 | FAILED/FAILED | 1 |
| pass1 | LL without the wait (negative test) | 3 | 32 KB | 0 | wrong result | 26.42 | 23.54 | 26.04 | 31.32 | 34.01 | FAILED/FAILED | 1 |
| pass1 | LL without the wait (negative test) | 3 | 32 KB | 1 | wrong result | 30.65 | 23.48 | 28.63 | 38.99 | 42.42 | FAILED/FAILED | 1 |
| pass1 | LL without the wait (negative test) | 3 | 32 KB | 2 | wrong result | 26.70 | 23.46 | 26.02 | 32.23 | 41.01 | FAILED/FAILED | 1 |
| pass2 | Baseline (with barrier) | 3 | 32 B | 0 | ok | 8.04 | 6.72 | 8.29 | 9.80 | 11.90 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 32 B | 1 | ok | 8.04 | 6.68 | 9.09 | 10.36 | 13.65 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 32 B | 2 | ok | 8.04 | 6.52 | 8.84 | 10.39 | 12.78 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 32 B | 0 | ok | 2.21 | 2.10 | 2.17 | 2.39 | 12.38 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 32 B | 1 | ok | 2.21 | 2.12 | 2.17 | 2.37 | 12.56 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 32 B | 2 | ok | 2.21 | 2.10 | 2.17 | 2.37 | 12.13 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 32 B | 0 | ok | 1.91 | 1.81 | 1.86 | 2.06 | 20.48 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 32 B | 1 | ok | 1.89 | 1.81 | 1.86 | 1.98 | 10.68 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 32 B | 2 | ok | 1.91 | 1.80 | 1.86 | 2.04 | 19.50 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 32 B | 0 | ok | 4.35 | 4.17 | 4.30 | 6.11 | 12.52 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 32 B | 1 | ok | 4.35 | 4.14 | 4.31 | 6.13 | 12.87 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 32 B | 2 | ok | 4.35 | 4.17 | 4.30 | 5.82 | 12.14 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 32 B | 0 | ok | 3.58 | 3.44 | 3.54 | 4.40 | 9.93 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 32 B | 1 | ok | 3.58 | 3.46 | 3.54 | 4.96 | 9.92 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 32 B | 2 | ok | 3.58 | 3.38 | 3.55 | 4.75 | 10.00 | PASSED/PASSED | 0 |
| pass2 | LL128 Atomic | 3 | 32 B | 0 | skipped | - | - | - | - | - | - | 2 |
| pass2 | LL128 Atomic | 3 | 32 B | 1 | skipped | - | - | - | - | - | - | 2 |
| pass2 | LL128 Atomic | 3 | 32 B | 2 | skipped | - | - | - | - | - | - | 2 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 B | 0 | ok | 13.85 | 13.42 | 13.75 | 15.51 | 30.98 | PASSED | 0 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 B | 1 | ok | 13.85 | 13.39 | 13.75 | 15.75 | 30.41 | PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 64 B | 0 | ok | 7.97 | 5.98 | 8.23 | 8.69 | 11.86 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 64 B | 1 | ok | 7.97 | 6.64 | 9.04 | 9.46 | 12.35 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 64 B | 2 | ok | 7.97 | 6.66 | 9.02 | 9.46 | 12.41 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 64 B | 0 | ok | 2.23 | 2.13 | 2.19 | 2.61 | 11.02 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 64 B | 1 | ok | 2.22 | 2.10 | 2.17 | 2.45 | 11.29 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 64 B | 2 | ok | 2.23 | 2.09 | 2.18 | 2.66 | 10.68 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 64 B | 0 | ok | 1.91 | 1.81 | 1.87 | 2.03 | 12.66 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 64 B | 1 | ok | 1.91 | 1.81 | 1.87 | 2.08 | 12.73 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 64 B | 2 | ok | 1.91 | 1.79 | 1.87 | 2.02 | 12.64 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 64 B | 0 | ok | 4.32 | 4.18 | 4.28 | 5.44 | 9.16 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 64 B | 1 | ok | 4.32 | 4.09 | 4.28 | 5.63 | 8.67 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 64 B | 2 | ok | 4.32 | 4.13 | 4.27 | 5.71 | 8.90 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 64 B | 0 | ok | 3.62 | 3.51 | 3.59 | 4.42 | 11.28 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 64 B | 1 | ok | 3.62 | 3.50 | 3.58 | 5.09 | 11.84 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 64 B | 2 | ok | 3.62 | 3.47 | 3.58 | 4.76 | 11.65 | PASSED/PASSED | 0 |
| pass2 | LL128 Atomic | 3 | 64 B | 0 | skipped | - | - | - | - | - | - | 2 |
| pass2 | LL128 Atomic | 3 | 64 B | 1 | skipped | - | - | - | - | - | - | 2 |
| pass2 | LL128 Atomic | 3 | 64 B | 2 | skipped | - | - | - | - | - | - | 2 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 64 B | 0 | ok | 13.82 | 13.47 | 13.73 | 15.22 | 29.48 | PASSED | 0 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 64 B | 1 | ok | 13.81 | 13.39 | 13.75 | 15.47 | 29.20 | PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 128 B | 0 | ok | 8.16 | 7.35 | 8.36 | 8.80 | 22.57 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 128 B | 1 | ok | 8.13 | 6.75 | 9.21 | 9.65 | 17.39 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 128 B | 2 | ok | 8.14 | 6.30 | 9.13 | 9.56 | 21.15 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 128 B | 0 | ok | 2.28 | 2.14 | 2.24 | 2.84 | 18.32 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 128 B | 1 | ok | 2.28 | 2.12 | 2.24 | 3.12 | 17.97 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 128 B | 2 | ok | 2.28 | 2.15 | 2.23 | 2.68 | 17.92 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 128 B | 0 | ok | 2.03 | 1.88 | 1.94 | 2.85 | 17.91 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 128 B | 1 | ok | 2.02 | 1.87 | 1.93 | 3.27 | 17.49 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 128 B | 2 | ok | 2.01 | 1.85 | 1.93 | 2.83 | 17.40 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 128 B | 0 | ok | 4.36 | 4.23 | 4.32 | 5.01 | 8.06 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 128 B | 1 | ok | 4.36 | 4.18 | 4.33 | 5.86 | 8.85 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 128 B | 2 | ok | 4.36 | 4.18 | 4.32 | 5.52 | 8.89 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 128 B | 0 | ok | 3.75 | 3.62 | 3.70 | 5.24 | 13.58 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 128 B | 1 | ok | 3.75 | 3.60 | 3.69 | 5.44 | 13.80 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 128 B | 2 | ok | 3.75 | 3.60 | 3.70 | 5.44 | 13.17 | PASSED/PASSED | 0 |
| pass2 | LL128 Atomic | 3 | 128 B | 0 | skipped | - | - | - | - | - | - | 2 |
| pass2 | LL128 Atomic | 3 | 128 B | 1 | skipped | - | - | - | - | - | - | 2 |
| pass2 | LL128 Atomic | 3 | 128 B | 2 | skipped | - | - | - | - | - | - | 2 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 128 B | 0 | ok | 13.57 | 13.25 | 13.40 | 16.54 | 24.21 | PASSED | 0 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 128 B | 1 | ok | 13.57 | 13.23 | 13.39 | 16.93 | 23.98 | PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 32 KB | 0 | ok | 44.28 | 41.58 | 44.72 | 47.11 | 53.86 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 32 KB | 1 | ok | 44.27 | 37.77 | 44.93 | 47.89 | 54.57 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 32 KB | 2 | ok | 44.27 | 36.06 | 44.44 | 47.72 | 61.15 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 32 KB | 0 | ok | 33.98 | 27.29 | 33.75 | 38.16 | 52.96 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 32 KB | 1 | ok | 33.98 | 22.84 | 33.81 | 39.29 | 50.62 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 32 KB | 2 | ok | 33.97 | 21.15 | 33.73 | 38.73 | 49.84 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 32 KB | 0 | ok | 42.73 | 34.40 | 42.84 | 49.33 | 59.78 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 32 KB | 1 | ok | 42.72 | 34.67 | 42.74 | 47.94 | 60.89 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 32 KB | 2 | ok | 42.71 | 34.59 | 42.61 | 49.81 | 61.02 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 32 KB | 0 | ok | 26.27 | 24.39 | 26.22 | 27.11 | 38.70 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 32 KB | 1 | ok | 26.28 | 23.81 | 26.24 | 26.69 | 39.15 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 32 KB | 2 | ok | 26.28 | 24.22 | 26.37 | 27.58 | 41.09 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 32 KB | 0 | ok | 33.19 | 30.49 | 32.92 | 37.65 | 49.54 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 32 KB | 1 | ok | 33.19 | 31.51 | 32.72 | 37.02 | 52.10 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 32 KB | 2 | ok | 33.18 | 30.86 | 32.82 | 36.60 | 49.33 | PASSED/PASSED | 0 |
| pass2 | LL128 Atomic | 3 | 32 KB | 0 | skipped | - | - | - | - | - | - | 2 |
| pass2 | LL128 Atomic | 3 | 32 KB | 1 | skipped | - | - | - | - | - | - | 2 |
| pass2 | LL128 Atomic | 3 | 32 KB | 2 | skipped | - | - | - | - | - | - | 2 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 KB | 0 | ok | 37.99 | 37.63 | 37.95 | 39.50 | 40.26 | PASSED | 0 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 KB | 1 | ok | 38.00 | 37.66 | 37.95 | 39.58 | 47.07 | PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 256 KB | 0 | ok | 238.22 | 233.17 | 238.07 | 246.50 | 247.33 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 256 KB | 1 | ok | 238.09 | 233.97 | 237.63 | 244.72 | 245.69 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 256 KB | 2 | ok | 238.57 | 232.25 | 238.25 | 244.15 | 286.53 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 256 KB | 0 | ok | 230.01 | 216.15 | 230.19 | 239.16 | 243.09 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 256 KB | 1 | ok | 230.59 | 222.04 | 229.94 | 243.88 | 283.41 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 256 KB | 2 | ok | 230.09 | 217.41 | 229.80 | 237.05 | 242.99 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 256 KB | 0 | ok | 285.69 | 272.55 | 285.91 | 299.42 | 303.33 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 256 KB | 1 | ok | 285.73 | 272.44 | 285.13 | 301.51 | 309.15 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 256 KB | 2 | ok | 285.58 | 273.08 | 285.13 | 301.33 | 308.34 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 256 KB | 0 | ok | 168.00 | 165.05 | 167.60 | 172.64 | 191.99 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 256 KB | 1 | ok | 167.95 | 163.63 | 167.36 | 172.04 | 192.10 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 256 KB | 2 | ok | 167.68 | 163.35 | 167.50 | 171.23 | 172.64 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 256 KB | 0 | ok | 203.19 | 198.51 | 202.11 | 212.66 | 213.59 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 256 KB | 1 | ok | 203.16 | 196.13 | 202.68 | 212.90 | 216.37 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 256 KB | 2 | ok | 203.27 | 197.61 | 202.15 | 211.09 | 220.57 | PASSED/PASSED | 0 |
| pass2 | LL128 Atomic | 3 | 256 KB | 0 | skipped | - | - | - | - | - | - | 2 |
| pass2 | LL128 Atomic | 3 | 256 KB | 1 | skipped | - | - | - | - | - | - | 2 |
| pass2 | LL128 Atomic | 3 | 256 KB | 2 | skipped | - | - | - | - | - | - | 2 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 256 KB | 0 | ok | 171.08 | 164.83 | 170.90 | 174.67 | 175.72 | PASSED | 0 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 256 KB | 1 | ok | 171.15 | 169.80 | 170.94 | 174.67 | 175.63 | PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 1 MB | 0 | ok | 911.39 | 901.79 | 911.38 | 916.95 | 918.27 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 1 MB | 1 | ok | 912.75 | 905.96 | 911.25 | 918.05 | 1,044.40 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 1 MB | 2 | ok | 912.76 | 888.18 | 911.19 | 929.96 | 1,064.49 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 1 MB | 0 | ok | 899.31 | 879.80 | 900.00 | 919.67 | 921.81 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 1 MB | 1 | ok | 899.47 | 880.20 | 900.47 | 917.85 | 919.02 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 1 MB | 2 | ok | 900.03 | 881.50 | 899.24 | 920.72 | 968.47 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 1 MB | 0 | ok | 1,117.61 | 1,097.67 | 1,125.81 | 1,151.47 | 1,151.98 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 1 MB | 1 | ok | 1,118.51 | 1,103.49 | 1,120.84 | 1,133.80 | 1,192.75 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 1 MB | 2 | ok | 1,119.18 | 1,091.95 | 1,124.79 | 1,154.45 | 1,251.45 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 1 MB | 0 | ok | 657.34 | 647.29 | 658.24 | 664.09 | 665.87 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 1 MB | 1 | ok | 657.26 | 640.69 | 656.89 | 670.44 | 672.40 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 1 MB | 2 | ok | 657.89 | 647.98 | 656.91 | 669.58 | 711.19 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 1 MB | 0 | ok | 784.68 | 771.78 | 784.76 | 795.48 | 795.65 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 1 MB | 1 | ok | 784.78 | 754.02 | 783.33 | 808.40 | 822.51 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 1 MB | 2 | ok | 785.73 | 758.63 | 784.38 | 814.93 | 904.46 | PASSED/PASSED | 0 |
| pass2 | LL128 Atomic | 3 | 1 MB | 0 | skipped | - | - | - | - | - | - | 2 |
| pass2 | LL128 Atomic | 3 | 1 MB | 1 | skipped | - | - | - | - | - | - | 2 |
| pass2 | LL128 Atomic | 3 | 1 MB | 2 | skipped | - | - | - | - | - | - | 2 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 1 MB | 0 | ok | 542.48 | 538.92 | 542.46 | 545.90 | 546.87 | PASSED | 0 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 1 MB | 1 | ok | 542.29 | 537.61 | 542.34 | 546.47 | 546.68 | PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 32 MB | 0 | ok | 36,638.80 | 36,176.35 | 36,527.44 | 37,897.05 | 38,166.17 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 32 MB | 1 | ok | 36,628.24 | 36,109.31 | 36,538.77 | 38,078.78 | 38,453.69 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 32 MB | 2 | ok | 36,596.58 | 32,087.55 | 36,527.88 | 42,876.43 | 43,377.21 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 32 MB | 0 | ok | 36,516.25 | 31,749.26 | 36,460.44 | 41,116.71 | 42,233.79 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 32 MB | 1 | ok | 36,516.98 | 31,573.56 | 36,480.49 | 41,312.47 | 42,943.22 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 32 MB | 2 | ok | 36,480.74 | 31,758.78 | 36,440.53 | 42,312.61 | 42,372.61 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 32 MB | 0 | ok | 38,730.84 | 38,236.06 | 38,483.26 | 42,274.59 | 42,780.90 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 32 MB | 1 | ok | 38,724.84 | 38,141.12 | 38,508.92 | 42,649.60 | 42,882.83 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 32 MB | 2 | ok | 38,683.04 | 36,243.11 | 38,540.77 | 43,723.94 | 45,800.01 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 32 MB | 0 | ok | 25,768.13 | 25,267.00 | 25,622.84 | 27,790.46 | 32,934.95 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 32 MB | 1 | ok | 25,790.28 | 25,018.61 | 25,650.49 | 28,743.62 | 29,500.93 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 32 MB | 2 | ok | 25,749.91 | 24,414.17 | 25,680.77 | 28,550.12 | 32,029.10 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 32 MB | 0 | ok | 28,766.92 | 28,353.35 | 28,610.40 | 31,398.44 | 33,140.46 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 32 MB | 1 | ok | 28,772.39 | 28,298.59 | 28,598.29 | 32,102.74 | 32,649.55 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 32 MB | 2 | ok | 28,748.79 | 27,835.69 | 28,611.63 | 32,692.87 | 34,033.63 | PASSED/PASSED | 0 |
| pass2 | LL128 Atomic | 3 | 32 MB | 0 | skipped | - | - | - | - | - | - | 2 |
| pass2 | LL128 Atomic | 3 | 32 MB | 1 | skipped | - | - | - | - | - | - | 2 |
| pass2 | LL128 Atomic | 3 | 32 MB | 2 | skipped | - | - | - | - | - | - | 2 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 MB | 0 | ok | 19,386.13 | 19,070.87 | 19,388.12 | 19,641.81 | 20,385.58 | PASSED | 0 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 MB | 1 | ok | 19,386.27 | 19,051.07 | 19,387.23 | 19,875.61 | 20,321.68 | PASSED | 0 |
| pass2 | Baseline (with barrier) | 2 | 32 B | 0 | ok | 4.12 | 2.64 | 4.41 | 4.66 | 14.82 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 2 | 32 B | 1 | ok | 4.14 | 3.09 | 4.40 | 4.61 | 17.16 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 2 | 32 B | 0 | ok | 1.79 | 1.69 | 1.77 | 1.83 | 12.03 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 2 | 32 B | 1 | ok | 1.79 | 1.68 | 1.77 | 1.83 | 11.95 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 2 | 32 KB | 0 | ok | 22.55 | 20.20 | 22.70 | 24.41 | 30.59 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 2 | 32 KB | 1 | ok | 22.56 | 21.94 | 22.78 | 24.46 | 36.36 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 2 | 32 KB | 0 | ok | 19.55 | 12.79 | 21.37 | 25.80 | 28.10 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 2 | 32 KB | 1 | ok | 19.55 | 12.87 | 21.21 | 25.67 | 29.10 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 2 | 32 MB | 0 | ok | 22,929.46 | 22,370.32 | 22,939.41 | 23,843.36 | 24,102.26 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 2 | 32 MB | 1 | ok | 22,936.69 | 22,409.36 | 22,910.61 | 23,870.49 | 23,885.78 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 2 | 32 MB | 0 | ok | 22,715.92 | 13,211.15 | 22,820.75 | 32,220.19 | 33,225.48 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 2 | 32 MB | 1 | ok | 22,806.59 | 13,179.33 | 23,047.69 | 32,849.95 | 33,083.62 | PASSED/PASSED | 0 |
| pass2 | Atomic_Probe | 2 | 32 B | 0 | no result | - | - | - | - | - | - | 0 |
| pass2 | Atomic_Probe | 2 | 32 B | 1 | no result | - | - | - | - | - | - | 0 |
| pass2 | LL without the wait (negative test) | 3 | 32 B | 0 | wrong result | 2.21 | 2.12 | 2.19 | 2.36 | 5.23 | FAILED/FAILED | 1 |
| pass2 | LL without the wait (negative test) | 3 | 32 B | 1 | wrong result | 2.19 | 2.11 | 2.17 | 2.35 | 6.01 | FAILED/FAILED | 1 |
| pass2 | LL without the wait (negative test) | 3 | 32 B | 2 | wrong result | 2.21 | 2.13 | 2.20 | 2.35 | 3.86 | PASSED/FAILED | 0 |
| pass2 | LL without the wait (negative test) | 3 | 32 KB | 0 | wrong result | 26.41 | 23.71 | 26.06 | 31.28 | 36.87 | FAILED/FAILED | 1 |
| pass2 | LL without the wait (negative test) | 3 | 32 KB | 1 | wrong result | 30.10 | 23.37 | 28.23 | 38.84 | 43.30 | FAILED/FAILED | 1 |
| pass2 | LL without the wait (negative test) | 3 | 32 KB | 2 | wrong result | 26.59 | 23.32 | 25.94 | 31.92 | 41.04 | FAILED/FAILED | 1 |

Logs and what each ran:

- `pass1_1_3ranks_32B_64B_128B_32KB.txt`: 3 ranks, 32 B / 64 B / 128 B / 32 KB, 1000 rounds
- `pass1_2_3ranks_256KB.txt`: 3 ranks, 256 KB, 100 rounds
- `pass1_3_3ranks_1MB.txt`: 3 ranks, 1 MB, 100 rounds
- `pass1_4_3ranks_32MB.txt`: 3 ranks, 32 MB, 100 rounds
- `pass1_5_2ranks.txt`: 2 ranks, Baseline and LL (ring for the baselines), 32 B / 32 KB (1000 rounds) and 32 MB (100 rounds)
- `pass1_6_atomic_probe.txt`: Atomic_Probe: does the network card support RDMA atomics (needed by LL128)
- `pass1_7_negative_test.txt`: negative test: LL with the wait removed, must print FAILED
- `pass2_1_3ranks_32B_64B_128B_32KB.txt`: 3 ranks, 32 B / 64 B / 128 B / 32 KB, 1000 rounds
- `pass2_2_3ranks_256KB.txt`: 3 ranks, 256 KB, 100 rounds
- `pass2_3_3ranks_1MB.txt`: 3 ranks, 1 MB, 100 rounds
- `pass2_4_3ranks_32MB.txt`: 3 ranks, 32 MB, 100 rounds
- `pass2_5_2ranks.txt`: 2 ranks, Baseline and LL (ring for the baselines), 32 B / 32 KB (1000 rounds) and 32 MB (100 rounds)
- `pass2_6_atomic_probe.txt`: Atomic_Probe: does the network card support RDMA atomics (needed by LL128)
- `pass2_7_negative_test.txt`: negative test: LL with the wait removed, must print FAILED

