# paper_algorithms_v6

Copy of `paper_algorithms_v5` where `Twoshot_LL` and `Twoshot_Sentinel` overlap the two phases
of the AllReduce. `paper_algorithms_v3`, `_v4` and `_v5` are not changed. The sums are done in
the same order as before, so the results are identical.

## What was slow in v5

Phase 2 (sending the finished chunk to everybody) started only after the whole chunk had
arrived and been summed, and `Twoshot_LL` then copied each received chunk into `output`. At
32 MB v5 takes about 22.8 ms for roughly 11 ms worth of data on the wire, so a good part of the
time is spent waiting in turn instead of overlapping.

## What changed

* A chunk is cut into blocks of at least 16384 floats (64 KB), at most 32 per chunk
  (`MIN_BLOCK`, `MAX_BLOCKS` at the top of the two programs). A chunk smaller than one block,
  which covers every size up to about 190 KB, is a single block, so there is nothing to
  overlap there. Those sizes still get a small gain (about 0.4 us at 32 B, measured) because the
  program no longer waits for the phase 1 write completion before starting phase 2 (both
  completions are collected at the end of the round), and `Twoshot_LL` no longer copies on
  arrival.
* `Twoshot_LL`: every block has its own flag. Phase 1 posts all blocks to all peers at once.
  When block b has arrived from every peer it is summed and sent on immediately (phase 2),
  while later blocks are still arriving. Phase 2 writes go straight into the peer's `output`
  vector, so the copy on arrival is gone; the round ends when every phase 2 flag has been seen.
  Only the last flag write of each phase is signaled, because RC completes in order.
* `Twoshot_Sentinel`: the finished chunk is sent block by block while the rest is still being
  summed. The receive side is unchanged (sentinel values cannot be written into `output`
  directly).
* `common`: send and completion queues are deeper (256 instead of 32), because a round now
  posts up to 4 work requests per block and peer. This applies to all programs but changes
  nothing for them.

## One thing to know about writing into `output` directly

A peer's phase 2 data is written into this rank's `output` while this rank is still inside the
call. That is safe here because a peer can only start phase 2 of the next round after it has got
this rank's phase 1 data of that round, which this rank sends after finishing the previous
one. The result of a round stays valid until the next call starts, as with any AllReduce
that writes into its output buffer.

## Build and run (on the cluster)

```
cd ~/IMT2023016/paper_algorithms_v6
bash build.sh
cd jobs
qsub quick_v6.pbs            # short test first, read ~/IMT2023016/results_v6/quick.txt
qsub compare_v3_v5_v6.pbs    # full comparison, then: python3 compare.py
```

The full job runs `LL` (control), `Twoshot_LL` and `Twoshot_Sentinel` at 32 B, 32 KB, 256 KB,
1 MB and 32 MB for v3, v5 and v6, each experiment twice in opposite order.

## Checks done

Compiles with `-Wall -Wextra` without warnings. It could not be run where it was written (no
RDMA device), so the quick job is the first real test. Check the "Correctness check ...
PASSED" lines before reading any timing.
