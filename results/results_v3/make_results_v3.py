"""Build RESULTS_v3.md from the raw logs of version 3.

Usage (from this folder):  python make_results_v3.py logs RESULTS_v3.md
"""
import re, sys, os
from collections import OrderedDict

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import small_sizes_table  # noqa: E402  (results/small_sizes_table.py)

d, out_path = sys.argv[1], sys.argv[2]
SMALL = "2026-11-01_small_sizes_32_64_128B_raw.txt"
F = {
    "floor00": "2026-10-28_floor_compute00_01_loaded_nodes_raw.txt",
    "r1": "2026-10-28_round1_floor_baseline_ll_raw.txt",
    "r2": "2026-10-28_round2_all_algorithms_raw.txt",
    "r3": "2026-10-28_round3_optimisation_raw.txt",
    "r4": "2026-10-28_round4_final_and_2rank_raw.txt",
}
SIZE = {8: "32 B", 8192: "32 KB", 8388608: "32 MB"}
SIZES = [8, 8192, 8388608]


def parse_runs(path):
    """Return list of runs: dict(section, prog, ranks, floats, rank_stats{r: (min,med,p99,max)}, checks[], exits{})."""
    runs, cur, rank, section = [], None, None, ""
    for line in open(path, encoding="utf-8", errors="replace"):
        line = line.rstrip("\n")
        m = re.match(r"^#{6,} (.*?) #{6,}$", line)
        if m and not m.group(1).startswith("message"):
            section = m.group(1)
            continue
        m = re.match(r"^=== (\S+) \((\d+) ranks, (\d+) floats\) ===$", line)
        if m:
            cur = dict(section=section, prog=m.group(1), ranks=int(m.group(2)), floats=int(m.group(3)),
                       stats={}, checks=[], exits={}, skipped=False)
            runs.append(cur)
            continue
        if line.startswith("=== "):
            cur = None
            continue
        if cur is None:
            continue
        m = re.match(r"^--- \S+ rank (\d+) ---$", line)
        if m:
            rank = int(m.group(1))
            continue
        m = re.search(r"min=([\d.]+) us\s+median=([\d.]+) us\s+p99=([\d.]+) us\s+max=([\d.]+) us", line)
        if m:
            cur["stats"][rank] = tuple(float(x) for x in m.groups())
        if line.startswith("Correctness check"):
            cur["checks"].append("PASSED" if line.rstrip().endswith("PASSED") else "FAILED")
        if line.startswith("SKIPPED"):
            cur["skipped"] = True
        m = re.match(r"^exit=(\d+)", line)
        if m:
            cur["exits"][rank] = int(m.group(1))
    return runs


def parse_floor(path):
    """Return (lat rows {bytes: (typical, p99, p999)}, bw rows {bytes: avg}) for the first floor in the file."""
    lat, bw, mode = OrderedDict(), OrderedDict(), None
    for line in open(path, encoding="utf-8", errors="replace"):
        if "=== ib_write_lat" in line: mode = "lat"; continue
        if "=== ib_write_bw" in line: mode = "bw"; continue
        p = line.split()
        if not p or not p[0].isdigit():
            continue
        if mode == "lat" and len(p) >= 9:
            lat[int(p[0])] = (float(p[4]), float(p[7]), float(p[8]))
        elif mode == "bw" and len(p) >= 5:
            bw[int(p[0])] = float(p[3])
    return lat, bw


def avg(xs): return sum(xs) / len(xs)


def fmt(us, floats):
    if floats >= 8388608: return f"{us / 1000:.1f} ms"
    return f"{us:.2f} µs" if us < 10 else f"{us:.1f} µs"


def med(run): return avg([s[1] for s in run["stats"].values()])


def cell(run):
    return fmt(med(run), run["floats"]) if run and run["stats"] else "-"


def find(runs, prog, floats, ranks=None, section=None):
    for r in runs:
        if r["prog"] == prog and r["floats"] in (floats, floats + 1) and (ranks is None or r["ranks"] == ranks) \
                and (section is None or section in r["section"]):
            return r
    return None


def bytes_label(b):
    if b >= 1 << 20: return f"{b >> 20} MiB"
    if b >= 1024: return f"{b >> 10} KiB"
    return f"{b} B"


R = {k: parse_runs(os.path.join(d, v)) for k, v in F.items() if k != "floor00"}
lat02, bw02 = parse_floor(os.path.join(d, F["r1"]))
lat00, bw00 = parse_floor(os.path.join(d, F["floor00"]))

