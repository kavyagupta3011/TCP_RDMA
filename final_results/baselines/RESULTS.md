# Results: ring (baselines)

**ring**: standard ring AllReduce (reduce-scatter + allgather), plain SEND/RECV.

Machines: rank 0 = compute02, rank 1 = compute03, rank 2 = compute04. PBS jobs: 30219.master.local, 30220.master.local.

A value is the time of one AllReduce (median over the timed rounds, averaged over the ranks that
finished correctly). Cells with two numbers are **pass 1 / pass 2**: the same experiment run twice,
the second time with the versions in the opposite order. If a rank crashed, hung (stopped by the
timeout) or got a wrong result, the cell says so. 1000 timed rounds for sizes up to 32 KB, 100
for 256 KB and above.

## 1. Median, 3 ranks (pass 1 / pass 2)

| Algorithm | 32 B | 64 B | 128 B | 32 KB | 256 KB | 1 MB | 32 MB |
|---|---|---|---|---|---|---|---|
| Ring (standard baseline) | 8.15 µs / 8.09 µs | 8.63 µs / 8.68 µs | 8.41 µs / 9.00 µs | 37.82 µs / 38.22 µs | 199.6 µs / 199.3 µs | 804.5 µs / 806.0 µs | 29.29 ms / 29.29 ms |

## 2. p99, 3 ranks (average of the passes)

99% of the rounds were faster than this.

| Algorithm | 32 B | 64 B | 128 B | 32 KB | 256 KB | 1 MB | 32 MB |
|---|---|---|---|---|---|---|---|
| Ring (standard baseline) | 9.48 µs | 9.38 µs | 10.22 µs | 42.61 µs | 230.0 µs | 855.4 µs | 139.32 ms |

## 3. 2 ranks vs 3 ranks

|  | 32 B | 32 KB | 32 MB |
|---|---|---|---|
| Ring (standard baseline), 2 ranks | 4.68 µs | 31.70 µs | 26.04 ms |
| Ring (standard baseline), 3 ranks | 8.12 µs | 38.02 µs | 29.29 ms |

## 4. Correctness and errors

| Log | PASSED lines | FAILED lines | ranks ok | crashed | hung | wrong result | skipped |
|---|---|---|---|---|---|---|---|
| `pass1_1_3ranks_32B_64B_128B_32KB.txt` | 24 | 0 | 12 | 0 | 0 | 0 | 0 |
| `pass1_2_3ranks_256KB.txt` | 6 | 0 | 3 | 0 | 0 | 0 | 0 |
| `pass1_3_3ranks_1MB.txt` | 6 | 0 | 3 | 0 | 0 | 0 | 0 |
| `pass1_4_3ranks_32MB.txt` | 6 | 0 | 3 | 0 | 0 | 0 | 0 |
| `pass1_5_2ranks.txt` | 12 | 0 | 6 | 0 | 0 | 0 | 0 |
| `pass2_1_3ranks_32B_64B_128B_32KB.txt` | 24 | 0 | 12 | 0 | 0 | 0 | 0 |
| `pass2_2_3ranks_256KB.txt` | 6 | 0 | 3 | 0 | 0 | 0 | 0 |
| `pass2_3_3ranks_1MB.txt` | 6 | 0 | 3 | 0 | 0 | 0 | 0 |
| `pass2_4_3ranks_32MB.txt` | 6 | 0 | 3 | 0 | 0 | 0 | 0 |
| `pass2_5_2ranks.txt` | 12 | 0 | 6 | 0 | 0 | 0 | 0 |

## 5. Every run, per rank

Times in µs. `-` = not printed by this version or the rank did not finish.

