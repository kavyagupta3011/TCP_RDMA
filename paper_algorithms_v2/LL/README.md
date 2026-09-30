# LL (Low-Latency) One-Shot AllReduce -- RDMA verbs version

Paper reference: Section IV.B.1. See `../README.md` first for the shared
"ordering instead of atomicity" design idea this file builds on.

## The idea, technically

Two RDMA WRITEs, posted back to back on the same queue pair: first the
full data vector, into the peer's `data_recv` region; then a single 8-byte
**epoch** value, into the peer's `flag_recv` region. RC ordering guarantees
the data has landed by the time the epoch write does. The receiver busy-
polls only that one local 8-byte word for the current epoch -- once it
matches, the data is guaranteed present, no separate barrier anywhere.
Because the epoch keeps incrementing every round rather than resetting to
a fixed value, this needs no reset step between repeated calls, exactly
like the CUDA version's self-clearing epoch flag.

## The idea, simply

You slide a full letter under a door, then immediately slide a small
colored card under right after it. The person on the other side just
watches for the card's color to change -- by the time they see the new
color, the letter is definitely already on the floor too, because you sent
it first through the same slot.

## N-node generalization

With N ranks this becomes a full-mesh push: every rank does the
WRITE-then-epoch trick above to EVERY other rank (N-1 times), then sums its
own input with all N-1 received vectors. Same per-peer mechanism, just
looped over the whole mesh instead of one fixed peer -- see `../README.md`
for how that mesh gets set up with no MPI involved.

## Build & run

```
make
./ll_allreduce_rdma <my_rank> <num_ranks> <base_port> <ip0> ... <ip(N-1)> [numFloats] [iters]
```

No MPI -- launch once per rank by hand. See `../README.md` for a full
worked 3-rank example and how the mesh bootstrap works.
