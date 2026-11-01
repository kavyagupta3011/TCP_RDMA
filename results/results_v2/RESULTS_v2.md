# Version 2 results (2026-10-05)

Results of **version 2** (`paper_algorithms_v2/`): the first RDMA version of the code plus the fixes
and speed-ups made on 2026-10-05. Every number below is copied from the files in `logs/`; the file
is named above each table.

How to read the tables:
- Size = the vector each rank contributes. The programs take it in floats:
  128 B = 32 floats, 1 KiB = 256, 4 KiB = 1,024, 16 KiB = 4,096, 64 KiB = 16,384, 256 KiB = 65,536.
  Two-shot programs pad to a multiple of 3 floats (33, 258, 1,026, 4,098, 16,386).
- A table value = the **mean** time of one AllReduce, averaged over the 3 ranks, unless the table
  says median. (Version 2 printed the mean, min, median, p99 and max per rank; the summaries kept
  mainly the mean.)
- 3 ranks, one per machine, on **compute00, compute01, compute02** (section 2: compute02-04). 1000 timed rounds, except the
  256 KiB runs (100 rounds).
- Bidirectional uses 2 ranks, and one of its rounds = 8 chunks one after the other, so its time is
  not comparable with the other rows.
- Every run listed here printed `Correctness check vs. CPU reference: PASSED` on every rank.

Contents
1. Main result: all algorithms, final version-2 code
2. Small sizes: 32 B, 64 B, 128 B (re-run 2026-11-01)
3. How version 2 got there (the steps of 2026-10-05)
4. Findings
5. Limits of these results (why version 3 was needed)
6. Where each number comes from

---

## 1. Main result: all algorithms, final version-2 code

File: `2026-10-05_all_algorithms_size_sweep.txt` (PBS job from `run_all_sizes.pbs`).

### 1.1 Mean time, average of the 3 ranks

| Algorithm | 128 B | 1 KiB | 4 KiB | 16 KiB | 64 KiB |
|---|---|---|---|---|---|
| Baseline (with barrier) | 8.20 µs | 10.08 µs | 16.31 µs | 37.93 µs | 121.0 µs |
| LL (one-shot) | 2.36 µs | **3.67 µs** | 8.99 µs | 27.24 µs | 95.52 µs |
| Sentinel (one-shot) | **2.10 µs** | 4.05 µs | 10.21 µs | 29.01 µs | 91.99 µs |
| Two-shot LL | 4.48 µs | 5.55 µs | **8.68 µs** | **22.26 µs** | **73.01 µs** |
| Two-shot Sentinel | 3.76 µs | 5.67 µs | 11.54 µs | 32.66 µs | 93.15 µs |
| LL128 Atomic | cannot run (no RDMA atomics) | | | | |
| Bidirectional (2 ranks, 8 chunks per round) | 14.17 µs | 14.14 µs | 19.69 µs | 27.11 µs | 59.85 µs |

Bold = fastest of the 3-rank algorithms at that size.

### 1.2 Median, rank 0

| Algorithm | 128 B | 1 KiB | 4 KiB | 16 KiB | 64 KiB |
|---|---|---|---|---|---|
| Baseline (with barrier) | 8.33 µs | 10.07 µs | 16.12 µs | 37.60 µs | 120.1 µs |
| LL (one-shot) | 2.32 µs | 3.60 µs | 9.00 µs | 27.30 µs | 91.43 µs |
| Sentinel (one-shot) | 2.02 µs | 3.96 µs | 10.05 µs | 29.23 µs | 88.72 µs |
| Two-shot LL | 4.39 µs | 5.50 µs | 8.55 µs | 22.14 µs | 72.36 µs |
| Two-shot Sentinel | 3.69 µs | 5.55 µs | 11.45 µs | 32.21 µs | 90.99 µs |
| Bidirectional (2 ranks) | 14.08 µs | 14.00 µs | 19.52 µs | 26.93 µs | 59.71 µs |

The medians are within a few percent of the means: there were no large one-off stalls in this run.

### 1.3 Removing the barrier: best barrier-free algorithm vs Baseline

