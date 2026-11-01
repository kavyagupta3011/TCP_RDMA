# paper_algorithms_v5

Copy of `paper_algorithms_v4` where `Twoshot_LL` and `Twoshot_Sentinel` no longer keep
per-peer copies of their data. `paper_algorithms_v3` and `_v4` are not changed. Results are
identical to v3 and v4.

## Why the copies existed

Every connection to a peer had its own protection domain (PD). A buffer registered on one PD
cannot be used for a send on a connection that belongs to another PD, so for each peer the
Twoshot programs registered and kept their own copy of the slice to send in phase 1 and of the
finished chunk to send in phase 2. v4 made filling those copies cheaper; v5 removes them.

## What changed

* `common/rdma_common.{c,h}`: `rdma_use_shared_pd()` makes all connections of the process share
  one PD (switched on only by the two programs below, so every other program behaves as in
  v4). `rdma_pd_is_shared()` checks that it worked. `rdma_reg_existing()` registers a buffer
  the program already owns.
* `Twoshot_LL`, `Twoshot_Sentinel`:
  * `input` and `output` are registered once. Phase 1 sends `input + j*chunk` straight to peer
    j. The reduced chunk is written into `output` and phase 2 sends it from there.
  * The `phase1_src` and `reduced` copies, the copy step after the sum, and the per-round
    refresh of the copies are gone. `Twoshot_LL` also uses one epoch word instead of one per
    peer.
  * `Twoshot_LL` sums with `reduce_blocked` (no copies are needed any more), `Twoshot_Sentinel`
    with `reduce_sentinel_blocked` and no copies.
* Everything else is identical to v4.

## Not done in this version

The receive side still copies: the chunk a peer sends in phase 2 lands in `ag_recv` or `ag_buf`
and is then copied into `output`. Writing straight into `output` would remove that too, but it
changes when `output` gets overwritten, so it is left for a later version.

## Build and run (on the cluster)

```
cd ~/IMT2023016/paper_algorithms_v5
bash build.sh
cd jobs && qsub compare_v3_v4_v5.pbs
# when it has finished:
python3 compare.py
```

`build.sh` only builds v5 (v3 and v4 are already built from the earlier comparison). The job
runs `LL` (control), `Twoshot_LL` and `Twoshot_Sentinel` at 32 B, 32 KB, 256 KB and 32 MB for
v3, v4 and v5, each experiment twice in opposite order.

## Checks done

Compiles with `-Wall -Wextra` without warnings. The shared protection domain can only be
tested on the real InfiniBand cluster (there is no RDMA device where the code was written), so
look at the "Correctness check ... PASSED" lines in the logs before reading the timings.
