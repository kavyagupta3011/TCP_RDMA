# Baseline: Explicit-Barrier AllReduce -- RDMA verbs version (not one of the paper's 5 techniques)

The control group, same role as the CUDA version's `Baseline_Barrier/`:
not a Table I row, just the "before" number the other six folders'
"after" numbers get compared against.

## The idea, technically

Both ranks write their full input directly into the peer's scratch buffer
-- plain RDMA WRITE, no flag, no polling for arrival. Correctness depends
entirely on `rdma_barrier()`: a genuine two-sided SEND+RECV round trip,
called right after the write. Only once that round trip completes on both
sides is either side allowed to read its scratch buffer.

This is arguably an even more honest "conventional" baseline than the
CUDA version's host-side stream-sync: two separate processes on two
separate machines genuinely do need a real network round trip to agree
"we're both ready" -- there's no single process here that could sequence
both sides' work internally the way one host process could sequence
multiple GPUs it directly controlled.

## The idea, simply

Two people pass notes, but this time person A has to explicitly shout
"ready!" and wait to hear person B shout "ready!" back before either one
is allowed to read what's in front of them -- even if the note actually
landed moments ago. All six other folders in this project remove that
shout-and-wait step; this one keeps it, on purpose, so you can measure
what it costs on your actual hardware.

## N-node generalization

Every rank writes its full input to every peer's per-peer scratch buffer,
then calls the genuine two-sided `rdma_barrier()` round trip on EVERY peer
connection, one at a time, before trusting any of them -- N-1 sequential
round trips per round, deliberately not parallelized, since this file's
whole purpose is to be the honest "before" number.

## Build & run

```
make
./baseline_barrier_rdma <my_rank> <num_ranks> <base_port> <ip0> ... <ip(N-1)> [numFloats] [iters]
```

No MPI -- launch once per rank by hand. See `../README.md` for a full
worked 3-rank example and how the mesh bootstrap works.

Run this with the same `numFloats` and rank count as any of the other six
folders and diff the "Average ... latency" lines -- that's your empirical
version of the paper's core speedup claim, on real InfiniBand hardware.