| Size | Baseline | Best without barrier | Faster by |
|---|---|---|---|
| 128 B | 8.20 µs | Sentinel 2.10 µs | 3.90x (LL: 3.47x) |
| 1 KiB | 10.08 µs | LL 3.67 µs | 2.75x |
| 4 KiB | 16.31 µs | Two-shot LL 8.68 µs | 1.88x |
| 16 KiB | 37.93 µs | Two-shot LL 22.26 µs | 1.70x |
| 64 KiB | 121.0 µs | Two-shot LL 73.01 µs | 1.66x |

The barrier costs about 6 µs per AllReduce, a fixed amount. So removing it helps most for tiny
messages (almost 4x) and less as the message grows and moving data takes over.

### 1.4 Two-shot vs one-shot (two-shot time / one-shot time)

| Pair | 128 B | 1 KiB | 4 KiB | 16 KiB | 64 KiB |
|---|---|---|---|---|---|
| Two-shot LL / LL | 1.90x | 1.51x | 0.97x | 0.82x | 0.76x |
| Two-shot Sentinel / Sentinel | 1.79x | 1.40x | 1.13x | 1.13x | 1.01x |

Above 1 = one-shot is faster. For LL, one-shot wins below about 4 KiB and two-shot above: two-shot
needs two steps (slow for tiny messages) but moves less data (helps for big ones). For Sentinel,
two-shot never wins in this range: checking and resetting every float on the CPU costs it too much.

---

## 2. Small sizes: 32 B, 64 B, 128 B (re-run 2026-11-01)

This section was measured later than the rest of this file: on 2026-11-01, with the same version-2
code (commit `421f67d`), on the idle machines compute02-04, right after version 3 in the same job.
So, unlike section 1, it has the median and comes from machines without leftover load.

Log: `logs/2026-11-01_small_sizes_32_64_128B_raw.txt`. PBS job 30202, 2026-11-01,
compute02-04, 1000 rounds. 32 B = 8 floats, 64 B = 16 floats, 128 B = 32 floats (two-shot pads to 9 / 18 / 33).

**Median**, average of the ranks:

| Algorithm | 32 B | 64 B | 128 B |
|---|---|---|---|
| Baseline (with barrier) | 8.79 µs | 8.82 µs | 8.96 µs |
| LL (one-shot) | 2.15 µs | 2.16 µs | 2.25 µs |
| Sentinel (one-shot) | 1.85 µs | 1.86 µs | 1.92 µs |
| Two-shot LL | 4.23 µs | 4.24 µs | 4.29 µs |
| Two-shot Sentinel | 3.49 µs | 3.55 µs | 3.69 µs |
| Bidirectional (2 ranks, 8 chunks per round) | 13.68 µs | 13.75 µs | 13.72 µs |

**p99**, average of the ranks:

| Algorithm | 32 B | 64 B | 128 B |
|---|---|---|---|
| Baseline (with barrier) | 9.81 µs | 9.69 µs | 9.62 µs |
| LL (one-shot) | 2.33 µs | 2.52 µs | 2.49 µs |
| Sentinel (one-shot) | 2.00 µs | 2.02 µs | 2.24 µs |
| Two-shot LL | 5.53 µs | 5.73 µs | 5.53 µs |
| Two-shot Sentinel | 4.70 µs | 4.63 µs | 5.39 µs |
| Bidirectional (2 ranks, 8 chunks per round) | 15.12 µs | 15.39 µs | 15.42 µs |

**Mean**, average of the ranks (the statistic the 2026-10-05 tables used):

| Algorithm | 32 B | 64 B | 128 B |
|---|---|---|---|
| Baseline (with barrier) | 8.06 µs | 8.07 µs | 8.18 µs |
| LL (one-shot) | 2.18 µs | 2.20 µs | 2.27 µs |
| Sentinel (one-shot) | 1.88 µs | 1.89 µs | 1.96 µs |
| Two-shot LL | 4.27 µs | 4.29 µs | 4.33 µs |
| Two-shot Sentinel | 3.53 µs | 3.59 µs | 3.74 µs |
| Bidirectional (2 ranks, 8 chunks per round) | 13.76 µs | 13.86 µs | 13.80 µs |

| | 32 B | 64 B | 128 B |
|---|---|---|---|
| Barrier cost (Baseline - LL, median) | 6.64 µs | 6.66 µs | 6.70 µs |
| Baseline / LL | 4.10x | 4.08x | 3.98x |

Correctness: 51 PASSED, 0 FAILED lines. Exit codes: 51 of 51 processes exited with 0.

