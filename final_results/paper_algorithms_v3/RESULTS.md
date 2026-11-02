# Results: v3 (paper_algorithms_v3)

**v3**: v2 + correctness check every round + blocked add.

Machines: rank 0 = compute02, rank 1 = compute03, rank 2 = compute04. PBS jobs: 30219.master.local, 30220.master.local.

A value is the time of one AllReduce (median over the timed rounds, averaged over the ranks that
finished correctly). Cells with two numbers are **pass 1 / pass 2**: the same experiment run twice,
the second time with the versions in the opposite order. If a rank crashed, hung (stopped by the
timeout) or got a wrong result, the cell says so. 1000 timed rounds for sizes up to 32 KB, 100
for 256 KB and above.

## 1. Median, 3 ranks (pass 1 / pass 2)

| Algorithm | 32 B | 64 B | 128 B | 32 KB | 256 KB | 1 MB | 32 MB |
|---|---|---|---|---|---|---|---|
| Baseline (with barrier) | 8.76 µs / 8.83 µs | 8.83 µs / 8.74 µs | 8.81 µs / 8.90 µs | 44.57 µs / 44.91 µs | 238.0 µs / 238.3 µs | 911.8 µs / 911.4 µs | 36.35 ms / 36.46 ms |
| LL (one-shot) | 2.18 µs / 2.15 µs | 2.16 µs / 2.16 µs | 2.25 µs / 2.24 µs | 33.83 µs / 33.83 µs | 230.6 µs / 230.5 µs | 900.5 µs / 900.5 µs | 38.07 ms / 36.52 ms |
| Sentinel (one-shot) | 1.86 µs / 1.85 µs | 1.86 µs / 1.86 µs | 1.91 µs / 1.92 µs | 42.82 µs / 42.98 µs | 291.9 µs / 283.5 µs | 1115.9 µs / 1109.7 µs | 38.50 ms / 38.53 ms |
| Two-shot LL | 4.27 µs / 4.25 µs | 4.25 µs / 4.22 µs | 4.29 µs / 4.30 µs | 34.34 µs / 34.38 µs | 229.4 µs / 229.3 µs | 901.0 µs / 905.4 µs | 31.63 ms / 31.71 ms |
| Two-shot Sentinel | 3.49 µs / 3.49 µs | 3.56 µs / 3.55 µs | 3.67 µs / 3.68 µs | 36.54 µs / 36.55 µs | 230.5 µs / 230.8 µs | 912.3 µs / 916.6 µs | 32.78 ms / 32.77 ms |
| LL128 Atomic | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) |
| Bidirectional (2 ranks, 8 chunks per round) | 13.63 µs / 13.85 µs | 13.75 µs / 13.78 µs | 13.60 µs / 13.67 µs | 37.90 µs / 37.94 µs | 170.4 µs / 171.4 µs | 542.1 µs / 541.2 µs | 19.46 ms / 19.38 ms |

## 2. p99, 3 ranks (average of the passes)

99% of the rounds were faster than this.

| Algorithm | 32 B | 64 B | 128 B | 32 KB | 256 KB | 1 MB | 32 MB |
|---|---|---|---|---|---|---|---|
| Baseline (with barrier) | 9.59 µs | 9.92 µs | 9.59 µs | 47.74 µs | 248.6 µs | 922.2 µs | 39.77 ms |
| LL (one-shot) | 2.48 µs | 2.45 µs | 2.46 µs | 37.50 µs | 243.7 µs | 922.4 µs | 45.92 ms |
| Sentinel (one-shot) | 2.20 µs | 2.03 µs | 2.47 µs | 49.19 µs | 306.3 µs | 1130.2 µs | 43.56 ms |
| Two-shot LL | 5.63 µs | 5.37 µs | 5.63 µs | 38.33 µs | 288.9 µs | 1077.2 µs | 57.02 ms |
| Two-shot Sentinel | 4.67 µs | 4.48 µs | 4.60 µs | 41.61 µs | 245.8 µs | 957.6 µs | 35.70 ms |
| LL128 Atomic | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) |
| Bidirectional (2 ranks, 8 chunks per round) | 15.25 µs | 15.44 µs | 15.55 µs | 39.53 µs | 174.8 µs | 546.6 µs | 19.86 ms |

## 3. Cost of the barrier: Baseline vs LL, 3 ranks

|  | 32 B | 64 B | 128 B | 32 KB | 256 KB | 1 MB | 32 MB |
|---|---|---|---|---|---|---|---|
| Baseline - LL | 6.63 µs | 6.62 µs | 6.61 µs | 10.90 µs | 7.64 µs | 11.11 µs | -889.96 µs |
| Baseline / LL | 4.06x | 4.07x | 3.95x | 1.32x | 1.03x | 1.01x | 0.98x |

## 4. 2 ranks vs 3 ranks

|  | 32 B | 32 KB | 32 MB |
|---|---|---|---|
| Baseline (with barrier), 2 ranks | 4.41 µs | 22.71 µs | 22.96 ms |
| Baseline (with barrier), 3 ranks | 8.80 µs | 44.74 µs | 36.40 ms |
| LL (one-shot), 2 ranks | 1.79 µs | 22.10 µs | 22.99 ms |
| LL (one-shot), 3 ranks | 2.16 µs | 33.83 µs | 37.29 ms |

## 5. Atomics probe and negative test

Atomic_Probe (`logs/pass1_6_atomic_probe.txt`):

