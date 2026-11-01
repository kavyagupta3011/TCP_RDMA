# Version 3 results (2026-10-28)

Results of **version 3** (`paper_algorithms_v3/`): the code after the professor's comments, measured
at 32 B, 32 KB and 32 MB on idle machines, with the network floor and a correctness check in every round.
Job scripts: `paper_algorithms_v3/jobs/`.

Every number below is read from the raw logs in `logs/` by `make_results_v3.py`
(no number typed by hand). The log each table comes from is named above it.

How to read the tables:
- Size = the vector each rank contributes: 32 B = 8 floats, 32 KB = 8,192 floats, 32 MB = 8,388,608 floats.
- A table value = the **median** time of one AllReduce, averaged over the ranks (one rank per machine).
- Machines: compute02, compute03, compute04 (2-rank runs: compute02, compute03), unless stated.
- Timed rounds: 1000 at 32 B and 32 KB, 100 at 32 MB.
- The per-rank min / median / p99 / max of every run are in section 9.

Contents
1. Network floor
2. Main result: Baseline vs LL, final code (round 4)
3. 2 ranks vs 3 ranks (round 4)
4. Small sizes: 32 B, 64 B, 128 B (2026-11-01), and version 2 vs version 3
5. All algorithms (round 2)
6. Improvement: before vs after the faster adding step (round 3)
7. Repeated runs of Baseline and LL
8. Correctness checks
9. Every run, per rank

---

## 1. Network floor

Files: `2026-10-28_round1_floor_baseline_ll_raw.txt` (compute02 -> compute03, idle) and `2026-10-28_floor_compute00_01_loaded_nodes_raw.txt` (compute00 -> compute01, loaded).
`ib_write_lat`: one RDMA write, one way, 1000 repetitions per size. `ib_write_bw`: average bandwidth.

| Message | typical, idle | 99%, idle | 99.9%, idle | typical, loaded | 99.9%, loaded | bandwidth, idle |
|---|---|---|---|---|---|---|
| 8 B | 1.00 µs | 1.02 µs | 3.08 µs | 0.95 µs | 8.41 µs | 43 MB/s |
| 16 B | 1.01 µs | 1.03 µs | 2.60 µs | 0.95 µs | 7.97 µs | 86 MB/s |
| 32 B | 1.07 µs | 1.09 µs | 2.83 µs | 1.02 µs | 8.62 µs | 172 MB/s |
| 64 B | 1.08 µs | 1.10 µs | 3.57 µs | 1.01 µs | 7.99 µs | 332 MB/s |
| 128 B | 1.15 µs | 1.18 µs | 1.59 µs | 1.09 µs | 8.53 µs | 683 MB/s |
| 256 B | 1.58 µs | 1.61 µs | 3.20 µs | 1.51 µs | 8.78 µs | 1,378 MB/s |
| 512 B | 1.75 µs | 1.78 µs | 2.93 µs | 1.66 µs | 8.48 µs | 2,729 MB/s |
| 1 KiB | 2.06 µs | 2.20 µs | 5.83 µs | 1.94 µs | 9.12 µs | 3,650 MB/s |
| 2 KiB | 2.74 µs | 2.90 µs | 4.12 µs | 2.50 µs | 9.62 µs | 3,733 MB/s |
| 4 KiB | 3.90 µs | 3.93 µs | 4.78 µs | 3.59 µs | 10.02 µs | 3,772 MB/s |
| 8 KiB | 4.87 µs | 5.02 µs | 5.56 µs | 4.66 µs | 11.73 µs | 3,785 MB/s |
| 16 KiB | 6.94 µs | 7.11 µs | 8.33 µs | 6.74 µs | 12.33 µs | 3,789 MB/s |
| 32 KiB | 11.07 µs | 11.23 µs | 11.87 µs | 10.84 µs | 17.16 µs | 3,792 MB/s |
| 64 KiB | 19.50 µs | 19.54 µs | 21.09 µs | 19.26 µs | 26.20 µs | 3,793 MB/s |
| 128 KiB | 35.91 µs | 36.22 µs | 39.83 µs | 35.75 µs | 40.59 µs | 3,793 MB/s |
| 256 KiB | 68.88 µs | 69.03 µs | 69.53 µs | 68.78 µs | 75.13 µs | 3,794 MB/s |
| 512 KiB | 134.8 µs | 135.0 µs | 135.5 µs | 134.8 µs | 140.1 µs | 3,794 MB/s |
| 1 MiB | 267.8 µs | 268.1 µs | 268.2 µs | 268.1 µs | 275.0 µs | 3,794 MB/s |
| 2 MiB | 531.4 µs | 531.8 µs | 532.1 µs | 532.4 µs | 538.5 µs | 3,794 MB/s |
| 4 MiB | 1,058.6 µs | 1,060.3 µs | 1,061.8 µs | 1,060.9 µs | 1,064.5 µs | 3,794 MB/s |
| 8 MiB | 2,117.9 µs | 2,118.5 µs | 2,122.4 µs | 2,117.6 µs | 2,126.1 µs | 3,794 MB/s |

- 32 B: 1.07 µs one way, so a round trip (write + acknowledgement) is about 2.1 µs.
- 32 KB: 11.07 µs; 64 KiB (= what one rank receives at 3 ranks for 32 KB): 19.5 µs.
- 32 MB is beyond the tool's 8 MiB limit: 4 x 8 MiB = about 8.5 ms; 64 MB (3 ranks) about 17 ms.
- Loaded vs idle: the typical values are about the same; the 99.9% values at small sizes are higher on the loaded machines.

---

## 2. Main result: Baseline vs LL, final code, 3 ranks (round 4)

File: `2026-10-28_round4_final_and_2rank_raw.txt`, part A.