| pass | Program | ranks | size | rank | status | mean | min | median | p99 | max | correctness | exit |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| pass1 | Ring (standard baseline) | 3 | 32 B | 0 | ok | 8.21 | 7.86 | 8.15 | 9.61 | 23.12 | PASSED/PASSED | 0 |
| pass1 | Ring (standard baseline) | 3 | 32 B | 1 | ok | 8.21 | 7.70 | 8.14 | 9.34 | 24.57 | PASSED/PASSED | 0 |
| pass1 | Ring (standard baseline) | 3 | 32 B | 2 | ok | 8.21 | 7.87 | 8.16 | 9.18 | 23.66 | PASSED/PASSED | 0 |
| pass1 | Ring (standard baseline) | 3 | 64 B | 0 | ok | 8.65 | 7.94 | 8.63 | 9.02 | 19.13 | PASSED/PASSED | 0 |
| pass1 | Ring (standard baseline) | 3 | 64 B | 1 | ok | 8.67 | 7.90 | 8.63 | 9.23 | 24.97 | PASSED/PASSED | 0 |
| pass1 | Ring (standard baseline) | 3 | 64 B | 2 | ok | 8.66 | 7.09 | 8.63 | 9.27 | 26.34 | PASSED/PASSED | 0 |
| pass1 | Ring (standard baseline) | 3 | 128 B | 0 | ok | 8.39 | 7.65 | 8.44 | 10.07 | 15.66 | PASSED/PASSED | 0 |
| pass1 | Ring (standard baseline) | 3 | 128 B | 1 | ok | 8.39 | 7.08 | 8.39 | 9.95 | 15.95 | PASSED/PASSED | 0 |
| pass1 | Ring (standard baseline) | 3 | 128 B | 2 | ok | 8.39 | 7.70 | 8.40 | 9.84 | 15.97 | PASSED/PASSED | 0 |
| pass1 | Ring (standard baseline) | 3 | 32 KB | 0 | ok | 38.09 | 35.05 | 38.43 | 41.03 | 50.07 | PASSED/PASSED | 0 |
| pass1 | Ring (standard baseline) | 3 | 32 KB | 1 | ok | 38.10 | 35.75 | 37.96 | 43.16 | 78.11 | PASSED/PASSED | 0 |
| pass1 | Ring (standard baseline) | 3 | 32 KB | 2 | ok | 38.13 | 35.53 | 37.06 | 43.24 | 83.42 | PASSED/PASSED | 0 |
| pass1 | Ring (standard baseline) | 3 | 256 KB | 0 | ok | 206.18 | 195.02 | 200.14 | 224.62 | 225.72 | PASSED/PASSED | 0 |
| pass1 | Ring (standard baseline) | 3 | 256 KB | 1 | ok | 205.88 | 189.14 | 197.90 | 232.23 | 232.79 | PASSED/PASSED | 0 |
| pass1 | Ring (standard baseline) | 3 | 256 KB | 2 | ok | 205.83 | 188.39 | 200.67 | 231.76 | 235.25 | PASSED/PASSED | 0 |
| pass1 | Ring (standard baseline) | 3 | 1 MB | 0 | ok | 802.81 | 773.57 | 804.80 | 848.32 | 876.08 | PASSED/PASSED | 0 |
| pass1 | Ring (standard baseline) | 3 | 1 MB | 1 | ok | 803.38 | 767.49 | 803.98 | 856.42 | 879.22 | PASSED/PASSED | 0 |
| pass1 | Ring (standard baseline) | 3 | 1 MB | 2 | ok | 803.03 | 768.69 | 804.70 | 845.92 | 888.66 | PASSED/PASSED | 0 |
| pass1 | Ring (standard baseline) | 3 | 32 MB | 0 | ok | 32,463.71 | 26,800.15 | 29,352.93 | 159,371.43 | 176,460.56 | PASSED/PASSED | 0 |
| pass1 | Ring (standard baseline) | 3 | 32 MB | 1 | ok | 32,444.54 | 27,350.66 | 29,314.54 | 132,025.87 | 203,821.64 | PASSED/PASSED | 0 |
| pass1 | Ring (standard baseline) | 3 | 32 MB | 2 | ok | 32,500.22 | 26,673.68 | 29,206.83 | 129,130.69 | 203,511.67 | PASSED/PASSED | 0 |
| pass1 | Ring (standard baseline) | 2 | 32 B | 0 | ok | 4.64 | 3.42 | 4.50 | 5.45 | 20.64 | PASSED/PASSED | 0 |
| pass1 | Ring (standard baseline) | 2 | 32 B | 1 | ok | 4.64 | 4.15 | 4.51 | 5.48 | 19.89 | PASSED/PASSED | 0 |
| pass1 | Ring (standard baseline) | 2 | 32 KB | 0 | ok | 31.61 | 26.63 | 31.72 | 32.68 | 42.35 | PASSED/PASSED | 0 |
| pass1 | Ring (standard baseline) | 2 | 32 KB | 1 | ok | 31.61 | 27.26 | 31.71 | 32.82 | 41.72 | PASSED/PASSED | 0 |
| pass1 | Ring (standard baseline) | 2 | 32 MB | 0 | ok | 27,492.28 | 24,099.72 | 26,067.92 | 117,349.23 | 134,867.92 | PASSED/PASSED | 0 |
| pass1 | Ring (standard baseline) | 2 | 32 MB | 1 | ok | 27,530.16 | 24,108.44 | 26,034.50 | 48,696.86 | 210,794.96 | PASSED/PASSED | 0 |
| pass2 | Ring (standard baseline) | 3 | 32 B | 0 | ok | 8.11 | 7.65 | 8.09 | 9.61 | 13.32 | PASSED/PASSED | 0 |
| pass2 | Ring (standard baseline) | 3 | 32 B | 1 | ok | 8.11 | 7.68 | 8.08 | 9.64 | 12.64 | PASSED/PASSED | 0 |
| pass2 | Ring (standard baseline) | 3 | 32 B | 2 | ok | 8.11 | 7.67 | 8.09 | 9.49 | 13.32 | PASSED/PASSED | 0 |
| pass2 | Ring (standard baseline) | 3 | 64 B | 0 | ok | 8.71 | 7.88 | 8.68 | 9.53 | 21.07 | PASSED/PASSED | 0 |
| pass2 | Ring (standard baseline) | 3 | 64 B | 1 | ok | 8.72 | 7.10 | 8.68 | 9.61 | 22.98 | PASSED/PASSED | 0 |
| pass2 | Ring (standard baseline) | 3 | 64 B | 2 | ok | 8.72 | 7.89 | 8.68 | 9.64 | 22.71 | PASSED/PASSED | 0 |
| pass2 | Ring (standard baseline) | 3 | 128 B | 0 | ok | 9.00 | 7.85 | 8.99 | 10.61 | 26.49 | PASSED/PASSED | 0 |
| pass2 | Ring (standard baseline) | 3 | 128 B | 1 | ok | 9.00 | 8.57 | 9.00 | 10.38 | 25.94 | PASSED/PASSED | 0 |
| pass2 | Ring (standard baseline) | 3 | 128 B | 2 | ok | 9.00 | 7.17 | 9.00 | 10.44 | 25.82 | PASSED/PASSED | 0 |
| pass2 | Ring (standard baseline) | 3 | 32 KB | 0 | ok | 38.39 | 35.09 | 38.94 | 42.41 | 50.09 | PASSED/PASSED | 0 |
| pass2 | Ring (standard baseline) | 3 | 32 KB | 1 | ok | 38.38 | 35.70 | 38.41 | 42.80 | 49.57 | PASSED/PASSED | 0 |
| pass2 | Ring (standard baseline) | 3 | 32 KB | 2 | ok | 38.37 | 34.33 | 37.31 | 42.98 | 50.76 | PASSED/PASSED | 0 |
| pass2 | Ring (standard baseline) | 3 | 256 KB | 0 | ok | 205.99 | 192.53 | 199.87 | 226.63 | 230.64 | PASSED/PASSED | 0 |
| pass2 | Ring (standard baseline) | 3 | 256 KB | 1 | ok | 205.83 | 186.34 | 197.79 | 229.97 | 230.91 | PASSED/PASSED | 0 |
| pass2 | Ring (standard baseline) | 3 | 256 KB | 2 | ok | 206.31 | 183.28 | 200.15 | 234.75 | 243.74 | PASSED/PASSED | 0 |
| pass2 | Ring (standard baseline) | 3 | 1 MB | 0 | ok | 807.17 | 780.10 | 804.75 | 863.35 | 883.29 | PASSED/PASSED | 0 |
| pass2 | Ring (standard baseline) | 3 | 1 MB | 1 | ok | 807.80 | 770.23 | 807.52 | 855.40 | 888.57 | PASSED/PASSED | 0 |
| pass2 | Ring (standard baseline) | 3 | 1 MB | 2 | ok | 807.32 | 771.16 | 805.60 | 863.01 | 880.94 | PASSED/PASSED | 0 |
| pass2 | Ring (standard baseline) | 3 | 32 MB | 0 | ok | 32,399.14 | 26,783.31 | 29,247.87 | 162,728.35 | 166,894.36 | PASSED/PASSED | 0 |
| pass2 | Ring (standard baseline) | 3 | 32 MB | 1 | ok | 32,385.25 | 27,267.48 | 29,200.79 | 130,087.63 | 199,539.22 | PASSED/PASSED | 0 |
| pass2 | Ring (standard baseline) | 3 | 32 MB | 2 | ok | 32,447.09 | 26,686.53 | 29,429.15 | 122,547.69 | 201,060.39 | PASSED/PASSED | 0 |
| pass2 | Ring (standard baseline) | 2 | 32 B | 0 | ok | 4.87 | 4.22 | 4.86 | 5.04 | 20.09 | PASSED/PASSED | 0 |
| pass2 | Ring (standard baseline) | 2 | 32 B | 1 | ok | 4.87 | 4.22 | 4.86 | 5.06 | 21.14 | PASSED/PASSED | 0 |
| pass2 | Ring (standard baseline) | 2 | 32 KB | 0 | ok | 31.41 | 26.88 | 31.69 | 32.62 | 35.21 | PASSED/PASSED | 0 |
| pass2 | Ring (standard baseline) | 2 | 32 KB | 1 | ok | 31.42 | 27.19 | 31.69 | 32.60 | 35.70 | PASSED/PASSED | 0 |
| pass2 | Ring (standard baseline) | 2 | 32 MB | 0 | ok | 27,471.07 | 24,120.21 | 26,046.96 | 111,544.88 | 140,948.45 | PASSED/PASSED | 0 |
| pass2 | Ring (standard baseline) | 2 | 32 MB | 1 | ok | 27,509.22 | 24,114.15 | 25,995.29 | 57,151.78 | 202,683.25 | PASSED/PASSED | 0 |

