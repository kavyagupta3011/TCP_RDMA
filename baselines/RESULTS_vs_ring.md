# Standard ring AllReduce against the paper algorithms (3 ranks, compute02 to compute04)

Median time per AllReduce, averaged over the 3 ranks.
Ring: job 30205 (baselines/Ring_AllReduce). Others: the comparison jobs of 2026-11-01
(Baseline_Barrier from results_updated/v3). Different jobs, same three nodes.

| Size | Ring | Baseline_Barrier | LL | Sentinel | Two-shot LL v3 | Two-shot LL v6 | Two-shot Sentinel v6 |
|---|---|---|---|---|---|---|---|
| 32 B | 7.82 us | 8.80 us | 2.16 us | 1.86 us | 4.26 us | 3.84 us | 3.17 us |
| 32 KB | 41.38 us | 44.38 us | 33.80 us | 42.92 us | 34.44 us | 23.78 us | 31.18 us |
| 256 KB | 199.4 us | 238.9 us | 229.8 us | 285.9 us | 229.3 us | 120.6 us | 175.0 us |
| 32 MB | 29.38 ms | 36.42 ms | 36.45 ms | 38.46 ms | 31.60 ms | 11.35 ms | 25.06 ms |

Ring speed-up of the best paper algorithm per size: 32 B 4.2x (Sentinel), 32 KB 1.74x
(Two-shot LL v6), 256 KB 1.65x (Two-shot LL v6), 32 MB 2.59x (Two-shot LL v6).

Note: the ring is a plain one, one message per step, no pipelining inside a step. A tuned
ring that splits its chunks and overlaps the steps would get closer to the wire speed
(about 11 ms at 32 MB), so the 32 MB gap is against this simple ring only.
