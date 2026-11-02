# final_results

One place for the final runs of everything: the six versions of the RDMA AllReduce programs and
the standard ring baseline, all run with the same experiments on the same three machines
(compute02, compute03, compute04, one rank per machine).

Nothing here changes the code. The programs are run from where they already are, only the logs
and result tables end up in this folder.

## Folders

| Folder | What was run | Code it ran |
|---|---|---|
| `paper_algorithms` | v1 | `run_updated/code/v1` on the cluster (the first version) |
| `paper_algorithms_v2` | v2 | `run_updated/code/v2` |
| `paper_algorithms_v3` | v3 | `run_updated/code/v3` |
| `paper_algorithms_v4` | v4 | `paper_algorithms_v4` |
| `paper_algorithms_v5` | v5 | `paper_algorithms_v5` |
| `paper_algorithms_v6` | v6 | `paper_algorithms_v6` |
| `baselines` | ring AllReduce | `baselines/Ring_AllReduce` |
| `floor` | `ib_write_lat` / `ib_write_bw` | the fastest the network itself can go |

Each folder has `logs/` (raw output of the job) and, after `make_final_results.py`, a `RESULTS.md`.
`SUMMARY.md` puts all versions side by side.

## Experiments

Run once as pass 1 (v1 to v6, then ring) and once as pass 2 (ring, then v6 down to v1), so that a
slow or fast moment on the cluster does not favour one version.

| # | Experiment | Programs |
|---|---|---|
| 1 | 3 ranks, 32 B / 64 B / 128 B / 32 KB, 1000 rounds | all seven algorithms (ring in `baselines`) |
| 2 | 3 ranks, 256 KB, 100 rounds | same |
| 3 | 3 ranks, 1 MB, 100 rounds | same |
| 4 | 3 ranks, 32 MB, 100 rounds | same |
| 5 | 2 ranks, 32 B / 32 KB (1000 rounds) and 32 MB (100 rounds) | Baseline_Barrier and LL (ring in `baselines`) |
| 6 | RDMA atomics probe | Atomic_Probe, where the version has it |
| 7 | negative test: LL without its wait, must print FAILED | where the version has it |
| 0 | network floor (pass 1 only) | - |

The sizes your professor asked for (32 B, 32 KB, 32 MB) are all in there; the others are the sizes
used in the earlier comparisons.

## How to run

1. Get this folder onto the cluster as `~/IMT2023016/final_results` (git pull or scp, the same way as the
   other folders).
2. `cd ~/IMT2023016/final_results && sed -i 's/\r$//' *.sh *.pbs *.py && bash setup.sh`. It only checks that every version is built and
   makes the log folders writable. If it says MISSING, build that version first.
3. `qsub run_pass1.pbs`. Wait until it has finished (`qstat`); it takes a couple of hours.
   Submit it once only: two jobs at the same time fight over the same ports.
4. `qsub run_pass2.pbs`, again once, again wait.
5. `python3 make_final_results.py` writes `SUMMARY.md` and the `RESULTS.md` in every folder.

If a pass runs out of its 4 hours, the logs written so far are still there; send them anyway and
the rest can be run on its own.

## Reading the numbers

A number is the median time of one AllReduce over the timed rounds, averaged over the three ranks.
v1 prints only the mean, so its numbers are means. A cell that says `hung`, `crashed` or `wrong
result` means that rank did not finish that run correctly; v1 has several of those. The network
floor is the lowest any of these could reach.
