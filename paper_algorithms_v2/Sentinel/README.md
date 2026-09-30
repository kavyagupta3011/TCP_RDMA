# Sentinel One-Shot AllReduce -- RDMA verbs version

Paper reference: Section IV.B.2. See `../README.md` first for the shared
design idea.

## The idea, technically

A single RDMA WRITE per round -- the real data, full width, no trailing
flag write at all. The receiving buffer is pre-filled with a reserved
sentinel bit pattern (`0x7fdead00`), and the receiver polls each float
for its bit pattern to stop matching the sentinel. This is correct at
per-float granularity because a 4-byte-aligned word write lands on real
hardware as one indivisible unit -- never observed half old, half new.

The catch: the buffer needs resetting before reuse. In the CUDA version, a
single host process could trivially guarantee "every GPU's reset is done"
before issuing any send. Here, the two ranks are separate processes on
separate machines -- there's no single orchestrator to sequence that
without an explicit cross-process round trip (i.e. a real barrier, the
exact thing this project is about avoiding). The fix: **two** scratch
buffers, reset to the sentinel exactly ONCE at startup (right after the
connection is established, which is itself already a natural one-time
synchronization point), ping-ponged by round parity thereafter. Because
both ranks execute rounds in the same strict sequential order, by the time
either side's round-`i+2` write could possibly reach a given buffer, the
receiver has necessarily already consumed that buffer's round-`i`
contents -- the same "implicit permission" argument the paper uses for
double buffering (Section IV.B.3), just applied across whole AllReduce
calls instead of across chunks within one call.

## The idea, simply

A mailbox slot starts the day with a "nothing here yet" card in it. You
just glance at the slot and the moment real mail replaces the card, you
know it's arrived. Instead of someone re-placing the "empty" card every
single day (which would need the sender to wait for confirmation first),
there are two slots that alternate day by day -- by the time slot A is
used again, you've long since already read yesterday's mail out of it.

## N-node generalization

With N ranks, each PEER gets its own pair of sentinel buffers (double
buffered across rounds, same reasoning as above, just applied
independently per connection). Every round, this rank writes its input
into every peer's current buffer and sums its own input with whatever
stops reading as the sentinel across all N-1 peer buffers -- same
full-mesh push as `../LL/README.md`, sentinel-style instead of flag-style.

## Build & run

```
make
./sentinel_allreduce_rdma <my_rank> <num_ranks> <base_port> <ip0> ... <ip(N-1)> [numFloats] [iters]
```

No MPI -- launch once per rank by hand. See `../README.md` for a full
worked 3-rank example and how the mesh bootstrap works.