| | 32 B | 32 KB | 32 MB |
|---|---|---|---|
| Baseline (with barrier) | 8.82 µs | 44.8 µs | 42.0 ms |
| LL (no barrier) | 2.15 µs | 33.8 µs | 36.4 ms |
| Baseline / LL | 4.10x | 1.33x | 1.15x |
| Network floor | 2.1 µs (round trip) | 19.5 µs | about 17 ms |

p99 (averaged over the ranks):

| | 32 B | 32 KB | 32 MB |
|---|---|---|---|
| Baseline p99 | 9.69 µs | 47.7 µs | 137.6 ms |
| LL p99 | 2.37 µs | 37.6 µs | 41.0 ms |

Baseline at 32 MB had a few rounds of 130-150 ms in this run (see the max column in section 9), which
pulled its median up. In round 3 (section 6), with the same code, it was 36.5 ms.

---

## 3. 2 ranks vs 3 ranks (round 4)

File: `2026-10-28_round4_final_and_2rank_raw.txt`, parts A (3 ranks) and B (2 ranks).

| | 32 B | 32 KB | 32 MB |
|---|---|---|---|
| LL, 2 ranks | 1.76 µs | 21.8 µs | 24.1 ms |
| LL, 3 ranks | 2.15 µs | 33.8 µs | 36.4 ms |
| Baseline, 2 ranks | 4.39 µs | 22.7 µs | 23.0 ms |
| Baseline, 3 ranks | 8.82 µs | 44.8 µs | 42.0 ms |

Growth from 2 to 3 ranks:

| | 32 B | 32 KB | 32 MB |
|---|---|---|---|
| LL | 1.22x | 1.55x | 1.51x |
| Baseline | 2.01x | 1.97x | 1.82x |

Barrier cost at 32 B (Baseline minus LL): 2 ranks 2.62 µs, 3 ranks 6.67 µs.

---

## 4. Small sizes: 32 B, 64 B, 128 B (2026-11-01)

Log: `logs/2026-11-01_small_sizes_32_64_128B_raw.txt`. PBS job 30202, 2026-11-01,
compute02-04, 1000 rounds. 32 B = 8 floats, 64 B = 16 floats, 128 B = 32 floats (two-shot pads to 9 / 18 / 33).

**Median**, average of the ranks:

| Algorithm | 32 B | 64 B | 128 B |
|---|---|---|---|
| Baseline (with barrier) | 8.45 µs | 8.64 µs | 8.65 µs |
| LL (one-shot) | 2.19 µs | 2.18 µs | 2.24 µs |
| Sentinel (one-shot) | 1.88 µs | 1.87 µs | 1.93 µs |
| Two-shot LL | 4.28 µs | 4.28 µs | 4.25 µs |
| Two-shot Sentinel | 3.52 µs | 3.57 µs | 3.67 µs |
| Bidirectional (2 ranks, 8 chunks per round) | 13.72 µs | 13.61 µs | 13.36 µs |

**p99**, average of the ranks:

| Algorithm | 32 B | 64 B | 128 B |
|---|---|---|---|
| Baseline (with barrier) | 9.81 µs | 9.36 µs | 9.67 µs |
| LL (one-shot) | 2.41 µs | 2.43 µs | 2.54 µs |
| Sentinel (one-shot) | 2.16 µs | 2.40 µs | 2.41 µs |
| Two-shot LL | 5.57 µs | 5.12 µs | 5.73 µs |
| Two-shot Sentinel | 4.46 µs | 4.71 µs | 4.94 µs |
| Bidirectional (2 ranks, 8 chunks per round) | 15.00 µs | 15.18 µs | 15.36 µs |

**Mean**, average of the ranks (the statistic the 2026-10-05 tables used):

| Algorithm | 32 B | 64 B | 128 B |
|---|---|---|---|
| Baseline (with barrier) | 8.00 µs | 7.98 µs | 7.92 µs |
| LL (one-shot) | 2.23 µs | 2.22 µs | 2.28 µs |
| Sentinel (one-shot) | 1.92 µs | 1.93 µs | 1.97 µs |
| Two-shot LL | 4.32 µs | 4.32 µs | 4.30 µs |
| Two-shot Sentinel | 3.57 µs | 3.63 µs | 3.71 µs |
| Bidirectional (2 ranks, 8 chunks per round) | 13.82 µs | 13.62 µs | 13.51 µs |

| | 32 B | 64 B | 128 B |
|---|---|---|---|
| Barrier cost (Baseline - LL, median) | 6.26 µs | 6.46 µs | 6.41 µs |
| Baseline / LL | 3.86x | 3.96x | 3.86x |

Correctness: 96 PASSED, 0 FAILED lines. Exit codes: 51 of 51 processes exited with 0.

The 32 B values here (Baseline 8.45 µs, LL 2.19 µs) differ by a few percent from section 2 (8.82 µs, 2.15 µs):
same code, different day; this is the normal run-to-run spread.

### Version 2 vs version 3 at 32 B, 64 B, 128 B (same job, same nodes)

Median, average of the ranks. Both versions ran one after the other in PBS job 30202.

| Algorithm | 32 B v2 | 32 B v3 | 64 B v2 | 64 B v3 | 128 B v2 | 128 B v3 |
|---|---|---|---|---|---|---|
| Baseline (with barrier) | 8.79 µs | 8.45 µs | 8.82 µs | 8.64 µs | 8.96 µs | 8.65 µs |
| LL (one-shot) | 2.15 µs | 2.19 µs | 2.16 µs | 2.18 µs | 2.25 µs | 2.24 µs |
| Sentinel (one-shot) | 1.85 µs | 1.88 µs | 1.86 µs | 1.87 µs | 1.92 µs | 1.93 µs |
| Two-shot LL | 4.23 µs | 4.28 µs | 4.24 µs | 4.28 µs | 4.29 µs | 4.25 µs |
| Two-shot Sentinel | 3.49 µs | 3.52 µs | 3.55 µs | 3.57 µs | 3.69 µs | 3.67 µs |
| Bidirectional (2 ranks, 8 chunks per round) | 13.68 µs | 13.72 µs | 13.75 µs | 13.61 µs | 13.72 µs | 13.36 µs |

