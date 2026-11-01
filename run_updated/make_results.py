#!/usr/bin/env python3
"""Builds the results files from the logs written by run_everything.pbs.

Run on the cluster (Python 3.6+) after the job has finished:
    cd ~/IMT2023016/run_updated && python3 make_results.py
or on the laptop, with the folder as argument:
    python make_results.py <path to results_updated>

Writes:
    results_updated/README.md          overview: what was run, all three versions side by side
    results_updated/v1/RESULTS_v1.md   (and v2, v3) every table for one version
    results_updated/floor/RESULTS_floor.md
Every number is read from the logs; nothing is typed in by hand.
"""
import os
import re
import sys
from collections import OrderedDict

OUT = sys.argv[1] if len(sys.argv) > 1 else "/home/student4/IMT2023016/results_updated"
VERSIONS = ["v1", "v2", "v3"]
CODE = {"v1": "paper_algorithms", "v2": "paper_algorithms_v2", "v3": "paper_algorithms_v3"}
ABOUT = {
    "v1": "the first RDMA version (git commit 0512582, 2026-09-30). Each RDMA write waits for its "
          "completion before the next one; one receive buffer reused every round; Sentinel never "
          "re-arms its buffers; flag waits use `!=`; prints only the mean; checks the result only "
          "in the first round.",
    "v2": "version 1 plus the fixes of 2026-10-05 (git commit 421f67d): writes posted back to back "
          "and small ones sent inline, two receive buffers used in turn, Sentinel re-armed, `<` "
          "flag waits, barrier receive fix, LL128 skips cleanly; prints min / median / p99 / max.",
    "v3": "version 2 plus the changes of 2026-10-28: a correctness check in every timed round, a "
          "faster adding step and -O3 for Baseline and LL, and the negative test program.",
}
PROGS = [
    ("Baseline_Barrier", "Baseline (with barrier)"),
    ("LL", "LL (one-shot)"),
    ("Sentinel", "Sentinel (one-shot)"),
    ("Twoshot_LL", "Two-shot LL"),
    ("Twoshot_Sentinel", "Two-shot Sentinel"),
    ("LL128_Atomic", "LL128 Atomic"),
    ("Bidirectional_DoubleBuffering", "Bidirectional (2 ranks, 8 chunks per round)"),
]
NAME = dict(PROGS)
NAME["LL_NoWait_NegativeTest"] = "LL without the wait (negative test)"
NAME["Atomic_Probe"] = "Atomic_Probe"
SIZES = [(8, "32 B"), (16, "64 B"), (32, "128 B"), (8192, "32 KB"), (65536, "256 KB"), (8388608, "32 MB")]
SIZE_NAME = dict(SIZES)


# ---------------------------------------------------------------- parsing

def parse_log(path):
    """Returns (header dict, runs). runs: {(prog, ranks, floats): {rank: info}}."""
    head, runs, key, rank = {}, OrderedDict(), None, None
    with open(path, encoding="utf-8", errors="replace") as fh:
        for line in fh:
            line = line.rstrip("\r\n")
            m = re.match(r"^# (version|experiment|job|nodes): (.*)$", line)
            if m:
                head.setdefault(m.group(1), m.group(2).strip())
                continue
            m = re.match(r"^rank (\d+) -> host (\S+)", line)
            if m:
                head.setdefault("hosts", OrderedDict())[int(m.group(1))] = m.group(2).replace(".local", "")
                continue
            m = re.match(r"^=== (\S+) \((\d+) ranks, (\d+) floats\) ===$", line)
            if m:
                key = (m.group(1), int(m.group(2)), int(m.group(3)))
                runs[key] = OrderedDict()
                rank = None
                continue
            if line.startswith("####") or line.startswith("=== "):
                key = None
                continue
            if key is None:
                continue
            if line.startswith("!! not built"):
                runs[key]["not_built"] = True
                continue
            m = re.match(r"^--- \S+ rank (\d+) ---$", line)
            if m:
                rank = int(m.group(1))
                runs[key][rank] = dict(mean=None, min=None, med=None, p99=None, max=None, checks=[],
                                       exit=None, errors=[], skipped=False, probe=[], missing=False)
                continue
            if rank is None:
                continue
            r = runs[key][rank]
            if line.startswith("(missing log file"):
                r["missing"] = True
            m = re.search(r"^Average .*: ([\d.]+) us", line)
            if m:
                r["mean"] = float(m.group(1))
            m = re.search(r"min=([\d.]+) us\s+median=([\d.]+) us\s+p99=([\d.]+) us\s+max=([\d.]+) us", line)
            if m:
                r["min"], r["med"], r["p99"], r["max"] = (float(x) for x in m.groups())
            if line.startswith("Correctness check"):
                r["checks"].append("PASSED" if line.rstrip().endswith("PASSED") else "FAILED")
            m = re.match(r"^exit=(\d+)", line)
            if m:
                r["exit"] = int(m.group(1))
            if line.startswith("SKIPPED"):
                r["skipped"] = True
            if line.startswith("PROBE"):
                r["probe"].append(line)
            if re.match(r"^(RDMA |verify:|.*rror)", line) and not line.startswith("Correctness"):
                r["errors"].append(line.strip())
    return head, runs