```
PROBE device atomic_cap=0 (0=NONE 1=HCA 2=GLOB) max_qp_rd_atom=16 max_qp_init_rd_atom=16
PROBE QP state=3 (3=RTS) access_flags=0xe (0x8=REMOTE_ATOMIC) max_rd_atomic=1 max_dest_rd_atomic=1 max_inline=188 max_send_sge=12
PROBE rank 0 -> peer 1, remote counter addr=1a2bdc0 rkey=1584970
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
PROBE rank 0 -> peer 1, remote counter addr=1873dc0 rkey=780030
PROBE 1 FETCH_AND_ADD, library result buf              post_send rc=22 (Invalid argument)
PROBE 2 FETCH_AND_ADD, fresh 64B-aligned result buf    post_send rc=22 (Invalid argument)
PROBE 3 CMP_AND_SWP                                    post_send rc=22 (Invalid argument)
PROBE 4 FETCH_AND_ADD after unsignaled inline WRITE    post_send rc=22 (Invalid argument)
PROBE remote counter should now equal the number of OK atomics above; reading it needs the peer (see its line)
PROBE device atomic_cap=0 (0=NONE 1=HCA 2=GLOB) max_qp_rd_atom=16 max_qp_init_rd_atom=16
PROBE QP state=3 (3=RTS) access_flags=0xe (0x8=REMOTE_ATOMIC) max_rd_atomic=1 max_dest_rd_atomic=1 max_inline=188 max_send_sge=12
PROBE rank 1 (counter host) final counter value = 0
```

LL without the wait (negative test) at 32 B (`logs/pass2_7_negative_test.txt`): 6 FAILED and 0 PASSED correctness lines. It must fail: this shows the check notices a missing wait.

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
| `pass2_7_negative_test.txt` | 0 | 12 | 0 | 0 | 0 | 6 | 0 |

Error messages (first one per rank):

- pass1 LL without the wait (negative test), 32 B, rank 0: `verify: 8 / 8 elements mismatched`
- pass1 LL without the wait (negative test), 32 B, rank 1: `verify: 8 / 8 elements mismatched`
- pass1 LL without the wait (negative test), 32 KB, rank 0: `verify: 2797 / 8192 elements mismatched`
- pass1 LL without the wait (negative test), 32 KB, rank 1: `verify: 2091 / 8192 elements mismatched`
- pass1 LL without the wait (negative test), 32 KB, rank 2: `verify: 5122 / 8192 elements mismatched`
- pass2 LL without the wait (negative test), 32 B, rank 0: `verify: 8 / 8 elements mismatched`
- pass2 LL without the wait (negative test), 32 B, rank 1: `verify: 8 / 8 elements mismatched`
- pass2 LL without the wait (negative test), 32 B, rank 2: `verify: 1 / 8 elements mismatched`
- pass2 LL without the wait (negative test), 32 KB, rank 0: `verify: 2230 / 8192 elements mismatched`
- pass2 LL without the wait (negative test), 32 KB, rank 1: `verify: 4077 / 8192 elements mismatched`
- pass2 LL without the wait (negative test), 32 KB, rank 2: `verify: 4024 / 8192 elements mismatched`

## 7. Every run, per rank

Times in µs. `-` = not printed by this version or the rank did not finish.

