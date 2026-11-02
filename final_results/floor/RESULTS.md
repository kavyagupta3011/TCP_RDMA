# Network floor

The fastest the network itself can be: one plain RDMA write between the first two machines of
the job, measured with the card maker's tools `ib_write_lat` (one write, one way) and
`ib_write_bw` (bandwidth). No AllReduce can be faster. Log: `logs/network_floor.txt`.

| Message | typical | 99% | 99.9% | bandwidth |
|---|---|---|---|---|
| 2 B | 1.01 µs | 1.10 µs | 5.27 µs | 11 MB/s |
| 4 B | 1.01 µs | 1.03 µs | 2.86 µs | 22 MB/s |
| 8 B | 1.01 µs | 1.03 µs | 2.60 µs | 43 MB/s |
| 16 B | 1.01 µs | 1.04 µs | 3.20 µs | 86 MB/s |
| 32 B | 1.07 µs | 1.10 µs | 3.13 µs | 174 MB/s |
| 64 B | 1.08 µs | 1.11 µs | 3.94 µs | 333 MB/s |
| 128 B | 1.14 µs | 1.18 µs | 2.29 µs | 684 MB/s |
| 256 B | 1.59 µs | 1.61 µs | 3.71 µs | 1,369 MB/s |
| 512 B | 1.74 µs | 1.77 µs | 3.76 µs | 2,734 MB/s |
| 1 KiB | 2.02 µs | 2.05 µs | 4.41 µs | 3,649 MB/s |
| 2 KiB | 2.58 µs | 2.71 µs | 3.75 µs | 3,733 MB/s |
| 4 KiB | 3.68 µs | 3.84 µs | 4.93 µs | 3,772 MB/s |
| 8 KiB | 4.73 µs | 4.86 µs | 5.54 µs | 3,785 MB/s |
| 16 KiB | 6.82 µs | 6.96 µs | 8.65 µs | 3,789 MB/s |
| 32 KiB | 10.95 µs | 11.13 µs | 11.15 µs | 3,792 MB/s |
| 64 KiB | 19.29 µs | 19.48 µs | 20.17 µs | 3,793 MB/s |
| 128 KiB | 35.77 µs | 35.95 µs | 39.64 µs | 3,793 MB/s |
| 256 KiB | 68.74 µs | 68.90 µs | 69.46 µs | 3,793 MB/s |
| 512 KiB | 134.62 µs | 134.79 µs | 135.51 µs | 3,794 MB/s |
| 1 MiB | 267.60 µs | 267.88 µs | 268.36 µs | 3,794 MB/s |
| 2 MiB | 531.17 µs | 531.52 µs | 531.62 µs | 3,794 MB/s |
| 4 MiB | 1058.49 µs | 1058.92 µs | 1062.15 µs | 3,794 MB/s |
| 8 MiB | 2117.87 µs | 2119.51 µs | 2121.79 µs | 3,794 MB/s |

A round trip (a write and its acknowledgement) is about twice the one-way time.
32 MB is above the tool's 8 MiB limit: 4 x the 8 MiB time.

