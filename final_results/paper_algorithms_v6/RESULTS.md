# Results: v6 (paper_algorithms_v6)

**v6**: v5 + block pipelining with per-block flags, deeper queues.

Machines: rank 0 = compute02, rank 1 = compute03, rank 2 = compute04. PBS jobs: 30219.master.local, 30220.master.local.

A value is the time of one AllReduce (median over the timed rounds, averaged over the ranks that
finished correctly). Cells with two numbers are **pass 1 / pass 2**: the same experiment run twice,
the second time with the versions in the opposite order. If a rank crashed, hung (stopped by the
timeout) or got a wrong result, the cell says so. 1000 timed rounds for sizes up to 32 KB, 100
for 256 KB and above.

## 1. Median, 3 ranks (pass 1 / pass 2)

| Algorithm | 32 B | 64 B | 128 B | 32 KB | 256 KB | 1 MB | 32 MB |
|---|---|---|---|---|---|---|---|
| Baseline (with barrier) | 8.82 µs / 8.78 µs | 8.83 µs / 8.81 µs | 8.88 µs / 8.88 µs | 44.63 µs / 45.00 µs | 238.5 µs / 238.0 µs | 912.2 µs / 911.9 µs | 36.48 ms / 36.44 ms |
| LL (one-shot) | 2.19 µs / 2.19 µs | 2.20 µs / 2.19 µs | 2.25 µs / 2.26 µs | 33.97 µs / 34.18 µs | 230.3 µs / 230.1 µs | 900.1 µs / 900.6 µs | 36.42 ms / 36.50 ms |
| Sentinel (one-shot) | 1.88 µs / 1.88 µs | 1.89 µs / 1.88 µs | 1.94 µs / 1.94 µs | 42.79 µs / 42.66 µs | 283.3 µs / 283.2 µs | 1118.2 µs / 1117.4 µs | 38.59 ms / 38.56 ms |
| Two-shot LL | 3.85 µs / 3.84 µs | 3.89 µs / 3.89 µs | 3.96 µs / 3.98 µs | 23.78 µs / 23.76 µs | 120.5 µs / 120.4 µs | 369.4 µs / 369.4 µs | 11.35 ms / 11.35 ms |
| Two-shot Sentinel | 3.17 µs / 3.17 µs | 3.24 µs / 3.24 µs | 3.38 µs / 3.40 µs | 31.28 µs / 30.98 µs | 175.1 µs / 175.3 µs | 674.0 µs / 668.5 µs | 25.02 ms / 25.06 ms |
| LL128 Atomic | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) |
| Bidirectional (2 ranks, 8 chunks per round) | 13.62 µs / 13.69 µs | 13.66 µs / 13.68 µs | 13.65 µs / 13.79 µs | 37.90 µs / 37.87 µs | 157.3 µs / 158.6 µs | 542.4 µs / 543.4 µs | 19.44 ms / 19.45 ms |

## 2. p99, 3 ranks (average of the passes)

99% of the rounds were faster than this.

| Algorithm | 32 B | 64 B | 128 B | 32 KB | 256 KB | 1 MB | 32 MB |
|---|---|---|---|---|---|---|---|
| Baseline (with barrier) | 10.88 µs | 10.99 µs | 10.69 µs | 48.07 µs | 248.8 µs | 922.2 µs | 39.68 ms |
| LL (one-shot) | 4.46 µs | 4.22 µs | 4.39 µs | 39.22 µs | 249.0 µs | 922.5 µs | 41.70 ms |
| Sentinel (one-shot) | 3.94 µs | 3.94 µs | 4.38 µs | 49.93 µs | 299.4 µs | 1154.6 µs | 42.93 ms |
| Two-shot LL | 6.11 µs | 6.83 µs | 6.24 µs | 25.12 µs | 125.7 µs | 374.0 µs | 11.40 ms |
| Two-shot Sentinel | 5.61 µs | 5.31 µs | 5.62 µs | 35.65 µs | 187.0 µs | 694.1 µs | 27.57 ms |
| LL128 Atomic | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) |
| Bidirectional (2 ranks, 8 chunks per round) | 15.93 µs | 15.65 µs | 15.75 µs | 40.18 µs | 162.4 µs | 548.1 µs | 19.95 ms |

## 3. Cost of the barrier: Baseline vs LL, 3 ranks

|  | 32 B | 64 B | 128 B | 32 KB | 256 KB | 1 MB | 32 MB |
|---|---|---|---|---|---|---|---|
| Baseline - LL | 6.62 µs | 6.62 µs | 6.62 µs | 10.74 µs | 8.10 µs | 11.69 µs | 0.80 µs |
| Baseline / LL | 4.03x | 4.02x | 3.93x | 1.32x | 1.04x | 1.01x | 1.00x |

## 4. 2 ranks vs 3 ranks

|  | 32 B | 32 KB | 32 MB |
|---|---|---|---|
| Baseline (with barrier), 2 ranks | 4.42 µs | 21.94 µs | 22.97 ms |
| Baseline (with barrier), 3 ranks | 8.80 µs | 44.82 µs | 36.46 ms |
| LL (one-shot), 2 ranks | 1.79 µs | 20.21 µs | 22.95 ms |
| LL (one-shot), 3 ranks | 2.19 µs | 34.08 µs | 36.46 ms |

## 5. Atomics probe and negative test

Atomic_Probe (`logs/pass1_6_atomic_probe.txt`):

