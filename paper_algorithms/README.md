# paper_algorithms/

Real, compiling, runnable CUDA implementations of all **five** low-latency
AllReduce algorithms from Table I of:

> Siyuan Shen, Anton Korzh, John Bachan, Tiancheng Chen, Arnav Goel, Ludwig
> Schneider, Pouya Kousha, Zhenhao He, Sylvain Jeaugey, Kamil Iskra, Nishank
> Chandawala, Jeff R. Hammond, Torsten Hoefler.
> **"Every µs Matters: Achieving Near Speed-of-Light Latency in GPU
> Collectives."** arXiv:2607.16100.

plus a sixth, explicit-barrier baseline folder used purely as a comparison
point (see "Why a Baseline folder" below).

```
paper_algorithms/
  common/
    gpu_common.cuh                  shared host-side helpers (every folder uses this)
  LL/                                Table I: One-shot (LL)
  Sentinel/                         Table I: One-shot (Sentinel)
  Twoshot_LL/                       Table I: Two-shot (LL)
  Twoshot_Sentinel/                 Table I: Two-shot (Sentinel)
  LL128_Atomic/                     Table I: Two-shot (LL128 Atomic) -- the paper's own new algorithm
  Baseline_Barrier/                 NOT in Table I -- conventional explicit-barrier reference point
  check_node.sh                     run this on your cluster node, paste back the output
  submit_template.pbs               generic PBS template -- needs your node's real queue/module info
```

Each of the six folders is self-contained: a `.cu` file, a `Makefile`, and
a `README.md` explaining that technique's mechanism both technically and
with a plain-language analogy.

## Requirements

- 2 or more NVIDIA GPUs **in the same NVLink (or otherwise P2P-capable)
  domain** on one node -- exactly the scope the paper itself targets
  (Section III-B: "GPUs residing in the same NVLink domain"). CUDA peer
  access must succeed between every pair; each program checks this at
  startup and exits with a clear message if it can't.
- CUDA Toolkit with `nvcc` (12.0+ recommended, for `-arch=native` in the
  Makefiles; see each Makefile for how to target an architecture by name
  instead, e.g. `make ARCH=sm_90`).
- No NCCL, no MPI, no multi-process launcher needed -- every program is a
  single process that drives all GPUs directly via `cudaSetDevice()`.

**Every `.cu` file in this project has been compiled and linked with a real
CUDA 12.0 `nvcc`** targeting a physical GPU architecture (`sm_80`), with no
errors or warnings, before being delivered to you. That confirms the code
is syntactically and semantically correct C++/CUDA. It does **not** confirm
runtime correctness on real hardware or real latency numbers -- there were
no physical GPUs available to run these programs on where they were
written. Running them, and getting the actual numbers, is the next step,
on your cluster node.

Build and run any one of them, e.g.:

```
cd LL && make && ./ll_allreduce 4 65536 100
```

Every program prints a correctness check (GPU result compared against a
CPU-computed reference sum) followed by the average AllReduce latency in
microseconds.

## Is this "RDMA code" or "MPI code"?

Neither, in the literal sense of either word -- and that's intentional.
Every program here uses **one-sided, direct GPU-to-GPU memory access**
(a GPU writing straight into another GPU's memory, and polling for
arrival) rather than calling a collective library function like
`MPI_Allreduce()`. That one-sided put/poll model is the same *family* of
idea as RDMA verbs (remote direct memory access, no two-sided
send/receive handshake) -- it's just implemented over NVLink peer-to-peer
memory instead of over InfiniBand. The paper's own Section II-B frames
NCCL's device-side API this exact way, as adopting the PGAS/one-sided
model that NVSHMEM (and RDMA-style libraries generally) are built on. If
your professor's "RDMA, not MPI" instruction meant "one-sided direct
memory access, not a collective library call," this project already
matches that. If he specifically meant literal InfiniBand `ibv_*` verbs
across multiple nodes (like your earlier TCP/RDMA benchmark project), that
would be a different, larger undertaking, since the paper's own algorithms
are explicitly scoped to intra-node NVLink, not inter-node RDMA -- worth
confirming directly before building that version.