Compared with section 1 at 128 B (mean, 2026-10-05, compute00-02): Baseline 8.20 -> 8.18 µs,
LL 2.36 -> 2.27 µs, Sentinel 2.10 -> 1.96 µs, Two-shot LL 4.48 -> 4.33 µs, Two-shot Sentinel
3.76 -> 3.74 µs. Same code, so the small differences come from the machines and the day.

Version 3 at the same sizes, from the same job: `../results_v3/RESULTS_v3.md`, section 4.

---

## 3. How version 2 got there (the steps of 2026-10-05)

### 3.1 Step 1: Sentinel fix (256 KiB, 100 rounds)

File: `2026-10-05_sentinel_fix_summary.txt` (raw output: `2026-10-05_sentinel_fix_raw.txt`).

The first code never put the sentinel value back after reading it, so from the second round on the
receiver did not wait for new data. Fixed: each value is reset to the sentinel after it is read; a
barrier before the timed loop; each round timed separately.

| Algorithm | Before (mean) | After (mean) | Change |
|---|---|---|---|
| Sentinel | 57,800 µs | 542.2 µs | the old number was not a real exchange |
| Two-shot Sentinel | 610 µs | 471.6 µs | 1.29x lower |

After the fix, per rank:

| Algorithm | Rank | Mean | Min | Median | p99 | Max |
|---|---|---|---|---|---|---|
| Sentinel | 0 | 540.7 µs | 475.9 µs | 542.5 µs | 611.4 µs | 617.0 µs |
| Sentinel | 1 | 542.5 µs | 475.6 µs | 538.8 µs | 616.9 µs | 680.0 µs |
| Sentinel | 2 | 543.5 µs | 489.5 µs | 536.0 µs | 622.3 µs | 764.5 µs |
| Two-shot Sentinel | 0 | 469.7 µs | 422.0 µs | 461.5 µs | 546.2 µs | 550.1 µs |
| Two-shot Sentinel | 1 | 472.6 µs | 421.9 µs | 465.0 µs | 555.7 µs | 778.8 µs |
| Two-shot Sentinel | 2 | 472.6 µs | 420.5 µs | 462.9 µs | 548.1 µs | 778.8 µs |

256 KiB is a large message, so these times are mostly moving data and the CPU sum, not waiting.
Next step was therefore to measure small sizes.

### 3.2 Step 2: faster writes (Sentinel and Two-shot Sentinel, 128 B to 64 KiB)

Files: `2026-10-05_small_size_sweep.txt` (before) and `2026-10-05_small_size_sweep_v2_postwrite.txt` (after).

Change: before, each RDMA write was posted and then waited for (a full network round trip) before
the next one. After: all writes are posted back to back and their completions collected later, and
writes up to 128 B are sent inline.

| Size | Sentinel before | Sentinel after | Faster by | Two-shot Sentinel before | after | Faster by |
|---|---|---|---|---|---|---|
| 128 B | 4.28 µs | 2.08 µs | 2.06x | 8.21 µs | 3.87 µs | 2.12x |
| 1 KiB | 7.00 µs | 4.07 µs | 1.72x | 10.01 µs | 5.58 µs | 1.79x |
| 4 KiB | 12.70 µs | 10.26 µs | 1.24x | 14.49 µs | 11.55 µs | 1.25x |
| 16 KiB | 43.52 µs | 29.77 µs | 1.46x | 44.07 µs | 32.54 µs | 1.35x |
| 64 KiB | 139.7 µs | 85.06 µs | 1.64x | 118.8 µs | 93.47 µs | 1.27x |

At 128 B, one-shot Sentinel dropped by about 2.2 µs, which is one network round trip: the wait that
was removed.

After the change, rank 0:

| Size | Sentinel median / p99 / max | Two-shot Sentinel median / p99 / max |
|---|---|---|
| 128 B | 2.02 / 2.20 / 11.43 µs | 3.74 / 8.00 / 15.86 µs |
| 1 KiB | 3.96 / 7.70 / 19.60 µs | 5.45 / 9.13 / 16.77 µs |
| 4 KiB | 10.06 / 14.64 / 22.34 µs | 11.46 / 14.33 / 23.31 µs |
| 16 KiB | 29.28 / 37.20 / 88.39 µs | 32.11 / 37.81 / 45.43 µs |
| 64 KiB | 82.78 / 107.4 / 114.9 µs | 91.17 / 115.9 / 137.6 µs |

