# AllReduce over InfiniBand RDMA: summary of the work

Last updated: 2026-10-15. Per-run numbers are in the other files in this folder.

## 1. Aim

We implemented the AllReduce (sum) algorithms from "Every µs Matters: Achieving Near Speed-of-Light
Latency in GPU Collectives" (arXiv:2607.16100) and measured them on the university cluster.

The paper looks at small AllReduce operations (the ones done after every attention and MLP block when an
LLM generates a token). For small messages the time is mostly latency, and a big part of that latency is
the memory barrier used to make sure the data has arrived. The paper removes the barrier with four ideas:
LL (a flag stored with the data), Sentinel (a reserved value that the receiver waits to disappear),
double buffering with bidirectional exchange, and LL128 Atomic (a hardware atomic counter). It also
compares one-shot and two-shot versions of the algorithms.

The paper uses GPUs connected by NVLink. Our cluster (172.16.201.12) has 8 CPU nodes with Mellanox
ConnectX-3 InfiniBand and no GPUs, so we wrote the same ideas with RDMA verbs (`paper_algorithms_v2/`).
The `paper_algorithms/` folder is an earlier CUDA version that was never run because no GPU was available.

One difference matters. InfiniBand has no atomic 16-byte store like NVLink. What it does guarantee is that
writes posted on the same reliable-connection queue pair reach remote memory in the order they were posted.
So "write the data, then write a small flag on the same queue pair" gives the receiver the same guarantee.

## 2. Test setup

- 3 ranks, one per node (compute00, compute01, compute02), connected over IPoIB (10.1.2.x).
- No MPI. Each rank is started with `pbsdsh` and is given its rank number and the addresses of all ranks.
- 1000 timed iterations per run (100 in the very first runs). Latency is measured per iteration and
  reported as mean, min, median, p99 and max.
- Message sizes from 128 B to 256 KiB.

## 3. The programs

| Folder | What it does |
|---|---|
| `Baseline_Barrier` | one-shot, with an explicit two-sided barrier (the "before" case) |
| `LL` | one-shot, data write followed by a flag write |
| `Sentinel` | one-shot, receiver waits for the sentinel value to disappear |
| `Twoshot_LL` | ReduceScatter + AllGather, with flags |
| `Twoshot_Sentinel` | ReduceScatter + AllGather, with the sentinel |
| `LL128_Atomic` | ReduceScatter + AllGather, completion signalled by an RDMA atomic counter |
| `Bidirectional_DoubleBuffering` | chunked exchange between 2 ranks (Fig. 4 of the paper) |
| `Atomic_Probe` | small test that prints what the adapter reports about atomics |

`common/` holds the connection setup (full mesh of RDMA connections) and shared helper functions.

## 4. Problems found and fixed

When we started, only two results existed: Sentinel at about 57,800 µs and Two-shot Sentinel at about 610 µs
(3 ranks, 256 KiB). These looked wrong, since the paper reports a few µs and two-shot only moves about 3
times less data than one-shot at 3 ranks. Reading the code showed several bugs.

1. **Sentinel was never put back.** The receive buffers were filled with the sentinel once at setup. From
   round 2 onwards they held old data, so the wait loop did not wait for the peer at all. The ranks drifted
   apart, and a rank that finished early closed its connections while the others still needed them, which
   is what made rank 0 crash with "transport retry counter exceeded". The check against the CPU result still
   passed because every round used the same input. The fix is to write the sentinel back after a value is
   read. Sentinel at 256 KiB went from 57,800 µs to about 542 µs, and Two-shot Sentinel from 610 µs to about
   472 µs. The old numbers did not measure a real exchange.
2. **One receive buffer reused every round** (LL, Twoshot_LL, LL128_Atomic, Baseline). A fast peer's next
   write could overwrite data still being read. Fixed by using two buffers, chosen by round parity.
3. **Flag waits used `!=`.** If a peer was already one round ahead the loop could spin forever. Changed to
   `<`, since flags only increase, and added a memory fence after the wait.
4. **LL128_Atomic waited for the wrong counter value.** Each peer adds 1 to its own counter per round, so the
   value is `it+2`, not `(it+2)*(n-1)`. The old formula only works for 2 ranks.
5. **Rare 2 second stall in the barrier.** One iteration took 2,151,641 µs on all ranks, which pushed the
   Baseline mean to about 12,500 µs. The most likely cause is that only one receive was posted for the
   barrier messages, so a message could arrive with no receive ready and the sender waited for a retry
   timer. Two receives are now kept posted. The stall did not come back afterwards, but it was rare, so this
   is not proven.
6. **LL128_Atomic cannot run on this cluster.** `Atomic_Probe` shows the adapter reports `atomic_cap = NONE`
   and rejects every atomic post with EINVAL (four variants tried). This is a hardware limit. The program now
   detects it at start-up, prints SKIPPED and exits with code 2.

## 5. Speed changes

Writes used to be blocking: post one write, wait for it to finish, then do the next peer. They now work like
this:

- Writes are posted to all peers first, and completions are collected after the sum is done.
- The data write is unsignaled and only the flag write (or atomic) behind it is signaled. Because
  completions on a queue pair arrive in order, that one completion covers both.
- Small payloads are sent inline (copied into the work request). The queue pair asks for 128 B of inline
  space and the adapter gave 188 B.
- Timing is now per iteration, and there is a barrier before the timed loop and before closing.

The barrier in `Baseline_Barrier` was left as it was, since it is the thing being measured.