At these sizes the two versions are within a few percent of each other. That is expected: what
version 3 changed (the faster adding step, the per-round check) only matters for larger messages.
Version 2's own tables are in `../results_v2/RESULTS_v2.md`.

---

## 5. All algorithms (round 2)

File: `2026-10-28_round2_all_algorithms_raw.txt`. Same code for every algorithm, measured before the faster adding step, so
Baseline and LL are slower here than in section 2 at 32 KB and 32 MB.

| 3 ranks | 32 B | 32 KB | 32 MB |
|---|---|---|---|
| Baseline_Barrier | 8.62 µs | 52.4 µs | 43.9 ms |
| LL | 2.18 µs | 45.3 µs | 44.4 ms |
| Sentinel | 1.87 µs | 42.7 µs | 38.6 ms |
| Twoshot_LL | 4.29 µs | 34.4 µs | 31.6 ms |
| Twoshot_Sentinel | 3.52 µs | 36.6 µs | 32.7 ms |
| LL128_Atomic | SKIPPED (no RDMA atomics), exit=2 | - | - |

| 2 ranks, one round = 8 chunks one after the other | 32 B | 32 KB | 32 MB |
|---|---|---|---|
| Bidirectional_DoubleBuffering | 13.8 µs | 38.0 µs | 19.4 ms |

Two-shot programs pad the vector to a multiple of 3 floats (9 / 8193 / 8,388,609 floats).

Negative test (LL with the flag wait removed on purpose; it must fail):

| | 32 B | 32 KB |
|---|---|---|
| median | 2.20 µs | 25.8 µs |
| correctness lines | 6 FAILED, 0 PASSED | 6 FAILED, 0 PASSED |

---

## 6. Improvement: before vs after the faster adding step (round 3)

File: `2026-10-28_round3_optimisation_raw.txt`. All builds in one job on the same machines. Before = old adding step, -O2
(the code of rounds 1 and 2). After = new adding step, -O3 (the final code).

| 3 ranks | 32 B | 32 KB | 32 MB |
|---|---|---|---|
| LL before | 2.16 µs | 45.8 µs | 44.2 ms |
| LL after | 2.16 µs | 33.8 µs (-26%) | 36.5 ms (-17%) |
| Baseline before | 8.74 µs | 52.3 µs | 44.0 ms |
| Baseline after | 8.83 µs | 44.8 µs (-14%) | 36.5 ms (-17%) |

All four builds (for reference):

| Build | adding step | flag | LL 32 B | LL 32 KB | LL 32 MB | Baseline 32 B | Baseline 32 KB | Baseline 32 MB |
|---|---|---|---|---|---|---|---|---|
| old_O2 | old | -O2 | 2.16 µs | 45.8 µs | 44.2 ms | 8.74 µs | 52.3 µs | 44.0 ms |
| old_O3 | old | -O3 | 2.16 µs | 46.4 µs | 45.9 ms | 8.74 µs | 52.3 µs | 43.9 ms |
| blk_O2 | new | -O2 | 2.16 µs | 40.2 µs | 39.3 ms | 8.66 µs | 46.5 µs | 39.3 ms |
| blk_O3 | new | -O3 | 2.16 µs | 33.8 µs | 36.5 ms | 8.83 µs | 44.8 µs | 36.5 ms |

---

## 7. Repeated runs of Baseline and LL

The same programs at the same sizes appear in several files. Rounds 1, 2 and round 3 `old_O2` used the
same (old) code; round 3 `blk_O3` and round 4 used the final code.

| 3 ranks | LL 32 B | LL 32 KB | LL 32 MB | Baseline 32 B | Baseline 32 KB | Baseline 32 MB |
|---|---|---|---|---|---|---|
| round 1 (old code) | 2.17 µs | 45.2 µs | 43.9 ms | 8.77 µs | 64.4 µs | 46.5 ms |
| round 2 (old code) | 2.18 µs | 45.3 µs | 44.4 ms | 8.62 µs | 52.4 µs | 43.9 ms |
| round 3 old_O2 (old code) | 2.16 µs | 45.8 µs | 44.2 ms | 8.74 µs | 52.3 µs | 44.0 ms |
| round 3 blk_O3 (final code) | 2.16 µs | 33.8 µs | 36.5 ms | 8.83 µs | 44.8 µs | 36.5 ms |
| round 4 (final code) | 2.15 µs | 33.8 µs | 36.4 ms | 8.82 µs | 44.8 µs | 42.0 ms |

---

## 8. Correctness checks

Each rank prints one or two `Correctness check` lines per run (first round, and every timed round).

| File | PASSED lines | FAILED lines | FAILED lines from the negative test | runs with exit not 0 |
|---|---|---|---|---|
| `2026-10-28_round1_floor_baseline_ll_raw.txt` | 18 | 0 | 0 | none |
| `2026-10-28_round2_all_algorithms_raw.txt` | 96 | 12 | 12 | LL_NoWait_NegativeTest (8 floats): exit [1]; LL_NoWait_NegativeTest (8192 floats): exit [1]; LL128_Atomic (8 floats): exit [2] |
| `2026-10-28_round3_optimisation_raw.txt` | 144 | 0 | 0 | none |
| `2026-10-28_round4_final_and_2rank_raw.txt` | 60 | 0 | 0 | none |

Round 1 has only the first-round check (the every-round check was added after it). LL128_Atomic exits
with 2 on purpose (SKIPPED). Bidirectional and Atomic_Probe have no every-round check.

---

## 9. Every run, per rank

min / median / p99 / max in µs for each rank. `section` = the heading in the raw file.