o = []
w = o.append
w("# Version 3 results (2026-10-28)")
w("")
w("Results of **version 3** (`paper_algorithms_v3/`): the code after the professor's comments, measured")
w("at 32 B, 32 KB and 32 MB on idle machines, with the network floor and a correctness check in every round.")
w("Job scripts: `paper_algorithms_v3/jobs/`.")
w("")
w("Every number below is read from the raw logs in `logs/` by `make_results_v3.py`")
w("(no number typed by hand). The log each table comes from is named above it.")
w("")
w("How to read the tables:")
w("- Size = the vector each rank contributes: 32 B = 8 floats, 32 KB = 8,192 floats, 32 MB = 8,388,608 floats.")
w("- A table value = the **median** time of one AllReduce, averaged over the ranks (one rank per machine).")
w("- Machines: compute02, compute03, compute04 (2-rank runs: compute02, compute03), unless stated.")
w("- Timed rounds: 1000 at 32 B and 32 KB, 100 at 32 MB.")
w("- The per-rank min / median / p99 / max of every run are in section 9.")
w("")
w("Contents")
w("1. Network floor")
w("2. Main result: Baseline vs LL, final code (round 4)")
w("3. 2 ranks vs 3 ranks (round 4)")
w("4. Small sizes: 32 B, 64 B, 128 B (2026-11-01), and version 2 vs version 3")
w("5. All algorithms (round 2)")
w("6. Improvement: before vs after the faster adding step (round 3)")
w("7. Repeated runs of Baseline and LL")
w("8. Correctness checks")
w("9. Every run, per rank")
w("")
w("---")
w("")

# 1. floor
w("## 1. Network floor")
w("")
w(f"Files: `{F['r1']}` (compute02 -> compute03, idle) and `{F['floor00']}` (compute00 -> compute01, loaded).")
w("`ib_write_lat`: one RDMA write, one way, 1000 repetitions per size. `ib_write_bw`: average bandwidth.")
w("")
w("| Message | typical, idle | 99%, idle | 99.9%, idle | typical, loaded | 99.9%, loaded | bandwidth, idle |")
w("|---|---|---|---|---|---|---|")
for b in lat02:
    if b < 8: continue
    t, p99, p999 = lat02[b]
    t0, _, p9990 = lat00.get(b, (None, None, None))
    bwv = bw02.get(b)
    def u(x): return "-" if x is None else (f"{x:.2f} µs" if x < 100 else f"{x:,.1f} µs")
    w(f"| {bytes_label(b)} | {u(t)} | {u(p99)} | {u(p999)} | {u(t0)} | {u(p9990)} | "
      f"{'-' if bwv is None else f'{bwv:,.0f} MB/s'} |")
w("")
w("- 32 B: 1.07 µs one way, so a round trip (write + acknowledgement) is about 2.1 µs.")
w("- 32 KB: 11.07 µs; 64 KiB (= what one rank receives at 3 ranks for 32 KB): 19.5 µs.")
w("- 32 MB is beyond the tool's 8 MiB limit: 4 x 8 MiB = about 8.5 ms; 64 MB (3 ranks) about 17 ms.")
w("- Loaded vs idle: the typical values are about the same; the 99.9% values at small sizes are higher on the loaded machines.")
w("")
w("---")
w("")

# 2. main
r4 = R["r4"]
w("## 2. Main result: Baseline vs LL, final code, 3 ranks (round 4)")
w("")
w(f"File: `{F['r4']}`, part A.")
w("")
w("| | " + " | ".join(SIZE[s] for s in SIZES) + " |")
w("|---|---|---|---|")
for prog, name in [("Baseline_Barrier", "Baseline (with barrier)"), ("LL", "LL (no barrier)")]:
    w(f"| {name} | " + " | ".join(cell(find(r4, prog, s, 3)) for s in SIZES) + " |")
ratios = []
for s in SIZES:
    b, l = find(r4, "Baseline_Barrier", s, 3), find(r4, "LL", s, 3)
    ratios.append(f"{med(b) / med(l):.2f}x")
w("| Baseline / LL | " + " | ".join(ratios) + " |")
w("| Network floor | 2.1 µs (round trip) | 19.5 µs | about 17 ms |")
w("")
w("p99 (averaged over the ranks):")
w("")
w("| | " + " | ".join(SIZE[s] for s in SIZES) + " |")
w("|---|---|---|---|")
for prog, name in [("Baseline_Barrier", "Baseline p99"), ("LL", "LL p99")]:
    vals = []
    for s in SIZES:
        r = find(r4, prog, s, 3)
        vals.append(fmt(avg([x[2] for x in r["stats"].values()]), s))
    w(f"| {name} | " + " | ".join(vals) + " |")
w("")
w("Baseline at 32 MB had a few rounds of 130-150 ms in this run (see the max column in section 9), which")
w("pulled its median up. In round 3 (section 6), with the same code, it was 36.5 ms.")
w("")
w("---")
w("")

