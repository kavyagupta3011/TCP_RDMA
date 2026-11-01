#!/usr/bin/env python3
"""Reads the logs of compare_v3_v4.pbs and prints v3 against v4 per program and size.

    python3 compare.py [path to results_v4]      (default ~/IMT2023016/results_v4)

For each program and size: the median latency of each rank is taken from the log, the three
ranks are averaged, and the two runs of each version (v3 first / v4 first) are averaged.
The last column is the change from v3 to v4 (negative = v4 is faster).
"""
import glob
import os
import re
import sys

OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.expanduser("~/IMT2023016/results_v4")


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


runs = {"v3": {}, "v4": {}}
for f in sorted(glob.glob(os.path.join(OUT, "logs", "*.txt"))):
    v = "v3" if f.endswith("_v3.txt") else "v4"
    for key, meds in parse(f).items():
        runs[v].setdefault(key, []).append(sum(meds) / len(meds))

keys = sorted(set(runs["v3"]) & set(runs["v4"]), key=lambda k: (k[1], k[0]))
print("%-18s %8s %12s %12s %9s   (runs v3/v4)" % ("program", "size", "v3 median", "v4 median", "change"))
for prog, size in keys:
    a, b = runs["v3"][(prog, size)], runs["v4"][(prog, size)]
    ma, mb = sum(a) / len(a), sum(b) / len(b)
    print("%-18s %8s %9.2f us %9.2f us %+8.1f%%   (%d/%d)" % (prog, label(size), ma, mb, 100 * (mb - ma) / ma, len(a), len(b)))
