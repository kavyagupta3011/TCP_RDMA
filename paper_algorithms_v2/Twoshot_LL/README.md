# Two-Shot LL AllReduce -- RDMA verbs version

Paper reference: Table I, row "Two-shot (LL)". See `../README.md` first,
especially "How the mesh bootstrap works" and the "two-shot's traffic
reduction is now visible" honesty note.

## The idea, technically

General N: the M-element vector splits into N chunks, one per rank --
rank `c` owns chunk `c`.

- **Phase 1 (ReduceScatter):** every rank `r` sends, to EVERY other rank
  `c`, its own local slice of chunk `c`. Once rank `c` has received chunk
  `c`'s slice from all N-1 other ranks, it sums those with its OWN slice,
  ending up as the sole holder of chunk `c`, fully reduced. Signaled with
  the same LL-style ordered write-then-flag trick as `../LL/`, once per
  peer.
- **Phase 2 (AllGather):** every rank broadcasts its now-complete chunk to
  all N-1 other ranks, using the same trick again, so every rank ends up
  with the full result.

Two phases, two ordered write-then-flag pairs per peer -- matching Table
I's "2 synchronizations" for this row, with zero explicit barrier calls
anywhere. At N=2 this specializes to exactly the simple "two people, two
halves" picture below; at N=3 or N=4, "chunk" literally means "a third" or
"a quarter" of the vector, and total network traffic is correspondingly
lower than the one-shot folders' (`../LL/`, `../Sentinel/`) full-vector
push to every peer.

## The idea, simply

Each person on a team of N is responsible for finishing a different slice
of a shared report. Everyone mails everyone else their notes on that other
person's slice. Once you've combined your own notes with what everyone
else sent you, your slice is fully done -- so you mail your finished slice
to everyone else, and they do the same, and now everyone has the complete
report.

## Build & run

```
make
./twoshot_ll_allreduce_rdma <my_rank> <num_ranks> <base_port> <ip0> ... <ip(N-1)> [numFloats] [iters]
```

No MPI -- launch once per rank by hand. See `../README.md` for a full
worked 3-rank example and how the mesh bootstrap works.