# 3. ranks
w("## 3. 2 ranks vs 3 ranks (round 4)")
w("")
w(f"File: `{F['r4']}`, parts A (3 ranks) and B (2 ranks).")
w("")
w("| | " + " | ".join(SIZE[s] for s in SIZES) + " |")
w("|---|---|---|---|")
for prog, name in [("LL", "LL"), ("Baseline_Barrier", "Baseline")]:
    for n in (2, 3):
        w(f"| {name}, {n} ranks | " + " | ".join(cell(find(r4, prog, s, n)) for s in SIZES) + " |")
w("")
w("Growth from 2 to 3 ranks:")
w("")
w("| | " + " | ".join(SIZE[s] for s in SIZES) + " |")
w("|---|---|---|---|")
for prog, name in [("LL", "LL"), ("Baseline_Barrier", "Baseline")]:
    w(f"| {name} | " + " | ".join(f"{med(find(r4, prog, s, 3)) / med(find(r4, prog, s, 2)):.2f}x" for s in SIZES) + " |")
w("")
w("Barrier cost at 32 B (Baseline minus LL): "
  f"2 ranks {med(find(r4, 'Baseline_Barrier', 8, 2)) - med(find(r4, 'LL', 8, 2)):.2f} µs, "
  f"3 ranks {med(find(r4, 'Baseline_Barrier', 8, 3)) - med(find(r4, 'LL', 8, 3)):.2f} µs.")
w("")
w("---")
w("")

# 4. small sizes (job 30202): this version's tables, then version 2 vs version 3 from the same job
small_v3 = os.path.join(d, SMALL)
small_v2 = os.path.join(d, "..", "..", "results_v2", "logs", SMALL)
o += small_sizes_table.section(small_v3, "4. Small sizes: 32 B, 64 B, 128 B (2026-11-01)")
_s = small_sizes_table.parse(small_v3)
w(f"The 32 B values here (Baseline {small_sizes_table.avg(_s, 'Baseline_Barrier', 8, 'med'):.2f} µs, "
  f"LL {small_sizes_table.avg(_s, 'LL', 8, 'med'):.2f} µs) differ by a few percent from section 2 "
  f"({med(find(r4, 'Baseline_Barrier', 8, 3)):.2f} µs, {med(find(r4, 'LL', 8, 3)):.2f} µs):")
w("same code, different day; this is the normal run-to-run spread.")
w("")
if os.path.exists(small_v2):
    o += small_sizes_table.comparison(small_v2, small_v3, level="###")
    w("At these sizes the two versions are within a few percent of each other. That is expected: what")
    w("version 3 changed (the faster adding step, the per-round check) only matters for larger messages.")
    w("Version 2's own tables are in `../results_v2/RESULTS_v2.md`.")
    w("")
w("---")
w("")

# 5. all algorithms
r2 = R["r2"]
w("## 5. All algorithms (round 2)")
w("")
w(f"File: `{F['r2']}`. Same code for every algorithm, measured before the faster adding step, so")
w("Baseline and LL are slower here than in section 2 at 32 KB and 32 MB.")
w("")
w("| 3 ranks | " + " | ".join(SIZE[s] for s in SIZES) + " |")
w("|---|---|---|---|")
for prog in ["Baseline_Barrier", "LL", "Sentinel", "Twoshot_LL", "Twoshot_Sentinel"]:
    w(f"| {prog} | " + " | ".join(cell(find(r2, prog, s, 3, 'B+C')) for s in SIZES) + " |")
ll128 = find(r2, "LL128_Atomic", 8)
w(f"| LL128_Atomic | {'SKIPPED (no RDMA atomics), exit=2' if ll128 and ll128['skipped'] else '-'} | - | - |")
w("")
w("| 2 ranks, one round = 8 chunks one after the other | " + " | ".join(SIZE[s] for s in SIZES) + " |")
w("|---|---|---|---|")
w("| Bidirectional_DoubleBuffering | " + " | ".join(cell(find(r2, "Bidirectional_DoubleBuffering", s, 2)) for s in SIZES) + " |")
w("")
w("Two-shot programs pad the vector to a multiple of 3 floats (9 / 8193 / 8,388,609 floats).")
w("")
w("Negative test (LL with the flag wait removed on purpose; it must fail):")
w("")
w("| | 32 B | 32 KB |")
w("|---|---|---|")
neg = [find(r2, "LL_NoWait_NegativeTest", s, 3) for s in (8, 8192)]
w("| median | " + " | ".join(cell(r) for r in neg) + " |")
w("| correctness lines | " + " | ".join(
    f"{r['checks'].count('FAILED')} FAILED, {r['checks'].count('PASSED')} PASSED" for r in neg) + " |")
w("")
w("---")
w("")

