# Two-Shot Sentinel AllReduce

Paper reference: Table I, row "Two-shot (Sentinel)" (page 5).

## The idea, technically

Same two-phase ReduceScatter-then-AllGather decomposition as
`Twoshot_LL/` (read that README first if you haven't -- the phase
structure is identical). The only difference is the per-transfer
synchronization mechanism: instead of packing a flag into every 16-byte
line (LL), this version writes data directly at full width and detects
arrival by polling for the value to stop matching a reserved sentinel bit
pattern -- exactly `Sentinel/`'s one-shot trick, just applied to both the
ReduceScatter and the AllGather legs.

Because Sentinel doesn't pay LL's flag-packing tax in either phase, Table I
gives it exactly half of Two-shot LL's communication volume:
`2(N-1)M/N` vs. `4(N-1)M/N`. The cost is the same one `Sentinel/` pays:
every scratch region used for detection -- here, BOTH the ReduceScatter
scratch and the AllGather output buffer -- must be reset to the sentinel
value before each round, since there's no self-clearing epoch the way LL
has. `resetSentinelKernel` runs on both buffers before every timed
iteration.

## The idea, simply

Same notetaker-per-topic picture as `Twoshot_LL/`'s README, except instead
of everyone taping a colored sticker to their note, the notetaker's inbox
starts each round with a stack of "nothing here yet" placeholder cards, one
per expected contributor. As real notes replace the placeholders, the
notetaker just watches for real handwriting to show up in place of the
placeholder card -- but before the next round, someone has to go put all
the placeholder cards back.

## How this maps to the code (`twoshot_sentinel_allreduce.cu`)

- `resetSentinelKernel`: resets the ReduceScatter scratch and the AllGather
  output buffer to the sentinel bit pattern, run at the start of every
  round.
- `rsSendKernel` / `rsReduceKernel`: Phase 1, full-width writes and
  bit-pattern polling, scoped per-partition exactly like `Twoshot_LL/`'s
  Phase 1 is scoped per-partition with LL lines.
- `agSendKernel` / `agFinalizeKernel`: Phase 2, same idea for the broadcast
  leg.

## Build & run

```
make
./twoshot_sentinel_allreduce [numGPUs] [numFloats] [iters]
./twoshot_sentinel_allreduce 4 65536 100
```

For a clean 4-way comparison of all four one-shot/two-shot LL/Sentinel
combinations at the same message size, run this alongside `LL/`,
`Sentinel/`, and `Twoshot_LL/` with identical `numFloats` and `numGPUs` --
that's the direct empirical version of Table I's comm-volume column.