def load_version(v):
    logdir = os.path.join(OUT, v, "logs")
    logs = OrderedDict()
    if os.path.isdir(logdir):
        for f in sorted(os.listdir(logdir)):
            if f.endswith(".txt"):
                logs[f] = parse_log(os.path.join(logdir, f))
    runs = OrderedDict()
    for f, (head, r) in logs.items():
        for k, info in r.items():
            runs[k] = (f, info)
    return logs, runs


# ---------------------------------------------------------------- formatting

def ranks_of(info):
    return OrderedDict((k, v) for k, v in info.items() if isinstance(k, int))


def status(r):
    if r["missing"]:
        return "no log"
    if r["skipped"]:
        return "skipped"
    if r["exit"] == 124:
        return "hung"
    if r["checks"] and "FAILED" in r["checks"]:
        return "wrong result"
    if r["mean"] is None and r["med"] is None:
        return "crashed" if r["exit"] not in (None, 0) else "no result"
    if r["exit"] not in (0, None):
        return "crashed"
    return "ok"


def fmt(us):
    if us is None:
        return "-"
    if us >= 10000:
        return "{:.2f} ms".format(us / 1000)
    if us >= 100:
        return "{:.1f} µs".format(us)
    return "{:.2f} µs".format(us)


def value(entry, field):
    """Average of `field` over the ranks that finished correctly, or None."""
    if not entry:
        return None
    _, info = entry
    vals = [r[field] for r in ranks_of(info).values() if status(r) == "ok" and r[field] is not None]
    return sum(vals) / len(vals) if vals else None


def cell(entry, field):
    """Table cell: the value, or what went wrong."""
    if not entry:
        return "not run"
    _, info = entry
    if info.get("not_built"):
        return "not built"
    rk = ranks_of(info)
    if not rk:
        return "no result"
    st = [status(r) for r in rk.values()]
    if all(s == "skipped" for s in st):
        return "skipped (no RDMA atomics)"
    v = value(entry, field)
    if v is None:
        bad = OrderedDict()
        for i, r in rk.items():
            bad.setdefault(status(r), []).append(str(i))
        return ", ".join("{} (rank {})".format(s, "/".join(i)) for s, i in bad.items())
    ok = sum(1 for s in st if s == "ok")
    text = fmt(v)
    if ok < len(st):
        problems = ", ".join("rank {} {}".format(i, s) for i, s in zip(rk.keys(), st) if s != "ok")
        text += " ({} of {} ranks; {})".format(ok, len(st), problems)
    return text


def table(rows, header):
    out = ["| " + " | ".join(header) + " |", "|" + "---|" * len(header)]
    for r in rows:
        out.append("| " + " | ".join(r) + " |")
    return out


def sizes_present(runs, ranks):
    have = {k[2] for k in runs if k[1] == ranks or (k[0] == "Bidirectional_DoubleBuffering" and ranks == 3)}
    return [(n, s) for n, s in SIZES if n in have]


def get(runs, prog, ranks, floats):
    if prog == "Bidirectional_DoubleBuffering":
        ranks = 2
    return runs.get((prog, ranks, floats))


# ---------------------------------------------------------------- per-version file

