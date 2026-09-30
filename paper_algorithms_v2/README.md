# paper_algorithms_v2/

The **RDMA verbs** version of the paper's five low-latency AllReduce
techniques plus a baseline, for `172.16.201.12` -- the cluster you actually
have access to, which `check_node.sh` confirmed has no GPU/NVLink hardware
on any of its 8 compute nodes, only CPUs and a real Mellanox ConnectX-3
InfiniBand fabric (`mlx4_0`). `paper_algorithms/` (the original delivery)
stays as the CUDA/NVLink version, ready for whenever GPU access is
confirmed on a different machine; this folder is what actually runs on the
hardware you have today.

```
paper_algorithms_v2/
  common/
    rdma_common.h / .c              shared RDMA CM + verbs connection setup, every folder uses this
  Baseline_Barrier/                 conventional: explicit two-sided barrier (the "before")
  LL/                                one-shot, LL-style ordered write+signal
  Sentinel/                         one-shot, sentinel polling + double-buffer-across-iterations
  Bidirectional_DoubleBuffering/    chunked pairwise exchange, Fig. 4's own mechanism (2 ranks only)
  Twoshot_LL/                       ReduceScatter+AllGather, LL-style signaling
  Twoshot_Sentinel/                 ReduceScatter+AllGather, sentinel signaling
  LL128_Atomic/                     completion signaled by a REAL RDMA hardware atomic
```

