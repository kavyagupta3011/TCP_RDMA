# paper_algorithms_v4

Copy of `paper_algorithms_v3` with a few changes to the adding step of three programs. Nothing
else is different, and `paper_algorithms_v3` itself is untouched. The result of every program is
identical to v3 (the additions are done in the same order).

## What changed

| Program | Change |
|---|---|
| `Twoshot_LL` | After the flags arrive, the chunk is summed one block (4096 floats) at a time, and each finished block is copied into the per-peer send buffers right away while it is still in cache. v3 wrote all the copies one element at a time inside the sum loop. New helper `reduce_blocked_copy` in `common/`. |
| `Twoshot_Sentinel` | Same idea, with the sentinel wait: `reduce_sentinel_blocked` in `common/`. Each received value is also read once instead of twice. The phase-2 loops are not changed. |
| `Sentinel` | Makefile only: `-O2` to `-O3`. The sum loop is the v3 one. I tried a blocked version of it first; on this machine it was about 25% slower than the v3 loop (the per-element sentinel test cannot be vectorised, and the blocked version adds extra stores), so it was dropped. |
| `Twoshot_LL`, `Twoshot_Sentinel` | Makefile: `-O2` to `-O3`. |

`Baseline_Barrier`, `LL`, `LL128_Atomic`, `Bidirectional_DoubleBuffering`, `Atomic_Probe` and
`LL_NoWait_NegativeTest` are identical to v3.

## What to expect

These only touch the local adding step, not the network, so they matter where that step is a
noticeable part of the time (32 KB and larger). At 32 B to 128 B nothing should move. A local
test of the adding step alone (no network) gave roughly: Twoshot_LL, 8192 floats, 7.7 us to
2.8 us for sum plus copies; Twoshot_Sentinel about 5 to 10% faster; Sentinel unchanged. The
real effect on the full AllReduce has to be measured on the cluster, which is what the job below
does.

## Build and run (on the cluster)

```
cd ~/IMT2023016/paper_algorithms_v4
bash build.sh                      # builds v3 and v4, makes ~/IMT2023016/results_v4
cd jobs && qsub compare_v3_v4.pbs
# when the job is finished:
python3 compare.py
```

The job runs `LL`, `Sentinel`, `Twoshot_LL` and `Twoshot_Sentinel` at 32 B, 32 KB, 256 KB and
32 MB on compute02 to compute04, for both versions, and each experiment twice (v3 first, then v4
first). `LL` is a control: its code is the same in both versions, so whatever difference shows
there is noise.

## Checks done before it was run on the cluster

* compiles with `-Wall -Wextra`, no warnings
* the new helpers give bit-identical output to the v3 loops for 1, 2 and 4 sources and for 1, 3,
  4095, 4096, 4097, 10000 and 100000 floats, including the copies and the sentinel re-arm
* the sentinel helper was also tested with writer threads filling the buffer late