Effect on Sentinel and Two-shot Sentinel (mean latency in µs, same code apart from this change):

| Size | Sentinel before | Sentinel after | Two-shot Sentinel before | Two-shot Sentinel after |
|---|---|---|---|---|
| 128 B | 4.28 | 2.08 | 8.21 | 3.87 |
| 1 KiB | 7.00 | 4.07 | 10.01 | 5.58 |
| 4 KiB | 12.70 | 10.26 | 14.49 | 11.55 |
| 16 KiB | 43.5 | 29.8 | 44.1 | 32.5 |
| 64 KiB | 139.7 | 85.1 | 118.8 | 93.5 |

The other algorithms were not measured before this change, so there is no before/after for them.

## 6. Results

Mean latency in µs, 3 ranks, 1000 iterations (job 28594). All programs printed PASSED for the correctness
check on every rank. For the two-shot programs the message is padded to a multiple of 3 floats.

| Algorithm | 128 B | 1 KiB | 4 KiB | 16 KiB | 64 KiB |
|---|---|---|---|---|---|
| Baseline (with barrier) | 8.26 | 9.94 | 16.6 | 37.4 | 119.8 |
| LL | 2.47 | 3.52 | 8.96 | 27.2 | 92.1 |
| Sentinel | 2.05 | 4.06 | 10.3 | 28.0 | 80.7 |
| Twoshot_LL | 4.33 | 5.61 | 8.83 | 22.0 | 69.8 |
| Twoshot_Sentinel | 3.84 | 5.60 | 11.6 | 32.5 | 79.8 |
| Bidirectional (2 ranks, 8 chunks) | 14.1 | 14.7 | 18.6 | 25.0 | 62.0 |
| LL128_Atomic | skipped, no atomics on this adapter | | | | |

Bidirectional uses 2 ranks and times a whole 8-chunk pass, so it cannot be compared with the others.

Best barrier-free algorithm compared with the baseline:

| Size | Best | Speed-up over baseline |
|---|---|---|
| 128 B | Sentinel, 2.05 µs | 4.0x |
| 1 KiB | LL, 3.52 µs | 2.8x |
| 4 KiB | Twoshot_LL, 8.83 µs | 1.9x |
| 16 KiB | Twoshot_LL, 22.0 µs | 1.7x |
| 64 KiB | Twoshot_LL, 69.8 µs | 1.7x |

## 7. Observations

- Removing the barrier saves about 6 µs per AllReduce. The relative gain is biggest for tiny messages and
  gets smaller as moving the data takes over.
- LL: one-shot is better below about 4 KiB and two-shot is better above that (0.76 times the one-shot time at
  64 KiB), because two-shot sends less data (4M/3 per rank instead of 2M) at the cost of a second round.
- Sentinel: one-shot is better or equal at every size we tested.
- Twoshot_Sentinel is slower than Twoshot_LL at larger sizes (79.8 against 69.8 µs at 64 KiB, 32.5 against
  22.0 µs at 16 KiB). We think this is because the sentinel version checks and resets every float on the
  CPU while LL waits on one flag, but we did not measure it.
- Sentinel is faster than LL at 128 B and 64 KiB, and LL is faster from 1 KiB to 16 KiB. This is not exactly
  the ordering in the paper. Our LL is a separate flag write, not the paper's single 16-byte store.

## 8. Limits of these results

- The paper's 1.404 µs figure is for NVLink and GPUs, so it is not a target for this cluster. We have not
  measured the raw InfiniBand latency of this fabric (for example with `ib_write_lat`), so we cannot say how
  close 2 µs is to the hardware limit.
- The CPU sum loops are still plain scalar loops. They are the main cost at large sizes.
- Only 3 ranks were tested. The cluster has 8 nodes.
- Some runs may have shared the network with other users' jobs.

## 9. Files and how to run

Code is in `paper_algorithms_v2/`. The changes are mostly in `common/rdma_common.c` and `.h` (the new
`rdma_post_write`, `rdma_post_write_ex`, `rdma_wait_write`, `rdma_post_atomic_add`, `rdma_wait_atomic` and
`rdma_print_stats` functions, two bootstrap receives, and correct error codes) and in the seven algorithm
files. `Atomic_Probe/atomic_probe.c` is new.

`run_all.pbs` in the repo root runs everything. What to run is set with environment variables (`PROGRAMS`,
`SIZES`, `ITERS`, `TIMEOUT`, `BASE`) and the number of ranks with `-l nodes=N`. The top of the file has
examples. A run that hangs ends after the timeout and shows `exit=124`.

To run on the cluster:

1. Copy the files with `scp` to `student4@172.16.201.12:~/IMT2023016/`.
2. On the cluster: `sed -i 's/\r$//' run_all.pbs` (the script needs Unix line endings), then build each
   program folder with `make -B`.
3. `qsub run_all.pbs`. When `qstat -u student4` no longer shows the job, read the `.out` file named by qsub
   (under `/home/student4/.pbs_route/output/`).

## 10. Next steps

1. Run with more than 3 ranks (up to 8 nodes). Two-shot's advantage should grow with the number of ranks.
2. Speed up the Sentinel sum loops (one check per cache line, reset in batches).
3. Measure the InfiniBand latency with `ib_write_lat`.
4. Try write-with-immediate as a completion signal in place of the atomic for LL128. It would have to be
   described as a substitute, since it is not the paper's atomic-add mechanism.
