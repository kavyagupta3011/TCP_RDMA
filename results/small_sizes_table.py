"""Markdown tables for the small-size runs (32 B, 64 B, 128 B) of one version.

Used by results_v3/make_results_v3.py, and on its own for version 2:
    python small_sizes_table.py results_v2/logs/2026-11-01_small_sizes_32_64_128B_raw.txt
prints the section to paste into results_v2/RESULTS_v2.md.
"""
import os
import re
import sys

SIZES = [(8, "32 B"), (16, "64 B"), (32, "128 B")]
PROGS = [("Baseline_Barrier", "Baseline (with barrier)"), ("LL", "LL (one-shot)"),
         ("Sentinel", "Sentinel (one-shot)"), ("Twoshot_LL", "Two-shot LL"),
         ("Twoshot_Sentinel", "Two-shot Sentinel"),
         ("Bidirectional_DoubleBuffering", "Bidirectional (2 ranks, 8 chunks per round)")]


def parse(path):
    """{(prog, floats): {rank: dict(mean, min, med, p99, max, checks[], exit)}}"""
    runs, key, rank = {}, None, None
    for line in open(path, encoding="utf-8", errors="replace"):
        m = re.match(r"^=== (\S+) \((\d+) ranks, (\d+) floats\) ===", line)
        if m:
            key = (m.group(1), int(m.group(3)))
            runs[key] = {}
            continue
        if line.startswith("=== ") or line.startswith("####"):
            key = None
            continue
        if key is None:
            continue
        m = re.match(r"^--- \S+ rank (\d+) ---", line)
        if m:
            rank = int(m.group(1))
            runs[key][rank] = dict(mean=None, min=None, med=None, p99=None, max=None, checks=[], exit=None)
            continue
        if rank is None:
            continue
        r = runs[key][rank]
        m = re.search(r"^Average .*: ([\d.]+) us", line)
        if m:
            r["mean"] = float(m.group(1))
        m = re.search(r"min=([\d.]+) us\s+median=([\d.]+) us\s+p99=([\d.]+) us\s+max=([\d.]+) us", line)
        if m:
            r["min"], r["med"], r["p99"], r["max"] = (float(x) for x in m.groups())
        if line.startswith("Correctness check"):
            r["checks"].append(line.rstrip().split()[-1])
        m = re.match(r"^exit=(\d+)", line)
        if m:
            r["exit"] = int(m.group(1))
    return runs


def avg(runs, prog, floats, field):
    ranks = runs.get((prog, floats), {})
    vals = [r[field] for r in ranks.values() if r[field] is not None]
    return sum(vals) / len(vals) if vals else None


def f(x):
    return "-" if x is None else f"{x:.2f} µs"


def table(runs, field):
    out = ["| Algorithm | " + " | ".join(s for _, s in SIZES) + " |", "|---|---|---|---|"]
    for prog, name in PROGS:
        out.append(f"| {name} | " + " | ".join(f(avg(runs, prog, n, field)) for n, _ in SIZES) + " |")
    return out


def section(path, heading, level="##"):
    runs = parse(path)
    o = [f"{level} {heading}", ""]
    o.append(f"Log: `logs/{os.path.basename(path)}`. PBS job 30202, 2026-11-01,")
    o.append("compute02-04, 1000 rounds. 32 B = 8 floats, 64 B = 16 floats, 128 B = 32 floats (two-shot pads to 9 / 18 / 33).")
    o.append("")
    o.append("**Median**, average of the ranks:")
    o.append("")
    o += table(runs, "med")
    o.append("")
    o.append("**p99**, average of the ranks:")
    o.append("")
    o += table(runs, "p99")
    o.append("")
    o.append("**Mean**, average of the ranks (the statistic the 2026-10-05 tables used):")
    o.append("")
    o += table(runs, "mean")
    o.append("")
    bar = ["| | " + " | ".join(s for _, s in SIZES) + " |", "|---|---|---|---|"]
    b = [avg(runs, "Baseline_Barrier", n, "med") for n, _ in SIZES]
    l = [avg(runs, "LL", n, "med") for n, _ in SIZES]
    bar.append("| Barrier cost (Baseline - LL, median) | " + " | ".join(f(x - y) for x, y in zip(b, l)) + " |")
    bar.append("| Baseline / LL | " + " | ".join(f"{x / y:.2f}x" for x, y in zip(b, l)) + " |")
    o += bar
    o.append("")
    checks = [c for r in runs.values() for rk in r.values() for c in rk["checks"]]
    exits = [rk["exit"] for r in runs.values() for rk in r.values()]
    o.append(f"Correctness: {checks.count('PASSED')} PASSED, {checks.count('FAILED')} FAILED lines. "
             f"Exit codes: {sum(1 for e in exits if e == 0)} of {len(exits)} processes exited with 0.")
    o.append("")
    return o


def comparison(path_v2, path_v3, level="##"):
    r2, r3 = parse(path_v2), parse(path_v3)
    o = [f"{level} Version 2 vs version 3 at 32 B, 64 B, 128 B (same job, same nodes)", ""]
    o.append("Median, average of the ranks. Both versions ran one after the other in PBS job 30202.")
    o.append("")
    o.append("| Algorithm | " + " | ".join(f"{s} v2 | {s} v3" for _, s in SIZES) + " |")
    o.append("|---|" + "---|" * (2 * len(SIZES)))
    for prog, name in PROGS:
        cells = []
        for n, _ in SIZES:
            cells += [f(avg(r2, prog, n, "med")), f(avg(r3, prog, n, "med"))]
        o.append(f"| {name} | " + " | ".join(cells) + " |")
    o.append("")
    return o


if __name__ == "__main__":
    print("\n".join(section(sys.argv[1], sys.argv[2] if len(sys.argv) > 2 else "Small sizes: 32 B, 64 B, 128 B")))
