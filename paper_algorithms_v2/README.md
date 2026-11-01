# paper_algorithms_v2

RDMA verbs implementations of the low-latency AllReduce algorithms from "Every Microsecond
Matters: Achieving Near Speed-of-Light Latency in GPU Collectives" (arXiv:2607.16100), plus a
barrier-based baseline. They run on the CPU nodes of the cluster over InfiniBand, with no MPI.
This is `paper_algorithms` (v1) with the fixes listed at the end.

```
common/                         connection setup shared by all programs (rdma_common.h / .c)
Baseline_Barrier/               one-shot, with an explicit two-sided barrier
LL/                             one-shot, data write followed by an epoch flag write
Sentinel/                       one-shot, receiver waits for a sentinel value to be overwritten
Bidirectional_DoubleBuffering/  chunked exchange between 2 ranks (Fig. 4 of the paper)
Twoshot_LL/                     ReduceScatter + AllGather, epoch flags
Twoshot_Sentinel/               ReduceScatter + AllGather, sentinel values
LL128_Atomic/                   two-shot with an RDMA atomic add as the completion signal
Atomic_Probe/                   diagnostic that prints what the adapter supports for atomics
```

## Build

Each folder has its own Makefile:

```
cd LL && make
```

It needs `libibverbs` and `librdmacm`.

## Run

One process per node, started by hand. Every process gets its own rank and the same list of
IPoIB addresses:

```
./ll_allreduce_rdma <my_rank> <num_ranks> <base_port> <ip0> <ip1> ... <ip(N-1)> [numFloats] [iters]
```

For 3 ranks:

```
node A:  ./ll_allreduce_rdma 0 3 20000 10.1.2.1 10.1.2.2 10.1.2.3 65536 100
node B:  ./ll_allreduce_rdma 1 3 20000 10.1.2.1 10.1.2.2 10.1.2.3 65536 100
node C:  ./ll_allreduce_rdma 2 3 20000 10.1.2.1 10.1.2.2 10.1.2.3 65536 100
```

`base_port` can be any free port, the same on every rank. Use the IPoIB address of each node
(`ib0`), not the Ethernet one. The processes can be started in any order. Every program
checks its result against a CPU reference sum and prints the latency statistics (average, min,
median, p99, max). `Bidirectional_DoubleBuffering` needs exactly 2 ranks and takes the number
of chunks as an extra argument after the address list.

## How the ranks connect

`rdma_mesh_connect()` in `common/rdma_common.c` connects every rank to every other rank. All
ranks go through the pairs (0,1), (0,2), ..., (N-2,N-1) in the same order. For a pair (a,b),
rank a listens and rank b connects. Before going through the pairs, every rank starts all of
its listeners, so a connect never arrives before the listener exists. After connecting, the
two sides swap buffer addresses and keys with one SEND/RECV pair. From then on the data moves
with one-sided RDMA WRITEs.

## Why there is no barrier

With a one-sided write the receiver gets no notification. InfiniBand RC queue pairs apply
operations on the remote side in the order they were posted, so the programs post the data
write and then a small second write (a flag) on the same queue pair. When the receiver sees the
flag it knows the data is already there. The paper does this on GPUs by packing flag and data
into one 16-byte store, which InfiniBand cannot do. Sentinel instead fills the receive buffer
with a NaN pattern and waits for each element to change.

Every connection has its own protection domain, so a buffer used as the source of a write has
to be registered for that connection. The programs therefore keep one registered copy of the
data to send per peer.

## LL128 Atomic

The paper's version uses an NVLink atomic add that sums the floating-point data itself.
InfiniBand atomics (`IBV_WR_ATOMIC_FETCH_AND_ADD`) only work on 64-bit integers. Here the atomic
is used as the completion counter, the data goes by an ordinary WRITE, and the CPU does the sum.
On this cluster's adapter atomics are not supported, so the program prints that it is skipped.

## Changes from v1

* Writes to all peers are posted back to back, small ones inline, and the completions are
  collected once after the sum. The data write is unsignaled; only the flag write is signaled.
* Two receive buffers per peer are used alternately by round, so a peer one round ahead cannot
  overwrite data that is still being read.
* Sentinel sets a slot back to the sentinel after reading it. Before, the buffers were reset
  only once, so after two rounds the wait returned immediately.
* Flag and counter waits use `<` instead of `!=`.
* Fix in the barrier's receive handling in Baseline_Barrier.
* LL128_Atomic exits cleanly when atomics are not available.
* Timing statistics (min, median, p99, max) and a barrier before the first and after the last
  round.
