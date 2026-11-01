# Network floor

The fastest the network itself can be: one plain RDMA write between the first two machines of
the job, measured with the card maker's tools `ib_write_lat` (one write, one way) and
`ib_write_bw` (bandwidth). No AllReduce can be faster. Log: `logs/network_floor.txt`.

| Message | typical | 99% | 99.9% | bandwidth |
|---|---|---|---|---|
| 2 B | 1.00 µs | 1.10 µs | 5.18 µs | 10 MB/s |
| 4 B | 1.00 µs | 1.03 µs | 3.22 µs | 21 MB/s |
| 8 B | 1.00 µs | 1.03 µs | 3.10 µs | 43 MB/s |
| 16 B | 1.00 µs | 1.03 µs | 3.06 µs | 86 MB/s |
| 32 B | 1.07 µs | 1.09 µs | 3.33 µs | 171 MB/s |
| 64 B | 1.07 µs | 1.10 µs | 3.86 µs | 331 MB/s |
| 128 B | 1.16 µs | 1.19 µs | 3.32 µs | 682 MB/s |
| 256 B | 1.60 µs | 1.62 µs | 3.51 µs | 1,366 MB/s |
| 512 B | 1.78 µs | 1.92 µs | 3.50 µs | 2,725 MB/s |
| 1 KiB | 2.05 µs | 2.22 µs | 4.44 µs | 3,651 MB/s |
| 2 KiB | 2.60 µs | 2.71 µs | 3.68 µs | 3,734 MB/s |
| 4 KiB | 3.69 µs | 3.87 µs | 3.95 µs | 3,773 MB/s |
| 8 KiB | 4.75 µs | 4.92 µs | 5.68 µs | 3,785 MB/s |
| 16 KiB | 6.81 µs | 6.98 µs | 8.22 µs | 3,790 MB/s |
| 32 KiB | 10.93 µs | 11.10 µs | 11.36 µs | 3,792 MB/s |
| 64 KiB | 19.28 µs | 19.45 µs | 19.86 µs | 3,793 MB/s |
| 128 KiB | 35.76 µs | 35.92 µs | 37.97 µs | 3,793 MB/s |
| 256 KiB | 68.73 µs | 68.86 µs | 69.09 µs | 3,794 MB/s |
| 512 KiB | 134.63 µs | 134.80 µs | 135.50 µs | 3,794 MB/s |
| 1 MiB | 267.59 µs | 267.88 µs | 268.10 µs | 3,794 MB/s |
| 2 MiB | 531.14 µs | 531.37 µs | 531.64 µs | 3,794 MB/s |
| 4 MiB | 1058.23 µs | 1058.44 µs | 1060.00 µs | 3,794 MB/s |
| 8 MiB | 2112.73 µs | 2112.90 µs | 2113.42 µs | 3,794 MB/s |

A round trip (a write and its acknowledgement) is about twice the one-way time.
32 MB is above the tool's 8 MiB limit: 4 x the 8 MiB time.

