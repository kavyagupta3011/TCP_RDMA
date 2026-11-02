# Results: v5 (paper_algorithms_v5)

**v5**: v4 + one shared protection domain, Two-shot sends straight from input/output.

Machines: rank 0 = compute02, rank 1 = compute03, rank 2 = compute04. PBS jobs: 30219.master.local, 30220.master.local.

A value is the time of one AllReduce (median over the timed rounds, averaged over the ranks that
finished correctly). Cells with two numbers are **pass 1 / pass 2**: the same experiment run twice,
the second time with the versions in the opposite order. If a rank crashed, hung (stopped by the
timeout) or got a wrong result, the cell says so. 1000 timed rounds for sizes up to 32 KB, 100
for 256 KB and above.

## 1. Median, 3 ranks (pass 1 / pass 2)

| Algorithm | 32 B | 64 B | 128 B | 32 KB | 256 KB | 1 MB | 32 MB |
|---|---|---|---|---|---|---|---|
| Baseline (with barrier) | 8.76 µs / 8.70 µs | 8.73 µs / 8.72 µs | 8.89 µs / 8.90 µs | 44.67 µs / 44.45 µs | 237.9 µs / 238.4 µs | 912.4 µs / 913.6 µs | 36.38 ms / 36.41 ms |
| LL (one-shot) | 2.15 µs / 2.16 µs | 2.16 µs / 2.17 µs | 2.24 µs / 2.24 µs | 33.78 µs / 33.82 µs | 230.1 µs / 230.2 µs | 902.1 µs / 900.4 µs | 36.42 ms / 36.39 ms |
| Sentinel (one-shot) | 1.85 µs / 1.86 µs | 1.86 µs / 1.87 µs | 1.91 µs / 1.91 µs | 42.86 µs / 42.80 µs | 283.3 µs / 291.7 µs | 1110.3 µs / 1113.6 µs | 38.40 ms / 38.49 ms |
| Two-shot LL | 4.24 µs / 4.24 µs | 4.21 µs / 4.23 µs | 4.25 µs / 4.27 µs | 24.58 µs / 24.58 µs | 149.7 µs / 149.7 µs | 579.2 µs / 578.6 µs | 21.96 ms / 21.87 ms |
| Two-shot Sentinel | 3.49 µs / 3.53 µs | 3.54 µs / 3.57 µs | 3.69 µs / 3.69 µs | 31.62 µs / 31.71 µs | 187.0 µs / 187.2 µs | 705.0 µs / 707.8 µs | 24.64 ms / 24.61 ms |
| LL128 Atomic | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) |
| Bidirectional (2 ranks, 8 chunks per round) | 13.68 µs / 13.67 µs | 13.64 µs / 13.39 µs | 13.71 µs / 13.56 µs | 37.97 µs / 37.93 µs | 172.0 µs / 170.7 µs | 542.5 µs / 541.5 µs | 19.44 ms / 19.44 ms |

## 2. p99, 3 ranks (average of the passes)

99% of the rounds were faster than this.

| Algorithm | 32 B | 64 B | 128 B | 32 KB | 256 KB | 1 MB | 32 MB |
|---|---|---|---|---|---|---|---|
| Baseline (with barrier) | 9.50 µs | 9.64 µs | 9.80 µs | 47.58 µs | 247.5 µs | 923.0 µs | 39.61 ms |
| LL (one-shot) | 2.42 µs | 2.76 µs | 2.80 µs | 38.29 µs | 240.8 µs | 919.5 µs | 40.84 ms |
| Sentinel (one-shot) | 2.31 µs | 2.01 µs | 2.41 µs | 48.94 µs | 302.1 µs | 1140.8 µs | 42.91 ms |
| Two-shot LL | 5.53 µs | 5.45 µs | 5.65 µs | 25.72 µs | 156.4 µs | 585.5 µs | 44.92 ms |
| Two-shot Sentinel | 4.91 µs | 5.19 µs | 4.78 µs | 35.43 µs | 197.9 µs | 722.0 µs | 26.68 ms |
| LL128 Atomic | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) |
| Bidirectional (2 ranks, 8 chunks per round) | 15.03 µs | 15.28 µs | 15.44 µs | 39.52 µs | 175.9 µs | 547.5 µs | 19.91 ms |

## 3. Cost of the barrier: Baseline vs LL, 3 ranks

|  | 32 B | 64 B | 128 B | 32 KB | 256 KB | 1 MB | 32 MB |
|---|---|---|---|---|---|---|---|
| Baseline - LL | 6.58 µs | 6.57 µs | 6.65 µs | 10.76 µs | 7.98 µs | 11.75 µs | -10.28 µs |
| Baseline / LL | 4.06x | 4.04x | 3.97x | 1.32x | 1.03x | 1.01x | 1.00x |

## 4. 2 ranks vs 3 ranks

|  | 32 B | 32 KB | 32 MB |
|---|---|---|---|
| Baseline (with barrier), 2 ranks | 4.40 µs | 23.13 µs | 22.98 ms |
| Baseline (with barrier), 3 ranks | 8.73 µs | 44.56 µs | 36.40 ms |
| LL (one-shot), 2 ranks | 1.75 µs | 21.11 µs | 23.09 ms |
| LL (one-shot), 3 ranks | 2.15 µs | 33.80 µs | 36.41 ms |

## 5. Atomics probe and negative test

Atomic_Probe (`logs/pass1_6_atomic_probe.txt`):

