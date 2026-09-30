# Two-Shot Sentinel AllReduce -- RDMA verbs version

Paper reference: Table I, row "Two-shot (Sentinel)". Combines
`Twoshot_LL/`'s two-phase structure with `Sentinel/`'s
double-buffering-across-iterations trick -- read both of those READMEs
first; this one doesn't repeat their reasoning.

## The idea, technically

Same ReduceScatter-then-AllGather split as `Twoshot_LL/` (two halves, one
per rank), but each phase's arrival detection is Sentinel-style (real
data written directly, receiver polls for "no longer the reserved
pattern") instead of LL-style. Each phase gets its OWN pair of
sentinel-initialized scratch buffers, reset once at startup and
ping-ponged by round parity -- exactly `Sentinel/`'s fix for avoiding a
per-round reset barrier, just applied independently to both phases here.

## The idea, simply

Same "two people each own half a report" picture as `Twoshot_LL/`, except
instead of a colored card confirming arrival, each person just watches
their inbox slot for the "nothing here yet" placeholder to be replaced by
real notes -- with two alternating inbox slots per phase so nobody ever
has to wait for a placeholder to be reset before the next round.

## N-node generalization

General N works exactly like `../Twoshot_LL/`'s (rank `c` owns chunk `c`,
ReduceScatter then AllGather across all N-1 peers) -- just with each
(phase, peer) pair's arrival detection done Sentinel-style (its own
double-buffered pair of scratch regions) instead of LL-style. See
`../README.md`'s "two-shot's traffic reduction is now visible" note.

## Build & run

```
make
./twoshot_sentinel_allreduce_rdma <my_rank> <num_ranks> <base_port> <ip0> ... <ip(N-1)> [numFloats] [iters]
```

No MPI -- launch once per rank by hand. See `../README.md` for a full
worked 3-rank example and how the mesh bootstrap works.