def version_file(v):
    logs, runs = load_version(v)
    o = ["# Results: {} ({})".format(v, CODE[v]), ""]
    o.append("**{}** = `{}`: {}".format(v, CODE[v], ABOUT[v]))
    o.append("")
    if not logs:
        o.append("No logs found in `logs/`. Run `run_everything.pbs` first.")
        return o
    any_head = next(iter(logs.values()))[0]
    hosts = any_head.get("hosts", {})
    o.append("Machines: " + ", ".join("rank {} = {}".format(i, h) for i, h in hosts.items())
             + ". PBS job " + any_head.get("job", "?").split()[0] + ".")
    o.append("")
    o.append("How to read: a value is the time of one AllReduce, averaged over the ranks that finished")
    o.append("correctly. If a rank crashed, hung (stopped by the timeout) or got a wrong result, the cell")
    o.append("says so. Sizes: 32 B = 8 floats, 64 B = 16, 128 B = 32, 32 KB = 8,192, 256 KB = 65,536,")
    o.append("32 MB = 8,388,608 floats per rank. 1000 timed rounds, 100 for 256 KB and 32 MB.")
    o.append("")
    has_median = any(r["med"] is not None for _, info in runs.values() for r in ranks_of(info).values())
    main = "med" if has_median else "mean"
    sz = sizes_present(runs, 3)
    n = 1

    o.append("## {}. All algorithms, 3 ranks: {}".format(n, "median" if has_median else "mean"))
    o.append("")
    if not has_median:
        o.append("This version prints only the mean (`Average ... latency`), so all its tables use the mean.")
        o.append("")
    rows = [[name] + [cell(get(runs, p, 3, s), main) for s, _ in sz] for p, name in PROGS]
    o += table(rows, ["Algorithm"] + [l for _, l in sz])
    o.append("")
    n += 1

    if has_median:
        o.append("## {}. All algorithms, 3 ranks: p99 and mean".format(n))
        o.append("")
        o.append("p99 (99% of the rounds were faster):")
        o.append("")
        rows = [[name] + [cell(get(runs, p, 3, s), "p99") for s, _ in sz] for p, name in PROGS]
        o += table(rows, ["Algorithm"] + [l for _, l in sz])
        o.append("")
        o.append("Mean (the only statistic version 1 prints, for comparison with it):")
        o.append("")
        rows = [[name] + [cell(get(runs, p, 3, s), "mean") for s, _ in sz] for p, name in PROGS]
        o += table(rows, ["Algorithm"] + [l for _, l in sz])
        o.append("")
        n += 1

    o.append("## {}. Cost of the barrier: Baseline vs LL, 3 ranks ({})".format(n, "median" if has_median else "mean"))
    o.append("")
    rows = []
    for label, fn in [("Baseline - LL", lambda b, l: fmt(b - l)), ("Baseline / LL", lambda b, l: "{:.2f}x".format(b / l))]:
        row = [label]
        for s, _ in sz:
            b, l = value(get(runs, "Baseline_Barrier", 3, s), main), value(get(runs, "LL", 3, s), main)
            row.append(fn(b, l) if b is not None and l else "-")
        rows.append(row)
    o += table(rows, [""] + [l for _, l in sz])
    o.append("")
    n += 1

    sz2 = [(s, l) for s, l in SIZES if any(k[1] == 2 and k[2] == s and k[0] in ("LL", "Baseline_Barrier") for k in runs)]
    if sz2:
        o.append("## {}. 2 ranks vs 3 ranks: Baseline and LL ({})".format(n, "median" if has_median else "mean"))
        o.append("")
        rows = []
        for p in ("Baseline_Barrier", "LL"):
            for k in (2, 3):
                rows.append(["{}, {} ranks".format(NAME[p], k)] + [cell(runs.get((p, k, s)), main) for s, _ in sz2])
        o += table(rows, [""] + [l for _, l in sz2])
        o.append("")
        n += 1

    special = [k for k in runs if k[0] in ("LL_NoWait_NegativeTest", "Atomic_Probe")]
    if special:
        o.append("## {}. Atomics probe and negative test".format(n))
        o.append("")
        for k in special:
            f, info = runs[k]
            if k[0] == "Atomic_Probe":
                lines = []
                for r in ranks_of(info).values():
                    lines += r["probe"]
                o.append("Atomic_Probe (`logs/{}`):".format(f))
                o.append("")
                o.append("```")
                o += lines
                o.append("```")
            else:
                checks = [c for r in ranks_of(info).values() for c in r["checks"]]
                o.append("{} at {} (`logs/{}`): {} FAILED and {} PASSED correctness lines. It must fail: "
                         "this shows the check catches a missing wait.".format(
                             NAME[k[0]], SIZE_NAME.get(k[2], k[2]), f, checks.count("FAILED"), checks.count("PASSED")))
            o.append("")
        n += 1

    o.append("## {}. Correctness and errors".format(n))
    o.append("")
    rows = []
    for f, (head, r) in logs.items():
        checks = [c for _, info in r.items() for rr in ranks_of(info).values() for c in rr["checks"]]
        sts = [status(rr) for _, info in r.items() for rr in ranks_of(info).values()]
        rows.append(["`{}`".format(f), str(checks.count("PASSED")), str(checks.count("FAILED")),
                     str(sts.count("ok")), str(sts.count("crashed")), str(sts.count("hung")),
                     str(sts.count("skipped"))])
    o += table(rows, ["Log", "PASSED lines", "FAILED lines", "ranks ok", "crashed", "hung", "skipped"])
    o.append("")
    errs = []
    for k, (f, info) in runs.items():
        for i, r in ranks_of(info).items():
            for e in r["errors"][:1]:
                errs.append("- {}, {}, rank {}: `{}`".format(NAME.get(k[0], k[0]), SIZE_NAME.get(k[2], k[2]), i, e))
    if errs:
        o.append("Error messages (first one per rank):")
        o.append("")
        o += errs
        o.append("")
    n += 1

    o.append("## {}. Every run, per rank".format(n))
    o.append("")
    o.append("Times in µs. `-` = not printed by this version or the rank did not finish.")
    o.append("")
    rows = []
    for k, (f, info) in runs.items():
        for i, r in ranks_of(info).items():
            rows.append([NAME.get(k[0], k[0]), str(k[1]), SIZE_NAME.get(k[2], str(k[2])), str(i), status(r),
                         *["-" if r[x] is None else "{:,.2f}".format(r[x]) for x in ("mean", "min", "med", "p99", "max")],
                         "/".join(r["checks"]) or "-", "-" if r["exit"] is None else str(r["exit"]), f.split("_")[0]])
    o += table(rows, ["Program", "ranks", "size", "rank", "status", "mean", "min", "median", "p99", "max",
                      "correctness", "exit", "log"])
    o.append("")
    o.append("`log` = the number at the start of the file name in `logs/`:")
    o.append("")
    for f, (head, _) in logs.items():
        o.append("- `{}`: {}".format(f, head.get("experiment", "")))
    o.append("")
    return o