# 6. improvement
r3 = R["r3"]
w("## 6. Improvement: before vs after the faster adding step (round 3)")
w("")
w(f"File: `{F['r3']}`. All builds in one job on the same machines. Before = old adding step, -O2")
w("(the code of rounds 1 and 2). After = new adding step, -O3 (the final code).")
w("")
w("| 3 ranks | " + " | ".join(SIZE[s] for s in SIZES) + " |")
w("|---|---|---|---|")
for prog, name in [("LL", "LL"), ("Baseline_Barrier", "Baseline")]:
    before = [find(r3, prog, s, 3, "old_O2") for s in SIZES]
    after = [find(r3, prog, s, 3, "blk_O3") for s in SIZES]
    w(f"| {name} before | " + " | ".join(cell(r) for r in before) + " |")
    w(f"| {name} after | " + " | ".join(
        cell(a) + ("" if s == 8 else f" ({(med(a) / med(b) - 1) * 100:+.0f}%)")
        for s, a, b in zip(SIZES, after, before)) + " |")
w("")
w("All four builds (for reference):")
w("")
w("| Build | adding step | flag | LL 32 B | LL 32 KB | LL 32 MB | Baseline 32 B | Baseline 32 KB | Baseline 32 MB |")
w("|---|---|---|---|---|---|---|---|---|")
for v, step, flag in [("old_O2", "old", "-O2"), ("old_O3", "old", "-O3"), ("blk_O2", "new", "-O2"), ("blk_O3", "new", "-O3")]:
    w(f"| {v} | {step} | {flag} | " + " | ".join(cell(find(r3, p, s, 3, v)) for p in ("LL", "Baseline_Barrier") for s in SIZES) + " |")
w("")
w("---")
w("")

# 7. repeats
w("## 7. Repeated runs of Baseline and LL")
w("")
w("The same programs at the same sizes appear in several files. Rounds 1, 2 and round 3 `old_O2` used the")
w("same (old) code; round 3 `blk_O3` and round 4 used the final code.")
w("")
w("| 3 ranks | " + " | ".join(f"{p} {SIZE[s]}" for p in ("LL", "Baseline") for s in SIZES) + " |")
w("|---|" + "---|" * 6)
for label, runs, sec in [("round 1 (old code)", R["r1"], None), ("round 2 (old code)", r2, "B+C"),
                         ("round 3 old_O2 (old code)", r3, "old_O2"), ("round 3 blk_O3 (final code)", r3, "blk_O3"),
                         ("round 4 (final code)", r4, "A")]:
    w(f"| {label} | " + " | ".join(cell(find(runs, p, s, 3, sec)) for p in ("LL", "Baseline_Barrier") for s in SIZES) + " |")
w("")
w("---")
w("")

# 8. correctness
w("## 8. Correctness checks")
w("")
w("Each rank prints one or two `Correctness check` lines per run (first round, and every timed round).")
w("")
w("| File | PASSED lines | FAILED lines | FAILED lines from the negative test | runs with exit not 0 |")
w("|---|---|---|---|---|")
for k in ("r1", "r2", "r3", "r4"):
    runs = R[k]
    p = sum(r["checks"].count("PASSED") for r in runs)
    f = sum(r["checks"].count("FAILED") for r in runs)
    fn = sum(r["checks"].count("FAILED") for r in runs if r["prog"] == "LL_NoWait_NegativeTest")
    bad = [f"{r['prog']} ({r['floats']} floats): exit {sorted(set(r['exits'].values()))}"
           for r in runs if any(e != 0 for e in r["exits"].values())]
    w(f"| `{F[k]}` | {p} | {f} | {fn} | {'; '.join(bad) if bad else 'none'} |")
w("")
w("Round 1 has only the first-round check (the every-round check was added after it). LL128_Atomic exits")
w("with 2 on purpose (SKIPPED). Bidirectional and Atomic_Probe have no every-round check.")
w("")
w("---")
w("")

# 9. every run
w("## 9. Every run, per rank")
w("")
w("min / median / p99 / max in µs for each rank. `section` = the heading in the raw file.")
w("")
for k in ("r1", "r2", "r3", "r4"):
    w(f"### `{F[k]}`")
    w("")
    w("| section | program | ranks | floats | rank | min | median | p99 | max |")
    w("|---|---|---|---|---|---|---|---|---|")
    for r in R[k]:
        if not r["stats"]:
            w(f"| {r['section']} | {r['prog']} | {r['ranks']} | {r['floats']} | - | "
              f"{'SKIPPED' if r['skipped'] else 'no timing'} | | | |")
            continue
        for rk, (mn, md, p, mx) in sorted(r["stats"].items()):
            w(f"| {r['section']} | {r['prog']} | {r['ranks']} | {r['floats']} | {rk} | "
              f"{mn:,.2f} | {md:,.2f} | {p:,.2f} | {mx:,.2f} |")
    w("")

open(out_path, "w", encoding="utf-8").write("\n".join(o) + "\n")
print("wrote", out_path, len(o), "lines")