## Why standard CUDA P2P instead of NCCL's device-side API

The paper builds all five algorithms on top of NCCL's **device-initiated
communication API**, introduced in NCCL 2.28 and described by the paper
itself as experimental (`ncclLLBuffer`, `ncclSymPtr`, symmetric "team" and
"peer" objects -- Fig. 6-9). That API is very new, its exact header
signatures aren't published in a stable, version-pinned form anywhere
verifiable, and it may not even be present in whatever NCCL build is on
your cluster node. Writing code against it risked producing something that
*looks* like it implements the paper but silently doesn't compile, or
compiles against the wrong NCCL version, on your actual hardware.

Instead, every algorithm here is implemented directly against the
**stable, public CUDA Runtime API** (`cudaDeviceEnablePeerAccess`, plain
pointers across devices, `atomicAdd` on peer memory). On an NVLink-
connected node, enabling peer access gives every GPU the same direct
load/store/atomic access to every other GPU's memory that the paper's
"symmetric heap / load-store-accessible (LSA)" abstraction (Fig. 2) is
built on top of -- `ncclLLBuffer` is, at its core, exactly this kind of
pointer wrapped in a convenience API. So what's implemented here is a
faithful, from-scratch reproduction of each algorithm's actual
synchronization *mechanism* (the part the paper is teaching), using
ordinary CUDA primitives guaranteed to be on your system today. The one
place a real hardware limitation forced a documented simplification
(LL128 Atomic's cache-line atomicity) is called out explicitly in
`LL128_Atomic/README.md`.

## Why a Baseline_Barrier folder

"How did the paper improve latency" is a before/after claim -- it needs a
"before." `Baseline_Barrier/` is a conventional one-shot AllReduce that
DOES use an explicit synchronization barrier between the push and the
reduce step (the design the paper's Section III-B measures and then
eliminates, Fig. 3). It is not one of Table I's five techniques; it exists
so the other five have something concrete to be measured against on your
own hardware. See `Baseline_Barrier/README.md` for exactly how it's built
and an important honesty note about how its absolute numbers relate to the
paper's own Fig. 3 measurement.

## What each folder demonstrates, in one line each

- **Baseline_Barrier** -- the conventional way: push, then an explicit
  barrier, then reduce. Not barrier-free. The "before" number.
- **LL** -- one-shot; flag packed into the same atomic transaction as the
  data (bandwidth cost, best for very small messages, no reset needed).
- **Sentinel** -- one-shot; data written directly, detected by "no longer
  equals a reserved value" (full bandwidth, needs an explicit reset each
  round).
- **Twoshot_LL** -- ReduceScatter + AllGather, both legs synchronized with
  LL's packed-flag trick; cuts total traffic to `O(M)` at the cost of a
  second sync round.
- **Twoshot_Sentinel** -- same two-phase structure, both legs synchronized
  with Sentinel's polling trick instead; half of Twoshot_LL's traffic, at
  the cost of resetting two buffers each round instead of one.
- **LL128_Atomic** -- the paper's own new algorithm: two-shot, but both
  phases are synchronized via atomic adds and an atomic counter instead of
  an explicit flag, cutting scratch space and bandwidth overhead further
  still, at the cost of being the only non-deterministic technique of the
  five.

## Next step: running this on your actual node

I don't know your node's GPU model, CUDA version, or PBS queue setup, so
I can't hand you a submission script that will actually run without
editing. Run `bash check_node.sh` on the cluster node with GPU access and
paste back everything it prints (GPU model + NVLink topology, `nvcc`
version, available modules, PBS queue names). From that I can:

- tell you the exact `ARCH=` value to pass to every Makefile here,
- confirm your GPUs are actually NVLink-connected (required -- see
  Requirements above),
- fill in the "EDIT ME" lines in `submit_template.pbs` with your cluster's
  real queue name, GPU resource request syntax, and module-load line, so
  it's ready to `qsub` as-is.