```
PROBE device atomic_cap=0 (0=NONE 1=HCA 2=GLOB) max_qp_rd_atom=16 max_qp_init_rd_atom=16
PROBE QP state=3 (3=RTS) access_flags=0xe (0x8=REMOTE_ATOMIC) max_rd_atomic=1 max_dest_rd_atomic=1 max_inline=188 max_send_sge=12
PROBE rank 0 -> peer 1, remote counter addr=12444c0 rkey=397638
PROBE 1 FETCH_AND_ADD, library result buf              post_send rc=22 (Invalid argument)
PROBE 2 FETCH_AND_ADD, fresh 64B-aligned result buf    post_send rc=22 (Invalid argument)
PROBE 3 CMP_AND_SWP                                    post_send rc=22 (Invalid argument)
PROBE 4 FETCH_AND_ADD after unsignaled inline WRITE    post_send rc=22 (Invalid argument)
PROBE remote counter should now equal the number of OK atomics above; reading it needs the peer (see its line)
PROBE device atomic_cap=0 (0=NONE 1=HCA 2=GLOB) max_qp_rd_atom=16 max_qp_init_rd_atom=16
PROBE QP state=3 (3=RTS) access_flags=0xe (0x8=REMOTE_ATOMIC) max_rd_atomic=1 max_dest_rd_atomic=1 max_inline=188 max_send_sge=12
PROBE rank 1 (counter host) final counter value = 0
```

LL without the wait (negative test) at 32 B (`logs/pass1_7_negative_test.txt`): 5 FAILED and 1 PASSED correctness lines. It must fail: this shows the check notices a missing wait.

LL without the wait (negative test) at 32 KB (`logs/pass1_7_negative_test.txt`): 6 FAILED and 0 PASSED correctness lines. It must fail: this shows the check notices a missing wait.

Atomic_Probe (`logs/pass2_6_atomic_probe.txt`):

