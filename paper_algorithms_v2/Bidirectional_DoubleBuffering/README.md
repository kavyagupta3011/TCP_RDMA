# Bidirectional Communication & Double Buffering -- RDMA verbs version

Paper reference: Section IV.B.3, Fig. 4. See `../README.md` first for the
shared design idea.

## The idea, technically

This was already scoped to exactly 2 ranks even in the CUDA version --
Fig. 4's own teaching example is 2 ranks -- so this is the most direct
port of the six. The CUDA version's single persistent GPU kernel with an
on-device loop over chunks becomes an ordinary CPU `for` loop over chunks:
each iteration does the data-then-flag ordered-write trick from `LL/`
(scoped to one chunk instead of the whole vector), into one of two
ping-ponged buffers. Because each side only reuses buffer 0 again two
chunks later, and by then it has necessarily already both sent its next
chunk and consumed the peer's previous one, no barrier or reset is ever
needed between chunks -- "each receive serves as an implicit permission
for the next send" (paper, Section IV.B.3), now expressed as sequential C
instead of a GPU kernel loop.

## The idea, simply

See `../LL/README.md`'s letter-and-card picture, repeated once per chunk,
alternating between two doorslots so a chunk two turns from now never has
to wait for confirmation that the previous chunk through THAT slot was
picked up -- by the time it's that slot's turn again, it always has been.

## Stays pairwise even in the N-node project

Every other folder in this project now generalizes to N ranks (see
`../README.md`); this one deliberately doesn't, because Fig. 4's own
teaching example is inherently 2-rank -- bidirectional traffic between
exactly two participants is the whole point of the picture. This program's
command line still accepts the same `<my_rank> <num_ranks> ...` shape as
every other program for a consistent launch story, but requires
`num_ranks` to be exactly 2.

## Build & run

```
make
./bidir_doublebuffer_rdma <my_rank> 2 <base_port> <ip0> <ip1> [numFloats] [numChunks] [iters]
```

No MPI -- launch once per rank by hand:
```
node A: ./bidir_doublebuffer_rdma 0 2 20000 10.1.2.1 10.1.2.2 262144 8 100
node B: ./bidir_doublebuffer_rdma 1 2 20000 10.1.2.1 10.1.2.2 262144 8 100
```
