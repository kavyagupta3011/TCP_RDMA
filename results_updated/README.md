# Results, all three versions

All three versions of the code were run with the same experiments, in one PBS job, on the
same 3 machines; within each experiment the versions ran one after the other. Every number
here is read from the logs by `make_results.py`.

| Version | Code | What it is |
|---|---|---|
| v1 | `paper_algorithms` | the first RDMA version (git commit 0512582, 2026-09-30). Each RDMA write waits for its completion before the next one; one receive buffer reused every round; Sentinel never re-arms its buffers; flag waits use `!=`; prints only the mean; checks the result only in the first round. |
| v2 | `paper_algorithms_v2` | version 1 plus the fixes of 2026-10-05 (git commit 421f67d): writes posted back to back and small ones sent inline, two receive buffers used in turn, Sentinel re-armed, `<` flag waits, barrier receive fix, LL128 skips cleanly; prints min / median / p99 / max. |
| v3 | `paper_algorithms_v3` | version 2 plus the changes of 2026-10-28: a correctness check in every timed round, a faster adding step and -O3 for Baseline and LL, and the negative test program. |

## Folder layout

```
results_updated/
  README.md              this overview
  floor/RESULTS_floor.md the network floor;  floor/logs/  its raw log
  v1/RESULTS_v1.md        every table of v1;   v1/logs/  its raw logs
  v2/RESULTS_v2.md        every table of v2;   v2/logs/  its raw logs
  v3/RESULTS_v3.md        every table of v3;   v3/logs/  its raw logs
```

Logs per version: `1_3ranks_32B_64B_128B_32KB.txt`, `2_3ranks_256KB.txt`, `3_3ranks_32MB.txt`,
`4_2ranks_baseline_ll.txt`, `5_atomic_probe.txt` (v2, v3), `6_negative_test.txt` (v3).

**Network floor:** one 32 B RDMA write takes 1.07 µs one way (a round trip about 2.14 µs); large messages move at 3,794 MB/s. Details: `floor/RESULTS_floor.md`.

## Side by side: mean time per AllReduce, 3 ranks

The mean is used here because it is the only statistic version 1 prints. The medians of v2
and v3 are in the next table and in their own files.

### 32 B

| Algorithm | v1 | v2 | v3 |
|---|---|---|---|
| Baseline (with barrier) | 11.38 µs | 8.03 µs | 8.08 µs |
| LL (one-shot) | hung (rank 0/1/2) | 2.20 µs | 2.20 µs |
| Sentinel (one-shot) | 4.04 µs | 1.90 µs | 1.90 µs |
| Two-shot LL | 15.94 µs | 4.33 µs | 4.30 µs |
| Two-shot Sentinel | 8.01 µs | 3.58 µs | 3.59 µs |
| LL128 Atomic | crashed (rank 0/1/2) | skipped (no RDMA atomics) | skipped (no RDMA atomics) |
| Bidirectional (2 ranks, 8 chunks per round) | 31.42 µs | 13.77 µs | 13.51 µs |

### 64 B

| Algorithm | v1 | v2 | v3 |
|---|---|---|---|
| Baseline (with barrier) | 11.39 µs | 8.07 µs | 8.02 µs |
| LL (one-shot) | hung (rank 0/1/2) | 2.21 µs | 2.20 µs |
| Sentinel (one-shot) | 4.10 µs | 1.92 µs | 1.90 µs |
| Two-shot LL | 15.89 µs | 4.30 µs | 4.29 µs |
| Two-shot Sentinel | 7.99 µs | 3.58 µs | 3.65 µs |
| LL128 Atomic | crashed (rank 0/1/2) | skipped (no RDMA atomics) | skipped (no RDMA atomics) |
| Bidirectional (2 ranks, 8 chunks per round) | 31.57 µs | 13.94 µs | 13.82 µs |

### 128 B

| Algorithm | v1 | v2 | v3 |
|---|---|---|---|
| Baseline (with barrier) | 11.53 µs | 8.20 µs | 8.15 µs |
| LL (one-shot) | hung (rank 0/1/2) | 2.29 µs | 2.30 µs |
| Sentinel (one-shot) | 4.22 µs | 1.95 µs | 1.97 µs |
| Two-shot LL | 15.79 µs | 4.34 µs | 4.39 µs |
| Two-shot Sentinel | 8.07 µs | 3.73 µs | 3.76 µs |
| LL128 Atomic | crashed (rank 0/1/2) | skipped (no RDMA atomics) | skipped (no RDMA atomics) |
| Bidirectional (2 ranks, 8 chunks per round) | 31.33 µs | 13.87 µs | 13.86 µs |

### 32 KB