### `2026-10-28_round1_floor_baseline_ll_raw.txt`

| section | program | ranks | floats | rank | min | median | p99 | max |
|---|---|---|---|---|---|---|---|---|
| STEP 2: 32 B and 32 KB | Baseline_Barrier | 3 | 8 | 0 | 6.10 | 8.20 | 9.39 | 13.32 |
| STEP 2: 32 B and 32 KB | Baseline_Barrier | 3 | 8 | 1 | 6.66 | 9.06 | 9.55 | 14.18 |
| STEP 2: 32 B and 32 KB | Baseline_Barrier | 3 | 8 | 2 | 4.71 | 9.04 | 9.71 | 15.20 |
| STEP 2: 32 B and 32 KB | LL | 3 | 8 | 0 | 2.11 | 2.18 | 2.63 | 18.45 |
| STEP 2: 32 B and 32 KB | LL | 3 | 8 | 1 | 2.10 | 2.17 | 2.60 | 18.58 |
| STEP 2: 32 B and 32 KB | LL | 3 | 8 | 2 | 2.12 | 2.17 | 2.66 | 18.42 |
| STEP 2: 32 B and 32 KB | Baseline_Barrier | 3 | 8192 | 0 | 45.82 | 64.48 | 81.14 | 88.54 |
| STEP 2: 32 B and 32 KB | Baseline_Barrier | 3 | 8192 | 1 | 47.92 | 64.64 | 78.49 | 83.08 |
| STEP 2: 32 B and 32 KB | Baseline_Barrier | 3 | 8192 | 2 | 45.48 | 63.97 | 83.11 | 97.02 |
| STEP 2: 32 B and 32 KB | LL | 3 | 8192 | 0 | 40.27 | 45.35 | 51.15 | 62.63 |
| STEP 2: 32 B and 32 KB | LL | 3 | 8192 | 1 | 28.92 | 45.34 | 56.12 | 61.45 |
| STEP 2: 32 B and 32 KB | LL | 3 | 8192 | 2 | 26.94 | 45.04 | 53.29 | 63.34 |
| STEP 3: 32 MB | Baseline_Barrier | 3 | 8388608 | 0 | 44,156.04 | 46,580.25 | 50,101.29 | 50,769.35 |
| STEP 3: 32 MB | Baseline_Barrier | 3 | 8388608 | 1 | 43,582.41 | 46,494.23 | 52,010.31 | 53,195.90 |
| STEP 3: 32 MB | Baseline_Barrier | 3 | 8388608 | 2 | 41,729.25 | 46,413.82 | 52,031.53 | 54,195.81 |
| STEP 3: 32 MB | LL | 3 | 8388608 | 0 | 42,544.33 | 43,891.03 | 47,798.74 | 47,800.37 |
| STEP 3: 32 MB | LL | 3 | 8388608 | 1 | 42,619.34 | 43,976.24 | 46,498.12 | 46,933.68 |
| STEP 3: 32 MB | LL | 3 | 8388608 | 2 | 39,599.72 | 43,867.72 | 49,403.90 | 49,563.76 |

### `2026-10-28_round2_all_algorithms_raw.txt`

