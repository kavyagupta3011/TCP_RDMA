# LL (Low-Latency) One-Shot AllReduce

Paper reference: Section IV.B.1, "1) LL" (page 4).

## The idea, technically

Every other synchronization scheme needs two things: the data itself, and a
separate signal ("has it arrived yet?") that the receiver can check *after*
a memory barrier guarantees the data write is visible. That barrier is what
costs the >1us per call the paper measures in Fig. 3.

LL's trick: pack the signal *into* the same transaction as the data. Take 8
bytes of real data and 8 bytes of flag, and write both together as a single,
naturally-aligned 16-byte store. Hardware guarantees that a 16-byte-aligned
store either lands completely or not at all as far as any other reader is
concerned -- there's no way to observe "half old flag, half new data." So
the receiver just keeps re-reading that 16-byte line; the instant it sees
its expected flag value, it *knows* the data next to it is valid, with zero
separate barrier calls.

The cost: half of every 16-byte transfer is flag, not data, so effective
bandwidth is halved. That's exactly why the paper calls LL "mostly suitable
for very small messages" -- for large ones, the wasted half outweighs the
saved barrier.

## The idea, simply

Imagine passing a note under a door. Instead of passing the note, then
knocking to say "it's there," you tape a big colored sticker to the note
itself. The other person just watches for the sticker's color to change --
the moment they see it, they already have the note in hand too, because it
was taped on, not sent separately. No knock (barrier) needed.

## How this maps to the code (`ll_allreduce.cu`)

- `struct LLLine` is the 16-byte packet: 2 floats' worth of data bits +
  a duplicated 4-byte flag (so it can be checked with two field reads
  instead of decoding a real 128-bit compare).
- `llSendKernel`: every GPU pushes its own data directly into every peer's
  scratch buffer, tagging each line with the current **epoch** (a counter
  that increments every AllReduce call) as the flag.
- `llRecvReduceKernel`: every GPU polls its *own* scratch buffer (already
  filled by peers -- no remote reads needed at this point) until each
  peer's line shows the current epoch, then sums.

Using an epoch instead of a fixed "1" flag is what lets this run repeatedly
with **no reset step between calls** -- a stale line from two calls ago just
won't match today's epoch. That's the concrete payoff of LL's "self
clearing" property the paper alludes to when contrasting it with Sentinel.

## Build & run

```
make                       # nvcc -arch=native by default; see Makefile to override
./ll_allreduce [numGPUs] [numFloats] [iters]
./ll_allreduce 4 65536 100
```

Prints a correctness check (GPU result vs. a CPU-computed reference sum)
and the average one-shot AllReduce latency in microseconds.