| Algorithm | v1 | v2 | v3 |
|---|---|---|---|
| Baseline (with barrier) | 72.92 µs | 63.25 µs | 44.23 µs |
| LL (one-shot) | 58.75 µs | 45.27 µs | 33.99 µs |
| Sentinel (one-shot) | 68.21 µs | 42.47 µs | 42.64 µs |
| Two-shot LL | 51.60 µs | 35.11 µs | 34.67 µs |
| Two-shot Sentinel | 49.30 µs (2 of 3 ranks; rank 0 crashed) | 45.18 µs | 36.93 µs |
| LL128 Atomic | crashed (rank 0/1/2) | skipped (no RDMA atomics) | skipped (no RDMA atomics) |
| Bidirectional (2 ranks, 8 chunks per round) | 55.62 µs | 38.04 µs | 38.03 µs |

### 256 KB

| Algorithm | v1 | v2 | v3 |
|---|---|---|---|
| Baseline (with barrier) | 439.6 µs | 405.4 µs | 238.9 µs |
| LL (one-shot) | 385.2 µs | 323.7 µs | 230.7 µs |
| Sentinel (one-shot) | 458.1 µs (2 of 3 ranks; rank 1 crashed) | 291.1 µs | 289.3 µs |
| Two-shot LL | 272.1 µs | 235.0 µs | 235.9 µs |
| Two-shot Sentinel | 312.7 µs | 285.6 µs | 231.6 µs |
| LL128 Atomic | crashed (rank 0/1/2) | skipped (no RDMA atomics) | skipped (no RDMA atomics) |
| Bidirectional (2 ranks, 8 chunks per round) | 178.7 µs | 172.3 µs | 170.2 µs |

### 32 MB

| Algorithm | v1 | v2 | v3 |
|---|---|---|---|
| Baseline (with barrier) | 54.52 ms | 46.96 ms | 36.51 ms |
| LL (one-shot) | 46.86 ms | 43.91 ms | 36.46 ms |
| Sentinel (one-shot) | 55.29 ms (2 of 3 ranks; rank 0 crashed) | 38.71 ms | 38.68 ms |
| Two-shot LL | 31.80 ms | 31.76 ms | 31.65 ms |
| Two-shot Sentinel | 34.92 ms (1 of 3 ranks; rank 0 crashed, rank 1 crashed) | 38.24 ms | 32.89 ms |
| LL128 Atomic | crashed (rank 0/1/2) | skipped (no RDMA atomics) | skipped (no RDMA atomics) |
| Bidirectional (2 ranks, 8 chunks per round) | 19.60 ms | 19.44 ms | 19.40 ms |

## Median, v2 vs v3, 3 ranks

| Algorithm | 32 B v2 | 32 B v3 | 64 B v2 | 64 B v3 | 128 B v2 | 128 B v3 | 32 KB v2 | 32 KB v3 | 256 KB v2 | 256 KB v3 | 32 MB v2 | 32 MB v3 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Baseline (with barrier) | 8.79 µs | 8.80 µs | 8.80 µs | 8.76 µs | 8.96 µs | 8.93 µs | 65.38 µs | 44.38 µs | 435.1 µs | 238.8 µs | 46.78 ms | 36.42 ms |
| LL (one-shot) | 2.16 µs | 2.14 µs | 2.17 µs | 2.16 µs | 2.23 µs | 2.24 µs | 45.04 µs | 33.78 µs | 323.6 µs | 230.6 µs | 43.85 ms | 36.57 ms |
| Sentinel (one-shot) | 1.87 µs | 1.85 µs | 1.87 µs | 1.87 µs | 1.92 µs | 1.93 µs | 42.49 µs | 42.84 µs | 293.6 µs | 292.0 µs | 38.41 ms | 38.41 ms |
| Two-shot LL | 4.27 µs | 4.24 µs | 4.24 µs | 4.26 µs | 4.28 µs | 4.32 µs | 34.40 µs | 34.40 µs | 229.6 µs | 229.6 µs | 31.63 ms | 31.56 ms |
| Two-shot Sentinel | 3.52 µs | 3.51 µs | 3.54 µs | 3.59 µs | 3.69 µs | 3.71 µs | 45.22 µs | 36.62 µs | 285.0 µs | 230.8 µs | 38.13 ms | 32.82 ms |
| LL128 Atomic | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) |
| Bidirectional (2 ranks, 8 chunks per round) | 13.67 µs | 13.38 µs | 13.87 µs | 13.74 µs | 13.79 µs | 13.78 µs | 37.97 µs | 37.99 µs | 172.1 µs | 170.0 µs | 19.44 ms | 19.39 ms |

## Did every version finish?

| Version | ranks ok | crashed | hung | wrong result | skipped | PASSED / FAILED lines |
|---|---|---|---|---|---|---|
| v1 | 102 | 23 | 15 | 0 | 0 | 107 / 0 |
| v2 | 122 | 0 | 0 | 0 | 18 | 122 / 0 |
| v3 | 122 | 0 | 0 | 6 | 18 | 233 / 11 |

For v3, the FAILED lines come from the negative test, which must fail. Crashes and hangs are
listed with their error messages in each version's file, section "Correctness and errors".

