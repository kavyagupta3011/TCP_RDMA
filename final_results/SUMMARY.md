# Final results: all versions and the ring baseline

Every experiment was run twice (`run_pass1.pbs`, `run_pass2.pbs`) on the same 3 machines, with the
versions in opposite order the second time. Numbers below are the median latency of one AllReduce,
averaged over the passes and over the ranks. v1 prints only the mean, so its numbers are means.
Everything is read from the logs by `make_final_results.py`.

| Version | Folder | What it is |
|---|---|---|
| v1 | `paper_algorithms` | first RDMA version: blocking writes, one receive buffer, mean only |
| v2 | `paper_algorithms_v2` | writes posted together, two receive buffers, min/median/p99/max, start and end barrier |
| v3 | `paper_algorithms_v3` | v2 + correctness check every round + blocked add |
| v4 | `paper_algorithms_v4` | v3 + -O3 and blocked add with copy-out for Two-shot |
| v5 | `paper_algorithms_v5` | v4 + one shared protection domain, Two-shot sends straight from input/output |
| v6 | `paper_algorithms_v6` | v5 + block pipelining with per-block flags, deeper queues |
| ring | `baselines` | standard ring AllReduce (reduce-scatter + allgather), plain SEND/RECV |

**Network floor:** one 32 B RDMA write takes 1.07 µs one way (about 2.14 µs round trip); large messages move at up to 3,794 MB/s. See `floor/RESULTS.md`.

## Side by side, 3 ranks

### 32 B

| Algorithm | v1 | v2 | v3 | v4 | v5 | v6 |
|---|---|---|---|---|---|---|
| Baseline (with barrier) | 1716.0 µs | 8.65 µs | 8.80 µs | 8.61 µs | 8.73 µs | 8.80 µs |
| LL (one-shot) | hung (rank 0/1/2) | 2.17 µs | 2.16 µs | 2.17 µs | 2.15 µs | 2.19 µs |
| Sentinel (one-shot) | 4.04 µs | 1.86 µs | 1.85 µs | 1.86 µs | 1.86 µs | 1.88 µs |
| Two-shot LL | 15.84 µs | 4.27 µs | 4.26 µs | 4.30 µs | 4.24 µs | 3.84 µs |
| Two-shot Sentinel | 7.99 µs | 3.55 µs | 3.49 µs | 3.53 µs | 3.51 µs | 3.17 µs |
| LL128 Atomic | crashed (rank 0/1/2) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) |
| Bidirectional (2 ranks, 8 chunks per round) | 31.47 µs | 13.71 µs | 13.74 µs | 13.67 µs | 13.68 µs | 13.65 µs |

### 64 B

| Algorithm | v1 | v2 | v3 | v4 | v5 | v6 |
|---|---|---|---|---|---|---|
| Baseline (with barrier) | 635.6 µs | 8.81 µs | 8.78 µs | 8.77 µs | 8.73 µs | 8.82 µs |
| LL (one-shot) | hung (rank 0/1/2) | 2.19 µs | 2.16 µs | 2.18 µs | 2.16 µs | 2.19 µs |
| Sentinel (one-shot) | 4.08 µs | 1.88 µs | 1.86 µs | 1.87 µs | 1.87 µs | 1.89 µs |
| Two-shot LL | 15.88 µs | 4.29 µs | 4.24 µs | 4.27 µs | 4.22 µs | 3.89 µs |
| Two-shot Sentinel | 7.90 µs | 3.57 µs | 3.56 µs | 3.59 µs | 3.55 µs | 3.24 µs |
| LL128 Atomic | crashed (rank 0/1/2) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) |
| Bidirectional (2 ranks, 8 chunks per round) | 31.48 µs | 13.67 µs | 13.77 µs | 13.75 µs | 13.52 µs | 13.67 µs |

### 128 B

| Algorithm | v1 | v2 | v3 | v4 | v5 | v6 |
|---|---|---|---|---|---|---|
| Baseline (with barrier) | 11.57 µs | 8.98 µs | 8.86 µs | 8.76 µs | 8.89 µs | 8.88 µs |
| LL (one-shot) | hung (rank 0/1/2) | 2.24 µs | 2.24 µs | 2.24 µs | 2.24 µs | 2.26 µs |
| Sentinel (one-shot) | 4.24 µs | 1.93 µs | 1.91 µs | 1.93 µs | 1.91 µs | 1.94 µs |
| Two-shot LL | 15.92 µs | 4.30 µs | 4.29 µs | 4.32 µs | 4.26 µs | 3.97 µs |
| Two-shot Sentinel | 8.08 µs | 3.69 µs | 3.68 µs | 3.71 µs | 3.69 µs | 3.39 µs |
| LL128 Atomic | crashed (rank 0/1/2) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) |
| Bidirectional (2 ranks, 8 chunks per round) | 31.56 µs | 13.72 µs | 13.64 µs | 13.55 µs | 13.63 µs | 13.72 µs |

### 32 KB

