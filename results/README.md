# Results

One folder per version of the code. In each: one `RESULTS_*.md` with all the tables and their
explanation, and a `logs/` folder with the original output files, unchanged.

| Folder | Code | Measured | Machines | Sizes | Read first |
|---|---|---|---|---|---|
| `results_v2/` | `paper_algorithms_v2/` | 2026-10-05, and 2026-11-01 | compute00-02 (loaded); 2026-11-01 run on compute02-04 | 32 B, 64 B, 128 B to 256 KiB | `results_v2/RESULTS_v2.md` |
| `results_v3/` | `paper_algorithms_v3/` | 2026-10-28, and 2026-11-01 | compute02-04 (idle) | 32 B, 64 B, 128 B, 32 KB, 32 MB, and the network floor | `results_v3/RESULTS_v3.md` |

```
results/
  README.md                this file
  small_sizes_table.py     tables for the 32 B / 64 B / 128 B runs (used for both versions)
  results_v2/
    RESULTS_v2.md          all version-2 tables
    logs/                  six files of 2026-10-05, and the version-2 part of job 30202
  results_v3/
    RESULTS_v3.md          all version-3 tables (made by make_results_v3.py)
    make_results_v3.py     reads logs/ and writes RESULTS_v3.md
    logs/                  five raw job outputs of 2026-10-28, and the version-3 part of job 30202
```

## Main numbers at small sizes

From PBS job 30202 (2026-11-01): both versions one after the other, same machines (compute02-04),
1000 rounds, median averaged over the 3 ranks. Job script: `paper_algorithms_v3/jobs/small_sizes_v2_v3.pbs`.

| | 32 B v2 | 32 B v3 | 64 B v2 | 64 B v3 | 128 B v2 | 128 B v3 |
|---|---|---|---|---|---|---|
| Baseline (with barrier) | 8.79 µs | 8.45 µs | 8.82 µs | 8.64 µs | 8.96 µs | 8.65 µs |
| LL (one-shot) | 2.15 µs | 2.19 µs | 2.16 µs | 2.18 µs | 2.25 µs | 2.24 µs |
| Sentinel (one-shot) | 1.85 µs | 1.88 µs | 1.86 µs | 1.87 µs | 1.92 µs | 1.93 µs |
| Two-shot LL | 4.23 µs | 4.28 µs | 4.24 µs | 4.28 µs | 4.29 µs | 4.25 µs |
| Two-shot Sentinel | 3.49 µs | 3.52 µs | 3.55 µs | 3.57 µs | 3.69 µs | 3.67 µs |

At these sizes the versions are within a few percent of each other: what version 3 changed (faster
adding step, per-round correctness check) matters only for larger messages (`RESULTS_v3.md`, section 6).
The network floor is 1.07 µs one way for 32 B, so a round trip is about 2.1 µs.

## Rebuilding the tables

After adding or replacing a version-3 log (same file names):

```
cd results_v3
python make_results_v3.py logs RESULTS_v3.md
```

For version 2, `RESULTS_v2.md` is edited by hand; its section 2 was made with
`python small_sizes_table.py results_v2/logs/2026-11-01_small_sizes_32_64_128B_raw.txt`.
