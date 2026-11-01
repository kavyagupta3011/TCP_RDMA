# baselines

Standard reference algorithms for the AllReduce comparison. Nothing in this folder changes
the programs in `paper_algorithms`, `paper_algorithms_v2` or `paper_algorithms_v3`.

## Ring_AllReduce

The textbook ring AllReduce: a reduce-scatter followed by an allgather, 2(n-1) steps, each
step one SEND to the next rank and one RECV from the previous rank. This is the algorithm
behind NCCL's ring kernels, which are the baseline in the paper.

It is plain two-sided RDMA on purpose: receives are posted before the connection is made,
every send is signaled, and there are no inline sends, flags, epochs, sentinel values or
double buffers. The add is a simple loop. The message size is rounded up to a multiple of
the number of ranks (8 floats become 9 with 3 ranks), as in the two-shot programs.

Same arguments as the other programs:

    ./ring_allreduce_rdma <my_rank> <num_ranks> <base_port> <ip0> ... <ip(N-1)> [numFloats] [iters]

and the same output lines (correctness check, average, min / median / p99 / max).

## Build and run on the cluster

    cd ~/IMT2023016/baselines/Ring_AllReduce
    make -B
    cd ..
    qsub ring_job.pbs

`ring_job.pbs` runs the same nodes and sizes as `run_updated/run_everything.pbs`
(32 B, 64 B, 128 B, 32 KB, 256 KB, 32 MB with 3 ranks, plus 2 ranks). The output goes to the
usual file under `~/.pbs_route/output/`.

## Checked so far

It compiles without warnings (`-Wall -Wextra`). The ring schedule and the program logic were
tested for 2 to 5 ranks and several sizes against a stand-in for the RDMA library on one
machine; all checks passed. It has not run on the cluster yet, so the first run should be
`ring_quick.pbs` before the full job.