| Algorithm | v1 | v2 | v3 | v4 | v5 | v6 |
|---|---|---|---|---|---|---|
| Baseline (with barrier) | 72.48 µs | 64.98 µs | 44.74 µs | 44.67 µs | 44.56 µs | 44.82 µs |
| LL (one-shot) | 59.67 µs | 45.17 µs | 33.83 µs | 33.80 µs | 33.80 µs | 34.08 µs |
| Sentinel (one-shot) | 68.10 µs | 43.01 µs | 42.90 µs | 42.61 µs | 42.83 µs | 42.73 µs |
| Two-shot LL | 50.89 µs | 34.42 µs | 34.36 µs | 26.29 µs | 24.58 µs | 23.77 µs |
| Two-shot Sentinel | 49.25 µs | 44.89 µs | 36.54 µs | 32.81 µs | 31.67 µs | 31.13 µs |
| LL128 Atomic | crashed (rank 0/1/2) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) |
| Bidirectional (2 ranks, 8 chunks per round) | 55.58 µs | 37.97 µs | 37.92 µs | 37.95 µs | 37.95 µs | 37.88 µs |

### 256 KB

| Algorithm | v1 | v2 | v3 | v4 | v5 | v6 |
|---|---|---|---|---|---|---|
| Baseline (with barrier) | 444.3 µs | 439.6 µs | 238.2 µs | 237.9 µs | 238.1 µs | 238.3 µs |
| LL (one-shot) | 386.5 µs | 323.1 µs | 230.5 µs | 230.1 µs | 230.1 µs | 230.2 µs |
| Sentinel (one-shot) | 440.7 µs | 289.3 µs | 287.7 µs | 287.4 µs | 287.5 µs | 283.2 µs |
| Two-shot LL | 274.0 µs | 229.3 µs | 229.3 µs | 168.0 µs | 149.7 µs | 120.4 µs |
| Two-shot Sentinel | 321.4 µs | 285.9 µs | 230.6 µs | 202.6 µs | 187.1 µs | 175.2 µs |
| LL128 Atomic | crashed (rank 0/1/2) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) |
| Bidirectional (2 ranks, 8 chunks per round) | 179.2 µs | 171.9 µs | 170.9 µs | 171.0 µs | 171.4 µs | 158.0 µs |

### 1 MB

| Algorithm | v1 | v2 | v3 | v4 | v5 | v6 |
|---|---|---|---|---|---|---|
| Baseline (with barrier) | 1649.6 µs | 1718.0 µs | 911.6 µs | 911.7 µs | 913.0 µs | 912.0 µs |
| LL (one-shot) | 1523.9 µs | 1263.4 µs | 900.5 µs | 900.5 µs | 901.3 µs | 900.3 µs |
| Sentinel (one-shot) | 1754.0 µs | 1113.1 µs | 1112.8 µs | 1119.7 µs | 1111.9 µs | 1117.8 µs |
| Two-shot LL | 988.8 µs | 902.2 µs | 903.2 µs | 657.9 µs | 578.9 µs | 369.4 µs |
| Two-shot Sentinel | 1133.5 µs | 1104.3 µs | 914.5 µs | 783.9 µs | 706.4 µs | 671.3 µs |
| LL128 Atomic | crashed (rank 0/1/2) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) |
| Bidirectional (2 ranks, 8 chunks per round) | 565.1 µs | 541.7 µs | 541.7 µs | 541.7 µs | 542.0 µs | 542.9 µs |

### 32 MB

| Algorithm | v1 | v2 | v3 | v4 | v5 | v6 |
|---|---|---|---|---|---|---|
| Baseline (with barrier) | 54.49 ms | 45.74 ms | 36.40 ms | 36.54 ms | 36.40 ms | 36.46 ms |
| LL (one-shot) | 46.90 ms | 43.91 ms | 37.29 ms | 36.49 ms | 36.41 ms | 36.46 ms |
| Sentinel (one-shot) | 55.55 ms | 38.45 ms | 38.52 ms | 38.48 ms | 38.45 ms | 38.57 ms |
| Two-shot LL | 32.57 ms | 31.64 ms | 31.67 ms | 25.70 ms | 21.91 ms | 11.35 ms |
| Two-shot Sentinel | 35.22 ms | 38.21 ms | 32.78 ms | 28.61 ms | 24.63 ms | 25.04 ms |
| LL128 Atomic | crashed (rank 0/1/2) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) | skipped (no RDMA atomics) |
| Bidirectional (2 ranks, 8 chunks per round) | 19.62 ms | 19.42 ms | 19.42 ms | 19.43 ms | 19.44 ms | 19.44 ms |

The ring has no versions, so it is compared in the next table and in `baselines/RESULTS.md`.

## Against the ring baseline (v6 programs, 3 ranks)

Speed-up = ring time / program time (above 1x means faster than the ring).