| pass | Program | ranks | size | rank | status | mean | min | median | p99 | max | correctness | exit |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| pass1 | Baseline (with barrier) | 3 | 32 B | 0 | ok | 7.99 | 7.49 | 8.22 | 8.93 | 15.33 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 32 B | 1 | ok | 7.99 | 6.65 | 9.06 | 9.43 | 16.89 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 32 B | 2 | ok | 7.99 | 5.95 | 9.00 | 9.57 | 16.03 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 32 B | 0 | ok | 2.22 | 2.10 | 2.18 | 2.55 | 11.04 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 32 B | 1 | ok | 2.22 | 2.08 | 2.17 | 2.50 | 11.29 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 32 B | 2 | ok | 2.22 | 2.10 | 2.18 | 2.54 | 10.81 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 32 B | 0 | ok | 1.93 | 1.81 | 1.86 | 2.28 | 18.33 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 32 B | 1 | ok | 1.93 | 1.80 | 1.85 | 2.68 | 18.34 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 32 B | 2 | ok | 1.93 | 1.79 | 1.86 | 2.31 | 17.63 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 32 B | 0 | ok | 4.32 | 4.17 | 4.28 | 5.02 | 11.16 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 32 B | 1 | ok | 4.32 | 4.06 | 4.26 | 5.72 | 11.22 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 32 B | 2 | ok | 4.32 | 4.14 | 4.26 | 5.50 | 11.40 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 32 B | 0 | ok | 3.55 | 3.42 | 3.50 | 4.10 | 13.37 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 32 B | 1 | ok | 3.56 | 3.40 | 3.48 | 5.01 | 14.01 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 32 B | 2 | ok | 3.55 | 3.41 | 3.48 | 4.73 | 13.72 | PASSED/PASSED | 0 |
| pass1 | LL128 Atomic | 3 | 32 B | 0 | skipped | - | - | - | - | - | - | 2 |
| pass1 | LL128 Atomic | 3 | 32 B | 1 | skipped | - | - | - | - | - | - | 2 |
| pass1 | LL128 Atomic | 3 | 32 B | 2 | skipped | - | - | - | - | - | - | 2 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 B | 0 | ok | 13.72 | 13.41 | 13.61 | 15.32 | 32.15 | PASSED | 0 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 B | 1 | ok | 13.72 | 13.37 | 13.65 | 15.38 | 31.82 | PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 64 B | 0 | ok | 8.03 | 6.09 | 8.28 | 9.35 | 19.06 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 64 B | 1 | ok | 8.03 | 6.63 | 9.11 | 9.81 | 17.26 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 64 B | 2 | ok | 8.03 | 6.30 | 9.08 | 9.83 | 19.95 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 64 B | 0 | ok | 2.20 | 2.10 | 2.17 | 2.41 | 12.19 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 64 B | 1 | ok | 2.20 | 2.07 | 2.16 | 2.39 | 12.78 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 64 B | 2 | ok | 2.20 | 2.07 | 2.16 | 2.39 | 12.63 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 64 B | 0 | ok | 1.90 | 1.79 | 1.86 | 2.06 | 12.16 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 64 B | 1 | ok | 1.89 | 1.80 | 1.86 | 2.05 | 12.75 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 64 B | 2 | ok | 1.90 | 1.80 | 1.86 | 1.98 | 12.51 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 64 B | 0 | ok | 4.28 | 4.14 | 4.26 | 4.88 | 6.96 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 64 B | 1 | ok | 4.28 | 4.08 | 4.25 | 5.27 | 8.15 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 64 B | 2 | ok | 4.28 | 3.77 | 4.25 | 5.36 | 7.70 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 64 B | 0 | ok | 3.61 | 3.47 | 3.56 | 4.20 | 21.07 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 64 B | 1 | ok | 3.62 | 3.48 | 3.56 | 5.00 | 20.02 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 64 B | 2 | ok | 3.62 | 3.47 | 3.56 | 4.63 | 21.32 | PASSED/PASSED | 0 |
| pass1 | LL128 Atomic | 3 | 64 B | 0 | skipped | - | - | - | - | - | - | 2 |
| pass1 | LL128 Atomic | 3 | 64 B | 1 | skipped | - | - | - | - | - | - | 2 |
| pass1 | LL128 Atomic | 3 | 64 B | 2 | skipped | - | - | - | - | - | - | 2 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 64 B | 0 | ok | 13.85 | 13.47 | 13.75 | 15.66 | 32.53 | PASSED | 0 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 64 B | 1 | ok | 13.85 | 13.40 | 13.76 | 16.18 | 32.61 | PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 128 B | 0 | ok | 8.04 | 6.21 | 8.25 | 8.70 | 18.27 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 128 B | 1 | ok | 8.04 | 6.70 | 9.12 | 9.54 | 19.75 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 128 B | 2 | ok | 8.04 | 6.14 | 9.06 | 9.46 | 17.47 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 128 B | 0 | ok | 2.27 | 2.13 | 2.26 | 2.48 | 8.26 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 128 B | 1 | ok | 2.27 | 2.12 | 2.23 | 2.52 | 9.63 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 128 B | 2 | ok | 2.27 | 2.12 | 2.25 | 2.44 | 9.04 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 128 B | 0 | ok | 1.94 | 1.85 | 1.91 | 2.38 | 7.33 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 128 B | 1 | ok | 1.94 | 1.85 | 1.91 | 2.59 | 7.43 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 128 B | 2 | ok | 1.94 | 1.77 | 1.91 | 2.35 | 7.46 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 128 B | 0 | ok | 4.33 | 4.20 | 4.29 | 5.10 | 9.27 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 128 B | 1 | ok | 4.33 | 4.17 | 4.28 | 5.73 | 9.94 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 128 B | 2 | ok | 4.32 | 4.18 | 4.29 | 5.37 | 9.97 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 128 B | 0 | ok | 3.70 | 3.58 | 3.67 | 4.29 | 8.19 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 128 B | 1 | ok | 3.70 | 3.56 | 3.67 | 4.28 | 8.70 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 128 B | 2 | ok | 3.71 | 3.59 | 3.67 | 4.38 | 8.62 | PASSED/PASSED | 0 |
| pass1 | LL128 Atomic | 3 | 128 B | 0 | skipped | - | - | - | - | - | - | 2 |
| pass1 | LL128 Atomic | 3 | 128 B | 1 | skipped | - | - | - | - | - | - | 2 |
| pass1 | LL128 Atomic | 3 | 128 B | 2 | skipped | - | - | - | - | - | - | 2 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 128 B | 0 | ok | 13.69 | 13.38 | 13.61 | 15.33 | 25.30 | PASSED | 0 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 128 B | 1 | ok | 13.69 | 13.42 | 13.59 | 15.28 | 24.50 | PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 32 KB | 0 | ok | 44.32 | 37.84 | 44.48 | 47.09 | 54.26 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 32 KB | 1 | ok | 44.32 | 37.59 | 44.69 | 48.10 | 55.15 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 32 KB | 2 | ok | 44.32 | 37.73 | 44.53 | 47.93 | 59.48 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 32 KB | 0 | ok | 34.20 | 27.33 | 33.82 | 37.19 | 52.12 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 32 KB | 1 | ok | 34.20 | 24.99 | 33.88 | 37.52 | 51.78 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 32 KB | 2 | ok | 34.19 | 21.54 | 33.80 | 37.39 | 50.99 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 32 KB | 0 | ok | 42.76 | 36.11 | 43.16 | 49.21 | 58.10 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 32 KB | 1 | ok | 42.76 | 34.82 | 42.63 | 49.79 | 58.68 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 32 KB | 2 | ok | 42.76 | 34.31 | 42.67 | 49.86 | 62.32 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 32 KB | 0 | ok | 34.32 | 32.40 | 34.49 | 36.84 | 47.80 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 32 KB | 1 | ok | 34.32 | 32.16 | 34.21 | 36.98 | 50.67 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 32 KB | 2 | ok | 34.32 | 32.53 | 34.31 | 36.11 | 49.80 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 32 KB | 0 | ok | 36.94 | 34.63 | 36.33 | 42.11 | 53.26 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 32 KB | 1 | ok | 36.95 | 34.28 | 36.70 | 41.04 | 52.30 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 32 KB | 2 | ok | 36.93 | 35.01 | 36.59 | 41.51 | 54.60 | PASSED/PASSED | 0 |
| pass1 | LL128 Atomic | 3 | 32 KB | 0 | skipped | - | - | - | - | - | - | 2 |
| pass1 | LL128 Atomic | 3 | 32 KB | 1 | skipped | - | - | - | - | - | - | 2 |
| pass1 | LL128 Atomic | 3 | 32 KB | 2 | skipped | - | - | - | - | - | - | 2 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 KB | 0 | ok | 37.96 | 37.64 | 37.90 | 39.54 | 40.74 | PASSED | 0 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 KB | 1 | ok | 37.97 | 37.65 | 37.90 | 39.73 | 46.35 | PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 256 KB | 0 | ok | 238.19 | 228.43 | 238.08 | 244.94 | 245.57 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 256 KB | 1 | ok | 238.63 | 234.65 | 238.15 | 245.34 | 271.88 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 256 KB | 2 | ok | 238.55 | 227.97 | 237.88 | 256.85 | 268.46 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 256 KB | 0 | ok | 230.65 | 221.24 | 230.66 | 245.50 | 245.87 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 256 KB | 1 | ok | 230.65 | 223.36 | 230.45 | 248.17 | 248.59 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 256 KB | 2 | ok | 230.62 | 211.98 | 230.70 | 249.59 | 250.53 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 256 KB | 0 | ok | 289.76 | 274.21 | 296.67 | 320.10 | 362.75 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 256 KB | 1 | ok | 289.90 | 273.51 | 290.59 | 301.88 | 351.98 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 256 KB | 2 | ok | 289.12 | 271.68 | 288.53 | 312.12 | 322.15 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 256 KB | 0 | ok | 233.51 | 225.71 | 229.47 | 290.97 | 291.32 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 256 KB | 1 | ok | 233.56 | 223.47 | 229.61 | 286.46 | 287.39 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 256 KB | 2 | ok | 233.41 | 221.82 | 229.00 | 290.88 | 292.48 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 256 KB | 0 | ok | 231.34 | 227.51 | 230.22 | 242.38 | 266.18 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 256 KB | 1 | ok | 231.35 | 224.69 | 230.95 | 246.26 | 271.24 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 256 KB | 2 | ok | 231.35 | 224.03 | 230.27 | 247.84 | 249.70 | PASSED/PASSED | 0 |
| pass1 | LL128 Atomic | 3 | 256 KB | 0 | skipped | - | - | - | - | - | - | 2 |
| pass1 | LL128 Atomic | 3 | 256 KB | 1 | skipped | - | - | - | - | - | - | 2 |
| pass1 | LL128 Atomic | 3 | 256 KB | 2 | skipped | - | - | - | - | - | - | 2 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 256 KB | 0 | ok | 170.39 | 161.94 | 170.36 | 175.07 | 176.34 | PASSED | 0 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 256 KB | 1 | ok | 170.48 | 169.12 | 170.42 | 175.11 | 176.30 | PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 1 MB | 0 | ok | 911.58 | 901.87 | 912.07 | 918.05 | 923.56 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 1 MB | 1 | ok | 912.97 | 906.42 | 911.48 | 917.79 | 1,044.74 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 1 MB | 2 | ok | 913.19 | 888.53 | 911.74 | 931.88 | 1,084.99 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 1 MB | 0 | ok | 900.34 | 875.61 | 900.54 | 927.00 | 927.18 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 1 MB | 1 | ok | 900.39 | 876.82 | 900.12 | 926.17 | 927.36 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 1 MB | 2 | ok | 900.44 | 880.45 | 900.81 | 919.42 | 923.24 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 1 MB | 0 | ok | 1,113.13 | 1,103.72 | 1,112.11 | 1,132.85 | 1,134.02 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 1 MB | 1 | ok | 1,114.10 | 1,096.87 | 1,117.81 | 1,128.05 | 1,190.51 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 1 MB | 2 | ok | 1,114.60 | 1,087.35 | 1,117.77 | 1,150.20 | 1,235.32 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 1 MB | 0 | ok | 915.55 | 888.11 | 900.38 | 1,134.55 | 1,135.29 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 1 MB | 1 | ok | 915.62 | 878.24 | 901.99 | 1,134.18 | 1,135.37 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 1 MB | 2 | ok | 916.08 | 887.99 | 900.61 | 1,136.91 | 1,138.74 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 1 MB | 0 | ok | 916.71 | 893.84 | 913.00 | 948.85 | 954.31 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 1 MB | 1 | ok | 917.44 | 877.53 | 911.04 | 961.50 | 980.43 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 1 MB | 2 | ok | 917.63 | 876.09 | 912.90 | 957.48 | 1,004.43 | PASSED/PASSED | 0 |
| pass1 | LL128 Atomic | 3 | 1 MB | 0 | skipped | - | - | - | - | - | - | 2 |
| pass1 | LL128 Atomic | 3 | 1 MB | 1 | skipped | - | - | - | - | - | - | 2 |
| pass1 | LL128 Atomic | 3 | 1 MB | 2 | skipped | - | - | - | - | - | - | 2 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 1 MB | 0 | ok | 542.45 | 539.02 | 542.19 | 547.26 | 550.35 | PASSED | 0 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 1 MB | 1 | ok | 542.29 | 536.53 | 542.08 | 547.52 | 548.85 | PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 32 MB | 0 | ok | 36,447.48 | 36,032.30 | 36,337.03 | 38,196.03 | 38,386.61 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 32 MB | 1 | ok | 36,455.57 | 35,983.92 | 36,350.58 | 38,153.39 | 38,243.13 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 32 MB | 2 | ok | 36,408.98 | 32,129.25 | 36,348.45 | 42,289.68 | 42,400.73 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 32 MB | 0 | ok | 38,529.74 | 29,710.19 | 38,000.25 | 50,399.00 | 51,975.37 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 32 MB | 1 | ok | 38,535.94 | 31,866.14 | 38,091.09 | 50,477.15 | 52,934.46 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 32 MB | 2 | ok | 38,493.60 | 31,792.09 | 38,106.10 | 49,405.28 | 57,790.65 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 32 MB | 0 | ok | 38,794.24 | 38,218.15 | 38,481.53 | 44,033.58 | 44,481.94 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 32 MB | 1 | ok | 38,798.98 | 38,151.42 | 38,498.77 | 43,739.49 | 43,976.86 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 32 MB | 2 | ok | 38,755.85 | 36,609.46 | 38,509.20 | 43,851.07 | 45,082.34 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 32 MB | 0 | ok | 34,485.15 | 31,188.75 | 31,613.42 | 75,947.87 | 86,797.49 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 32 MB | 1 | ok | 34,482.00 | 30,925.98 | 31,651.51 | 76,032.63 | 87,834.67 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 32 MB | 2 | ok | 34,447.61 | 30,161.67 | 31,632.40 | 78,773.99 | 81,514.77 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 32 MB | 0 | ok | 32,776.76 | 31,923.21 | 32,760.76 | 34,955.23 | 35,023.30 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 32 MB | 1 | ok | 32,786.36 | 32,031.08 | 32,807.04 | 35,307.46 | 36,178.79 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 32 MB | 2 | ok | 32,766.88 | 31,617.25 | 32,782.40 | 36,508.78 | 37,349.58 | PASSED/PASSED | 0 |
| pass1 | LL128 Atomic | 3 | 32 MB | 0 | skipped | - | - | - | - | - | - | 2 |
| pass1 | LL128 Atomic | 3 | 32 MB | 1 | skipped | - | - | - | - | - | - | 2 |
| pass1 | LL128 Atomic | 3 | 32 MB | 2 | skipped | - | - | - | - | - | - | 2 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 MB | 0 | ok | 19,460.68 | 19,157.07 | 19,470.70 | 19,912.60 | 20,386.35 | PASSED | 0 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 MB | 1 | ok | 19,459.11 | 19,220.97 | 19,452.38 | 20,042.52 | 20,484.13 | PASSED | 0 |
| pass1 | Baseline (with barrier) | 2 | 32 B | 0 | ok | 4.11 | 2.35 | 4.39 | 4.65 | 14.91 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 2 | 32 B | 1 | ok | 4.11 | 3.58 | 4.39 | 4.56 | 13.71 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 2 | 32 B | 0 | ok | 1.83 | 1.70 | 1.76 | 2.07 | 17.98 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 2 | 32 B | 1 | ok | 1.83 | 1.67 | 1.77 | 2.13 | 17.59 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 2 | 32 KB | 0 | ok | 22.55 | 21.69 | 22.56 | 24.66 | 31.79 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 2 | 32 KB | 1 | ok | 22.55 | 20.97 | 22.78 | 23.86 | 33.18 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 2 | 32 KB | 0 | ok | 19.68 | 12.65 | 23.53 | 26.67 | 29.09 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 2 | 32 KB | 1 | ok | 19.70 | 12.84 | 23.36 | 26.96 | 29.14 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 2 | 32 MB | 0 | ok | 22,978.05 | 22,158.74 | 22,969.15 | 23,563.35 | 24,260.13 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 2 | 32 MB | 1 | ok | 22,977.85 | 22,512.75 | 22,972.40 | 23,763.32 | 23,816.94 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 2 | 32 MB | 0 | ok | 22,746.57 | 13,320.84 | 23,022.22 | 32,489.88 | 33,025.70 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 2 | 32 MB | 1 | ok | 22,833.95 | 13,202.91 | 23,389.06 | 32,530.07 | 33,584.03 | PASSED/PASSED | 0 |
| pass1 | Atomic_Probe | 2 | 32 B | 0 | no result | - | - | - | - | - | - | 0 |
| pass1 | Atomic_Probe | 2 | 32 B | 1 | no result | - | - | - | - | - | - | 0 |
| pass1 | LL without the wait (negative test) | 3 | 32 B | 0 | wrong result | 2.19 | 2.12 | 2.18 | 2.33 | 3.98 | FAILED/FAILED | 1 |
| pass1 | LL without the wait (negative test) | 3 | 32 B | 1 | wrong result | 2.21 | 2.13 | 2.19 | 2.35 | 7.42 | FAILED/FAILED | 1 |
| pass1 | LL without the wait (negative test) | 3 | 32 B | 2 | wrong result | 2.23 | 2.13 | 2.20 | 2.38 | 7.94 | PASSED/FAILED | 0 |
| pass1 | LL without the wait (negative test) | 3 | 32 KB | 0 | wrong result | 25.56 | 23.56 | 24.78 | 30.93 | 35.94 | FAILED/FAILED | 1 |
| pass1 | LL without the wait (negative test) | 3 | 32 KB | 1 | wrong result | 28.16 | 23.52 | 27.15 | 38.88 | 41.85 | FAILED/FAILED | 1 |
| pass1 | LL without the wait (negative test) | 3 | 32 KB | 2 | wrong result | 25.65 | 23.38 | 24.81 | 31.77 | 42.02 | FAILED/FAILED | 1 |
| pass2 | Baseline (with barrier) | 3 | 32 B | 0 | ok | 8.04 | 7.48 | 8.29 | 9.69 | 24.00 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 32 B | 1 | ok | 8.03 | 6.66 | 9.13 | 9.80 | 22.35 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 32 B | 2 | ok | 8.04 | 5.98 | 9.08 | 10.11 | 25.03 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 32 B | 0 | ok | 2.19 | 2.09 | 2.15 | 2.43 | 10.67 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 32 B | 1 | ok | 2.19 | 2.10 | 2.15 | 2.38 | 10.88 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 32 B | 2 | ok | 2.19 | 2.09 | 2.15 | 2.46 | 10.36 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 32 B | 0 | ok | 1.89 | 1.79 | 1.85 | 2.00 | 8.84 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 32 B | 1 | ok | 1.88 | 1.81 | 1.85 | 1.99 | 8.79 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 32 B | 2 | ok | 1.89 | 1.79 | 1.85 | 1.95 | 10.25 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 32 B | 0 | ok | 4.30 | 4.10 | 4.25 | 5.96 | 11.37 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 32 B | 1 | ok | 4.30 | 4.10 | 4.25 | 5.84 | 11.35 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 32 B | 2 | ok | 4.30 | 4.12 | 4.25 | 5.73 | 11.34 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 32 B | 0 | ok | 3.54 | 3.43 | 3.48 | 4.68 | 13.13 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 32 B | 1 | ok | 3.54 | 3.42 | 3.49 | 4.92 | 13.71 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 32 B | 2 | ok | 3.54 | 3.42 | 3.49 | 4.56 | 13.19 | PASSED/PASSED | 0 |
| pass2 | LL128 Atomic | 3 | 32 B | 0 | skipped | - | - | - | - | - | - | 2 |
| pass2 | LL128 Atomic | 3 | 32 B | 1 | skipped | - | - | - | - | - | - | 2 |
| pass2 | LL128 Atomic | 3 | 32 B | 2 | skipped | - | - | - | - | - | - | 2 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 B | 0 | ok | 13.90 | 13.40 | 13.85 | 15.24 | 22.21 | PASSED | 0 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 B | 1 | ok | 13.90 | 13.47 | 13.85 | 15.06 | 22.30 | PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 64 B | 0 | ok | 8.02 | 6.15 | 8.25 | 9.78 | 13.87 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 64 B | 1 | ok | 8.02 | 6.65 | 9.08 | 10.24 | 15.43 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 64 B | 2 | ok | 8.01 | 6.68 | 8.90 | 10.49 | 13.63 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 64 B | 0 | ok | 2.19 | 2.09 | 2.16 | 2.47 | 10.93 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 64 B | 1 | ok | 2.19 | 2.10 | 2.16 | 2.51 | 11.23 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 64 B | 2 | ok | 2.19 | 2.09 | 2.16 | 2.54 | 10.77 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 64 B | 0 | ok | 1.90 | 1.79 | 1.86 | 2.03 | 11.52 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 64 B | 1 | ok | 1.90 | 1.80 | 1.86 | 2.04 | 11.59 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 64 B | 2 | ok | 1.90 | 1.80 | 1.86 | 2.02 | 11.63 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 64 B | 0 | ok | 4.26 | 4.11 | 4.22 | 5.34 | 14.64 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 64 B | 1 | ok | 4.26 | 4.12 | 4.22 | 5.74 | 14.88 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 64 B | 2 | ok | 4.26 | 4.13 | 4.22 | 5.62 | 14.36 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 64 B | 0 | ok | 3.60 | 3.48 | 3.55 | 4.43 | 13.69 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 64 B | 1 | ok | 3.60 | 3.47 | 3.55 | 4.42 | 12.89 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 64 B | 2 | ok | 3.60 | 3.49 | 3.56 | 4.17 | 13.20 | PASSED/PASSED | 0 |
| pass2 | LL128 Atomic | 3 | 64 B | 0 | skipped | - | - | - | - | - | - | 2 |
| pass2 | LL128 Atomic | 3 | 64 B | 1 | skipped | - | - | - | - | - | - | 2 |
| pass2 | LL128 Atomic | 3 | 64 B | 2 | skipped | - | - | - | - | - | - | 2 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 64 B | 0 | ok | 13.85 | 13.57 | 13.78 | 14.81 | 23.80 | PASSED | 0 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 64 B | 1 | ok | 13.85 | 13.57 | 13.79 | 15.10 | 24.35 | PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 128 B | 0 | ok | 8.18 | 7.62 | 8.35 | 9.68 | 21.19 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 128 B | 1 | ok | 8.18 | 6.76 | 9.22 | 10.02 | 20.92 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 128 B | 2 | ok | 8.18 | 6.11 | 9.15 | 10.15 | 20.73 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 128 B | 0 | ok | 2.27 | 2.14 | 2.25 | 2.47 | 9.42 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 128 B | 1 | ok | 2.27 | 2.10 | 2.23 | 2.49 | 10.25 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 128 B | 2 | ok | 2.26 | 2.11 | 2.25 | 2.40 | 7.06 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 128 B | 0 | ok | 1.96 | 1.86 | 1.92 | 2.35 | 9.60 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 128 B | 1 | ok | 1.96 | 1.85 | 1.92 | 2.58 | 9.74 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 128 B | 2 | ok | 1.96 | 1.84 | 1.92 | 2.58 | 9.79 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 128 B | 0 | ok | 4.35 | 4.20 | 4.30 | 5.74 | 12.32 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 128 B | 1 | ok | 4.35 | 4.15 | 4.30 | 5.96 | 13.58 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 128 B | 2 | ok | 4.35 | 4.17 | 4.29 | 5.91 | 12.90 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 128 B | 0 | ok | 3.72 | 3.59 | 3.68 | 4.63 | 8.02 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 128 B | 1 | ok | 3.72 | 3.58 | 3.68 | 5.14 | 8.51 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 128 B | 2 | ok | 3.72 | 3.57 | 3.68 | 4.85 | 8.40 | PASSED/PASSED | 0 |
| pass2 | LL128 Atomic | 3 | 128 B | 0 | skipped | - | - | - | - | - | - | 2 |
| pass2 | LL128 Atomic | 3 | 128 B | 1 | skipped | - | - | - | - | - | - | 2 |
| pass2 | LL128 Atomic | 3 | 128 B | 2 | skipped | - | - | - | - | - | - | 2 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 128 B | 0 | ok | 13.74 | 13.45 | 13.66 | 15.63 | 21.94 | PASSED | 0 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 128 B | 1 | ok | 13.74 | 13.36 | 13.69 | 15.94 | 21.77 | PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 32 KB | 0 | ok | 44.56 | 40.17 | 44.99 | 47.30 | 56.63 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 32 KB | 1 | ok | 44.55 | 38.06 | 44.99 | 47.77 | 58.21 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 32 KB | 2 | ok | 44.57 | 34.72 | 44.73 | 48.27 | 63.34 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 32 KB | 0 | ok | 34.20 | 27.60 | 33.82 | 37.35 | 53.39 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 32 KB | 1 | ok | 34.21 | 27.51 | 33.90 | 38.16 | 51.83 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 32 KB | 2 | ok | 34.17 | 21.16 | 33.78 | 37.40 | 51.57 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 32 KB | 0 | ok | 42.91 | 34.58 | 43.35 | 49.16 | 59.89 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 32 KB | 1 | ok | 42.91 | 34.74 | 42.72 | 47.65 | 59.04 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 32 KB | 2 | ok | 42.92 | 34.31 | 42.89 | 49.45 | 62.13 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 32 KB | 0 | ok | 34.47 | 32.05 | 34.49 | 40.00 | 50.54 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 32 KB | 1 | ok | 34.48 | 33.15 | 34.28 | 40.01 | 51.10 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 32 KB | 2 | ok | 34.47 | 32.55 | 34.38 | 40.03 | 52.51 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 32 KB | 0 | ok | 37.01 | 34.66 | 36.31 | 42.34 | 49.64 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 32 KB | 1 | ok | 37.01 | 33.57 | 36.70 | 41.29 | 56.92 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 32 KB | 2 | ok | 36.99 | 35.23 | 36.63 | 41.39 | 53.90 | PASSED/PASSED | 0 |
| pass2 | LL128 Atomic | 3 | 32 KB | 0 | skipped | - | - | - | - | - | - | 2 |
| pass2 | LL128 Atomic | 3 | 32 KB | 1 | skipped | - | - | - | - | - | - | 2 |
| pass2 | LL128 Atomic | 3 | 32 KB | 2 | skipped | - | - | - | - | - | - | 2 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 KB | 0 | ok | 38.01 | 37.59 | 37.95 | 39.46 | 49.11 | PASSED | 0 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 KB | 1 | ok | 38.01 | 37.62 | 37.94 | 39.38 | 49.16 | PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 256 KB | 0 | ok | 238.36 | 228.80 | 238.14 | 246.10 | 246.35 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 256 KB | 1 | ok | 238.81 | 233.89 | 238.14 | 245.90 | 274.00 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 256 KB | 2 | ok | 238.75 | 230.05 | 238.72 | 252.68 | 274.86 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 256 KB | 0 | ok | 230.40 | 222.37 | 230.39 | 240.03 | 240.49 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 256 KB | 1 | ok | 230.77 | 222.69 | 230.42 | 239.61 | 258.21 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 256 KB | 2 | ok | 230.41 | 223.53 | 230.64 | 239.19 | 240.47 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 256 KB | 0 | ok | 283.73 | 274.55 | 283.29 | 301.57 | 303.96 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 256 KB | 1 | ok | 283.67 | 272.89 | 283.76 | 300.04 | 300.55 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 256 KB | 2 | ok | 283.60 | 272.79 | 283.55 | 302.22 | 303.07 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 256 KB | 0 | ok | 235.62 | 226.21 | 229.04 | 290.59 | 290.80 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 256 KB | 1 | ok | 235.62 | 222.95 | 229.64 | 285.65 | 286.47 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 256 KB | 2 | ok | 235.69 | 225.02 | 229.23 | 288.76 | 291.02 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 256 KB | 0 | ok | 231.59 | 227.85 | 230.64 | 243.11 | 247.10 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 256 KB | 1 | ok | 231.89 | 222.22 | 230.82 | 247.80 | 265.19 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 256 KB | 2 | ok | 231.95 | 223.18 | 230.91 | 247.15 | 276.14 | PASSED/PASSED | 0 |
| pass2 | LL128 Atomic | 3 | 256 KB | 0 | skipped | - | - | - | - | - | - | 2 |
| pass2 | LL128 Atomic | 3 | 256 KB | 1 | skipped | - | - | - | - | - | - | 2 |
| pass2 | LL128 Atomic | 3 | 256 KB | 2 | skipped | - | - | - | - | - | - | 2 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 256 KB | 0 | ok | 171.41 | 162.66 | 171.33 | 174.53 | 174.85 | PASSED | 0 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 256 KB | 1 | ok | 171.49 | 170.21 | 171.42 | 174.38 | 174.85 | PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 1 MB | 0 | ok | 911.55 | 904.88 | 911.57 | 916.33 | 917.74 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 1 MB | 1 | ok | 912.78 | 907.25 | 911.61 | 917.40 | 1,029.51 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 1 MB | 2 | ok | 912.56 | 887.80 | 911.09 | 931.76 | 1,024.78 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 1 MB | 0 | ok | 900.08 | 575.76 | 900.63 | 922.50 | 1,222.54 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 1 MB | 1 | ok | 903.39 | 879.96 | 900.29 | 922.23 | 1,224.84 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 1 MB | 2 | ok | 903.88 | 884.56 | 900.54 | 917.17 | 1,271.98 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 1 MB | 0 | ok | 1,110.25 | 1,099.06 | 1,109.55 | 1,121.18 | 1,122.63 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 1 MB | 1 | ok | 1,111.00 | 1,104.14 | 1,109.20 | 1,122.39 | 1,181.36 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 1 MB | 2 | ok | 1,111.36 | 1,093.74 | 1,110.35 | 1,126.77 | 1,218.48 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 1 MB | 0 | ok | 916.93 | 892.67 | 902.78 | 1,028.70 | 1,132.39 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 1 MB | 1 | ok | 917.27 | 880.95 | 909.23 | 1,004.64 | 1,123.50 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 1 MB | 2 | ok | 918.12 | 888.68 | 904.23 | 1,024.03 | 1,136.81 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 1 MB | 0 | ok | 919.39 | 892.38 | 914.76 | 951.39 | 1,001.71 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 1 MB | 1 | ok | 919.82 | 872.34 | 918.20 | 957.55 | 961.58 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 1 MB | 2 | ok | 919.57 | 876.04 | 916.92 | 968.99 | 996.27 | PASSED/PASSED | 0 |
| pass2 | LL128 Atomic | 3 | 1 MB | 0 | skipped | - | - | - | - | - | - | 2 |
| pass2 | LL128 Atomic | 3 | 1 MB | 1 | skipped | - | - | - | - | - | - | 2 |
| pass2 | LL128 Atomic | 3 | 1 MB | 2 | skipped | - | - | - | - | - | - | 2 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 1 MB | 0 | ok | 541.71 | 538.84 | 541.14 | 545.74 | 558.55 | PASSED | 0 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 1 MB | 1 | ok | 541.56 | 539.13 | 541.28 | 545.87 | 547.04 | PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 32 MB | 0 | ok | 36,568.86 | 36,228.52 | 36,472.79 | 38,291.73 | 38,384.31 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 32 MB | 1 | ok | 36,580.23 | 36,015.26 | 36,457.09 | 38,789.31 | 38,941.42 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 32 MB | 2 | ok | 36,537.19 | 32,093.19 | 36,446.34 | 42,873.43 | 42,894.23 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 32 MB | 0 | ok | 36,439.63 | 31,364.10 | 36,471.20 | 41,315.84 | 42,187.32 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 32 MB | 1 | ok | 36,443.21 | 31,380.92 | 36,734.69 | 41,346.76 | 41,698.06 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 32 MB | 2 | ok | 36,407.60 | 30,754.39 | 36,348.70 | 42,600.90 | 43,087.62 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 32 MB | 0 | ok | 38,788.28 | 38,248.94 | 38,523.94 | 42,880.48 | 43,441.62 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 32 MB | 1 | ok | 38,793.21 | 38,195.23 | 38,547.04 | 42,747.69 | 43,396.86 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 32 MB | 2 | ok | 38,746.18 | 36,357.93 | 38,531.23 | 44,108.93 | 45,044.09 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 32 MB | 0 | ok | 31,836.39 | 31,081.24 | 31,668.65 | 38,102.17 | 38,113.49 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 32 MB | 1 | ok | 31,847.67 | 31,058.84 | 31,723.83 | 35,601.19 | 36,790.91 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 32 MB | 2 | ok | 31,820.92 | 30,131.56 | 31,733.80 | 37,659.54 | 38,424.42 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 32 MB | 0 | ok | 32,724.83 | 31,964.92 | 32,750.15 | 35,118.99 | 35,373.78 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 32 MB | 1 | ok | 32,721.59 | 32,121.09 | 32,794.53 | 35,186.91 | 35,632.74 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 32 MB | 2 | ok | 32,716.36 | 31,668.13 | 32,764.81 | 37,094.64 | 37,519.22 | PASSED/PASSED | 0 |
| pass2 | LL128 Atomic | 3 | 32 MB | 0 | skipped | - | - | - | - | - | - | 2 |
| pass2 | LL128 Atomic | 3 | 32 MB | 1 | skipped | - | - | - | - | - | - | 2 |
| pass2 | LL128 Atomic | 3 | 32 MB | 2 | skipped | - | - | - | - | - | - | 2 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 MB | 0 | ok | 19,387.30 | 18,979.85 | 19,373.82 | 19,925.54 | 20,392.39 | PASSED | 0 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 MB | 1 | ok | 19,384.61 | 19,128.42 | 19,388.18 | 19,555.64 | 20,248.37 | PASSED | 0 |
| pass2 | Baseline (with barrier) | 2 | 32 B | 0 | ok | 4.26 | 2.67 | 4.44 | 5.09 | 20.96 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 2 | 32 B | 1 | ok | 4.27 | 2.96 | 4.42 | 5.09 | 19.13 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 2 | 32 B | 0 | ok | 1.87 | 1.73 | 1.82 | 2.08 | 20.53 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 2 | 32 B | 1 | ok | 1.88 | 1.71 | 1.82 | 2.21 | 19.32 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 2 | 32 KB | 0 | ok | 22.57 | 21.64 | 22.68 | 24.57 | 30.94 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 2 | 32 KB | 1 | ok | 22.57 | 20.58 | 22.84 | 23.81 | 34.01 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 2 | 32 KB | 0 | ok | 19.50 | 12.62 | 20.79 | 25.97 | 30.38 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 2 | 32 KB | 1 | ok | 19.51 | 12.91 | 20.71 | 26.50 | 35.05 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 2 | 32 MB | 0 | ok | 22,948.89 | 22,425.23 | 22,937.04 | 23,700.97 | 23,798.70 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 2 | 32 MB | 1 | ok | 22,951.16 | 22,192.46 | 22,961.39 | 23,446.30 | 23,753.14 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 2 | 32 MB | 0 | ok | 22,773.70 | 13,436.61 | 22,947.94 | 32,345.53 | 32,894.21 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 2 | 32 MB | 1 | ok | 22,683.12 | 13,267.46 | 22,614.04 | 32,487.44 | 33,258.17 | PASSED/PASSED | 0 |
| pass2 | Atomic_Probe | 2 | 32 B | 0 | no result | - | - | - | - | - | - | 0 |
| pass2 | Atomic_Probe | 2 | 32 B | 1 | no result | - | - | - | - | - | - | 0 |
| pass2 | LL without the wait (negative test) | 3 | 32 B | 0 | wrong result | 2.21 | 2.12 | 2.19 | 2.35 | 6.50 | FAILED/FAILED | 1 |
| pass2 | LL without the wait (negative test) | 3 | 32 B | 1 | wrong result | 2.19 | 2.12 | 2.18 | 2.32 | 4.40 | FAILED/FAILED | 1 |
| pass2 | LL without the wait (negative test) | 3 | 32 B | 2 | wrong result | 2.21 | 2.12 | 2.19 | 2.34 | 8.03 | FAILED/FAILED | 1 |
| pass2 | LL without the wait (negative test) | 3 | 32 KB | 0 | wrong result | 25.76 | 23.46 | 24.83 | 31.08 | 37.22 | FAILED/FAILED | 1 |
| pass2 | LL without the wait (negative test) | 3 | 32 KB | 1 | wrong result | 29.24 | 23.50 | 27.75 | 38.88 | 41.84 | FAILED/FAILED | 1 |
| pass2 | LL without the wait (negative test) | 3 | 32 KB | 2 | wrong result | 26.00 | 23.39 | 25.05 | 31.82 | 40.87 | FAILED/FAILED | 1 |

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

