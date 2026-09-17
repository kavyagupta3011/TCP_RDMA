/* mpi-bw.c
 *
 * Classic MPI streaming bandwidth benchmark - two ranks only. Rank 0
 * fires a burst of messages of a given size at rank 1 back-to-back using
 * non-blocking sends so it doesn't wait for each one individually, then
 * waits for all of them to actually complete before timing stops -
 * output format maintained the same
 *
 * Build:  mpicc -O2 -Wall -Wextra -o mpi-bw mpi-bw.c
 * Run:    mpirun -np 2 ./mpi-bw
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>

#define MAX_SIZE (4 * 1024 * 1024)

static int bandwidth_iters(int size) {
    long iters = (8L * 1024 * 1024) / size;
    if (iters < 5) iters = 5;
    if (iters > 200) iters = 200;   /* MPI_Request array below stays small and cheap */
    return (int)iters;
}

int main(int argc, char **argv) {
    int rank, nprocs;
    int src_rank = 0, dst_rank = 1;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);

    if (nprocs != 2) {
        if (rank == 0) fprintf(stderr, "this benchmark needs exactly 2 ranks (got %d)\n", nprocs);
        MPI_Finalize();
        return 1;
    }

    printf("Bandwidth benchmark between src_rank=%d and dst_rank=%d\n", src_rank, dst_rank);

    int sizes[] = {
        1, 2, 4, 8, 16, 32, 64, 128, 256, 512,
        1024, 2048, 4096, 8192, 16384, 32768, 65536,
        131072, 262144, 524288, 1048576, 2097152, 4194304
    };
    int n_sizes = (int)(sizeof(sizes) / sizeof(sizes[0]));

    char *buf = malloc(MAX_SIZE);
    if (!buf) { perror("malloc"); MPI_Abort(MPI_COMM_WORLD, 1); }
    memset(buf, 'A', MAX_SIZE);

    MPI_Request *reqs = malloc(sizeof(MPI_Request) * 200);
    if (!reqs) { perror("malloc"); MPI_Abort(MPI_COMM_WORLD, 1); }

    if (rank == src_rank) {
        printf("%10s %15s\n", "Size", "Bandwidth MB/s");
    }

    for (int i = 0; i < n_sizes; i++) {
        int size = sizes[i];
        int iters = bandwidth_iters(size);

        MPI_Barrier(MPI_COMM_WORLD);

        if (rank == src_rank) {
            double t0 = MPI_Wtime();
            /* Fire every send as non-blocking so we don't wait on each one
             * individually - MPI_Isend just queues it and returns. Only
             * after ALL of them are queued do we wait for the whole batch
             * to actually finish, via one MPI_Waitall. */
            for (int it = 0; it < iters; it++) {
                MPI_Isend(buf, size, MPI_CHAR, dst_rank, 0, MPI_COMM_WORLD, &reqs[it]);
            }
            MPI_Waitall(iters, reqs, MPI_STATUSES_IGNORE);

            /* One tiny blocking round trip so we know the destination has
             * actually finished draining everything before we stop the
             * clock - MPI_Isend completing on our side only means the
             * data left US, not that the peer has received it all yet. */
            char ack;
            MPI_Sendrecv(&ack, 1, MPI_CHAR, dst_rank, 1, &ack, 1, MPI_CHAR, dst_rank, 1,
                         MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            double t1 = MPI_Wtime();

            double total_bytes = (double)size * (double)iters;
            double mb_per_sec = (total_bytes / (t1 - t0)) / (1024.0 * 1024.0);
            printf("%10d %15.2f\n", size, mb_per_sec);
            fflush(stdout);
        } else if (rank == dst_rank) {
            for (int it = 0; it < iters; it++) {
                MPI_Irecv(buf, size, MPI_CHAR, src_rank, 0, MPI_COMM_WORLD, &reqs[it]);
            }
            MPI_Waitall(iters, reqs, MPI_STATUSES_IGNORE);

            char ack = 1;
            MPI_Sendrecv(&ack, 1, MPI_CHAR, src_rank, 1, &ack, 1, MPI_CHAR, src_rank, 1,
                         MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
    }

    free(reqs);
    free(buf);
    MPI_Finalize();
    return 0;
}