Every `.c` file has been compiled AND linked with real `gcc` (via each
folder's own `make`) against the actual `libibverbs`/`librdmacm` headers
(the same ones your cluster's `mlx4_0` adapter uses) -- zero errors, zero
warnings. This sandbox has no InfiniBand hardware of its own (confirmed: no
`/sys/class/infiniband`, no software RDMA device available), so none of
these programs have actually been *run* yet -- only compiled. Running them,
across several of your cluster's real compute nodes, is the next step.

## N-node, no MPI

Every program here (except `Bidirectional_DoubleBuffering/`, see below)
now runs across **any number of ranks N >= 2** -- 3 or 4 nodes, or however
many your professor wants -- with **zero MPI involvement anywhere**: no
`mpirun`, no MPI headers, no MPI linked in. You launch one OS process per
rank by hand (e.g. one SSH session per compute node, or one PBS job step
per node), and tell each process its own rank number and the SAME shared
list of every rank's IPoIB address:

```
./<program> <my_rank> <num_ranks> <base_port> <ip0> <ip1> ... <ip(N-1)> [numFloats] [iters]
```

Every rank is given the identical `<ip0> ... <ip(N-1)>` list, in the same
order -- rank `i`'s own address is `ip<i>` in that list. From that alone,
every rank works out who it needs to talk to and in what role, with no
central coordinator and no messages exchanged just to set up the
connections themselves (see "How the mesh bootstrap works" below).

### Example: 3 ranks

On **compute node A** (its own IPoIB address is `10.1.2.1`):
```
cd paper_algorithms_v2/LL
make
./ll_allreduce_rdma 0 3 20000 10.1.2.1 10.1.2.2 10.1.2.3 65536 100
```

On **compute node B** (`10.1.2.2`), started within a few seconds:
```
cd paper_algorithms_v2/LL
make
./ll_allreduce_rdma 1 3 20000 10.1.2.1 10.1.2.2 10.1.2.3 65536 100
```

On **compute node C** (`10.1.2.3`):
```
cd paper_algorithms_v2/LL
make
./ll_allreduce_rdma 2 3 20000 10.1.2.1 10.1.2.2 10.1.2.3 65536 100
```

The three numbers before the IP list are: my own rank (0, 1, or 2), the
total rank count (3), and a base port number (any free port -- every rank
uses the same one; the mesh bootstrap derives a distinct actual port per
connection from it automatically). Every program prints a correctness
check (its result compared against a CPU-computed reference sum across all
N ranks' inputs) and the average AllReduce latency in microseconds, once
all N processes have connected to each other. Every folder follows this
exact same `<my_rank> <num_ranks> <base_port> <ips...>` pattern; see each
folder's own README for any extra trailing arguments (e.g.
`Bidirectional_DoubleBuffering/` also takes a chunk count, after its IP
list).

You don't have to start the N processes in rank order, or even close
together in time -- see the bootstrap explanation below for why that's
safe.

### How the mesh bootstrap works

`common/rdma_common.c`'s `rdma_mesh_connect()` is what every algorithm
calls first, before any AllReduce data ever moves. Every rank independently
computes the exact same list of rank *pairs* -- `(0,1), (0,2), ..., (0,N-1),
(1,2), ..., (N-2,N-1)` -- from `num_ranks` alone (no communication needed to
agree on this list; it's a pure function of N). For a pair `(a,b)`, rank `a`
always plays "server" and rank `b` always plays "client" for that one
connection.

This happens in two phases:
1. **Every rank starts every listener it will ever need, for every pair
   where it's the server**, before connecting to anything. This is what
   makes "start the N processes in any order, whenever you get around to
   it" safe -- there's no window where a client could try to reach a
   server that hasn't called `rdma_listen()` yet.
2. **Every rank then walks the same pair list a second time**, accepting on
   the pairs where it's the server and connecting on the pairs where it's
   the client. Since every rank does this in the identical order, there's
   never any ambiguity about which pair is "active" right now, without a
   single coordination message being exchanged about it.

The result is a full mesh: every rank ends up with one dedicated RDMA
connection (and its own protection domain, queue pair, and registered
buffers) to every OTHER rank, and every algorithm below loops over that
mesh exactly the way the original CUDA version loops over GPUs on an
NVLink domain.

### `Bidirectional_DoubleBuffering/` stays 2-rank

This is the one exception, matching the original CUDA version's own scope
decision: the paper's own Fig. 4 is inherently a 2-rank teaching example
(bidirectional traffic between exactly two participants), so this program
requires `num_ranks` to be exactly 2. Use any of the other six programs for
an N>2 demonstration.

## The core design idea, and how it differs from the CUDA version

Every technique in the CUDA version relies on a GPU/NVLink-specific
hardware guarantee: an aligned 128-bit store either lands whole or not at
all, as observed by any reader. InfiniBand has no equivalent "atomic 16-
byte store." What it DOES guarantee, on a Reliable Connection (RC) queue
pair, is **ordering**: multiple operations posted on the same QP are
applied to remote memory in the exact order they were posted. Every
barrier-free technique in this folder is built on that guarantee instead:
post an RDMA WRITE of the data, then post a second, small operation (a
trailing flag WRITE, or -- for LL128_Atomic -- a genuine hardware atomic
increment) on the SAME queue pair right after it. Because the second
operation is guaranteed to land only after the first, a receiver busy-
polling ONLY that second, small value can safely assume the data write
has already completed -- no separate barrier call, exactly preserving the
paper's actual point, just built on InfiniBand's real ordering guarantee
instead of NVLink's real atomicity guarantee. Each folder's own README
explains its specific version of this in more depth.

With N ranks, every connection has its OWN protection domain, so any
buffer used as the LOCAL (send) side of a WRITE on a given connection has
to be registered against that specific connection -- each program keeps a
small per-peer registered copy of whatever it's sending, one per outbound
connection, rather than a single shared registration.

## Two honesty notes, please read before your professor does

**Two-shot's traffic reduction is now visible.** With the N-node
generalization, Table I's two-shot ReduceScatter+AllGather programs
(`Twoshot_LL/`, `Twoshot_Sentinel/`, `LL128_Atomic/`) now actually show the
communication-volume reduction they're chosen for: at N=2 one-shot and
two-shot move identical total traffic (`2(N-1)M` = `4(N-1)M/N` when N=2),
but at N=3 or N=4 two-shot moves less. Run any one-shot program (`LL/`,
`Sentinel/`, `Baseline_Barrier/`) and any two-shot program at the same
`numFloats` and same N to see this directly.

**LL128 Atomic's hardware gap.** The CUDA version's LL128 Atomic gets two
things from one NVLink primitive: multiple GPUs' floating-point
contributions are summed directly by the memory fabric, AND a completion
counter rides along in the same operation. InfiniBand's native RDMA atomic
(`IBV_WR_ATOMIC_FETCH_AND_ADD`) is a real hardware atomic -- but it only
operates on a 64-bit signed integer, never a float; there is no remote
floating-point atomic add on InfiniBand. This version's `LL128_Atomic/`
keeps the *completion-signal* half of the idea (a genuine atomic increment
replaces an explicit flag, now accumulated from all N-1 peers) but sums
the actual float data locally, once the atomic-driven counter says it's
ready -- see that folder's README for the full reasoning. Don't present
this as "hardware-summed floats over RDMA" -- that specific part isn't
something real InfiniBand can do.

## Requirements

- Three or four (or however many your professor wants) of your cluster's
  compute nodes, each with a working InfiniBand interface (`ibv_devinfo`
  should show `mlx4_0`, `PORT_ACTIVE`, matching what `check_node.sh`
  already found).
- Each node's **IPoIB address** (NOT its regular hostname/Ethernet IP --
  your earlier `rdma_bench/client.c` hardcodes `10.1.2.1` for exactly this
  reason). Find yours with `ip addr show ib0` (or `ibstat`) on each node.
- `gcc` and the `libibverbs-dev` / `librdmacm-dev` headers (already
  present on this cluster, since your earlier RDMA verbs work compiled
  there).

## Getting this onto several compute nodes at once

You'll need one terminal (or SSH session) per rank, open around the same
time -- every process blocks in `rdma_mesh_connect()` until all N ranks
have shown up. If you don't yet know whether you can `ssh` directly from
`master` to a compute node (e.g. `ssh compute01`), try it; if that's not
allowed, an N-node interactive PBS job (`qsub -I -l select=N:...`) is the
other route -- you'd get N shells inside that one job, one per allocated
node, and run one rank in each. Either way, no MPI job launcher is
involved: PBS here would only be handing you N shells to type these
commands into by hand, exactly like N separate `ssh` sessions would.
`submit_template.pbs` back in `paper_algorithms/` can be adapted for a
non-interactive version of this once you confirm which access pattern this
cluster expects. Let me know which works and I'll tighten up the exact
commands.