```
PROBE device atomic_cap=0 (0=NONE 1=HCA 2=GLOB) max_qp_rd_atom=16 max_qp_init_rd_atom=16
PROBE QP state=3 (3=RTS) access_flags=0xe (0x8=REMOTE_ATOMIC) max_rd_atomic=1 max_dest_rd_atomic=1 max_inline=188 max_send_sge=12
PROBE rank 0 -> peer 1, remote counter addr=12fc4c0 rkey=407196
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
| `pass1_7_negative_test.txt` | 1 | 11 | 0 | 0 | 0 | 6 | 0 |
| `pass2_1_3ranks_32B_64B_128B_32KB.txt` | 128 | 0 | 68 | 0 | 0 | 0 | 12 |
| `pass2_2_3ranks_256KB.txt` | 32 | 0 | 17 | 0 | 0 | 0 | 3 |
| `pass2_3_3ranks_1MB.txt` | 32 | 0 | 17 | 0 | 0 | 0 | 3 |
| `pass2_4_3ranks_32MB.txt` | 32 | 0 | 17 | 0 | 0 | 0 | 3 |
| `pass2_5_2ranks.txt` | 24 | 0 | 12 | 0 | 0 | 0 | 0 |
| `pass2_6_atomic_probe.txt` | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| `pass2_7_negative_test.txt` | 1 | 11 | 0 | 0 | 0 | 6 | 0 |

Error messages (first one per rank):

- pass1 LL without the wait (negative test), 32 B, rank 1: `verify: 8 / 8 elements mismatched`
- pass1 LL without the wait (negative test), 32 B, rank 2: `verify: 8 / 8 elements mismatched`
- pass1 LL without the wait (negative test), 32 KB, rank 0: `verify: 1909 / 8192 elements mismatched`
- pass1 LL without the wait (negative test), 32 KB, rank 1: `verify: 3694 / 8192 elements mismatched`
- pass1 LL without the wait (negative test), 32 KB, rank 2: `verify: 3356 / 8192 elements mismatched`
- pass2 LL without the wait (negative test), 32 B, rank 0: `verify: 8 / 8 elements mismatched`
- pass2 LL without the wait (negative test), 32 B, rank 1: `verify: 8 / 8 elements mismatched`
- pass2 LL without the wait (negative test), 32 KB, rank 0: `verify: 2057 / 8192 elements mismatched`
- pass2 LL without the wait (negative test), 32 KB, rank 1: `verify: 2158 / 8192 elements mismatched`
- pass2 LL without the wait (negative test), 32 KB, rank 2: `verify: 3356 / 8192 elements mismatched`

## 7. Every run, per rank

Times in µs. `-` = not printed by this version or the rank did not finish.

| pass | Program | ranks | size | rank | status | mean | min | median | p99 | max | correctness | exit |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| pass1 | Baseline (with barrier) | 3 | 32 B | 0 | ok | 8.10 | 6.65 | 8.35 | 10.11 | 18.91 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 32 B | 1 | ok | 8.10 | 6.73 | 9.06 | 10.44 | 20.36 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 32 B | 2 | ok | 8.10 | 5.46 | 9.05 | 10.90 | 20.97 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 32 B | 0 | ok | 2.27 | 2.12 | 2.19 | 4.35 | 9.08 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 32 B | 1 | ok | 2.27 | 2.10 | 2.18 | 4.42 | 10.12 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 32 B | 2 | ok | 2.26 | 2.10 | 2.19 | 4.29 | 8.20 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 32 B | 0 | ok | 1.96 | 1.81 | 1.89 | 4.16 | 8.77 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 32 B | 1 | ok | 1.96 | 1.79 | 1.88 | 3.84 | 9.50 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 32 B | 2 | ok | 1.96 | 1.80 | 1.88 | 3.97 | 9.11 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 32 B | 0 | ok | 3.94 | 3.25 | 3.84 | 6.03 | 13.14 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 32 B | 1 | ok | 3.95 | 3.48 | 3.85 | 6.44 | 13.12 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 32 B | 2 | ok | 3.94 | 3.48 | 3.85 | 6.29 | 13.26 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 32 B | 0 | ok | 3.25 | 2.78 | 3.16 | 5.36 | 9.15 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 32 B | 1 | ok | 3.25 | 2.91 | 3.17 | 5.38 | 9.43 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 32 B | 2 | ok | 3.25 | 2.94 | 3.17 | 5.37 | 9.20 | PASSED/PASSED | 0 |
| pass1 | LL128 Atomic | 3 | 32 B | 0 | skipped | - | - | - | - | - | - | 2 |
| pass1 | LL128 Atomic | 3 | 32 B | 1 | skipped | - | - | - | - | - | - | 2 |
| pass1 | LL128 Atomic | 3 | 32 B | 2 | skipped | - | - | - | - | - | - | 2 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 B | 0 | ok | 13.76 | 13.43 | 13.60 | 15.98 | 41.42 | PASSED | 0 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 B | 1 | ok | 13.76 | 13.35 | 13.65 | 15.86 | 41.45 | PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 64 B | 0 | ok | 8.11 | 6.91 | 8.31 | 10.27 | 21.32 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 64 B | 1 | ok | 8.11 | 6.70 | 9.11 | 10.84 | 22.98 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 64 B | 2 | ok | 8.10 | 6.05 | 9.06 | 11.12 | 22.67 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 64 B | 0 | ok | 2.27 | 2.12 | 2.20 | 4.48 | 11.06 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 64 B | 1 | ok | 2.27 | 2.11 | 2.19 | 4.04 | 11.88 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 64 B | 2 | ok | 2.27 | 2.12 | 2.20 | 4.06 | 11.76 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 64 B | 0 | ok | 1.97 | 1.81 | 1.89 | 3.52 | 11.32 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 64 B | 1 | ok | 1.97 | 1.81 | 1.89 | 3.75 | 12.12 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 64 B | 2 | ok | 1.97 | 1.80 | 1.89 | 3.81 | 11.63 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 64 B | 0 | ok | 3.97 | 3.26 | 3.87 | 6.68 | 13.84 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 64 B | 1 | ok | 3.97 | 3.52 | 3.90 | 6.50 | 13.61 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 64 B | 2 | ok | 3.97 | 3.52 | 3.90 | 6.85 | 13.65 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 64 B | 0 | ok | 3.34 | 2.87 | 3.24 | 5.28 | 20.08 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 64 B | 1 | ok | 3.34 | 3.00 | 3.24 | 5.42 | 21.29 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 64 B | 2 | ok | 3.34 | 2.98 | 3.24 | 5.43 | 21.45 | PASSED/PASSED | 0 |
| pass1 | LL128 Atomic | 3 | 64 B | 0 | skipped | - | - | - | - | - | - | 2 |
| pass1 | LL128 Atomic | 3 | 64 B | 1 | skipped | - | - | - | - | - | - | 2 |
| pass1 | LL128 Atomic | 3 | 64 B | 2 | skipped | - | - | - | - | - | - | 2 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 64 B | 0 | ok | 13.78 | 13.39 | 13.69 | 15.60 | 42.69 | PASSED | 0 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 64 B | 1 | ok | 13.78 | 13.47 | 13.63 | 15.56 | 43.64 | PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 128 B | 0 | ok | 8.18 | 6.17 | 8.37 | 10.12 | 13.39 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 128 B | 1 | ok | 8.19 | 6.76 | 9.14 | 10.97 | 15.66 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 128 B | 2 | ok | 8.19 | 6.78 | 9.12 | 10.87 | 15.61 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 128 B | 0 | ok | 2.32 | 2.14 | 2.26 | 4.12 | 10.20 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 128 B | 1 | ok | 2.32 | 2.13 | 2.25 | 4.01 | 10.40 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 128 B | 2 | ok | 2.32 | 2.13 | 2.25 | 4.11 | 10.22 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 128 B | 0 | ok | 2.03 | 1.86 | 1.94 | 3.71 | 17.88 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 128 B | 1 | ok | 2.02 | 1.87 | 1.94 | 3.75 | 17.44 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 128 B | 2 | ok | 2.03 | 1.85 | 1.93 | 3.84 | 17.24 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 128 B | 0 | ok | 4.01 | 3.27 | 3.93 | 6.22 | 14.11 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 128 B | 1 | ok | 4.01 | 3.60 | 3.98 | 6.49 | 13.94 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 128 B | 2 | ok | 4.02 | 3.58 | 3.98 | 6.19 | 14.35 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 128 B | 0 | ok | 3.46 | 3.04 | 3.38 | 5.77 | 12.80 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 128 B | 1 | ok | 3.47 | 3.13 | 3.38 | 5.65 | 12.82 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 128 B | 2 | ok | 3.47 | 3.15 | 3.38 | 5.82 | 12.75 | PASSED/PASSED | 0 |
| pass1 | LL128 Atomic | 3 | 128 B | 0 | skipped | - | - | - | - | - | - | 2 |
| pass1 | LL128 Atomic | 3 | 128 B | 1 | skipped | - | - | - | - | - | - | 2 |
| pass1 | LL128 Atomic | 3 | 128 B | 2 | skipped | - | - | - | - | - | - | 2 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 128 B | 0 | ok | 13.75 | 13.39 | 13.67 | 15.95 | 31.83 | PASSED | 0 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 128 B | 1 | ok | 13.75 | 13.46 | 13.64 | 16.05 | 31.93 | PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 32 KB | 0 | ok | 44.49 | 39.46 | 44.52 | 47.16 | 57.86 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 32 KB | 1 | ok | 44.49 | 38.38 | 45.00 | 48.23 | 59.65 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 32 KB | 2 | ok | 44.49 | 38.19 | 44.38 | 48.26 | 59.49 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 32 KB | 0 | ok | 34.25 | 27.68 | 33.97 | 37.82 | 54.03 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 32 KB | 1 | ok | 34.24 | 24.05 | 34.00 | 39.81 | 53.49 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 32 KB | 2 | ok | 34.25 | 21.30 | 33.93 | 39.91 | 52.63 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 32 KB | 0 | ok | 42.83 | 34.48 | 43.15 | 49.57 | 90.17 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 32 KB | 1 | ok | 42.83 | 34.71 | 42.47 | 50.55 | 95.63 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 32 KB | 2 | ok | 42.84 | 34.32 | 42.77 | 51.32 | 92.75 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 32 KB | 0 | ok | 23.85 | 22.41 | 23.78 | 25.81 | 35.31 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 32 KB | 1 | ok | 23.85 | 21.55 | 23.78 | 25.85 | 39.67 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 32 KB | 2 | ok | 23.84 | 21.59 | 23.77 | 25.50 | 35.93 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 32 KB | 0 | ok | 31.55 | 28.62 | 31.25 | 35.97 | 45.45 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 32 KB | 1 | ok | 31.54 | 29.32 | 31.34 | 35.08 | 49.23 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 32 KB | 2 | ok | 31.53 | 29.94 | 31.24 | 35.07 | 45.09 | PASSED/PASSED | 0 |
| pass1 | LL128 Atomic | 3 | 32 KB | 0 | skipped | - | - | - | - | - | - | 2 |
| pass1 | LL128 Atomic | 3 | 32 KB | 1 | skipped | - | - | - | - | - | - | 2 |
| pass1 | LL128 Atomic | 3 | 32 KB | 2 | skipped | - | - | - | - | - | - | 2 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 KB | 0 | ok | 38.00 | 37.63 | 37.90 | 40.79 | 45.85 | PASSED | 0 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 KB | 1 | ok | 38.00 | 37.66 | 37.89 | 40.57 | 46.09 | PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 256 KB | 0 | ok | 238.59 | 229.54 | 238.44 | 247.73 | 247.98 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 256 KB | 1 | ok | 239.18 | 235.24 | 238.48 | 248.28 | 290.08 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 256 KB | 2 | ok | 239.17 | 229.50 | 238.61 | 250.68 | 293.19 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 256 KB | 0 | ok | 230.84 | 223.49 | 230.49 | 248.73 | 252.77 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 256 KB | 1 | ok | 230.94 | 224.69 | 230.12 | 254.09 | 260.39 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 256 KB | 2 | ok | 230.63 | 201.65 | 230.25 | 254.78 | 256.15 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 256 KB | 0 | ok | 283.54 | 275.57 | 283.32 | 300.64 | 300.75 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 256 KB | 1 | ok | 283.62 | 275.54 | 283.06 | 299.38 | 305.97 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 256 KB | 2 | ok | 283.87 | 271.58 | 283.46 | 303.95 | 314.57 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 256 KB | 0 | ok | 120.73 | 115.26 | 120.43 | 128.91 | 130.03 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 256 KB | 1 | ok | 120.70 | 118.56 | 120.51 | 126.76 | 128.07 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 256 KB | 2 | ok | 120.61 | 118.43 | 120.41 | 125.42 | 126.54 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 256 KB | 0 | ok | 175.24 | 164.54 | 175.01 | 186.58 | 198.76 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 256 KB | 1 | ok | 175.23 | 163.82 | 175.44 | 188.21 | 191.62 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 256 KB | 2 | ok | 174.97 | 164.07 | 174.80 | 189.11 | 189.42 | PASSED/PASSED | 0 |
| pass1 | LL128 Atomic | 3 | 256 KB | 0 | skipped | - | - | - | - | - | - | 2 |
| pass1 | LL128 Atomic | 3 | 256 KB | 1 | skipped | - | - | - | - | - | - | 2 |
| pass1 | LL128 Atomic | 3 | 256 KB | 2 | skipped | - | - | - | - | - | - | 2 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 256 KB | 0 | ok | 158.14 | 156.42 | 157.36 | 160.81 | 202.89 | PASSED | 0 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 256 KB | 1 | ok | 158.09 | 156.13 | 157.21 | 164.24 | 202.74 | PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 1 MB | 0 | ok | 912.13 | 904.47 | 912.50 | 918.56 | 920.31 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 1 MB | 1 | ok | 913.65 | 907.11 | 912.26 | 918.11 | 1,060.04 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 1 MB | 2 | ok | 913.54 | 895.16 | 911.86 | 930.55 | 1,068.33 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 1 MB | 0 | ok | 900.36 | 873.57 | 899.93 | 922.21 | 926.75 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 1 MB | 1 | ok | 900.48 | 877.72 | 899.99 | 924.15 | 927.20 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 1 MB | 2 | ok | 901.38 | 879.77 | 900.40 | 922.31 | 1,009.15 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 1 MB | 0 | ok | 1,115.58 | 1,101.77 | 1,114.63 | 1,134.54 | 1,136.38 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 1 MB | 1 | ok | 1,115.90 | 1,098.67 | 1,123.55 | 1,150.18 | 1,150.93 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 1 MB | 2 | ok | 1,116.30 | 1,092.89 | 1,116.51 | 1,158.14 | 1,161.13 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 1 MB | 0 | ok | 369.66 | 364.00 | 369.52 | 372.78 | 377.15 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 1 MB | 1 | ok | 369.68 | 365.61 | 369.40 | 374.56 | 378.11 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 1 MB | 2 | ok | 369.62 | 358.73 | 369.42 | 377.73 | 384.43 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 1 MB | 0 | ok | 703.71 | 653.94 | 676.02 | 698.89 | 3,676.29 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 1 MB | 1 | ok | 673.62 | 661.60 | 673.38 | 689.56 | 689.65 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 1 MB | 2 | ok | 703.48 | 649.28 | 672.73 | 701.73 | 3,666.27 | PASSED/PASSED | 0 |
| pass1 | LL128 Atomic | 3 | 1 MB | 0 | skipped | - | - | - | - | - | - | 2 |
| pass1 | LL128 Atomic | 3 | 1 MB | 1 | skipped | - | - | - | - | - | - | 2 |
| pass1 | LL128 Atomic | 3 | 1 MB | 2 | skipped | - | - | - | - | - | - | 2 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 1 MB | 0 | ok | 542.84 | 539.51 | 542.43 | 547.86 | 547.90 | PASSED | 0 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 1 MB | 1 | ok | 542.68 | 538.67 | 542.33 | 549.59 | 549.98 | PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 32 MB | 0 | ok | 36,583.51 | 36,116.72 | 36,494.76 | 38,218.93 | 38,348.59 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 32 MB | 1 | ok | 36,588.67 | 36,169.81 | 36,469.53 | 38,100.56 | 38,206.08 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 32 MB | 2 | ok | 36,548.37 | 32,093.44 | 36,483.30 | 42,801.39 | 42,996.96 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 32 MB | 0 | ok | 36,453.24 | 31,598.72 | 36,458.73 | 41,230.49 | 41,721.67 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 32 MB | 1 | ok | 36,457.64 | 31,923.15 | 36,401.14 | 40,893.52 | 42,352.62 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 32 MB | 2 | ok | 36,405.24 | 31,527.54 | 36,391.22 | 42,328.78 | 42,332.09 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 32 MB | 0 | ok | 38,849.80 | 38,268.52 | 38,589.36 | 42,445.51 | 43,100.46 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 32 MB | 1 | ok | 38,841.96 | 38,196.57 | 38,596.49 | 43,019.23 | 43,305.95 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 32 MB | 2 | ok | 38,795.03 | 36,693.84 | 38,570.28 | 43,362.99 | 45,930.94 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 32 MB | 0 | ok | 11,351.92 | 11,326.42 | 11,349.55 | 11,408.47 | 11,458.55 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 32 MB | 1 | ok | 11,351.21 | 11,323.64 | 11,349.17 | 11,389.41 | 11,391.35 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 32 MB | 2 | ok | 11,353.22 | 11,319.78 | 11,349.71 | 11,391.59 | 11,534.66 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 32 MB | 0 | ok | 25,075.23 | 24,764.65 | 24,998.98 | 26,899.19 | 27,760.12 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 32 MB | 1 | ok | 25,073.89 | 24,663.60 | 25,002.93 | 26,935.78 | 28,092.63 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 32 MB | 2 | ok | 25,078.46 | 22,715.00 | 25,054.10 | 29,276.14 | 29,307.45 | PASSED/PASSED | 0 |
| pass1 | LL128 Atomic | 3 | 32 MB | 0 | skipped | - | - | - | - | - | - | 2 |
| pass1 | LL128 Atomic | 3 | 32 MB | 1 | skipped | - | - | - | - | - | - | 2 |
| pass1 | LL128 Atomic | 3 | 32 MB | 2 | skipped | - | - | - | - | - | - | 2 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 MB | 0 | ok | 19,443.36 | 19,143.99 | 19,436.50 | 20,059.52 | 20,528.60 | PASSED | 0 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 MB | 1 | ok | 19,444.34 | 19,175.22 | 19,442.67 | 20,021.90 | 20,119.85 | PASSED | 0 |
| pass1 | Baseline (with barrier) | 2 | 32 B | 0 | ok | 4.24 | 3.69 | 4.45 | 5.89 | 26.07 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 2 | 32 B | 1 | ok | 4.22 | 2.44 | 4.45 | 5.28 | 25.88 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 2 | 32 B | 0 | ok | 1.85 | 1.72 | 1.80 | 2.56 | 18.06 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 2 | 32 B | 1 | ok | 1.85 | 1.66 | 1.80 | 3.26 | 17.68 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 2 | 32 KB | 0 | ok | 21.80 | 20.34 | 21.84 | 24.06 | 30.26 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 2 | 32 KB | 1 | ok | 21.82 | 19.56 | 22.01 | 24.02 | 33.19 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 2 | 32 KB | 0 | ok | 18.75 | 12.12 | 19.96 | 26.92 | 36.15 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 2 | 32 KB | 1 | ok | 18.74 | 12.31 | 19.86 | 25.07 | 35.89 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 2 | 32 MB | 0 | ok | 22,978.89 | 22,354.02 | 22,951.08 | 23,605.86 | 23,989.28 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 2 | 32 MB | 1 | ok | 22,984.33 | 22,243.15 | 22,970.83 | 23,705.05 | 23,853.28 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 2 | 32 MB | 0 | ok | 22,933.55 | 13,477.61 | 23,030.01 | 32,629.30 | 33,202.44 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 2 | 32 MB | 1 | ok | 22,841.65 | 13,471.95 | 22,971.04 | 32,714.88 | 32,896.97 | PASSED/PASSED | 0 |
| pass1 | Atomic_Probe | 2 | 32 B | 0 | no result | - | - | - | - | - | - | 0 |
| pass1 | Atomic_Probe | 2 | 32 B | 1 | no result | - | - | - | - | - | - | 0 |
| pass1 | LL without the wait (negative test) | 3 | 32 B | 0 | wrong result | 2.23 | 2.12 | 2.19 | 2.44 | 10.07 | PASSED/FAILED | 0 |
| pass1 | LL without the wait (negative test) | 3 | 32 B | 1 | wrong result | 2.24 | 2.14 | 2.20 | 2.49 | 8.87 | FAILED/FAILED | 1 |
| pass1 | LL without the wait (negative test) | 3 | 32 B | 2 | wrong result | 2.25 | 2.12 | 2.21 | 2.50 | 8.06 | FAILED/FAILED | 1 |
| pass1 | LL without the wait (negative test) | 3 | 32 KB | 0 | wrong result | 26.61 | 23.78 | 26.23 | 31.23 | 35.75 | FAILED/FAILED | 1 |
| pass1 | LL without the wait (negative test) | 3 | 32 KB | 1 | wrong result | 30.53 | 23.48 | 28.59 | 39.92 | 44.56 | FAILED/FAILED | 1 |
| pass1 | LL without the wait (negative test) | 3 | 32 KB | 2 | wrong result | 26.98 | 23.36 | 26.51 | 32.36 | 41.89 | FAILED/FAILED | 1 |
| pass2 | Baseline (with barrier) | 3 | 32 B | 0 | ok | 8.04 | 6.15 | 8.24 | 10.85 | 17.93 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 32 B | 1 | ok | 8.04 | 6.62 | 9.08 | 11.43 | 19.27 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 32 B | 2 | ok | 8.04 | 5.44 | 9.03 | 11.56 | 16.99 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 32 B | 0 | ok | 2.28 | 2.11 | 2.19 | 4.24 | 9.57 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 32 B | 1 | ok | 2.28 | 2.09 | 2.18 | 4.82 | 10.73 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 32 B | 2 | ok | 2.28 | 2.10 | 2.19 | 4.66 | 10.04 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 32 B | 0 | ok | 1.98 | 1.80 | 1.88 | 3.83 | 17.80 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 32 B | 1 | ok | 1.98 | 1.79 | 1.88 | 3.92 | 17.53 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 32 B | 2 | ok | 1.97 | 1.79 | 1.88 | 3.96 | 17.27 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 32 B | 0 | ok | 3.92 | 3.24 | 3.82 | 6.03 | 12.81 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 32 B | 1 | ok | 3.93 | 3.49 | 3.84 | 5.94 | 12.88 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 32 B | 2 | ok | 3.93 | 3.48 | 3.85 | 5.91 | 13.04 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 32 B | 0 | ok | 3.27 | 2.75 | 3.16 | 5.80 | 18.54 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 32 B | 1 | ok | 3.27 | 2.93 | 3.18 | 5.98 | 18.52 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 32 B | 2 | ok | 3.26 | 2.93 | 3.17 | 5.80 | 18.74 | PASSED/PASSED | 0 |
| pass2 | LL128 Atomic | 3 | 32 B | 0 | skipped | - | - | - | - | - | - | 2 |
| pass2 | LL128 Atomic | 3 | 32 B | 1 | skipped | - | - | - | - | - | - | 2 |
| pass2 | LL128 Atomic | 3 | 32 B | 2 | skipped | - | - | - | - | - | - | 2 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 B | 0 | ok | 13.69 | 13.23 | 13.68 | 15.93 | 24.49 | PASSED | 0 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 B | 1 | ok | 13.70 | 13.29 | 13.69 | 15.93 | 23.95 | PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 64 B | 0 | ok | 8.07 | 7.49 | 8.27 | 10.69 | 15.53 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 64 B | 1 | ok | 8.07 | 6.67 | 9.10 | 11.52 | 17.35 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 64 B | 2 | ok | 8.08 | 6.68 | 9.05 | 11.52 | 17.37 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 64 B | 0 | ok | 2.27 | 2.10 | 2.19 | 4.24 | 12.88 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 64 B | 1 | ok | 2.28 | 2.11 | 2.19 | 4.37 | 13.25 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 64 B | 2 | ok | 2.27 | 2.12 | 2.19 | 4.12 | 12.77 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 64 B | 0 | ok | 1.96 | 1.80 | 1.88 | 4.18 | 11.44 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 64 B | 1 | ok | 1.97 | 1.80 | 1.88 | 4.16 | 12.05 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 64 B | 2 | ok | 1.97 | 1.80 | 1.88 | 4.20 | 11.78 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 64 B | 0 | ok | 3.96 | 3.25 | 3.87 | 7.05 | 11.19 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 64 B | 1 | ok | 3.96 | 3.52 | 3.89 | 6.76 | 11.01 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 64 B | 2 | ok | 3.96 | 3.51 | 3.90 | 7.14 | 11.10 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 64 B | 0 | ok | 3.32 | 2.86 | 3.24 | 5.14 | 13.50 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 64 B | 1 | ok | 3.32 | 3.00 | 3.24 | 5.26 | 13.50 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 64 B | 2 | ok | 3.32 | 3.01 | 3.24 | 5.34 | 13.45 | PASSED/PASSED | 0 |
| pass2 | LL128 Atomic | 3 | 64 B | 0 | skipped | - | - | - | - | - | - | 2 |
| pass2 | LL128 Atomic | 3 | 64 B | 1 | skipped | - | - | - | - | - | - | 2 |
| pass2 | LL128 Atomic | 3 | 64 B | 2 | skipped | - | - | - | - | - | - | 2 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 64 B | 0 | ok | 13.79 | 13.38 | 13.70 | 15.70 | 32.95 | PASSED | 0 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 64 B | 1 | ok | 13.79 | 13.44 | 13.66 | 15.74 | 33.09 | PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 128 B | 0 | ok | 8.16 | 7.09 | 8.35 | 10.20 | 15.21 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 128 B | 1 | ok | 8.16 | 6.75 | 9.19 | 11.05 | 16.33 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 128 B | 2 | ok | 8.16 | 6.18 | 9.09 | 10.94 | 15.53 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 128 B | 0 | ok | 2.37 | 2.14 | 2.27 | 5.00 | 28.75 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 128 B | 1 | ok | 2.37 | 2.14 | 2.25 | 4.57 | 18.47 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 128 B | 2 | ok | 2.37 | 2.15 | 2.26 | 4.55 | 28.63 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 128 B | 0 | ok | 2.09 | 1.86 | 1.95 | 4.86 | 23.49 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 128 B | 1 | ok | 2.08 | 1.85 | 1.94 | 5.03 | 22.25 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 128 B | 2 | ok | 2.09 | 1.86 | 1.94 | 5.11 | 23.21 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 128 B | 0 | ok | 4.02 | 3.30 | 3.93 | 6.44 | 12.23 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 128 B | 1 | ok | 4.03 | 3.59 | 4.00 | 5.92 | 12.36 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 128 B | 2 | ok | 4.02 | 3.58 | 4.00 | 6.20 | 12.41 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 128 B | 0 | ok | 3.48 | 3.09 | 3.39 | 5.35 | 19.77 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 128 B | 1 | ok | 3.49 | 3.17 | 3.40 | 5.71 | 20.06 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 128 B | 2 | ok | 3.48 | 3.13 | 3.40 | 5.43 | 19.14 | PASSED/PASSED | 0 |
| pass2 | LL128 Atomic | 3 | 128 B | 0 | skipped | - | - | - | - | - | - | 2 |
| pass2 | LL128 Atomic | 3 | 128 B | 1 | skipped | - | - | - | - | - | - | 2 |
| pass2 | LL128 Atomic | 3 | 128 B | 2 | skipped | - | - | - | - | - | - | 2 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 128 B | 0 | ok | 13.86 | 13.39 | 13.79 | 15.68 | 24.65 | PASSED | 0 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 128 B | 1 | ok | 13.87 | 13.47 | 13.79 | 15.33 | 24.90 | PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 32 KB | 0 | ok | 44.62 | 38.67 | 44.93 | 47.71 | 57.40 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 32 KB | 1 | ok | 44.62 | 37.21 | 45.18 | 48.83 | 59.38 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 32 KB | 2 | ok | 44.62 | 35.43 | 44.90 | 48.22 | 63.56 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 32 KB | 0 | ok | 34.63 | 29.93 | 34.20 | 38.57 | 43.12 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 32 KB | 1 | ok | 34.64 | 23.54 | 34.18 | 39.86 | 45.17 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 32 KB | 2 | ok | 34.62 | 23.07 | 34.18 | 39.36 | 47.10 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 32 KB | 0 | ok | 42.68 | 34.49 | 42.97 | 49.29 | 58.97 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 32 KB | 1 | ok | 42.68 | 35.24 | 42.48 | 49.25 | 58.84 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 32 KB | 2 | ok | 42.68 | 34.17 | 42.52 | 49.61 | 60.34 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 32 KB | 0 | ok | 23.80 | 22.14 | 23.76 | 24.98 | 34.20 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 32 KB | 1 | ok | 23.80 | 22.26 | 23.76 | 24.27 | 37.59 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 32 KB | 2 | ok | 23.80 | 21.58 | 23.76 | 24.32 | 36.20 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 32 KB | 0 | ok | 31.61 | 28.15 | 30.91 | 36.35 | 48.56 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 32 KB | 1 | ok | 31.61 | 29.41 | 30.75 | 35.89 | 45.40 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 32 KB | 2 | ok | 31.59 | 30.09 | 31.27 | 35.52 | 48.89 | PASSED/PASSED | 0 |
| pass2 | LL128 Atomic | 3 | 32 KB | 0 | skipped | - | - | - | - | - | - | 2 |
| pass2 | LL128 Atomic | 3 | 32 KB | 1 | skipped | - | - | - | - | - | - | 2 |
| pass2 | LL128 Atomic | 3 | 32 KB | 2 | skipped | - | - | - | - | - | - | 2 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 KB | 0 | ok | 37.94 | 37.52 | 37.87 | 39.71 | 47.21 | PASSED | 0 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 KB | 1 | ok | 37.95 | 37.45 | 37.87 | 39.65 | 47.27 | PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 256 KB | 0 | ok | 238.10 | 233.65 | 237.86 | 246.22 | 248.94 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 256 KB | 1 | ok | 238.19 | 234.04 | 238.19 | 246.81 | 247.64 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 256 KB | 2 | ok | 238.05 | 227.68 | 238.04 | 252.97 | 253.66 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 256 KB | 0 | ok | 230.32 | 181.46 | 230.08 | 244.53 | 269.99 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 256 KB | 1 | ok | 230.73 | 221.39 | 230.14 | 244.80 | 264.05 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 256 KB | 2 | ok | 230.84 | 213.32 | 229.94 | 247.19 | 284.72 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 256 KB | 0 | ok | 283.52 | 273.22 | 282.89 | 294.71 | 296.37 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 256 KB | 1 | ok | 283.55 | 273.02 | 282.83 | 296.97 | 297.64 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 256 KB | 2 | ok | 283.50 | 272.42 | 283.84 | 300.95 | 304.80 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 256 KB | 0 | ok | 120.68 | 115.98 | 120.00 | 126.08 | 126.24 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 256 KB | 1 | ok | 120.61 | 112.79 | 120.64 | 123.71 | 125.22 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 256 KB | 2 | ok | 120.71 | 119.39 | 120.51 | 123.31 | 123.69 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 256 KB | 0 | ok | 175.03 | 164.02 | 175.36 | 187.72 | 188.38 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 256 KB | 1 | ok | 175.02 | 164.22 | 175.44 | 186.40 | 189.32 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 256 KB | 2 | ok | 174.94 | 163.89 | 175.14 | 183.91 | 192.68 | PASSED/PASSED | 0 |
| pass2 | LL128 Atomic | 3 | 256 KB | 0 | skipped | - | - | - | - | - | - | 2 |
| pass2 | LL128 Atomic | 3 | 256 KB | 1 | skipped | - | - | - | - | - | - | 2 |
| pass2 | LL128 Atomic | 3 | 256 KB | 2 | skipped | - | - | - | - | - | - | 2 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 256 KB | 0 | ok | 159.00 | 157.33 | 158.75 | 161.52 | 164.63 | PASSED | 0 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 256 KB | 1 | ok | 158.93 | 157.82 | 158.53 | 162.98 | 163.17 | PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 1 MB | 0 | ok | 911.84 | 903.65 | 911.91 | 918.42 | 927.67 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 1 MB | 1 | ok | 911.99 | 905.82 | 911.85 | 918.28 | 922.79 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 1 MB | 2 | ok | 911.83 | 895.66 | 911.85 | 929.16 | 929.46 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 1 MB | 0 | ok | 900.70 | 830.00 | 900.77 | 925.46 | 973.21 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 1 MB | 1 | ok | 901.42 | 884.25 | 900.96 | 916.23 | 973.91 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 1 MB | 2 | ok | 901.15 | 875.22 | 900.04 | 924.43 | 967.18 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 1 MB | 0 | ok | 1,113.04 | 1,092.83 | 1,110.29 | 1,157.95 | 1,175.71 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 1 MB | 1 | ok | 1,114.01 | 1,092.38 | 1,123.72 | 1,167.25 | 1,174.24 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 1 MB | 2 | ok | 1,113.77 | 1,084.99 | 1,118.06 | 1,159.24 | 1,161.51 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 1 MB | 0 | ok | 369.45 | 364.69 | 369.36 | 373.13 | 374.33 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 1 MB | 1 | ok | 369.50 | 365.59 | 369.53 | 372.92 | 374.41 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 1 MB | 2 | ok | 369.38 | 358.96 | 369.45 | 372.64 | 374.43 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 1 MB | 0 | ok | 669.57 | 652.84 | 668.33 | 689.54 | 690.12 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 1 MB | 1 | ok | 669.42 | 652.59 | 668.48 | 695.04 | 695.71 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 1 MB | 2 | ok | 669.55 | 649.92 | 668.68 | 690.06 | 691.04 | PASSED/PASSED | 0 |
| pass2 | LL128 Atomic | 3 | 1 MB | 0 | skipped | - | - | - | - | - | - | 2 |
| pass2 | LL128 Atomic | 3 | 1 MB | 1 | skipped | - | - | - | - | - | - | 2 |
| pass2 | LL128 Atomic | 3 | 1 MB | 2 | skipped | - | - | - | - | - | - | 2 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 1 MB | 0 | ok | 543.46 | 540.01 | 543.26 | 547.28 | 548.25 | PASSED | 0 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 1 MB | 1 | ok | 543.62 | 540.97 | 543.45 | 547.71 | 548.86 | PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 32 MB | 0 | ok | 36,531.00 | 36,030.70 | 36,443.43 | 38,091.34 | 38,348.27 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 32 MB | 1 | ok | 36,537.31 | 36,056.49 | 36,428.74 | 38,344.39 | 38,376.25 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 32 MB | 2 | ok | 36,491.97 | 32,063.52 | 36,437.64 | 42,546.40 | 42,562.89 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 32 MB | 0 | ok | 36,530.41 | 31,392.69 | 36,522.16 | 41,571.89 | 41,716.99 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 32 MB | 1 | ok | 36,529.48 | 31,529.34 | 36,539.51 | 41,463.77 | 41,516.93 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 32 MB | 2 | ok | 36,484.89 | 31,662.47 | 36,439.83 | 42,686.78 | 42,840.97 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 32 MB | 0 | ok | 38,793.98 | 38,276.05 | 38,540.54 | 42,203.49 | 42,839.04 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 32 MB | 1 | ok | 38,799.01 | 38,095.53 | 38,571.85 | 43,038.15 | 43,650.92 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 32 MB | 2 | ok | 38,747.62 | 36,282.99 | 38,570.74 | 43,488.39 | 45,149.80 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 32 MB | 0 | ok | 11,352.44 | 11,329.69 | 11,351.16 | 11,388.71 | 11,454.94 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 32 MB | 1 | ok | 11,351.03 | 11,315.15 | 11,350.39 | 11,401.36 | 11,401.89 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 32 MB | 2 | ok | 11,354.37 | 11,314.12 | 11,350.23 | 11,400.63 | 11,639.26 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 32 MB | 0 | ok | 25,116.99 | 24,768.90 | 25,052.77 | 26,522.05 | 27,108.23 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 32 MB | 1 | ok | 25,116.06 | 24,850.06 | 25,067.25 | 27,215.06 | 28,264.23 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 32 MB | 2 | ok | 25,119.07 | 23,077.01 | 25,061.13 | 28,597.63 | 29,662.41 | PASSED/PASSED | 0 |
| pass2 | LL128 Atomic | 3 | 32 MB | 0 | skipped | - | - | - | - | - | - | 2 |
| pass2 | LL128 Atomic | 3 | 32 MB | 1 | skipped | - | - | - | - | - | - | 2 |
| pass2 | LL128 Atomic | 3 | 32 MB | 2 | skipped | - | - | - | - | - | - | 2 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 MB | 0 | ok | 19,451.76 | 19,207.50 | 19,439.20 | 19,866.94 | 20,522.74 | PASSED | 0 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 MB | 1 | ok | 19,452.99 | 19,150.00 | 19,451.05 | 19,856.30 | 20,443.75 | PASSED | 0 |
| pass2 | Baseline (with barrier) | 2 | 32 B | 0 | ok | 4.20 | 3.20 | 4.41 | 5.07 | 20.23 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 2 | 32 B | 1 | ok | 4.20 | 3.25 | 4.38 | 5.27 | 13.70 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 2 | 32 B | 0 | ok | 1.87 | 1.68 | 1.78 | 3.28 | 22.11 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 2 | 32 B | 1 | ok | 1.86 | 1.68 | 1.78 | 3.54 | 23.32 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 2 | 32 KB | 0 | ok | 21.81 | 20.94 | 21.89 | 24.43 | 31.37 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 2 | 32 KB | 1 | ok | 21.82 | 21.18 | 22.00 | 24.12 | 30.59 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 2 | 32 KB | 0 | ok | 18.75 | 12.20 | 20.36 | 25.30 | 28.88 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 2 | 32 KB | 1 | ok | 18.75 | 12.35 | 20.67 | 25.40 | 29.36 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 2 | 32 MB | 0 | ok | 22,997.46 | 22,483.22 | 22,982.01 | 23,522.30 | 24,196.09 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 2 | 32 MB | 1 | ok | 22,991.23 | 22,299.79 | 22,979.66 | 23,476.32 | 23,943.88 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 2 | 32 MB | 0 | ok | 22,761.00 | 13,313.89 | 22,939.29 | 32,631.70 | 32,701.12 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 2 | 32 MB | 1 | ok | 22,853.95 | 13,381.53 | 22,866.72 | 32,545.82 | 33,663.27 | PASSED/PASSED | 0 |
| pass2 | Atomic_Probe | 2 | 32 B | 0 | no result | - | - | - | - | - | - | 0 |
| pass2 | Atomic_Probe | 2 | 32 B | 1 | no result | - | - | - | - | - | - | 0 |
| pass2 | LL without the wait (negative test) | 3 | 32 B | 0 | wrong result | 2.23 | 2.12 | 2.19 | 2.48 | 9.94 | FAILED/FAILED | 1 |
| pass2 | LL without the wait (negative test) | 3 | 32 B | 1 | wrong result | 2.22 | 2.10 | 2.18 | 3.47 | 9.43 | FAILED/FAILED | 1 |
| pass2 | LL without the wait (negative test) | 3 | 32 B | 2 | wrong result | 2.23 | 2.13 | 2.20 | 2.53 | 7.30 | PASSED/FAILED | 0 |
| pass2 | LL without the wait (negative test) | 3 | 32 KB | 0 | wrong result | 26.45 | 23.61 | 25.83 | 32.31 | 37.81 | FAILED/FAILED | 1 |
| pass2 | LL without the wait (negative test) | 3 | 32 KB | 1 | wrong result | 30.80 | 23.47 | 28.49 | 39.10 | 44.17 | FAILED/FAILED | 1 |
| pass2 | LL without the wait (negative test) | 3 | 32 KB | 2 | wrong result | 26.54 | 23.36 | 25.81 | 32.59 | 40.21 | FAILED/FAILED | 1 |

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