| section | program | ranks | floats | rank | min | median | p99 | max |
|---|---|---|---|---|---|---|---|---|
| A: negative test | LL_NoWait_NegativeTest | 3 | 8 | 0 | 2.13 | 2.21 | 2.40 | 4.61 |
| A: negative test | LL_NoWait_NegativeTest | 3 | 8 | 1 | 2.14 | 2.20 | 2.35 | 3.66 |
| A: negative test | LL_NoWait_NegativeTest | 3 | 8 | 2 | 2.13 | 2.20 | 2.41 | 6.66 |
| A: negative test | LL_NoWait_NegativeTest | 3 | 8192 | 0 | 23.64 | 24.89 | 30.49 | 36.44 |
| A: negative test | LL_NoWait_NegativeTest | 3 | 8192 | 1 | 23.45 | 27.52 | 38.80 | 41.05 |
| A: negative test | LL_NoWait_NegativeTest | 3 | 8192 | 2 | 23.44 | 24.93 | 31.65 | 40.65 |
| B+C: 32 B and 32 KB | Baseline_Barrier | 3 | 8 | 0 | 6.10 | 8.22 | 9.01 | 11.76 |
| B+C: 32 B and 32 KB | Baseline_Barrier | 3 | 8 | 1 | 6.66 | 8.99 | 9.43 | 13.04 |
| B+C: 32 B and 32 KB | Baseline_Barrier | 3 | 8 | 2 | 6.58 | 8.65 | 9.41 | 13.03 |
| B+C: 32 B and 32 KB | LL | 3 | 8 | 0 | 2.11 | 2.18 | 2.73 | 20.20 |
| B+C: 32 B and 32 KB | LL | 3 | 8 | 1 | 2.10 | 2.17 | 2.35 | 19.64 |
| B+C: 32 B and 32 KB | LL | 3 | 8 | 2 | 2.11 | 2.18 | 2.39 | 8.15 |
| B+C: 32 B and 32 KB | Sentinel | 3 | 8 | 0 | 1.81 | 1.87 | 2.06 | 7.14 |
| B+C: 32 B and 32 KB | Sentinel | 3 | 8 | 1 | 1.81 | 1.87 | 2.03 | 7.71 |
| B+C: 32 B and 32 KB | Sentinel | 3 | 8 | 2 | 1.79 | 1.87 | 2.02 | 7.78 |
| B+C: 32 B and 32 KB | Twoshot_LL | 3 | 8 | 0 | 4.19 | 4.30 | 5.40 | 8.55 |
| B+C: 32 B and 32 KB | Twoshot_LL | 3 | 8 | 1 | 4.16 | 4.29 | 5.66 | 9.61 |
| B+C: 32 B and 32 KB | Twoshot_LL | 3 | 8 | 2 | 4.17 | 4.29 | 5.38 | 8.94 |
| B+C: 32 B and 32 KB | Twoshot_Sentinel | 3 | 8 | 0 | 3.46 | 3.52 | 4.35 | 10.26 |
| B+C: 32 B and 32 KB | Twoshot_Sentinel | 3 | 8 | 1 | 3.43 | 3.52 | 4.97 | 10.86 |
| B+C: 32 B and 32 KB | Twoshot_Sentinel | 3 | 8 | 2 | 3.42 | 3.51 | 4.75 | 10.47 |
| B+C: 32 B and 32 KB | Baseline_Barrier | 3 | 8192 | 0 | 49.17 | 52.56 | 56.63 | 65.82 |
| B+C: 32 B and 32 KB | Baseline_Barrier | 3 | 8192 | 1 | 48.80 | 52.26 | 57.72 | 66.46 |
| B+C: 32 B and 32 KB | Baseline_Barrier | 3 | 8192 | 2 | 47.25 | 52.35 | 56.85 | 71.17 |
| B+C: 32 B and 32 KB | LL | 3 | 8192 | 0 | 39.43 | 45.37 | 50.55 | 62.06 |
| B+C: 32 B and 32 KB | LL | 3 | 8192 | 1 | 36.46 | 45.23 | 49.77 | 60.15 |
| B+C: 32 B and 32 KB | LL | 3 | 8192 | 2 | 26.75 | 45.22 | 49.90 | 64.33 |
| B+C: 32 B and 32 KB | Sentinel | 3 | 8192 | 0 | 34.96 | 43.26 | 48.92 | 55.70 |
| B+C: 32 B and 32 KB | Sentinel | 3 | 8192 | 1 | 36.37 | 42.50 | 48.07 | 57.94 |
| B+C: 32 B and 32 KB | Sentinel | 3 | 8192 | 2 | 34.26 | 42.23 | 48.22 | 59.77 |
| B+C: 32 B and 32 KB | Twoshot_LL | 3 | 8192 | 0 | 30.08 | 34.63 | 40.27 | 47.83 |
| B+C: 32 B and 32 KB | Twoshot_LL | 3 | 8192 | 1 | 31.50 | 34.28 | 40.02 | 50.55 |
| B+C: 32 B and 32 KB | Twoshot_LL | 3 | 8192 | 2 | 32.60 | 34.44 | 39.95 | 49.80 |
| B+C: 32 B and 32 KB | Twoshot_Sentinel | 3 | 8192 | 0 | 34.73 | 36.48 | 41.74 | 49.34 |
| B+C: 32 B and 32 KB | Twoshot_Sentinel | 3 | 8192 | 1 | 34.53 | 36.72 | 41.46 | 54.18 |
| B+C: 32 B and 32 KB | Twoshot_Sentinel | 3 | 8192 | 2 | 35.14 | 36.73 | 40.28 | 53.38 |
| B+C: 32 MB | Baseline_Barrier | 3 | 8388608 | 0 | 43,730.04 | 43,859.35 | 46,686.97 | 47,345.57 |
| B+C: 32 MB | Baseline_Barrier | 3 | 8388608 | 1 | 43,680.69 | 43,865.94 | 47,224.31 | 47,679.46 |
| B+C: 32 MB | Baseline_Barrier | 3 | 8388608 | 2 | 41,158.10 | 43,849.47 | 49,889.81 | 50,329.25 |
| B+C: 32 MB | LL | 3 | 8388608 | 0 | 39,734.33 | 44,485.66 | 48,761.11 | 50,276.95 |
| B+C: 32 MB | LL | 3 | 8388608 | 1 | 41,006.50 | 44,482.98 | 47,893.10 | 47,899.81 |
| B+C: 32 MB | LL | 3 | 8388608 | 2 | 39,461.24 | 44,146.88 | 49,138.41 | 52,554.12 |
| B+C: 32 MB | Sentinel | 3 | 8388608 | 0 | 38,263.92 | 38,579.74 | 42,785.21 | 43,133.68 |
| B+C: 32 MB | Sentinel | 3 | 8388608 | 1 | 38,143.33 | 38,578.97 | 43,325.02 | 43,804.29 |
| B+C: 32 MB | Sentinel | 3 | 8388608 | 2 | 36,399.13 | 38,552.43 | 43,674.84 | 45,885.32 |
| B+C: 32 MB | Twoshot_LL | 3 | 8388608 | 0 | 31,170.74 | 31,606.34 | 36,402.90 | 37,389.43 |
| B+C: 32 MB | Twoshot_LL | 3 | 8388608 | 1 | 31,005.80 | 31,641.46 | 35,890.22 | 38,089.58 |
| B+C: 32 MB | Twoshot_LL | 3 | 8388608 | 2 | 30,177.33 | 31,628.98 | 37,271.32 | 37,366.30 |
| B+C: 32 MB | Twoshot_Sentinel | 3 | 8388608 | 0 | 31,786.94 | 32,672.12 | 34,830.28 | 35,294.25 |
| B+C: 32 MB | Twoshot_Sentinel | 3 | 8388608 | 1 | 32,013.67 | 32,708.42 | 35,955.28 | 36,938.72 |
| B+C: 32 MB | Twoshot_Sentinel | 3 | 8388608 | 2 | 31,912.03 | 32,666.66 | 35,900.89 | 36,800.64 |
| D: Bidirectional (2 ranks, 8 chunks per pass) | Bidirectional_DoubleBuffering | 2 | 8 | 0 | 13.56 | 13.78 | 15.27 | 25.74 |
| D: Bidirectional (2 ranks, 8 chunks per pass) | Bidirectional_DoubleBuffering | 2 | 8 | 1 | 13.55 | 13.76 | 15.21 | 24.96 |
| D: Bidirectional (2 ranks, 8 chunks per pass) | Bidirectional_DoubleBuffering | 2 | 8192 | 0 | 37.63 | 37.98 | 40.06 | 49.57 |
| D: Bidirectional (2 ranks, 8 chunks per pass) | Bidirectional_DoubleBuffering | 2 | 8192 | 1 | 37.61 | 37.97 | 40.13 | 48.80 |
| D: Bidirectional (2 ranks, 8 chunks per pass) | Bidirectional_DoubleBuffering | 2 | 8388608 | 0 | 19,163.54 | 19,435.95 | 19,992.80 | 20,127.04 |
| D: Bidirectional (2 ranks, 8 chunks per pass) | Bidirectional_DoubleBuffering | 2 | 8388608 | 1 | 19,030.21 | 19,435.45 | 20,037.00 | 20,037.99 |
| E: atomics | Atomic_Probe | 2 | 8 | - | no timing | | | |
| E: atomics | LL128_Atomic | 3 | 8 | - | SKIPPED | | | |