Logs and what each ran:

- `pass1_1_3ranks_32B_64B_128B_32KB.txt`: 3 ranks, 32 B / 64 B / 128 B / 32 KB, 1000 rounds
- `pass1_2_3ranks_256KB.txt`: 3 ranks, 256 KB, 100 rounds
- `pass1_3_3ranks_1MB.txt`: 3 ranks, 1 MB, 100 rounds
- `pass1_4_3ranks_32MB.txt`: 3 ranks, 32 MB, 100 rounds
- `pass1_5_2ranks.txt`: 2 ranks, Baseline and LL (ring for the baselines), 32 B / 32 KB (1000 rounds) and 32 MB (100 rounds)
- `pass2_1_3ranks_32B_64B_128B_32KB.txt`: 3 ranks, 32 B / 64 B / 128 B / 32 KB, 1000 rounds
- `pass2_2_3ranks_256KB.txt`: 3 ranks, 256 KB, 100 rounds
- `pass2_3_3ranks_1MB.txt`: 3 ranks, 1 MB, 100 rounds
- `pass2_4_3ranks_32MB.txt`: 3 ranks, 32 MB, 100 rounds
- `pass2_5_2ranks.txt`: 2 ranks, Baseline and LL (ring for the baselines), 32 B / 32 KB (1000 rounds) and 32 MB (100 rounds)

