# Two-shot results across versions (3 ranks, compute02 to compute04)

v1 to v3: mean time, from results_updated (v1 prints only the mean).
v4 to v6: median time, from the comparison jobs of 2026-11-01 (results_v4, results_v5, results_v6).
Different jobs, so small differences between v3 and v4 may include a little noise; the unchanged
LL control stayed within 2% across all of those jobs.

## Two-shot LL

| Size | v1 | v2 | v3 | v4 | v5 | v6 |
|---|---|---|---|---|---|---|
| 32 B | 15.94 us | 4.33 us | 4.30 us | 4.24 us | 4.25 us | 3.84 us |
| 32 KB | 51.60 us | 35.11 us | 34.67 us | 26.37 us | 24.57 us | 23.78 us |
| 256 KB | 272.1 us | 235.0 us | 235.9 us | 167.5 us | 149.7 us | 120.6 us |
| 32 MB | 31.80 ms | 31.76 ms | 31.65 ms | 25.84 ms | 22.78 ms | 11.35 ms |

## Two-shot Sentinel

| Size | v1 | v2 | v3 | v4 | v5 | v6 |
|---|---|---|---|---|---|---|
| 32 B | 8.01 us | 3.58 us | 3.59 us | 3.49 us | 3.51 us | 3.17 us |
| 32 KB | 49.30 us (2 of 3 ranks) | 45.18 us | 36.93 us | 32.85 us | 31.65 us | 31.18 us |
| 256 KB | 312.7 us | 285.6 us | 231.6 us | 202.5 us | 186.8 us | 175.0 us |
| 32 MB | 34.92 ms (1 of 3 ranks) | 38.24 ms | 32.89 ms | 28.63 ms | 24.66 ms | 25.06 ms |

Network floor at 32 MB: each rank sends about 43 MB, at the measured 3.79 GB/s that is
about 11.3 ms. Two-shot LL in v6 takes 11.35 ms.
