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
pbsdsh and prints the results. `qsub` on this cluster is a wrapper that takes only the
script name (`qsub -v` does not work), so the settings go into a small job script that
calls `run_all.pbs`, for example:

    PROGRAMS=Baseline_Barrier:LL SIZES=8:8192 ITERS=1000 bash run_all.pbs

The job scripts actually used are in `../jobs/`. `PROGRAMS` and `SIZES` are
colon-separated lists (`SIZES` is in floats: 8 = 32 B, 8192 = 32 KB, 8388608 = 32 MB),
`ITERS` is the number of timed iterations, `RANKS` optionally uses fewer nodes than
were allocated. `../run_floor.pbs` measures the network itself with `ib_write_lat` and
`ib_write_bw`.

## Correctness checks

Every program prints two lines:
- `Correctness check vs. CPU reference`: the whole output of the first (untimed) round
  against a sum computed on the CPU.
- `Correctness check, every timed round`: in every timed round the first and last
  element carry a round number, and the output must hold the exact expected sum. This
  catches a round that read old data. `LL_NoWait_NegativeTest` (LL with the wait removed)
  is used to show that this check does fail when the code is wrong.

## Notes

- The adapter is `mlx5_0` (vendor part id 4113), not a ConnectX-3 as written in older notes.
- `LL128_Atomic` is skipped on this cluster: the adapter reports `atomic_cap = NONE` and
  rejects every atomic with `EINVAL` (see `Atomic_Probe/`).
- `Baseline_Barrier` and `LL` sum with `reduce_blocked()` (common/) and are built with -O3.
  The other programs still use the per-element loop and -O2.
- `Bidirectional_DoubleBuffering` always uses exactly 2 ranks.
- Run on compute02-07 for timing: compute00 and compute01 have leftover processes at 100% CPU.
- Measured results are in `../results/`.
