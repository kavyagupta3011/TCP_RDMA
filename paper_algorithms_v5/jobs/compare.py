#!/usr/bin/env python3
"""Reads the logs of compare_v3_v4_v5.pbs and prints v3, v4 and v5 side by side.

    python3 compare.py [path to results_v5]      (default ~/IMT2023016/results_v5)

For each program and size: the median latency of each rank is taken from the log, the three
ranks are averaged, and the two runs of each version (forward / reverse order) are averaged.
The two change columns are v5 against v4 and v5 against v3 (negative = v5 is faster).
"""
import glob
import os
import re
import sys

OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.expanduser("~/IMT2023016/results_v5")
VERS = ["v3", "v4", "v5"]


def parse(path):
    """-> {(program, floats): [median of rank 0, rank 1, ...]}"""
    res = {}
    size = prog = None
    for line in open(path, errors="replace"):
        m = re.match(r"#+ message = (\d+) floats", line)
        if m:
            size = int(m.group(1))
            continue
        m = re.match(r"=== (\S+) ", line)
        if m:
            prog = m.group(1)
            continue
        m = re.search(r"median=([\d.]+) us", line)
        if m and prog and size:
            res.setdefault((prog, size), []).append(float(m.group(1)))
    return res


def label(floats):
    b = floats * 4
    return "%d B" % b if b < 1024 else ("%d KB" % (b // 1024) if b < 1 << 20 else "%d MB" % (b >> 20))


runs = {v: {} for v in VERS}
for f in sorted(glob.glob(os.path.join(OUT, "logs", "*.txt"))):
    m = re.search(r"_(v\d)\.txt$", f)
    if not m or m.group(1) not in runs:
        continue
    for key, meds in parse(f).items():
        runs[m.group(1)].setdefault(key, []).append(sum(meds) / len(meds))

keys = sorted(set.intersection(*[set(runs[v]) for v in VERS]), key=lambda k: (k[1], k[0]))
print("%-17s %6s %11s %11s %11s %9s %9s  runs" % ("program", "size", "v3", "v4", "v5", "v5 vs v4", "v5 vs v3"))
for prog, size in keys:
    mean = {v: sum(runs[v][(prog, size)]) / len(runs[v][(prog, size)]) for v in VERS}
    print("%-17s %6s %8.2f us %8.2f us %8.2f us %+8.1f%% %+8.1f%%  %s" % (
        prog, label(size), mean["v3"], mean["v4"], mean["v5"],
        100 * (mean["v5"] - mean["v4"]) / mean["v4"], 100 * (mean["v5"] - mean["v3"]) / mean["v3"],
        "/".join(str(len(runs[v][(prog, size)])) for v in VERS)))
