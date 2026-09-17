/* mpi-latency.c
 *
 * Classic MPI ping-pong latency benchmark - two ranks only. Rank 0 sends
 * a message of a given size to rank 1 and blocks waiting for it to be
 * echoed straight back; the round trip is timed and divided by 2 to get
 * a one-way latency estimate, exactly like ib_write_lat/OSU's
 * osu_latency. Run it under
 * different `mpirun -genv I_MPI_FABRICS=...` (Intel MPI) or plain
 * `mpirun --mca btl ...` (OpenMPI) settings to compare transports - the
 * benchmark code itself never changes, only which transport MPI uses
 * underneath it.
 *
 * Build:  mpicc -O2 -Wall -Wextra -o mpi-latency mpi-latency.c
 * Run:    mpirun -np 2 ./mpi-latency
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>

#define MAX_SIZE (4 * 1024 * 1024)

static int latency_iters(int size) {
    if (size <= 8192) return 1000;
    if (size <= 131072) return 300;
    return 50;
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

    printf("Latency benchmark between src_rank=%d and dst_rank=%d\n", src_rank, dst_rank);

    int sizes[] = {
        1, 2, 4, 8, 16, 32, 64, 128, 256, 512,
        1024, 2048, 4096, 8192, 16384, 32768, 65536,
        131072, 262144, 524288, 1048576, 2097152, 4194304
    };
    int n_sizes = (int)(sizeof(sizes) / sizeof(sizes[0]));

    char *buf = malloc(MAX_SIZE);
    if (!buf) { perror("malloc"); MPI_Abort(MPI_COMM_WORLD, 1); }
    memset(buf, 'A', MAX_SIZE);

    if (rank == src_rank) {
        printf("%10s   %s %s\n", "Size", "Latency (usecs)", "usecs");
    }

    for (int i = 0; i < n_sizes; i++) {
        int size = sizes[i];
        int iters = latency_iters(size);
        int warmup = 5;

        MPI_Barrier(MPI_COMM_WORLD);

        if (rank == src_rank) {
            /* Untimed warmup round trips first. */
            for (int w = 0; w < warmup; w++) {
                MPI_Send(buf, size, MPI_CHAR, dst_rank, 0, MPI_COMM_WORLD);
                MPI_Recv(buf, size, MPI_CHAR, dst_rank, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            }

            double t0 = MPI_Wtime();
            for (int it = 0; it < iters; it++) {
                MPI_Send(buf, size, MPI_CHAR, dst_rank, 0, MPI_COMM_WORLD);
                MPI_Recv(buf, size, MPI_CHAR, dst_rank, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            }
            double t1 = MPI_Wtime();

            double rtt_avg_usec = (t1 - t0) * 1e6 / iters;
            printf("%10d %15.2f\n", size, rtt_avg_usec / 2.0);
            fflush(stdout);
        } else if (rank == dst_rank) {
            /* Plain echo - dst does all the same round trips (warmup +
             * timed), it just never measures anything itself. */
            for (int w = 0; w < warmup; w++) {
                MPI_Recv(buf, size, MPI_CHAR, src_rank, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                MPI_Send(buf, size, MPI_CHAR, src_rank, 0, MPI_COMM_WORLD);
            }
            for (int it = 0; it < iters; it++) {
                MPI_Recv(buf, size, MPI_CHAR, src_rank, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                MPI_Send(buf, size, MPI_CHAR, src_rank, 0, MPI_COMM_WORLD);
            }
        }
    }

    free(buf);
    MPI_Finalize();
    return 0;
}
