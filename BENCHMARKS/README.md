# BENCHMARKS

Your professor showed you MPI bandwidth/latency numbers (`mpi-bw.c`/`mpi-latency.c`) across `tcp`, `ofa` (InfiniBand/RDMA), and `shm` fabrics, and asked you to figure out how to get comparable numbers yourself — without his code. This folder has two independent ways of doing that:

- `raw_socket_rdma/` — built directly on the TCP and RDMA code you already wrote and showed him. No MPI at all. Same idea (measure latency and bandwidth across a size sweep, compare TCP vs RDMA), same size sweep (1 byte to 4MB) as his table, but using your own sockets/verbs code.
- `mpi_bench/` — your own `mpi-bw.c`/`mpi-latency.c`, written from scratch in the same OSU/ping-pong style his almost certainly was, matching his exact output format (`Size` / `Bandwidth MB/s`, `Size` / `Latency (usecs)`) so your numbers drop straight next to his for comparison.

All of this was compiled and actually run in this sandbox before being sent to you — the TCP and MPI versions ran a full local test with sane, physically-reasonable output; the RDMA version compiled clean with `-Wall -Wextra -Wpedantic` (zero warnings) but could only be compile-verified here, same limitation as your original RDMA folder — you'll need to run it on the InfiniBand node to get real numbers.

## raw_socket_rdma/tcp_bench

Plain sockets, no MPI. `client.c` drives a size sweep from 1 byte to 4MB, measuring ping-pong latency (round trip / 2) and streaming bandwidth for each size; `server.c` just echoes for the latency phase and drains + acks for the bandwidth phase.

```
gcc -O2 -Wall -Wextra -o server server.c
gcc -O2 -Wall -Wextra -o client client.c
./server &
./client        # edit SERVER_IP in client.c first if running across two machines
```

## raw_socket_rdma/rdma_bench

Same size sweep, but using RDMA verbs directly instead of sockets: `IBV_WR_RDMA_WRITE_WITH_IMM` ping-pong for latency (one-sided WRITE + immediate value, no separate SEND needed — exactly `3_write_with_immediate` from your RDMA folder), and windowed one-sided `IBV_WR_RDMA_WRITE` bursts for bandwidth (mostly unsignaled, reaping one completion every 64 writes, relying on RC's per-QP ordering guarantee — same trick your bandwidth phase in the RDMA verbs world always uses).

```
gcc -O2 -Wall -Wextra -Wpedantic -o server server.c -lrdmacm -libverbs
gcc -O2 -Wall -Wextra -Wpedantic -o client client.c -lrdmacm -libverbs
./server &
./client        # edit SERVER_IP in client.c to the server node's real IB IP before running across two nodes
```

Run this on the InfiniBand node your professor gave you access to — that's the only way to get a real `ofa`-style number out of it. Loopback on the same node will still run correctly (RDMA over loopback works on most modern setups) but won't show you the real network's numbers.

## mpi_bench

Two classic MPI microbenchmarks, `mpi-latency.c` and `mpi-bw.c`, deliberately written in the same style/format as the numbers your professor showed you.

**Compiling:** your cluster's `mpicc` defaults to an old C standard that rejects declaring a loop variable inside `for(...)` — exactly the error you hit with his `mpi-bw.c`. Either add `-std=gnu99`, or do what you already found works and use `mpicxx`:

```
mpicc -O2 -std=gnu99 -Wall -Wextra -o mpi-latency mpi-latency.c
mpicc -O2 -std=gnu99 -Wall -Wextra -o mpi-bw mpi-bw.c
```

**Running, matching his exact commands (Intel MPI):**

```
module load intel-2018
mpirun -genv I_MPI_DEBUG=5 -genv I_MPI_FABRICS=tcp -np 2 ./mpi-latency
mpirun -genv I_MPI_DEBUG=5 -genv I_MPI_FABRICS=ofa -np 2 ./mpi-latency
mpirun -genv I_MPI_DEBUG=5 -genv I_MPI_FABRICS=shm -np 2 ./mpi-latency

mpirun -genv I_MPI_DEBUG=5 -genv I_MPI_FABRICS=tcp -np 2 ./mpi-bw
mpirun -genv I_MPI_DEBUG=5 -genv I_MPI_FABRICS=ofa -np 2 ./mpi-bw
mpirun -genv I_MPI_DEBUG=5 -genv I_MPI_FABRICS=shm -np 2 ./mpi-bw
```

Same 23-size sweep (1 byte → 4MB) as his table, so you can put your `tcp`/`ofa`/`shm` rows right next to his and they should land in the same ballpark — small-message `ofa` latency several times lower than `tcp`, `ofa` bandwidth noticeably higher than `tcp` at large sizes, `shm` fastest of all since it never leaves the node.

## What "comparable" means here

You're not trying to match his exact numbers to the decimal — cluster load, exact node pairing, and driver versions all shift the absolute values. What matters, and what these three approaches (raw sockets, raw RDMA verbs, MPI) all independently demonstrate, is the *shape*: RDMA/`ofa` beating plain TCP by roughly the same multiple at both the raw-verbs level and the MPI level, because MPI's `ofa` fabric is itself just calling the same `libibverbs`/`rdma_cm` functions you used directly in `rdma_bench/`. Showing that the gap shows up consistently across three independent implementations is a stronger answer than matching his numbers exactly.
