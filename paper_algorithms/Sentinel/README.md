# Sentinel One-Shot AllReduce

Paper reference: Section IV.B.2, "2) Sentinel" (page 4).

## The idea, technically

LL buys its barrier-free property by giving up half its bandwidth to the
flag. Sentinel buys the *same* barrier-free property a different way, at
full bandwidth: before any data is sent, the receiving scratch slot is
pre-filled with a value that legitimate data can never take -- the paper's
example is a specific NaN bit pattern (`-NaN`). The sender then writes real
data directly into that slot, at full width, no packing. The receiver polls
the slot and the instant its value differs from the sentinel, it knows real
data has arrived -- again, no separate barrier.

Two costs this buys, both explicit in the paper and both implemented here:

1. **Reset cost.** Unlike LL's self-clearing epoch flag, a sentinel slot
   must be put back to the sentinel value before it can be reused, or a
   receiver from the *next* round could mistake this round's leftover data
   for "not arrived yet" -- or worse, this round's sentinel for "already
   here." `sentinelResetKernel` is that explicit reset, and it's included
   in every timed iteration in `main()` because it's a genuine per-call cost
   of this technique, not an artifact of the benchmark.
2. **Excluded-value constraint.** The sentinel bit pattern must never be a
   value real data (or a real sum) could legitimately take. We pick a
   specific quiet-NaN payload (`0x7fdead00`) and compare *raw bits*, not
   floating-point equality -- IEEE 754 NaN famously isn't equal to itself
   under `==`, so a naive `value == sentinel` check would be wrong even for
   the sentinel slot itself. `sentinelBits()` / `sentinelValue()` show the
   correct way to do this comparison.

## The idea, simply

Instead of taping a sticker to every note (LL), you agree in advance that
the mailbox slot will hold a "this slot is empty" card until a real note is
slid in. You just keep glancing at the slot; the moment the card is gone and
something else is there, you know mail has arrived. But before you can reuse
that mailbox slot tomorrow, someone has to put the "empty" card back in.

## How this maps to the code (`sentinel_allreduce.cu`)

- `sentinelBits()` / `sentinelValue()`: the reserved bit pattern.
- `sentinelResetKernel`: fills scratch back to the sentinel before each round.
- `sentinelSendKernel`: full-width, unpacked writes of real data (no flag).
- `sentinelRecvReduceKernel`: polls each slot's raw bits until they differ
  from the sentinel, then sums.

## Build & run

```
make
./sentinel_allreduce [numGPUs] [numFloats] [iters]
./sentinel_allreduce 4 65536 100
```

Prints a correctness check and the average one-shot AllReduce latency
(including the per-call reset pass) in microseconds. Compare this number
against `LL/`'s at a few different message sizes to see the paper's claim
play out directly: Sentinel should look relatively better as `numFloats`
grows, because it isn't paying LL's bandwidth tax -- but it's paying the
reset tax LL never has to.