### 3.3 Step 3: the same fixes for every algorithm

The same faster writes, plus the bug fixes in section 4, were applied to LL, Two-shot LL, LL128 and
Baseline. The result is the table in section 1.

---

## 4. Findings

File: `2026-10-05_all_algorithms_partial_and_findings.txt`.

**LL128 Atomic cannot run on this cluster.** `Atomic_Probe` showed the card reports
`atomic_cap = 0 (NONE)`, and all four kinds of atomic operation were rejected with error 22
(`EINVAL`), although the connection was ready and allowed remote atomics. It is not a code bug.
LL128 now prints `SKIPPED` and exits with code 2 instead of crashing. Also fixed: it waited for the
wrong counter value (`(it+2)*(n-1)` instead of `it+2`), which would hang at 3 ranks anyway.

**A one-off 2.15 s stall in Baseline (1 KiB).** One round took 2,151,641 µs on all ranks, which
raised the mean to 12,521 µs while the median stayed 11 µs. Likely cause: the barrier put its
receive back only after finishing, so a peer's next message could arrive with no receive ready and
had to wait for a retry timer. Fixed by always keeping two receives ready. Not seen again afterwards
(max at most 55 µs over 3 sizes x 1000 rounds), which fits the fix but does not prove it.
Baseline at 128 B was 9.36 µs before this fix and 8.21 µs after.

**Bugs fixed in LL, Two-shot LL, LL128 and Baseline:**
- one receive buffer was reused every round, so a fast peer could overwrite data still being read.
  Hidden because every round used the same input. Now two buffers, used in turn;
- flag waits used `!=`, which could spin forever if a peer was already one round ahead. Now `<`.

---

## 5. Limits of these results (why version 3 was needed)

These apply to the 2026-10-05 results (sections 1, 3, 4). Section 2 was measured later, on idle
machines and with the median, so the first three points do not apply to it.

- **Mean, not median:** one slow round raises the mean (as the 2.15 s stall did).
- **Sizes 128 B to 256 KiB**, not the professor's 32 B / 32 KB / 32 MB, and the first numbers sent
  did not say their size.
- **Loaded machines:** compute00 and compute01 had leftover processes at 100% CPU (found on
  2026-10-28).
- **No network floor** to compare with.
- **Correctness was checked only in the first round**; a later round reading old data would pass.
- **Wrong card name:** the files say ConnectX-3 (`mlx4_0`); the card is `mlx5_0` (vendor part 4113).

Version 3 (`../results_v3/RESULTS_v3.md`) addresses all of these.

---

## 6. Where each number comes from

All files are in `logs/`, unchanged. The full PBS outputs they were made from are on the cluster in
`~/.pbs_route/output/`.

| File in `logs/` | What it is | Used in | Cluster output |
|---|---|---|---|
| `2026-10-05_sentinel_fix_raw.txt` | raw output, Sentinel and Two-shot Sentinel at 256 KiB after the fix | 3.1 | `student4_20261005_073728_117238717.out` |
| `2026-10-05_sentinel_fix_summary.txt` | summary of the fix, before and after | 3.1 | (same) |
| `2026-10-05_small_size_sweep.txt` | Sentinel / Two-shot Sentinel, 128 B to 64 KiB, before the faster writes | 3.2 | `student4_20261005_074644_491099120.out` |
| `2026-10-05_small_size_sweep_v2_postwrite.txt` | the same, after the faster writes | 3.2 | `student4_20261005_075334_971720342.out` |
| `2026-10-05_all_algorithms_partial_and_findings.txt` | first all-algorithm run (partly captured), atomics probe, barrier stall | 4 | `..._081934_993950100.out`, `..._082403_033786095.out`, `..._082649_072222504.out` |
| `2026-10-05_all_algorithms_size_sweep.txt` | all algorithms, 128 B to 64 KiB, final version-2 code | 1 | `student4_20261005_083006_635475435.out` |
| `2026-11-01_small_sizes_32_64_128B_raw.txt` | raw output, all algorithms at 32 B / 64 B / 128 B, version-2 part of job 30202 (compute02-04) | 2 | `student4_20261101_162138_144163203.out` |

Note: the 2026-10-05 files still say "v1" / "v2" for the two write methods, and "ConnectX-3" for
the card; they are kept as they were.
