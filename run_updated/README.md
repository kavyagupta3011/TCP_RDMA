# run_updated: run all three versions, results in results_updated/

| Version | Code folder | On the cluster |
|---|---|---|
| v1 | `paper_algorithms/` | `~/IMT2023016/run_updated/code/v1` |
| v2 | `paper_algorithms_v2/` | `~/IMT2023016/run_updated/code/v2` |
| v3 | `paper_algorithms_v3/` | `~/IMT2023016/run_updated/code/v3` |

One PBS job runs the same experiments for all three versions on compute02, 03, 04. Within each
experiment the versions run one after the other (v1, v2, v3), so they see the same conditions.

| # | Experiment | Versions | Log name |
|---|---|---|---|
| 0 | network floor (`ib_write_lat`, `ib_write_bw`) | - | `floor/logs/network_floor.txt` |
| 1 | 3 ranks, all algorithms, 32 B / 64 B / 128 B / 32 KB, 1000 rounds | v1 v2 v3 | `1_3ranks_32B_64B_128B_32KB.txt` |
| 2 | 3 ranks, all algorithms, 256 KB, 100 rounds (the size of the first results) | v1 v2 v3 | `2_3ranks_256KB.txt` |
| 3 | 3 ranks, all algorithms, 32 MB, 100 rounds | v1 v2 v3 | `3_3ranks_32MB.txt` |
| 4 | 2 ranks, Baseline and LL, 32 B to 32 KB and 32 MB | v1 v2 v3 | `4_2ranks_baseline_ll.txt` |
| 5 | RDMA atomics probe | v2 v3 | `5_atomic_probe.txt` |
| 6 | negative test (LL without the wait; must FAIL) | v3 | `6_negative_test.txt` |

Every run has a timeout, so a version-1 program that hangs is stopped (`exit=124`) and the job
goes on. Expect some crashes and hangs in v1; they are part of its result.

## Files

| File | What it does |
|---|---|
| `setup.sh` | once: builds the three versions, creates `results_updated/` (an older one is renamed) |
| `run_everything.pbs` | the job: all experiments, one log per experiment and version |
| `run_allreduce.sh` | starts one program on all nodes (used by the job) |
| `run_floor.sh` | the network floor (used by the job) |
| `make_results.py` | reads the logs and writes the results files |

## Commands

Everything below is already on the cluster in `~/IMT2023016/run_updated/`.

**1. Log in and set up** (about a minute):

```bash
ssh student4@172.16.201.12
cd ~/IMT2023016/run_updated
sed -i 's/\r$//' *.sh *.pbs *.py
bash setup.sh
```

It should print `built ...` for 24 programs and `Setup OK. Next: qsub run_everything.pbs`.

**2. Submit the job:**

```bash
qsub run_everything.pbs
```

It prints the job number and its output file. The job takes roughly 30-60 minutes (longer if
many v1 programs hang until their timeout).

**3. Follow it:**

```bash
qstat                                   # your job: Q waiting, R running; gone = finished
ls ~/IMT2023016/results_updated/*/logs  # logs appear one by one
cat $(ls -t ~/.pbs_route/output/*.out | head -1)   # progress: one "done: ..." line per log
```

(Jobs 28350, 28532, 28928, 28929 are always listed; ignore them.)

**4. Build the results files** (after the job has finished):

```bash
cd ~/IMT2023016/run_updated
python3 make_results.py
```

**5. Copy them to the laptop.** `exit` the cluster, then in PowerShell:

```powershell
cd "C:\Users\Aishwarya Sharma\Desktop\College Work\Semester 7\Courses\NHPC PE\TCP_RDMA"
scp -r student4@172.16.201.12:IMT2023016/results_updated .
```

## What you get

```
TCP_RDMA/results_updated/
  README.md              start here: all three versions side by side
  floor/RESULTS_floor.md network floor table          floor/logs/  raw log
  v1/RESULTS_v1.md       every table for version 1    v1/logs/     raw logs
  v2/RESULTS_v2.md       every table for version 2    v2/logs/     raw logs
  v3/RESULTS_v3.md       every table for version 3    v3/logs/     raw logs
```

- `README.md`: what each version is, the network floor, the mean of every algorithm at every
  size for v1 / v2 / v3 side by side, the medians of v2 vs v3, and how many runs crashed or hung.
- `RESULTS_vN.md`: all algorithms at all sizes (median, p99, mean; v1 only has the mean), the
  barrier cost, 2 vs 3 ranks, atomics / negative test, every error message, and every rank of
  every run.

## After changing code on the laptop

Copy the changed version up again, then rerun from step 1 (`setup.sh` rebuilds everything and
moves the previous `results_updated` aside). For example, for v3, in PowerShell in `TCP_RDMA`:

```powershell
ssh student4@172.16.201.12 "rm -rf IMT2023016/run_updated/code/v3"
scp -r paper_algorithms_v3 student4@172.16.201.12:IMT2023016/run_updated/code/v3
```

(Use `paper_algorithms` -> `code/v1` and `paper_algorithms_v2` -> `code/v2` for the others.)
