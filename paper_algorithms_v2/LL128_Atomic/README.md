# Two-Shot LL128 Atomic AllReduce -- RDMA verbs version

Paper reference: Section IV.B.4, Fig. 5. **Read `../README.md`'s "LL128
Atomic's hardware gap" note before anything else in this file** -- it's
the single most important thing to understand about this specific folder
before presenting it.

## The idea, technically

Same two-phase (ReduceScatter + AllGather) structure as `Twoshot_LL/`, but
the completion signal for each phase is a REAL InfiniBand hardware atomic
(`IBV_WR_ATOMIC_FETCH_AND_ADD`, a genuine 64-bit atomic increment on the
peer's memory) instead of a separate flag WRITE. Posted right after the
data WRITE on the same queue pair, RC ordering guarantees the data has
landed by the time the atomic's effect is visible -- the same ordering
discipline as `LL/`, just with a hardware atomic standing in for the
trailing flag write.

Because the counter is advanced by a genuine atomic add rather than
overwritten by a plain WRITE, it's naturally monotonic across the whole
program's lifetime -- like LL's epoch, it never needs resetting, and
(unlike `Sentinel/`/`Twoshot_Sentinel/`) this file needs no double
buffering anywhere. Round `i` (0-indexed) is complete once the counter
reads exactly `i+1`.

**What this version can and can't demonstrate:** the paper's original
LL128 Atomic gets two things from one NVLink primitive -- floating-point
data summed directly by the fabric, AND a completion counter riding along
in that same operation. InfiniBand's atomics are 64-bit integer only; there
is no remote float atomic add. So this file keeps the completion-signal
half faithfully (a real hardware atomic, genuinely used as an implicit
"everyone's contributed" signal, with zero explicit flag writes) but sums
the actual float data locally, in a per-sender slot, once the atomic
counter says it's ready -- the same data path as `Twoshot_LL/`/
`Twoshot_Sentinel/`. With the N-node generalization, this now really does
demonstrate "counter reaches N-1" faithfully: every one of my N-1 peers
issues its own atomic `+1` to my counter each round/phase, and my counter's
target value is `(round+1)*(N-1)` (see the file's own header comment) --
at N=3 or N=4 there's a genuinely richer story here than at N=2, where
there was only ever one remote contributor. Expect this file's latency to
look similar to `Twoshot_LL/`'s on this hardware, not faster -- its point
is demonstrating the CORRECT use of a real hardware atomic as a completion
signal accumulated from multiple peers, not a performance win specific to
this technique.

## The idea, simply

Instead of someone shouting "got it!" after receiving something (a
separate signal), imagine a tally counter nailed to the wall that
physically clicks forward by itself the instant a delivery arrives -- you
don't need anyone to announce anything, you just glance at the counter.
The counter click IS the announcement.

## Build & run

```
make
./ll128_atomic_allreduce_rdma <my_rank> <num_ranks> <base_port> <ip0> ... <ip(N-1)> [numFloats] [iters]
```

No MPI -- launch once per rank by hand. See `../README.md` for a full
worked 3-rank example and how the mesh bootstrap works.
