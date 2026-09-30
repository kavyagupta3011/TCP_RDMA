# Bidirectional Communication & Double Buffering

Paper reference: Section IV.B.3, "3) Bidirectional Communication & Double
Buffering" and Fig. 4 (page 4).

## The idea, technically

LL and Sentinel each remove the barrier for a *single* exchange. But a
message too big to fit in one round of scratch space has to be sent in
chunks -- and naively, you'd insert a barrier between chunks so a sender
never overwrites chunk *i*'s buffer before the receiver has actually
finished reading it. That reintroduces exactly the cost LL/Sentinel just
got rid of.

The paper's fix (Fig. 4): use **two** scratch buffers, and alternate which
one is "active" as chunks advance (buffer 0 for even chunk indices, buffer 1
for odd). Both ranks push their chunk into the peer's current buffer, then
wait for the peer's chunk to land in their own current buffer, reduce it,
and only *then* move to the next chunk/buffer. Because each side won't touch
buffer 0 again until chunk *i+2*, and by then it has necessarily already
both sent its chunk *i+1* (into the peer's buffer 1) and consumed the peer's
chunk *i* (out of its own buffer 0), the two loops implicitly guarantee
correctness without any extra synchronization call. The paper's own phrase
for this: "each receive from a peer serves as an implicit permission for the
next send" -- i.e. credit-based flow control with a credit of exactly one
buffer, for free, as a byproduct of the loop structure.

This mechanism is orthogonal to *which* per-chunk signaling scheme you use
underneath -- the paper explicitly notes it works with either LL or
Sentinel. This implementation uses LL-style flag packing (the chunk index as
an epoch) for the per-chunk exchange, since that also sidesteps Sentinel's
reset cost between chunks.

## The idea, simply

Two people are handing sheets of paper back and forth across a table, but
the table only has room for two sheets at a time -- a "left" spot and a
"right" spot. Sheet 1 goes in "left," sheet 2 goes in "right," sheet 3 goes
back in "left" -- but by the time sheet 3 needs "left," sheet 1 has
definitely already been picked up (you were both working through the stack
in order), so there's never a pileup, and neither person ever has to stop
and ask "are you done with that yet?"

## How this maps to the code (`bidir_doublebuffer.cu`)

- `struct DBLine`: the same 16-byte LL-style packed line used in `LL/`.
- `bidirDoubleBufferKernel`: **one persistent kernel per GPU** with the
  entire chunk loop written as a device-side `for` loop -- deliberately, so
  there is no host round-trip (and therefore no host-imposed barrier)
  between chunks. Inside the loop: push this chunk into the peer's current
  buffer, poll for the peer's chunk in this GPU's own current buffer,
  reduce, advance to the next buffer.
- The file focuses on exactly the **pairwise** (2-GPU) case the paper's
  Fig. 4 teaches. Extending this to N>2 ranks means composing this same
  pairwise exchange as a building block of a larger schedule (e.g. a ring
  or recursive-doubling pattern over pairs) -- intentionally out of scope
  here, to keep the core two-buffer mechanism itself unambiguous and
  correct.

## Build & run

```
make                       # always uses exactly 2 GPUs
./bidir_doublebuffer [numFloats] [numChunks] [iters]
./bidir_doublebuffer 262144 8 100
```

Prints a correctness check and the average latency for the whole chunked
reduction (all `numChunks` chunks, one kernel launch per GPU) in
microseconds. Try increasing `numChunks` for a fixed `numFloats` (i.e.
shrinking each chunk) and watch how little the per-call overhead grows --
that's the double-buffering barrier-free property paying off directly.