### `2026-10-28_round3_optimisation_raw.txt`

| section | program | ranks | floats | rank | min | median | p99 | max |
|---|---|---|---|---|---|---|---|---|
| variant old_O2: 32 B and 32 KB | Baseline_Barrier | 3 | 8 | 0 | 6.09 | 8.19 | 9.89 | 21.89 |
| variant old_O2: 32 B and 32 KB | Baseline_Barrier | 3 | 8 | 1 | 6.66 | 9.03 | 9.42 | 23.37 |
| variant old_O2: 32 B and 32 KB | Baseline_Barrier | 3 | 8 | 2 | 6.68 | 9.00 | 9.82 | 21.49 |
| variant old_O2: 32 B and 32 KB | LL | 3 | 8 | 0 | 2.08 | 2.17 | 2.38 | 9.47 |
| variant old_O2: 32 B and 32 KB | LL | 3 | 8 | 1 | 2.10 | 2.15 | 2.37 | 9.65 |
| variant old_O2: 32 B and 32 KB | LL | 3 | 8 | 2 | 2.10 | 2.16 | 2.37 | 10.15 |
| variant old_O2: 32 B and 32 KB | Baseline_Barrier | 3 | 8192 | 0 | 49.70 | 52.38 | 56.44 | 71.43 |
| variant old_O2: 32 B and 32 KB | Baseline_Barrier | 3 | 8192 | 1 | 48.69 | 52.20 | 57.11 | 69.44 |
| variant old_O2: 32 B and 32 KB | Baseline_Barrier | 3 | 8192 | 2 | 45.42 | 52.30 | 56.99 | 70.19 |
| variant old_O2: 32 B and 32 KB | LL | 3 | 8192 | 0 | 35.48 | 46.13 | 55.07 | 64.45 |
| variant old_O2: 32 B and 32 KB | LL | 3 | 8192 | 1 | 32.99 | 45.85 | 54.38 | 64.51 |
| variant old_O2: 32 B and 32 KB | LL | 3 | 8192 | 2 | 28.10 | 45.44 | 56.44 | 66.87 |
| variant old_O2: 32 MB | Baseline_Barrier | 3 | 8388608 | 0 | 43,755.17 | 43,960.91 | 46,856.54 | 47,057.65 |
| variant old_O2: 32 MB | Baseline_Barrier | 3 | 8388608 | 1 | 43,695.46 | 43,953.51 | 46,991.14 | 47,037.94 |
| variant old_O2: 32 MB | Baseline_Barrier | 3 | 8388608 | 2 | 41,228.20 | 43,951.51 | 50,484.18 | 50,830.74 |
| variant old_O2: 32 MB | LL | 3 | 8388608 | 0 | 40,966.93 | 44,378.35 | 49,019.78 | 49,637.91 |
| variant old_O2: 32 MB | LL | 3 | 8388608 | 1 | 41,135.75 | 44,345.00 | 47,924.31 | 48,424.50 |
| variant old_O2: 32 MB | LL | 3 | 8388608 | 2 | 39,277.39 | 43,906.67 | 49,629.34 | 51,076.50 |
| variant old_O3: 32 B and 32 KB | Baseline_Barrier | 3 | 8 | 0 | 6.22 | 8.23 | 9.37 | 12.47 |
| variant old_O3: 32 B and 32 KB | Baseline_Barrier | 3 | 8 | 1 | 6.63 | 9.00 | 9.52 | 12.11 |
| variant old_O3: 32 B and 32 KB | Baseline_Barrier | 3 | 8 | 2 | 6.61 | 8.98 | 9.63 | 12.16 |
| variant old_O3: 32 B and 32 KB | LL | 3 | 8 | 0 | 2.10 | 2.17 | 2.45 | 8.19 |
| variant old_O3: 32 B and 32 KB | LL | 3 | 8 | 1 | 2.07 | 2.16 | 2.39 | 8.58 |
| variant old_O3: 32 B and 32 KB | LL | 3 | 8 | 2 | 2.10 | 2.16 | 2.37 | 7.87 |
| variant old_O3: 32 B and 32 KB | Baseline_Barrier | 3 | 8192 | 0 | 50.03 | 52.52 | 56.49 | 65.28 |
| variant old_O3: 32 B and 32 KB | Baseline_Barrier | 3 | 8192 | 1 | 48.92 | 52.18 | 56.98 | 64.64 |
| variant old_O3: 32 B and 32 KB | Baseline_Barrier | 3 | 8192 | 2 | 45.40 | 52.28 | 56.78 | 71.00 |
| variant old_O3: 32 B and 32 KB | LL | 3 | 8192 | 0 | 25.28 | 46.54 | 60.10 | 70.38 |
| variant old_O3: 32 B and 32 KB | LL | 3 | 8192 | 1 | 28.29 | 46.54 | 59.05 | 68.27 |
| variant old_O3: 32 B and 32 KB | LL | 3 | 8192 | 2 | 28.89 | 46.13 | 59.58 | 63.05 |
| variant old_O3: 32 MB | Baseline_Barrier | 3 | 8388608 | 0 | 43,634.43 | 43,926.03 | 46,609.15 | 47,641.91 |
| variant old_O3: 32 MB | Baseline_Barrier | 3 | 8388608 | 1 | 43,603.15 | 43,916.89 | 47,210.59 | 47,540.32 |
| variant old_O3: 32 MB | Baseline_Barrier | 3 | 8388608 | 2 | 41,221.58 | 43,893.45 | 50,204.31 | 50,436.68 |
| variant old_O3: 32 MB | LL | 3 | 8388608 | 0 | 35,991.18 | 46,077.05 | 54,319.92 | 54,723.73 |
| variant old_O3: 32 MB | LL | 3 | 8388608 | 1 | 41,130.33 | 46,047.65 | 52,185.84 | 52,463.72 |
| variant old_O3: 32 MB | LL | 3 | 8388608 | 2 | 41,051.31 | 45,681.31 | 51,606.37 | 55,096.91 |
| variant blk_O2: 32 B and 32 KB | Baseline_Barrier | 3 | 8 | 0 | 6.01 | 8.32 | 9.06 | 20.85 |
| variant blk_O2: 32 B and 32 KB | Baseline_Barrier | 3 | 8 | 1 | 6.71 | 9.10 | 9.70 | 22.27 |
| variant blk_O2: 32 B and 32 KB | Baseline_Barrier | 3 | 8 | 2 | 6.56 | 8.56 | 9.79 | 22.09 |
| variant blk_O2: 32 B and 32 KB | LL | 3 | 8 | 0 | 2.09 | 2.17 | 2.49 | 29.44 |
| variant blk_O2: 32 B and 32 KB | LL | 3 | 8 | 1 | 2.08 | 2.15 | 2.45 | 29.48 |
| variant blk_O2: 32 B and 32 KB | LL | 3 | 8 | 2 | 2.10 | 2.16 | 2.48 | 27.58 |
| variant blk_O2: 32 B and 32 KB | Baseline_Barrier | 3 | 8192 | 0 | 44.60 | 46.61 | 50.24 | 58.87 |
| variant blk_O2: 32 B and 32 KB | Baseline_Barrier | 3 | 8192 | 1 | 43.49 | 46.50 | 51.46 | 60.56 |
| variant blk_O2: 32 B and 32 KB | Baseline_Barrier | 3 | 8192 | 2 | 43.59 | 46.52 | 51.20 | 63.04 |
| variant blk_O2: 32 B and 32 KB | LL | 3 | 8192 | 0 | 34.03 | 40.24 | 45.32 | 90.43 |
| variant blk_O2: 32 B and 32 KB | LL | 3 | 8192 | 1 | 22.05 | 40.10 | 46.22 | 90.25 |
| variant blk_O2: 32 B and 32 KB | LL | 3 | 8192 | 2 | 24.11 | 40.11 | 43.93 | 90.21 |
| variant blk_O2: 32 MB | Baseline_Barrier | 3 | 8388608 | 0 | 39,136.50 | 39,330.14 | 41,979.56 | 42,224.93 |
| variant blk_O2: 32 MB | Baseline_Barrier | 3 | 8388608 | 1 | 39,159.23 | 39,326.20 | 42,464.26 | 42,645.62 |
| variant blk_O2: 32 MB | Baseline_Barrier | 3 | 8388608 | 2 | 36,029.60 | 39,312.72 | 45,438.98 | 45,541.51 |
| variant blk_O2: 32 MB | LL | 3 | 8388608 | 0 | 37,477.89 | 39,316.15 | 43,332.58 | 43,528.11 |
| variant blk_O2: 32 MB | LL | 3 | 8388608 | 1 | 37,289.85 | 39,303.25 | 42,379.25 | 42,854.09 |
| variant blk_O2: 32 MB | LL | 3 | 8388608 | 2 | 35,334.42 | 39,319.90 | 44,758.69 | 45,681.92 |
| variant blk_O3: 32 B and 32 KB | Baseline_Barrier | 3 | 8 | 0 | 6.94 | 8.28 | 9.76 | 22.71 |
| variant blk_O3: 32 B and 32 KB | Baseline_Barrier | 3 | 8 | 1 | 6.68 | 9.13 | 10.46 | 24.15 |
| variant blk_O3: 32 B and 32 KB | Baseline_Barrier | 3 | 8 | 2 | 4.83 | 9.07 | 10.56 | 25.60 |
| variant blk_O3: 32 B and 32 KB | LL | 3 | 8 | 0 | 2.10 | 2.16 | 2.37 | 9.04 |
| variant blk_O3: 32 B and 32 KB | LL | 3 | 8 | 1 | 2.09 | 2.15 | 2.38 | 9.29 |
| variant blk_O3: 32 B and 32 KB | LL | 3 | 8 | 2 | 2.10 | 2.16 | 2.34 | 8.73 |
| variant blk_O3: 32 B and 32 KB | Baseline_Barrier | 3 | 8192 | 0 | 40.73 | 44.83 | 46.91 | 56.33 |
| variant blk_O3: 32 B and 32 KB | Baseline_Barrier | 3 | 8192 | 1 | 36.88 | 44.87 | 47.84 | 58.57 |
| variant blk_O3: 32 B and 32 KB | Baseline_Barrier | 3 | 8192 | 2 | 38.30 | 44.70 | 47.76 | 59.13 |
| variant blk_O3: 32 B and 32 KB | LL | 3 | 8192 | 0 | 28.68 | 33.76 | 37.43 | 44.27 |
| variant blk_O3: 32 B and 32 KB | LL | 3 | 8192 | 1 | 22.52 | 33.81 | 37.84 | 48.73 |
| variant blk_O3: 32 B and 32 KB | LL | 3 | 8192 | 2 | 22.90 | 33.73 | 37.48 | 47.03 |
| variant blk_O3: 32 MB | Baseline_Barrier | 3 | 8388608 | 0 | 36,150.99 | 36,490.78 | 37,966.44 | 38,101.49 |
| variant blk_O3: 32 MB | Baseline_Barrier | 3 | 8388608 | 1 | 36,179.84 | 36,480.32 | 38,179.23 | 38,236.24 |
| variant blk_O3: 32 MB | Baseline_Barrier | 3 | 8388608 | 2 | 32,071.30 | 36,454.47 | 42,535.43 | 42,686.93 |
| variant blk_O3: 32 MB | LL | 3 | 8388608 | 0 | 32,413.47 | 36,591.91 | 39,959.10 | 41,395.00 |
| variant blk_O3: 32 MB | LL | 3 | 8388608 | 1 | 32,200.55 | 36,470.24 | 40,166.28 | 43,027.13 |
| variant blk_O3: 32 MB | LL | 3 | 8388608 | 2 | 31,461.68 | 36,503.68 | 42,619.51 | 42,719.70 |