# ---------------------------------------------------------------- floor

def parse_floor(path):
    lat, bw, mode, info = OrderedDict(), OrderedDict(), None, []
    with open(path, encoding="utf-8", errors="replace") as fh:
        for line in fh:
            if "=== ib_write_lat" in line:
                mode = "lat"; continue
            if "=== ib_write_bw" in line:
                mode = "bw"; continue
            if re.match(r"^\s*(hca_id|vendor_part_id|state|compute\d+)", line):
                info.append(line.strip())
            p = line.split()
            if not p or not p[0].isdigit():
                continue
            if mode == "lat" and len(p) >= 9:
                lat[int(p[0])] = (float(p[4]), float(p[7]), float(p[8]))
            elif mode == "bw" and len(p) >= 5:
                bw[int(p[0])] = float(p[3])
    return lat, bw, info


def bytes_label(b):
    if b >= 1 << 20:
        return "{} MiB".format(b >> 20)
    if b >= 1024:
        return "{} KiB".format(b >> 10)
    return "{} B".format(b)


def floor_file():
    path = os.path.join(OUT, "floor", "logs", "network_floor.txt")
    o = ["# Network floor", ""]
    if not os.path.exists(path):
        return o + ["No log found (`logs/network_floor.txt`)."], {}, {}
    lat, bw, info = parse_floor(path)
    o.append("The fastest the network itself can be: one plain RDMA write between the first two machines of")
    o.append("the job, measured with the card maker's tools `ib_write_lat` (one write, one way) and")
    o.append("`ib_write_bw` (bandwidth). No AllReduce can be faster. Log: `logs/network_floor.txt`.")
    o.append("")
    rows = []
    for b in lat:
        t, p99, p999 = lat[b]
        rows.append([bytes_label(b), "{:.2f} µs".format(t), "{:.2f} µs".format(p99), "{:.2f} µs".format(p999),
                     "{:,.0f} MB/s".format(bw[b]) if b in bw else "-"])
    o += table(rows, ["Message", "typical", "99%", "99.9%", "bandwidth"])
    o.append("")
    o.append("A round trip (a write and its acknowledgement) is about twice the one-way time.")
    o.append("32 MB is above the tool's 8 MiB limit: 4 x the 8 MiB time.")
    o.append("")
    return o, lat, bw


# ---------------------------------------------------------------- overview