```
PROBE device atomic_cap=0 (0=NONE 1=HCA 2=GLOB) max_qp_rd_atom=16 max_qp_init_rd_atom=16
PROBE QP state=3 (3=RTS) access_flags=0xe (0x8=REMOTE_ATOMIC) max_rd_atomic=1 max_dest_rd_atomic=1 max_inline=188 max_send_sge=12
PROBE rank 0 -> peer 1, remote counter addr=1e6bdc0 rkey=12452
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
PROBE rank 0 -> peer 1, remote counter addr=127ddc0 rkey=1057122
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
- pass1 LL without the wait (negative test), 32 KB, rank 0: `verify: 1138 / 8192 elements mismatched`
- pass1 LL without the wait (negative test), 32 KB, rank 1: `verify: 3161 / 8192 elements mismatched`
- pass1 LL without the wait (negative test), 32 KB, rank 2: `verify: 5420 / 8192 elements mismatched`
- pass2 LL without the wait (negative test), 32 B, rank 0: `verify: 8 / 8 elements mismatched`
- pass2 LL without the wait (negative test), 32 B, rank 1: `verify: 8 / 8 elements mismatched`
- pass2 LL without the wait (negative test), 32 B, rank 2: `verify: 8 / 8 elements mismatched`
- pass2 LL without the wait (negative test), 32 KB, rank 0: `verify: 2274 / 8192 elements mismatched`
- pass2 LL without the wait (negative test), 32 KB, rank 1: `verify: 3115 / 8192 elements mismatched`
- pass2 LL without the wait (negative test), 32 KB, rank 2: `verify: 3228 / 8192 elements mismatched`

## 7. Every run, per rank

Times in µs. `-` = not printed by this version or the rank did not finish.

| pass | Program | ranks | size | rank | status | mean | min | median | p99 | max | correctness | exit |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| pass1 | Baseline (with barrier) | 3 | 32 B | 0 | ok | 8.02 | 7.50 | 8.20 | 8.69 | 17.60 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 32 B | 1 | ok | 8.01 | 6.65 | 9.05 | 9.58 | 14.99 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 32 B | 2 | ok | 8.02 | 6.60 | 9.03 | 10.22 | 18.33 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 32 B | 0 | ok | 2.18 | 2.08 | 2.15 | 2.51 | 7.86 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 32 B | 1 | ok | 2.18 | 2.08 | 2.15 | 2.45 | 8.10 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 32 B | 2 | ok | 2.18 | 2.09 | 2.15 | 2.44 | 7.55 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 32 B | 0 | ok | 1.88 | 1.80 | 1.85 | 2.08 | 8.36 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 32 B | 1 | ok | 1.88 | 1.79 | 1.85 | 2.05 | 8.31 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 32 B | 2 | ok | 1.88 | 1.80 | 1.85 | 2.04 | 8.23 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 32 B | 0 | ok | 4.28 | 4.09 | 4.24 | 5.62 | 13.54 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 32 B | 1 | ok | 4.29 | 4.10 | 4.24 | 5.58 | 13.84 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 32 B | 2 | ok | 4.29 | 4.15 | 4.25 | 5.00 | 13.15 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 32 B | 0 | ok | 3.56 | 3.41 | 3.49 | 4.10 | 20.45 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 32 B | 1 | ok | 3.56 | 3.42 | 3.49 | 4.50 | 20.39 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 32 B | 2 | ok | 3.56 | 3.29 | 3.49 | 4.29 | 19.15 | PASSED/PASSED | 0 |
| pass1 | LL128 Atomic | 3 | 32 B | 0 | skipped | - | - | - | - | - | - | 2 |
| pass1 | LL128 Atomic | 3 | 32 B | 1 | skipped | - | - | - | - | - | - | 2 |
| pass1 | LL128 Atomic | 3 | 32 B | 2 | skipped | - | - | - | - | - | - | 2 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 B | 0 | ok | 13.77 | 13.31 | 13.70 | 14.98 | 32.93 | PASSED | 0 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 B | 1 | ok | 13.77 | 13.44 | 13.67 | 14.94 | 32.06 | PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 64 B | 0 | ok | 8.02 | 6.07 | 8.21 | 9.16 | 17.23 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 64 B | 1 | ok | 8.01 | 6.68 | 9.05 | 9.99 | 15.60 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 64 B | 2 | ok | 8.01 | 5.54 | 8.93 | 10.26 | 18.01 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 64 B | 0 | ok | 2.18 | 2.09 | 2.16 | 2.41 | 8.19 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 64 B | 1 | ok | 2.18 | 2.10 | 2.16 | 2.46 | 8.16 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 64 B | 2 | ok | 2.19 | 2.09 | 2.16 | 2.44 | 8.19 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 64 B | 0 | ok | 1.89 | 1.80 | 1.86 | 2.02 | 9.85 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 64 B | 1 | ok | 1.89 | 1.79 | 1.86 | 1.98 | 9.83 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 64 B | 2 | ok | 1.89 | 1.80 | 1.87 | 2.00 | 9.83 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 64 B | 0 | ok | 4.25 | 4.10 | 4.21 | 5.18 | 8.90 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 64 B | 1 | ok | 4.24 | 4.12 | 4.20 | 5.26 | 9.20 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 64 B | 2 | ok | 4.25 | 4.12 | 4.21 | 5.11 | 8.65 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 64 B | 0 | ok | 3.62 | 3.48 | 3.55 | 4.52 | 25.40 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 64 B | 1 | ok | 3.62 | 3.47 | 3.54 | 4.91 | 23.37 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 64 B | 2 | ok | 3.62 | 3.48 | 3.54 | 4.59 | 25.29 | PASSED/PASSED | 0 |
| pass1 | LL128 Atomic | 3 | 64 B | 0 | skipped | - | - | - | - | - | - | 2 |
| pass1 | LL128 Atomic | 3 | 64 B | 1 | skipped | - | - | - | - | - | - | 2 |
| pass1 | LL128 Atomic | 3 | 64 B | 2 | skipped | - | - | - | - | - | - | 2 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 64 B | 0 | ok | 13.74 | 13.35 | 13.66 | 15.37 | 30.11 | PASSED | 0 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 64 B | 1 | ok | 13.74 | 13.45 | 13.62 | 15.32 | 30.01 | PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 128 B | 0 | ok | 8.13 | 6.75 | 8.34 | 9.49 | 27.65 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 128 B | 1 | ok | 8.13 | 6.73 | 9.19 | 9.71 | 27.70 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 128 B | 2 | ok | 8.13 | 6.09 | 9.13 | 10.33 | 27.91 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 128 B | 0 | ok | 2.27 | 2.11 | 2.25 | 2.62 | 10.04 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 128 B | 1 | ok | 2.27 | 2.13 | 2.23 | 2.50 | 12.03 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 128 B | 2 | ok | 2.27 | 2.13 | 2.25 | 2.56 | 9.74 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 128 B | 0 | ok | 1.93 | 1.85 | 1.91 | 2.44 | 6.99 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 128 B | 1 | ok | 1.93 | 1.86 | 1.91 | 2.44 | 6.98 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 128 B | 2 | ok | 1.93 | 1.85 | 1.91 | 2.51 | 7.00 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 128 B | 0 | ok | 4.29 | 4.13 | 4.25 | 5.03 | 11.11 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 128 B | 1 | ok | 4.29 | 4.13 | 4.24 | 5.85 | 10.91 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 128 B | 2 | ok | 4.29 | 4.11 | 4.24 | 5.72 | 11.46 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 128 B | 0 | ok | 3.73 | 3.56 | 3.69 | 4.83 | 13.09 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 128 B | 1 | ok | 3.72 | 3.56 | 3.68 | 4.61 | 10.79 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 128 B | 2 | ok | 3.73 | 3.57 | 3.69 | 4.75 | 12.59 | PASSED/PASSED | 0 |
| pass1 | LL128 Atomic | 3 | 128 B | 0 | skipped | - | - | - | - | - | - | 2 |
| pass1 | LL128 Atomic | 3 | 128 B | 1 | skipped | - | - | - | - | - | - | 2 |
| pass1 | LL128 Atomic | 3 | 128 B | 2 | skipped | - | - | - | - | - | - | 2 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 128 B | 0 | ok | 13.81 | 13.46 | 13.70 | 15.79 | 37.04 | PASSED | 0 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 128 B | 1 | ok | 13.81 | 13.35 | 13.71 | 15.97 | 36.04 | PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 32 KB | 0 | ok | 44.41 | 36.65 | 44.49 | 46.88 | 57.24 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 32 KB | 1 | ok | 44.43 | 37.83 | 44.74 | 48.06 | 59.08 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 32 KB | 2 | ok | 44.43 | 36.11 | 44.78 | 48.22 | 62.57 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 32 KB | 0 | ok | 34.07 | 27.35 | 33.76 | 38.26 | 54.49 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 32 KB | 1 | ok | 34.09 | 27.22 | 33.82 | 39.67 | 53.46 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 32 KB | 2 | ok | 34.07 | 21.37 | 33.77 | 39.39 | 53.16 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 32 KB | 0 | ok | 42.85 | 35.67 | 43.16 | 49.28 | 73.05 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 32 KB | 1 | ok | 42.84 | 35.83 | 42.75 | 49.25 | 72.85 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 32 KB | 2 | ok | 42.83 | 34.20 | 42.67 | 49.35 | 75.50 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 32 KB | 0 | ok | 24.51 | 21.89 | 24.59 | 25.86 | 38.75 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 32 KB | 1 | ok | 24.54 | 23.02 | 24.54 | 25.69 | 42.00 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 32 KB | 2 | ok | 24.52 | 22.33 | 24.62 | 25.65 | 39.74 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 32 KB | 0 | ok | 31.80 | 28.24 | 31.68 | 36.56 | 43.79 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 32 KB | 1 | ok | 31.80 | 29.33 | 31.59 | 35.64 | 50.16 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 32 KB | 2 | ok | 31.77 | 30.71 | 31.60 | 35.20 | 45.98 | PASSED/PASSED | 0 |
| pass1 | LL128 Atomic | 3 | 32 KB | 0 | skipped | - | - | - | - | - | - | 2 |
| pass1 | LL128 Atomic | 3 | 32 KB | 1 | skipped | - | - | - | - | - | - | 2 |
| pass1 | LL128 Atomic | 3 | 32 KB | 2 | skipped | - | - | - | - | - | - | 2 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 KB | 0 | ok | 38.02 | 37.59 | 37.98 | 39.52 | 45.16 | PASSED | 0 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 KB | 1 | ok | 38.02 | 37.59 | 37.96 | 39.57 | 45.14 | PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 256 KB | 0 | ok | 238.19 | 228.52 | 237.67 | 247.39 | 250.54 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 256 KB | 1 | ok | 238.73 | 233.47 | 238.01 | 250.84 | 282.05 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 256 KB | 2 | ok | 238.61 | 227.78 | 238.00 | 254.34 | 273.58 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 256 KB | 0 | ok | 230.58 | 221.30 | 230.29 | 240.92 | 261.17 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 256 KB | 1 | ok | 230.28 | 195.66 | 230.09 | 245.43 | 263.22 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 256 KB | 2 | ok | 230.68 | 212.38 | 229.86 | 244.11 | 267.77 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 256 KB | 0 | ok | 283.27 | 275.52 | 283.58 | 297.36 | 303.60 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 256 KB | 1 | ok | 283.23 | 274.05 | 283.14 | 293.35 | 301.93 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 256 KB | 2 | ok | 283.26 | 271.94 | 283.32 | 297.15 | 303.54 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 256 KB | 0 | ok | 150.29 | 148.09 | 149.69 | 156.88 | 157.94 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 256 KB | 1 | ok | 150.19 | 143.20 | 149.80 | 156.11 | 156.62 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 256 KB | 2 | ok | 150.13 | 147.00 | 149.69 | 156.51 | 157.24 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 256 KB | 0 | ok | 187.58 | 179.67 | 187.41 | 199.07 | 210.98 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 256 KB | 1 | ok | 187.71 | 181.24 | 187.09 | 199.19 | 203.11 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 256 KB | 2 | ok | 187.69 | 180.14 | 186.53 | 197.94 | 207.60 | PASSED/PASSED | 0 |
| pass1 | LL128 Atomic | 3 | 256 KB | 0 | skipped | - | - | - | - | - | - | 2 |
| pass1 | LL128 Atomic | 3 | 256 KB | 1 | skipped | - | - | - | - | - | - | 2 |
| pass1 | LL128 Atomic | 3 | 256 KB | 2 | skipped | - | - | - | - | - | - | 2 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 256 KB | 0 | ok | 172.24 | 163.94 | 172.01 | 177.79 | 179.44 | PASSED | 0 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 256 KB | 1 | ok | 172.32 | 170.43 | 172.01 | 177.74 | 179.35 | PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 1 MB | 0 | ok | 912.20 | 904.39 | 912.29 | 916.58 | 919.71 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 1 MB | 1 | ok | 913.36 | 906.60 | 912.43 | 918.29 | 1,014.63 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 1 MB | 2 | ok | 913.43 | 891.53 | 912.49 | 929.28 | 1,045.96 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 1 MB | 0 | ok | 901.45 | 882.55 | 901.46 | 922.03 | 922.49 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 1 MB | 1 | ok | 901.34 | 881.42 | 902.72 | 920.84 | 924.60 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 1 MB | 2 | ok | 901.87 | 882.83 | 902.18 | 919.18 | 948.76 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 1 MB | 0 | ok | 1,110.69 | 1,099.04 | 1,109.75 | 1,126.97 | 1,157.24 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 1 MB | 1 | ok | 1,111.06 | 1,098.89 | 1,110.48 | 1,150.15 | 1,150.45 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 1 MB | 2 | ok | 1,111.24 | 1,093.83 | 1,110.70 | 1,163.37 | 1,165.18 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 1 MB | 0 | ok | 579.21 | 573.61 | 579.33 | 584.61 | 585.61 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 1 MB | 1 | ok | 579.25 | 572.92 | 579.41 | 584.07 | 584.35 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 1 MB | 2 | ok | 579.17 | 571.83 | 578.96 | 585.77 | 588.00 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 1 MB | 0 | ok | 705.25 | 689.34 | 705.31 | 726.77 | 733.83 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 1 MB | 1 | ok | 705.33 | 693.73 | 704.77 | 721.53 | 731.27 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 1 MB | 2 | ok | 705.39 | 695.98 | 704.94 | 719.14 | 723.44 | PASSED/PASSED | 0 |
| pass1 | LL128 Atomic | 3 | 1 MB | 0 | skipped | - | - | - | - | - | - | 2 |
| pass1 | LL128 Atomic | 3 | 1 MB | 1 | skipped | - | - | - | - | - | - | 2 |
| pass1 | LL128 Atomic | 3 | 1 MB | 2 | skipped | - | - | - | - | - | - | 2 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 1 MB | 0 | ok | 542.68 | 537.25 | 542.27 | 548.03 | 549.42 | PASSED | 0 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 1 MB | 1 | ok | 542.88 | 538.70 | 542.67 | 548.54 | 549.08 | PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 32 MB | 0 | ok | 36,486.12 | 36,131.39 | 36,379.56 | 38,429.43 | 38,460.24 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 32 MB | 1 | ok | 36,489.07 | 35,922.07 | 36,381.56 | 38,147.54 | 38,372.47 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 3 | 32 MB | 2 | ok | 36,446.38 | 32,055.43 | 36,388.55 | 42,285.39 | 42,598.72 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 32 MB | 0 | ok | 36,468.47 | 31,553.06 | 36,568.10 | 40,748.99 | 41,843.89 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 32 MB | 1 | ok | 36,468.68 | 31,921.44 | 36,330.95 | 41,357.08 | 42,552.53 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 3 | 32 MB | 2 | ok | 36,420.66 | 31,433.37 | 36,371.29 | 42,396.14 | 42,510.46 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 32 MB | 0 | ok | 38,658.85 | 38,097.60 | 38,394.58 | 42,301.56 | 42,566.53 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 32 MB | 1 | ok | 38,651.05 | 38,049.02 | 38,360.14 | 42,875.45 | 43,049.66 | PASSED/PASSED | 0 |
| pass1 | Sentinel (one-shot) | 3 | 32 MB | 2 | ok | 38,608.60 | 36,260.61 | 38,434.31 | 42,867.63 | 44,854.73 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 32 MB | 0 | ok | 22,006.96 | 21,641.70 | 21,981.32 | 23,783.80 | 24,346.49 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 32 MB | 1 | ok | 21,977.31 | 21,297.06 | 21,918.44 | 23,910.49 | 24,936.98 | PASSED/PASSED | 0 |
| pass1 | Two-shot LL | 3 | 32 MB | 2 | ok | 21,995.52 | 21,157.98 | 21,967.74 | 24,791.83 | 25,170.90 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 32 MB | 0 | ok | 24,701.63 | 24,341.13 | 24,647.80 | 26,649.27 | 26,746.05 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 32 MB | 1 | ok | 24,704.18 | 24,473.97 | 24,637.74 | 26,541.42 | 27,859.96 | PASSED/PASSED | 0 |
| pass1 | Two-shot Sentinel | 3 | 32 MB | 2 | ok | 24,707.06 | 24,033.32 | 24,645.30 | 27,137.26 | 29,038.96 | PASSED/PASSED | 0 |
| pass1 | LL128 Atomic | 3 | 32 MB | 0 | skipped | - | - | - | - | - | - | 2 |
| pass1 | LL128 Atomic | 3 | 32 MB | 1 | skipped | - | - | - | - | - | - | 2 |
| pass1 | LL128 Atomic | 3 | 32 MB | 2 | skipped | - | - | - | - | - | - | 2 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 MB | 0 | ok | 19,449.23 | 19,128.73 | 19,431.39 | 20,231.13 | 20,419.56 | PASSED | 0 |
| pass1 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 MB | 1 | ok | 19,448.48 | 19,226.72 | 19,449.10 | 19,637.51 | 20,139.92 | PASSED | 0 |
| pass1 | Baseline (with barrier) | 2 | 32 B | 0 | ok | 4.22 | 2.63 | 4.38 | 4.84 | 29.07 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 2 | 32 B | 1 | ok | 4.22 | 2.33 | 4.39 | 4.91 | 29.48 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 2 | 32 B | 0 | ok | 1.77 | 1.68 | 1.74 | 1.85 | 11.53 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 2 | 32 B | 1 | ok | 1.78 | 1.68 | 1.74 | 1.90 | 11.02 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 2 | 32 KB | 0 | ok | 23.39 | 21.19 | 23.42 | 25.40 | 28.18 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 2 | 32 KB | 1 | ok | 23.40 | 22.76 | 23.64 | 24.96 | 28.88 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 2 | 32 KB | 0 | ok | 19.49 | 12.65 | 21.15 | 25.92 | 35.73 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 2 | 32 KB | 1 | ok | 19.48 | 12.86 | 20.90 | 25.59 | 35.67 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 2 | 32 MB | 0 | ok | 23,065.99 | 22,649.10 | 23,057.74 | 23,521.08 | 24,148.08 | PASSED/PASSED | 0 |
| pass1 | Baseline (with barrier) | 2 | 32 MB | 1 | ok | 23,063.49 | 22,620.49 | 23,081.17 | 23,547.49 | 24,670.10 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 2 | 32 MB | 0 | ok | 22,714.74 | 13,437.94 | 22,714.48 | 32,253.69 | 32,816.19 | PASSED/PASSED | 0 |
| pass1 | LL (one-shot) | 2 | 32 MB | 1 | ok | 22,809.79 | 13,280.49 | 22,871.42 | 32,402.90 | 32,502.25 | PASSED/PASSED | 0 |
| pass1 | Atomic_Probe | 2 | 32 B | 0 | no result | - | - | - | - | - | - | 0 |
| pass1 | Atomic_Probe | 2 | 32 B | 1 | no result | - | - | - | - | - | - | 0 |
| pass1 | LL without the wait (negative test) | 3 | 32 B | 0 | wrong result | 2.22 | 2.14 | 2.20 | 2.34 | 6.41 | FAILED/FAILED | 1 |
| pass1 | LL without the wait (negative test) | 3 | 32 B | 1 | wrong result | 2.23 | 2.14 | 2.20 | 2.37 | 12.75 | FAILED/FAILED | 1 |
| pass1 | LL without the wait (negative test) | 3 | 32 B | 2 | wrong result | 2.21 | 2.14 | 2.20 | 2.36 | 4.46 | PASSED/FAILED | 0 |
| pass1 | LL without the wait (negative test) | 3 | 32 KB | 0 | wrong result | 26.48 | 23.53 | 25.86 | 31.59 | 36.87 | FAILED/FAILED | 1 |
| pass1 | LL without the wait (negative test) | 3 | 32 KB | 1 | wrong result | 30.60 | 23.47 | 28.44 | 38.97 | 43.37 | FAILED/FAILED | 1 |
| pass1 | LL without the wait (negative test) | 3 | 32 KB | 2 | wrong result | 26.52 | 23.34 | 25.86 | 31.99 | 41.41 | FAILED/FAILED | 1 |
| pass2 | Baseline (with barrier) | 3 | 32 B | 0 | ok | 7.99 | 7.45 | 8.19 | 9.18 | 23.14 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 32 B | 1 | ok | 7.99 | 6.64 | 9.01 | 9.63 | 24.04 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 32 B | 2 | ok | 7.98 | 4.85 | 8.90 | 9.68 | 24.26 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 32 B | 0 | ok | 2.20 | 2.09 | 2.16 | 2.36 | 11.44 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 32 B | 1 | ok | 2.19 | 2.07 | 2.15 | 2.38 | 11.70 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 32 B | 2 | ok | 2.19 | 2.07 | 2.16 | 2.39 | 11.17 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 32 B | 0 | ok | 1.93 | 1.79 | 1.86 | 2.81 | 18.15 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 32 B | 1 | ok | 1.92 | 1.79 | 1.86 | 2.35 | 17.84 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 32 B | 2 | ok | 1.92 | 1.79 | 1.86 | 2.52 | 17.62 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 32 B | 0 | ok | 4.30 | 4.17 | 4.25 | 5.29 | 11.05 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 32 B | 1 | ok | 4.30 | 4.04 | 4.24 | 5.92 | 11.58 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 32 B | 2 | ok | 4.30 | 4.11 | 4.25 | 5.76 | 10.87 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 32 B | 0 | ok | 3.61 | 3.43 | 3.53 | 5.53 | 19.47 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 32 B | 1 | ok | 3.61 | 3.42 | 3.52 | 5.40 | 18.78 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 32 B | 2 | ok | 3.61 | 3.29 | 3.54 | 5.65 | 19.48 | PASSED/PASSED | 0 |
| pass2 | LL128 Atomic | 3 | 32 B | 0 | skipped | - | - | - | - | - | - | 2 |
| pass2 | LL128 Atomic | 3 | 32 B | 1 | skipped | - | - | - | - | - | - | 2 |
| pass2 | LL128 Atomic | 3 | 32 B | 2 | skipped | - | - | - | - | - | - | 2 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 B | 0 | ok | 13.72 | 13.43 | 13.65 | 14.99 | 24.61 | PASSED | 0 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 B | 1 | ok | 13.72 | 13.36 | 13.69 | 15.22 | 24.51 | PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 64 B | 0 | ok | 7.97 | 6.18 | 8.23 | 9.07 | 16.23 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 64 B | 1 | ok | 7.97 | 6.63 | 9.04 | 9.49 | 16.40 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 64 B | 2 | ok | 7.97 | 5.95 | 8.90 | 9.83 | 15.46 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 64 B | 0 | ok | 2.23 | 2.10 | 2.17 | 3.01 | 18.68 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 64 B | 1 | ok | 2.23 | 2.08 | 2.16 | 2.70 | 18.80 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 64 B | 2 | ok | 2.23 | 2.08 | 2.17 | 3.57 | 18.31 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 64 B | 0 | ok | 1.90 | 1.80 | 1.87 | 2.01 | 10.86 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 64 B | 1 | ok | 1.90 | 1.80 | 1.87 | 2.05 | 10.79 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 64 B | 2 | ok | 1.90 | 1.80 | 1.87 | 2.03 | 10.84 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 64 B | 0 | ok | 4.28 | 4.14 | 4.23 | 5.42 | 9.05 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 64 B | 1 | ok | 4.28 | 4.07 | 4.24 | 5.92 | 10.21 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 64 B | 2 | ok | 4.28 | 4.11 | 4.24 | 5.82 | 9.77 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 64 B | 0 | ok | 3.65 | 3.48 | 3.57 | 5.64 | 20.43 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 64 B | 1 | ok | 3.64 | 3.48 | 3.56 | 6.01 | 20.28 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 64 B | 2 | ok | 3.64 | 3.48 | 3.57 | 5.46 | 21.16 | PASSED/PASSED | 0 |
| pass2 | LL128 Atomic | 3 | 64 B | 0 | skipped | - | - | - | - | - | - | 2 |
| pass2 | LL128 Atomic | 3 | 64 B | 1 | skipped | - | - | - | - | - | - | 2 |
| pass2 | LL128 Atomic | 3 | 64 B | 2 | skipped | - | - | - | - | - | - | 2 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 64 B | 0 | ok | 13.56 | 13.25 | 13.39 | 15.15 | 32.40 | PASSED | 0 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 64 B | 1 | ok | 13.57 | 13.23 | 13.39 | 15.28 | 32.73 | PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 128 B | 0 | ok | 8.13 | 6.17 | 8.36 | 9.43 | 15.70 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 128 B | 1 | ok | 8.13 | 6.74 | 9.20 | 9.60 | 14.89 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 128 B | 2 | ok | 8.13 | 6.12 | 9.14 | 10.21 | 16.62 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 128 B | 0 | ok | 2.29 | 2.12 | 2.25 | 2.90 | 20.15 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 128 B | 1 | ok | 2.29 | 2.11 | 2.23 | 3.59 | 19.14 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 128 B | 2 | ok | 2.27 | 2.13 | 2.24 | 2.61 | 9.38 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 128 B | 0 | ok | 1.97 | 1.85 | 1.91 | 2.46 | 16.35 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 128 B | 1 | ok | 1.97 | 1.86 | 1.91 | 2.31 | 18.10 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 128 B | 2 | ok | 1.96 | 1.86 | 1.91 | 2.28 | 18.11 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 128 B | 0 | ok | 4.31 | 4.16 | 4.27 | 5.47 | 8.54 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 128 B | 1 | ok | 4.31 | 4.18 | 4.26 | 5.97 | 9.02 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 128 B | 2 | ok | 4.31 | 4.16 | 4.27 | 5.85 | 9.06 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 128 B | 0 | ok | 3.73 | 3.59 | 3.69 | 4.93 | 10.33 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 128 B | 1 | ok | 3.73 | 3.60 | 3.69 | 4.57 | 10.23 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 128 B | 2 | ok | 3.73 | 3.59 | 3.69 | 5.00 | 9.50 | PASSED/PASSED | 0 |
| pass2 | LL128 Atomic | 3 | 128 B | 0 | skipped | - | - | - | - | - | - | 2 |
| pass2 | LL128 Atomic | 3 | 128 B | 1 | skipped | - | - | - | - | - | - | 2 |
| pass2 | LL128 Atomic | 3 | 128 B | 2 | skipped | - | - | - | - | - | - | 2 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 128 B | 0 | ok | 13.67 | 13.39 | 13.55 | 14.99 | 23.12 | PASSED | 0 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 128 B | 1 | ok | 13.67 | 13.41 | 13.56 | 14.99 | 22.74 | PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 32 KB | 0 | ok | 44.31 | 39.19 | 44.53 | 46.56 | 56.93 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 32 KB | 1 | ok | 44.29 | 37.65 | 44.22 | 47.84 | 56.95 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 32 KB | 2 | ok | 44.31 | 39.67 | 44.62 | 47.96 | 56.81 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 32 KB | 0 | ok | 34.01 | 28.04 | 33.80 | 37.20 | 42.15 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 32 KB | 1 | ok | 34.01 | 26.27 | 33.85 | 37.88 | 44.55 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 32 KB | 2 | ok | 34.01 | 26.46 | 33.81 | 37.37 | 46.07 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 32 KB | 0 | ok | 42.72 | 34.98 | 43.05 | 48.98 | 58.60 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 32 KB | 1 | ok | 42.71 | 34.94 | 42.75 | 48.28 | 59.71 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 32 KB | 2 | ok | 42.71 | 34.49 | 42.59 | 48.51 | 59.68 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 32 KB | 0 | ok | 24.49 | 21.88 | 24.61 | 25.79 | 38.21 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 32 KB | 1 | ok | 24.51 | 22.29 | 24.50 | 25.66 | 39.54 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 32 KB | 2 | ok | 24.49 | 22.09 | 24.65 | 25.66 | 38.96 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 32 KB | 0 | ok | 31.82 | 28.46 | 31.82 | 35.15 | 47.39 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 32 KB | 1 | ok | 31.83 | 29.33 | 31.65 | 35.18 | 48.50 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 32 KB | 2 | ok | 31.81 | 30.97 | 31.66 | 34.82 | 48.63 | PASSED/PASSED | 0 |
| pass2 | LL128 Atomic | 3 | 32 KB | 0 | skipped | - | - | - | - | - | - | 2 |
| pass2 | LL128 Atomic | 3 | 32 KB | 1 | skipped | - | - | - | - | - | - | 2 |
| pass2 | LL128 Atomic | 3 | 32 KB | 2 | skipped | - | - | - | - | - | - | 2 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 KB | 0 | ok | 37.99 | 37.58 | 37.94 | 39.50 | 50.25 | PASSED | 0 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 KB | 1 | ok | 37.99 | 37.52 | 37.93 | 39.50 | 50.30 | PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 256 KB | 0 | ok | 238.18 | 229.96 | 238.20 | 242.53 | 243.01 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 256 KB | 1 | ok | 238.70 | 233.17 | 238.48 | 245.61 | 280.59 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 256 KB | 2 | ok | 238.63 | 233.32 | 238.39 | 244.51 | 277.71 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 256 KB | 0 | ok | 230.40 | 224.52 | 230.46 | 238.76 | 239.67 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 256 KB | 1 | ok | 230.28 | 223.38 | 230.12 | 238.18 | 242.62 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 256 KB | 2 | ok | 230.32 | 218.41 | 230.04 | 237.59 | 244.25 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 256 KB | 0 | ok | 290.81 | 281.43 | 291.11 | 303.08 | 310.29 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 256 KB | 1 | ok | 290.86 | 273.13 | 292.85 | 313.63 | 317.50 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 256 KB | 2 | ok | 290.88 | 272.73 | 291.05 | 308.15 | 311.31 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 256 KB | 0 | ok | 150.09 | 146.57 | 149.79 | 155.50 | 157.47 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 256 KB | 1 | ok | 150.09 | 144.46 | 149.72 | 158.11 | 160.68 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 256 KB | 2 | ok | 150.02 | 146.14 | 149.62 | 155.16 | 156.15 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 256 KB | 0 | ok | 187.82 | 180.25 | 187.14 | 199.63 | 200.06 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 256 KB | 1 | ok | 187.85 | 183.19 | 187.23 | 197.34 | 204.44 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 256 KB | 2 | ok | 187.83 | 182.27 | 187.13 | 194.16 | 203.78 | PASSED/PASSED | 0 |
| pass2 | LL128 Atomic | 3 | 256 KB | 0 | skipped | - | - | - | - | - | - | 2 |
| pass2 | LL128 Atomic | 3 | 256 KB | 1 | skipped | - | - | - | - | - | - | 2 |
| pass2 | LL128 Atomic | 3 | 256 KB | 2 | skipped | - | - | - | - | - | - | 2 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 256 KB | 0 | ok | 170.75 | 162.70 | 170.72 | 174.40 | 174.47 | PASSED | 0 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 256 KB | 1 | ok | 170.83 | 168.66 | 170.73 | 173.67 | 174.59 | PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 1 MB | 0 | ok | 913.59 | 906.93 | 913.40 | 921.79 | 923.78 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 1 MB | 1 | ok | 914.64 | 908.39 | 913.39 | 923.10 | 1,014.90 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 1 MB | 2 | ok | 913.37 | 891.75 | 914.06 | 928.97 | 929.66 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 1 MB | 0 | ok | 899.72 | 836.88 | 900.25 | 915.66 | 958.95 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 1 MB | 1 | ok | 900.32 | 884.16 | 900.52 | 917.11 | 954.06 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 1 MB | 2 | ok | 900.73 | 882.30 | 900.46 | 922.28 | 996.15 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 1 MB | 0 | ok | 1,113.10 | 1,107.99 | 1,112.50 | 1,133.48 | 1,133.78 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 1 MB | 1 | ok | 1,113.46 | 1,096.31 | 1,114.12 | 1,130.91 | 1,138.38 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 1 MB | 2 | ok | 1,113.74 | 1,093.69 | 1,114.03 | 1,140.20 | 1,173.03 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 1 MB | 0 | ok | 578.58 | 568.06 | 578.60 | 584.08 | 585.47 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 1 MB | 1 | ok | 578.69 | 571.28 | 578.48 | 588.27 | 588.85 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 1 MB | 2 | ok | 578.66 | 559.85 | 578.63 | 586.33 | 597.08 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 1 MB | 0 | ok | 708.67 | 699.50 | 708.66 | 718.30 | 719.96 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 1 MB | 1 | ok | 708.60 | 693.59 | 707.84 | 724.95 | 725.35 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 1 MB | 2 | ok | 708.54 | 697.36 | 706.89 | 721.29 | 724.40 | PASSED/PASSED | 0 |
| pass2 | LL128 Atomic | 3 | 1 MB | 0 | skipped | - | - | - | - | - | - | 2 |
| pass2 | LL128 Atomic | 3 | 1 MB | 1 | skipped | - | - | - | - | - | - | 2 |
| pass2 | LL128 Atomic | 3 | 1 MB | 2 | skipped | - | - | - | - | - | - | 2 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 1 MB | 0 | ok | 541.91 | 538.95 | 541.56 | 548.03 | 550.58 | PASSED | 0 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 1 MB | 1 | ok | 541.62 | 538.27 | 541.39 | 545.46 | 546.04 | PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 32 MB | 0 | ok | 36,504.16 | 35,817.92 | 36,431.19 | 38,024.02 | 38,137.68 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 32 MB | 1 | ok | 36,517.39 | 36,106.28 | 36,416.06 | 38,306.51 | 38,397.81 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 3 | 32 MB | 2 | ok | 36,467.97 | 32,156.28 | 36,387.08 | 42,482.60 | 42,533.69 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 32 MB | 0 | ok | 36,462.42 | 34,518.16 | 36,376.70 | 39,123.78 | 39,579.61 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 32 MB | 1 | ok | 36,461.68 | 34,536.00 | 36,402.77 | 38,935.36 | 38,943.74 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 3 | 32 MB | 2 | ok | 36,421.02 | 31,564.92 | 36,395.85 | 42,453.77 | 42,490.10 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 32 MB | 0 | ok | 38,778.11 | 38,103.11 | 38,486.57 | 42,721.33 | 43,407.54 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 32 MB | 1 | ok | 38,783.17 | 38,090.43 | 38,490.10 | 43,228.88 | 43,796.71 | PASSED/PASSED | 0 |
| pass2 | Sentinel (one-shot) | 3 | 32 MB | 2 | ok | 38,735.88 | 36,753.77 | 38,505.29 | 43,478.11 | 45,107.00 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 32 MB | 0 | ok | 23,019.49 | 21,503.97 | 21,900.23 | 70,101.35 | 85,912.38 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 32 MB | 1 | ok | 23,021.44 | 21,433.32 | 21,826.26 | 69,999.62 | 87,432.01 | PASSED/PASSED | 0 |
| pass2 | Two-shot LL | 3 | 32 MB | 2 | ok | 23,033.80 | 21,551.15 | 21,885.74 | 56,920.99 | 92,937.58 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 32 MB | 0 | ok | 24,676.92 | 24,345.74 | 24,594.55 | 26,460.20 | 26,837.32 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 32 MB | 1 | ok | 24,675.66 | 24,413.84 | 24,627.00 | 26,646.01 | 27,981.69 | PASSED/PASSED | 0 |
| pass2 | Two-shot Sentinel | 3 | 32 MB | 2 | ok | 24,678.64 | 23,910.35 | 24,608.74 | 26,644.72 | 29,179.69 | PASSED/PASSED | 0 |
| pass2 | LL128 Atomic | 3 | 32 MB | 0 | skipped | - | - | - | - | - | - | 2 |
| pass2 | LL128 Atomic | 3 | 32 MB | 1 | skipped | - | - | - | - | - | - | 2 |
| pass2 | LL128 Atomic | 3 | 32 MB | 2 | skipped | - | - | - | - | - | - | 2 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 MB | 0 | ok | 19,434.00 | 19,069.66 | 19,442.85 | 20,001.77 | 20,182.96 | PASSED | 0 |
| pass2 | Bidirectional (2 ranks, 8 chunks per round) | 2 | 32 MB | 1 | ok | 19,433.57 | 19,210.56 | 19,436.40 | 19,776.76 | 20,528.41 | PASSED | 0 |
| pass2 | Baseline (with barrier) | 2 | 32 B | 0 | ok | 4.26 | 2.41 | 4.43 | 4.64 | 15.59 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 2 | 32 B | 1 | ok | 4.26 | 3.31 | 4.40 | 4.62 | 18.05 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 2 | 32 B | 0 | ok | 1.78 | 1.73 | 1.77 | 1.88 | 10.91 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 2 | 32 B | 1 | ok | 1.78 | 1.72 | 1.77 | 1.94 | 11.21 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 2 | 32 KB | 0 | ok | 22.54 | 21.72 | 22.69 | 24.02 | 32.92 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 2 | 32 KB | 1 | ok | 22.54 | 21.88 | 22.77 | 23.94 | 27.84 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 2 | 32 KB | 0 | ok | 19.50 | 12.67 | 21.28 | 25.98 | 29.66 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 2 | 32 KB | 1 | ok | 19.50 | 12.84 | 21.11 | 26.68 | 33.37 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 2 | 32 MB | 0 | ok | 22,904.32 | 22,331.96 | 22,884.24 | 23,734.28 | 23,780.37 | PASSED/PASSED | 0 |
| pass2 | Baseline (with barrier) | 2 | 32 MB | 1 | ok | 22,908.91 | 22,374.03 | 22,911.29 | 23,634.07 | 23,927.10 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 2 | 32 MB | 0 | ok | 22,854.69 | 13,341.69 | 23,219.62 | 32,805.92 | 32,923.76 | PASSED/PASSED | 0 |
| pass2 | LL (one-shot) | 2 | 32 MB | 1 | ok | 22,945.87 | 13,517.22 | 23,551.67 | 32,686.82 | 33,694.77 | PASSED/PASSED | 0 |
| pass2 | Atomic_Probe | 2 | 32 B | 0 | no result | - | - | - | - | - | - | 0 |
| pass2 | Atomic_Probe | 2 | 32 B | 1 | no result | - | - | - | - | - | - | 0 |
| pass2 | LL without the wait (negative test) | 3 | 32 B | 0 | wrong result | 2.20 | 2.11 | 2.19 | 2.32 | 6.63 | FAILED/FAILED | 1 |
| pass2 | LL without the wait (negative test) | 3 | 32 B | 1 | wrong result | 2.19 | 2.12 | 2.18 | 2.31 | 6.30 | FAILED/FAILED | 1 |
| pass2 | LL without the wait (negative test) | 3 | 32 B | 2 | wrong result | 2.21 | 2.10 | 2.19 | 2.34 | 6.81 | FAILED/FAILED | 1 |
| pass2 | LL without the wait (negative test) | 3 | 32 KB | 0 | wrong result | 26.31 | 23.51 | 25.69 | 31.43 | 37.41 | FAILED/FAILED | 1 |
| pass2 | LL without the wait (negative test) | 3 | 32 KB | 1 | wrong result | 29.21 | 23.45 | 26.85 | 38.78 | 42.61 | FAILED/FAILED | 1 |
| pass2 | LL without the wait (negative test) | 3 | 32 KB | 2 | wrong result | 26.38 | 23.29 | 25.77 | 31.77 | 39.73 | FAILED/FAILED | 1 |

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