### `2026-10-28_round4_final_and_2rank_raw.txt`

| section | program | ranks | floats | rank | min | median | p99 | max |
|---|---|---|---|---|---|---|---|---|
| A: 3 ranks, final code | Baseline_Barrier | 3 | 8 | 0 | 7.04 | 8.31 | 9.14 | 27.43 |
| A: 3 ranks, final code | Baseline_Barrier | 3 | 8 | 1 | 6.67 | 9.10 | 9.87 | 27.50 |
| A: 3 ranks, final code | Baseline_Barrier | 3 | 8 | 2 | 4.78 | 9.06 | 10.07 | 27.75 |
| A: 3 ranks, final code | LL | 3 | 8 | 0 | 2.09 | 2.16 | 2.37 | 8.15 |
| A: 3 ranks, final code | LL | 3 | 8 | 1 | 2.09 | 2.15 | 2.37 | 8.21 |
| A: 3 ranks, final code | LL | 3 | 8 | 2 | 2.10 | 2.15 | 2.36 | 5.86 |
| A: 3 ranks, final code | Baseline_Barrier | 3 | 8192 | 0 | 37.49 | 44.83 | 47.10 | 59.59 |
| A: 3 ranks, final code | Baseline_Barrier | 3 | 8192 | 1 | 39.18 | 44.93 | 48.02 | 60.99 |
| A: 3 ranks, final code | Baseline_Barrier | 3 | 8192 | 2 | 37.74 | 44.68 | 47.98 | 59.01 |
| A: 3 ranks, final code | LL | 3 | 8192 | 0 | 27.36 | 33.82 | 37.38 | 53.14 |
| A: 3 ranks, final code | LL | 3 | 8192 | 1 | 27.93 | 33.84 | 37.45 | 53.05 |
| A: 3 ranks, final code | LL | 3 | 8192 | 2 | 21.30 | 33.78 | 37.86 | 53.01 |
| A: 3 ranks, final code | Baseline_Barrier | 3 | 8388608 | 0 | 36,176.11 | 42,038.22 | 136,054.47 | 136,120.65 |
| A: 3 ranks, final code | Baseline_Barrier | 3 | 8388608 | 1 | 36,058.53 | 42,010.77 | 131,359.36 | 132,932.45 |
| A: 3 ranks, final code | Baseline_Barrier | 3 | 8388608 | 2 | 32,663.01 | 41,937.93 | 145,399.23 | 149,511.60 |
| A: 3 ranks, final code | LL | 3 | 8388608 | 0 | 32,558.61 | 36,388.57 | 40,479.20 | 41,851.49 |
| A: 3 ranks, final code | LL | 3 | 8388608 | 1 | 32,046.31 | 36,402.87 | 40,186.68 | 41,766.81 |
| A: 3 ranks, final code | LL | 3 | 8388608 | 2 | 31,601.10 | 36,381.80 | 42,285.24 | 42,555.91 |
| B: 2 ranks, final code | Baseline_Barrier | 2 | 8 | 0 | 3.07 | 4.42 | 4.65 | 15.16 |
| B: 2 ranks, final code | Baseline_Barrier | 2 | 8 | 1 | 3.63 | 4.35 | 4.65 | 14.49 |
| B: 2 ranks, final code | LL | 2 | 8 | 0 | 1.69 | 1.76 | 1.87 | 10.74 |
| B: 2 ranks, final code | LL | 2 | 8 | 1 | 1.68 | 1.76 | 1.87 | 11.50 |
| B: 2 ranks, final code | Baseline_Barrier | 2 | 8192 | 0 | 21.67 | 22.66 | 24.73 | 31.45 |
| B: 2 ranks, final code | Baseline_Barrier | 2 | 8192 | 1 | 21.43 | 22.81 | 24.56 | 32.55 |
| B: 2 ranks, final code | LL | 2 | 8192 | 0 | 12.65 | 21.82 | 26.40 | 36.56 |
| B: 2 ranks, final code | LL | 2 | 8192 | 1 | 12.85 | 21.87 | 26.66 | 36.73 |
| B: 2 ranks, final code | Baseline_Barrier | 2 | 8388608 | 0 | 22,548.12 | 23,001.12 | 23,928.34 | 24,205.63 |
| B: 2 ranks, final code | Baseline_Barrier | 2 | 8388608 | 1 | 22,545.76 | 23,032.39 | 23,387.57 | 24,593.13 |
| B: 2 ranks, final code | LL | 2 | 8388608 | 0 | 16,138.87 | 24,013.64 | 33,763.43 | 44,402.76 |
| B: 2 ranks, final code | LL | 2 | 8388608 | 1 | 15,304.03 | 24,099.43 | 35,637.13 | 43,131.15 |