| Size | Ring | Baseline (barrier) | LL | Sentinel | Two-shot LL | Two-shot Sentinel |
|---|---|---|---|---|---|---|
| 32 B | 8.12 µs | 8.80 µs (0.92x) | 2.19 µs (3.71x) | 1.88 µs (4.32x) | 3.84 µs (2.11x) | 3.17 µs (2.56x) |
| 64 B | 8.65 µs | 8.82 µs (0.98x) | 2.19 µs (3.95x) | 1.89 µs (4.59x) | 3.89 µs (2.23x) | 3.24 µs (2.67x) |
| 128 B | 8.70 µs | 8.88 µs (0.98x) | 2.26 µs (3.86x) | 1.94 µs (4.49x) | 3.97 µs (2.19x) | 3.39 µs (2.57x) |
| 32 KB | 38.02 µs | 44.82 µs (0.85x) | 34.08 µs (1.12x) | 42.73 µs (0.89x) | 23.77 µs (1.60x) | 31.13 µs (1.22x) |
| 256 KB | 199.4 µs | 238.3 µs (0.84x) | 230.2 µs (0.87x) | 283.2 µs (0.70x) | 120.4 µs (1.66x) | 175.2 µs (1.14x) |
| 1 MB | 805.2 µs | 912.0 µs (0.88x) | 900.3 µs (0.89x) | 1117.8 µs (0.72x) | 369.4 µs (2.18x) | 671.3 µs (1.20x) |
| 32 MB | 29.29 ms | 36.46 ms (0.80x) | 36.46 ms (0.80x) | 38.57 ms (0.76x) | 11.35 ms (2.58x) | 25.04 ms (1.17x) |

## v3 to v6 for the Two-shot programs, 3 ranks

Change relative to v3 (negative = faster).

| Program | Size | v3 | v4 | v5 | v6 |
|---|---|---|---|---|---|
| Two-shot LL | 32 B | 4.26 µs | 4.30 µs (+1.1%) | 4.24 µs (-0.4%) | 3.84 µs (-9.8%) |
| Two-shot LL | 64 B | 4.24 µs | 4.27 µs (+0.7%) | 4.22 µs (-0.3%) | 3.89 µs (-8.2%) |
| Two-shot LL | 128 B | 4.29 µs | 4.32 µs (+0.7%) | 4.26 µs (-0.8%) | 3.97 µs (-7.5%) |
| Two-shot LL | 32 KB | 34.36 µs | 26.29 µs (-23.5%) | 24.58 µs (-28.5%) | 23.77 µs (-30.8%) |
| Two-shot LL | 256 KB | 229.3 µs | 168.0 µs (-26.7%) | 149.7 µs (-34.7%) | 120.4 µs (-47.5%) |
| Two-shot LL | 1 MB | 903.2 µs | 657.9 µs (-27.2%) | 578.9 µs (-35.9%) | 369.4 µs (-59.1%) |
| Two-shot LL | 32 MB | 31.67 ms | 25.70 ms (-18.9%) | 21.91 ms (-30.8%) | 11.35 ms (-64.2%) |
| Two-shot Sentinel | 32 B | 3.49 µs | 3.53 µs (+1.1%) | 3.51 µs (+0.6%) | 3.17 µs (-9.2%) |
| Two-shot Sentinel | 64 B | 3.56 µs | 3.59 µs (+0.8%) | 3.55 µs (-0.0%) | 3.24 µs (-8.8%) |
| Two-shot Sentinel | 128 B | 3.68 µs | 3.71 µs (+0.9%) | 3.69 µs (+0.3%) | 3.39 µs (-7.9%) |
| Two-shot Sentinel | 32 KB | 36.54 µs | 32.81 µs (-10.2%) | 31.67 µs (-13.3%) | 31.13 µs (-14.8%) |
| Two-shot Sentinel | 256 KB | 230.6 µs | 202.6 µs (-12.2%) | 187.1 µs (-18.9%) | 175.2 µs (-24.0%) |
| Two-shot Sentinel | 1 MB | 914.5 µs | 783.9 µs (-14.3%) | 706.4 µs (-22.8%) | 671.3 µs (-26.6%) |
| Two-shot Sentinel | 32 MB | 32.78 ms | 28.61 ms (-12.7%) | 24.63 ms (-24.9%) | 25.04 ms (-23.6%) |

## Did every run finish?

| Version | ranks ok | crashed | hung | wrong result | skipped | PASSED / FAILED lines |
|---|---|---|---|---|---|---|
| v1 | 228 | 54 | 22 | 0 | 0 | 240 / 0 |
| v2 | 262 | 0 | 0 | 0 | 42 | 262 / 0 |
| v3 | 262 | 0 | 0 | 12 | 42 | 497 / 23 |
| v4 | 262 | 0 | 0 | 12 | 42 | 497 / 23 |
| v5 | 262 | 0 | 0 | 12 | 42 | 497 / 23 |
| v6 | 262 | 0 | 0 | 12 | 42 | 498 / 22 |
| ring | 54 | 0 | 0 | 0 | 0 | 108 / 0 |

FAILED lines in v3 to v6 come from the negative test, which must fail. Crashes and hangs are listed
with their error messages in each folder's `RESULTS.md`, section "Correctness and errors".

