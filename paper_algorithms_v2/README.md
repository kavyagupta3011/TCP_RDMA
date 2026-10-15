# paper_algorithms_v2

AllReduce (sum) algorithms from "Every us Matters" (arXiv:2607.16100), written with
InfiniBand RDMA verbs (libibverbs + librdmacm) and run on the cluster's compute nodes.
One process per node, no MPI.

## Folders

- `common/` shared connection setup and helpers (rdma_common.c / rdma_common.h)
- `Baseline_Barrier/` writes, then an explicit two-sided barrier (the "with barrier" case)
- `LL/` one-shot, data write followed by an epoch flag write
- `Sentinel/` one-shot, receiver waits for the sentinel value to disappear
- `Twoshot_LL/` ReduceScatter + AllGather, flags
- `Twoshot_Sentinel/` ReduceScatter + AllGather, sentinel
- `LL128_Atomic/` ReduceScatter + AllGather, RDMA atomic counter (needs atomic support)
- `Bidirectional_DoubleBuffering/` chunked exchange between 2 ranks only
- `Atomic_Probe/` small test that prints what the adapter reports about atomics

## Build

In each folder:

    make -B

## Run

Start one process per node with its rank, the number of ranks, a base port and the IPoIB
address of every rank:

    ./<program> <my_rank> <num_ranks> <base_port> <ip0> ... <ip(N-1)> [numFloats] [iters]

On the cluster `run_all.pbs` (in the folder above this one) starts every rank with
pbsdsh and prints the results:

    qsub run_all.pbs
    qsub -v PROGRAMS=Sentinel:Twoshot_Sentinel run_all.pbs
    qsub -v SIZES=32:256:1024,ITERS=1000 run_all.pbs

`PROGRAMS` and `SIZES` are colon-separated lists (`SIZES` is in floats), `ITERS` is the
number of timed iterations. The number of ranks is the number of nodes requested.

## Notes

- `LL128_Atomic` is skipped on this cluster. The ConnectX-3 adapter reports
  `atomic_cap = NONE`, so RDMA atomics are not available (see `Atomic_Probe/`).
- `Bidirectional_DoubleBuffering` always uses exactly 2 ranks.
- Measured results are in `../results/`.