def overview(lat, bw):
    data = {v: load_version(v) for v in VERSIONS}
    o = ["# Results, all three versions", ""]
    o.append("All three versions of the code were run with the same experiments, in one PBS job, on the")
    o.append("same 3 machines; within each experiment the versions ran one after the other. Every number")
    o.append("here is read from the logs by `make_results.py`.")
    o.append("")
    o.append("| Version | Code | What it is |")
    o.append("|---|---|---|")
    for v in VERSIONS:
        o.append("| {} | `{}` | {} |".format(v, CODE[v], ABOUT[v]))
    o.append("")
    o.append("## Folder layout")
    o.append("")
    o.append("```")
    o.append("results_updated/")
    o.append("  README.md              this overview")
    o.append("  floor/RESULTS_floor.md the network floor;  floor/logs/  its raw log")
    for v in VERSIONS:
        o.append("  {0}/RESULTS_{0}.md        every table of {0};   {0}/logs/  its raw logs".format(v))
    o.append("```")
    o.append("")
    o.append("Logs per version: `1_3ranks_32B_64B_128B_32KB.txt`, `2_3ranks_256KB.txt`, `3_3ranks_32MB.txt`,")
    o.append("`4_2ranks_baseline_ll.txt`, `5_atomic_probe.txt` (v2, v3), `6_negative_test.txt` (v3).")
    o.append("")
    if lat:
        t32 = lat.get(32, (None,))[0]
        o.append("**Network floor:** one 32 B RDMA write takes {} one way (a round trip about {}); large messages "
                 "move at {} MB/s. Details: `floor/RESULTS_floor.md`.".format(
                     fmt(t32), fmt(2 * t32) if t32 else "-", "{:,.0f}".format(max(bw.values())) if bw else "-"))
        o.append("")
    o.append("## Side by side: mean time per AllReduce, 3 ranks")
    o.append("")
    o.append("The mean is used here because it is the only statistic version 1 prints. The medians of v2")
    o.append("and v3 are in the next table and in their own files.")
    o.append("")
    for s, label in SIZES:
        if not any(get(data[v][1], p, 3, s) for v in VERSIONS for p, _ in PROGS):
            continue
        o.append("### {}".format(label))
        o.append("")
        rows = [[name] + [cell(get(data[v][1], p, 3, s), "mean") for v in VERSIONS] for p, name in PROGS]
        o += table(rows, ["Algorithm", "v1", "v2", "v3"])
        o.append("")
    o.append("## Median, v2 vs v3, 3 ranks")
    o.append("")
    sz = [(s, l) for s, l in SIZES if any(get(data[v][1], "LL", 3, s) for v in ("v2", "v3"))]
    hdr = ["Algorithm"] + ["{} {}".format(l, v) for s, l in sz for v in ("v2", "v3")]
    rows = []
    for p, name in PROGS:
        row = [name]
        for s, _ in sz:
            for v in ("v2", "v3"):
                row.append(cell(get(data[v][1], p, 3, s), "med"))
        rows.append(row)
    o += table(rows, hdr)
    o.append("")
    o.append("## Did every version finish?")
    o.append("")
    rows = []
    for v in VERSIONS:
        logs, runs = data[v]
        sts = [status(r) for _, info in runs.values() for r in ranks_of(info).values()]
        checks = [c for _, info in runs.values() for r in ranks_of(info).values() for c in r["checks"]]
        rows.append([v, str(sts.count("ok")), str(sts.count("crashed")), str(sts.count("hung")),
                     str(sts.count("wrong result")), str(sts.count("skipped")),
                     "{} / {}".format(checks.count("PASSED"), checks.count("FAILED"))])
    o += table(rows, ["Version", "ranks ok", "crashed", "hung", "wrong result", "skipped", "PASSED / FAILED lines"])
    o.append("")
    o.append("For v3, the FAILED lines come from the negative test, which must fail. Crashes and hangs are")
    o.append("listed with their error messages in each version's file, section \"Correctness and errors\".")
    o.append("")
    return o


def write(path, lines):
    with open(path, "w", encoding="utf-8") as fh:
        fh.write("\n".join(lines) + "\n")
    print("wrote", path)


if __name__ == "__main__":
    fl, lat, bw = floor_file()
    write(os.path.join(OUT, "floor", "RESULTS_floor.md"), fl)
    for v in VERSIONS:
        write(os.path.join(OUT, v, "RESULTS_{}.md".format(v)), version_file(v))
    write(os.path.join(OUT, "README.md"), overview(lat, bw))
