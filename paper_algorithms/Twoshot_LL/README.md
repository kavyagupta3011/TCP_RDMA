# Two-Shot LL AllReduce

Paper reference: Table I, row "Two-shot (LL)" (page 5), built from the
mechanisms in Section IV.B.1 and III-A2.

## The idea, technically

`LL/` reduces everyone's full vector against everyone else -- every GPU
sends its complete data to every other GPU (one-shot). That's simple, but
total network traffic grows with `N`: `2(N-1)M` bytes-equivalent per GPU
per Table I.

Two-shot restructures the *same* AllReduce as ReduceScatter followed by
AllGather:

- **ReduceScatter**: split the M-element result into N partitions, one
  "owned" by each GPU. Every GPU sends every OTHER GPU only the slice of
  its own data that belongs to that GPU's partition (size `M/N`, not `M`).
  Once an owner has received the `N-1` slices meant for it and added its
  own local slice, it alone holds the fully-reduced value for its
  partition.
- **AllGather**: each owner broadcasts its now-complete partition out to
  everyone else, so every GPU ends up holding the full `M`-element result.

Total traffic per GPU drops to `O(M)`, independent of `N` -- the price is
two rounds of synchronization instead of one. This file makes **both**
rounds barrier-free using exactly `LL/`'s trick: every send is a single
aligned 16-byte store packing 2 floats + a duplicated epoch flag; every
receive is a busy-poll on that same line. No separate barrier call appears
anywhere in `runOnce()`.

## The idea, simply

`LL/` is like everyone in a group chat sending their full update to every
other member individually. Two-shot LL is more like: each topic gets one
assigned "notetaker." Everyone sends their bit on that topic only to that
topic's notetaker (much less total traffic than messaging everyone about
everything). Once a notetaker has heard from everybody on their topic, they
send the finished summary back out to the whole group. Two rounds of
messages instead of one, but far less total chatter.

## How this maps to the code (`twoshot_ll_allreduce.cu`)

- `rsSendKernel` / `rsReduceKernel`: Phase 1 (ReduceScatter). Grid is
  organized as N "target groups" (`blockIdx.x / blocksPerChunk` selects
  which partition-owner a block is contributing to), reusing `LL/`'s packed
  line + busy-poll idiom, just scoped to one `M/N`-sized slice per
  destination instead of the whole vector.
- `agSendKernel` / `agFinalizeKernel`: Phase 2 (AllGather). Each owner
  pushes its completed partition out, LL-flagged, to every peer's output.

A quick sanity check against Table I's `4(N-1)M/N` comm-volume figure is
in the file header -- it comes out exactly right by construction, since
both phases reuse the identical LL packing.

## Build & run

```
make
./twoshot_ll_allreduce [numGPUs] [numFloats] [iters]
./twoshot_ll_allreduce 4 65536 100
```

Compare its latency at a given `numFloats` against `LL/`'s one-shot number
at the same size: at small sizes one-shot LL should look competitive or
even better (only 1 sync, and the `M/N` split doesn't save much when `M`
is already tiny); as `numFloats` grows, two-shot should start to pull
ahead, matching Table I's `O(M)` vs `O(N*M)` volume story.
