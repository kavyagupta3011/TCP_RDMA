#!/usr/bin/env python3
"""Reads the logs in final_results/<folder>/logs/ and writes the result files.

Run on the cluster (Python 3.6+) after both passes have finished:
    cd ~/IMT2023016/final_results && python3 make_final_results.py
or on the laptop, with the folder as argument:
    python make_final_results.py <path to final_results>

Writes:
    SUMMARY.md                    all versions side by side, ring comparison, did everything finish
    <folder>/RESULTS.md           every table of one version (paper_algorithms ... paper_algorithms_v6, baselines)
    floor/RESULTS.md              the network floor
Every number is read from the logs. A table cell shows pass 1 / pass 2 where both passes ran.
"""
import os
import re
import sys
from collections import OrderedDict

OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.expanduser("~/IMT2023016/final_results")
PASSES = ["pass1", "pass2"]
# label, folder, what it is
VERS = [
    ("v1", "paper_algorithms", "first RDMA version: blocking writes, one receive buffer, mean only"),
    ("v2", "paper_algorithms_v2", "writes posted together, two receive buffers, min/median/p99/max, start and end barrier"),
    ("v3", "paper_algorithms_v3", "v2 + correctness check every round + blocked add"),
    ("v4", "paper_algorithms_v4", "v3 + -O3 and blocked add with copy-out for Two-shot"),
    ("v5", "paper_algorithms_v5", "v4 + one shared protection domain, Two-shot sends straight from input/output"),
    ("v6", "paper_algorithms_v6", "v5 + block pipelining with per-block flags, deeper queues"),
    ("ring", "baselines", "standard ring AllReduce (reduce-scatter + allgather), plain SEND/RECV"),
]
FOLDER = {v: f for v, f, _ in VERS}
ABOUT = {v: a for v, _, a in VERS}
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
NAME["Ring_AllReduce"] = "Ring (standard baseline)"
NAME["LL_NoWait_NegativeTest"] = "LL without the wait (negative test)"
NAME["Atomic_Probe"] = "Atomic_Probe"
SIZES = [(8, "32 B"), (16, "64 B"), (32, "128 B"), (8192, "32 KB"), (65536, "256 KB"),
         (262144, "1 MB"), (8388608, "32 MB")]
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




# ---------------------------------------------------------------- loading and helpers

def load(v):
    """-> ({pass: {(prog, ranks, floats): (logname, info)}}, {logname: header})"""
    d = os.path.join(OUT, FOLDER[v], "logs")
    runs = {p: OrderedDict() for p in PASSES}
    heads = OrderedDict()
    if os.path.isdir(d):
        for f in sorted(os.listdir(d)):
            m = re.match(r"(pass\d)_.*\.txt$", f)
            if not m or m.group(1) not in runs:
                continue
            head, r = parse_log(os.path.join(d, f))
            heads[f] = head
            for k, info in r.items():
                runs[m.group(1)][k] = (f, info)
    return runs, heads


def get(runs, prog, ranks, floats):
    """entries of that run in every pass that has it"""
    if prog == "Bidirectional_DoubleBuffering":
        ranks = 2
    return [runs[p][(prog, ranks, floats)] for p in PASSES if (prog, ranks, floats) in runs[p]]


def best(entry, field="med"):
    """median if the version prints it, else the mean (version 1 prints only the mean)"""
    v = value(entry, field)
    return value(entry, "mean") if v is None and field == "med" else v


def avg(entries, field="med"):
    vals = [best(e, field) for e in entries]
    vals = [x for x in vals if x is not None]
    return sum(vals) / len(vals) if vals else None


def acell(entries, field="med"):
    """one number per cell: the average over the passes, or what went wrong"""
    if not entries:
        return "not run"
    a = avg(entries, field)
    if a is not None:
        return fmt(a)
    return cell(entries[0], field)


def pcell(entries, field="med"):
    """pass 1 / pass 2"""
    if not entries:
        return "not run"
    parts = []
    for e in entries:
        v = best(e, field)
        parts.append(fmt(v) if v is not None else cell(e, field))
    if len(set(parts)) == 1 and best(entries[0], field) is None:   # same problem in both passes
        return parts[0]
    return " / ".join(parts)


def sizes_in(runs_by_pass, ranks=3):
    have = {k[2] for p in runs_by_pass.values() for k in p if k[1] == ranks}
    return [(n, s) for n, s in SIZES if n in have]


def progs_in(runs_by_pass):
    have = {k[0] for p in runs_by_pass.values() for k in p}
    return [(p, NAME[p]) for p in [x for x, _ in PROGS] + ["Ring_AllReduce"] if p in have]


# ---------------------------------------------------------------- one folder

def version_file(v):
    runs, heads = load(v)
    o = ["# Results: {} ({})".format(v, FOLDER[v]), "", "**{}**: {}.".format(v, ABOUT[v]), ""]
    if not heads:
        return o + ["No logs found in `logs/`. Run `run_pass1.pbs` and `run_pass2.pbs` first."]
    h0 = next(iter(heads.values()))
    hosts = ", ".join("rank {} = {}".format(i, h) for i, h in h0.get("hosts", {}).items())
    jobs = sorted({h.get("job", "?").split()[0] for h in heads.values()})
    o.append("Machines: {}. PBS jobs: {}.".format(hosts or "?", ", ".join(jobs)))
    o.append("")
    o.append("A value is the time of one AllReduce (median over the timed rounds, averaged over the ranks that")
    o.append("finished correctly). Cells with two numbers are **pass 1 / pass 2**: the same experiment run twice,")
    o.append("the second time with the versions in the opposite order. If a rank crashed, hung (stopped by the")
    o.append("timeout) or got a wrong result, the cell says so. 1000 timed rounds for sizes up to 32 KB, 100")
    o.append("for 256 KB and above.")
    if v == "v1":
        o.append("")
        o.append("v1 prints only the mean, so every number for v1 is a mean, not a median.")
    o.append("")
    allruns = {p: runs[p] for p in PASSES}
    sz = sizes_in(allruns, 3)
    progs = progs_in(allruns)
    n = 1

    o += ["## {}. Median, 3 ranks (pass 1 / pass 2)".format(n), ""]
    o += table([[name] + [pcell(get(allruns, p, 3, s)) for s, _ in sz] for p, name in progs],
               ["Algorithm"] + [l for _, l in sz])
    o.append(""); n += 1

    o += ["## {}. p99, 3 ranks (average of the passes)".format(n), "",
          "99% of the rounds were faster than this.", ""]
    o += table([[name] + [acell(get(allruns, p, 3, s), "p99") for s, _ in sz] for p, name in progs],
               ["Algorithm"] + [l for _, l in sz])
    o.append(""); n += 1

    if any(p == "Baseline_Barrier" for p, _ in progs):
        o += ["## {}. Cost of the barrier: Baseline vs LL, 3 ranks".format(n), ""]
        rows = []
        for label, fn in [("Baseline - LL", lambda b, l: fmt(b - l)), ("Baseline / LL", lambda b, l: "{:.2f}x".format(b / l))]:
            row = [label]
            for s, _ in sz:
                b = avg(get(allruns, "Baseline_Barrier", 3, s))
                l = avg(get(allruns, "LL", 3, s))
                row.append(fn(b, l) if b is not None and l else "-")
            rows.append(row)
        o += table(rows, [""] + [l for _, l in sz])
        o.append(""); n += 1

    sz2 = [(s, l) for s, l in SIZES if any(k[1] == 2 and k[2] == s and k[0] != "Bidirectional_DoubleBuffering"
                                          for p in allruns.values() for k in p)]
    if sz2:
        o += ["## {}. 2 ranks vs 3 ranks".format(n), ""]
        rows = []
        for p in ("Baseline_Barrier", "LL", "Ring_AllReduce"):
            if p not in [x for x, _ in progs]:
                continue
            for k in (2, 3):
                rows.append(["{}, {} ranks".format(NAME[p], k)] + [acell(get(allruns, p, k, s)) for s, _ in sz2])
        o += table(rows, [""] + [l for _, l in sz2])
        o.append(""); n += 1

    special = [(p, k, e) for p in PASSES for k, e in runs[p].items() if k[0] in ("LL_NoWait_NegativeTest", "Atomic_Probe")]
    if special:
        o += ["## {}. Atomics probe and negative test".format(n), ""]
        for p, k, (f, info) in special:
            if k[0] == "Atomic_Probe":
                lines = [x for r in ranks_of(info).values() for x in r["probe"]]
                o += ["Atomic_Probe (`logs/{}`):".format(f), "", "```"] + lines + ["```", ""]
            else:
                checks = [c for r in ranks_of(info).values() for c in r["checks"]]
                o += ["{} at {} (`logs/{}`): {} FAILED and {} PASSED correctness lines. It must fail: this "
                      "shows the check notices a missing wait.".format(
                          NAME[k[0]], SIZE_NAME.get(k[2], k[2]), f, checks.count("FAILED"), checks.count("PASSED")), ""]
        n += 1

    o += ["## {}. Correctness and errors".format(n), ""]
    rows = []
    for p in PASSES:
        for f in [f for f in heads if f.startswith(p)]:
            r = [(k, e) for k, (ff, e) in runs[p].items() if ff == f]
            checks = [c for _, info in r for rr in ranks_of(info).values() for c in rr["checks"]]
            sts = [status(rr) for _, info in r for rr in ranks_of(info).values()]
            rows.append(["`{}`".format(f), str(checks.count("PASSED")), str(checks.count("FAILED")),
                         str(sts.count("ok")), str(sts.count("crashed")), str(sts.count("hung")),
                         str(sts.count("wrong result")), str(sts.count("skipped"))])
    o += table(rows, ["Log", "PASSED lines", "FAILED lines", "ranks ok", "crashed", "hung", "wrong result", "skipped"])
    o.append("")
    errs = []
    for p in PASSES:
        for k, (f, info) in runs[p].items():
            for i, r in ranks_of(info).items():
                for e in r["errors"][:1]:
                    errs.append("- {} {}, {}, rank {}: `{}`".format(p, NAME.get(k[0], k[0]), SIZE_NAME.get(k[2], k[2]), i, e))
    if errs:
        o += ["Error messages (first one per rank):", ""] + errs + [""]
    n += 1

    o += ["## {}. Every run, per rank".format(n), "",
          "Times in µs. `-` = not printed by this version or the rank did not finish.", ""]
    rows = []
    for p in PASSES:
        for k, (f, info) in runs[p].items():
            for i, r in ranks_of(info).items():
                rows.append([p, NAME.get(k[0], k[0]), str(k[1]), SIZE_NAME.get(k[2], str(k[2])), str(i), status(r),
                             *["-" if r[x] is None else "{:,.2f}".format(r[x]) for x in ("mean", "min", "med", "p99", "max")],
                             "/".join(r["checks"]) or "-", "-" if r["exit"] is None else str(r["exit"])])
    o += table(rows, ["pass", "Program", "ranks", "size", "rank", "status", "mean", "min", "median", "p99", "max",
                      "correctness", "exit"])
    o.append("")
    o.append("Logs and what each ran:")
    o.append("")
    for f, h in heads.items():
        o.append("- `{}`: {}".format(f, h.get("experiment", "")))
    o.append("")
    return o


# ---------------------------------------------------------------- summary

def summary(lat, bw):
    data = {v: load(v)[0] for v, _, _ in VERS}
    vs = [v for v, _, _ in VERS if v != "ring"]
    o = ["# Final results: all versions and the ring baseline", ""]
    o.append("Every experiment was run twice (`run_pass1.pbs`, `run_pass2.pbs`) on the same 3 machines, with the")
    o.append("versions in opposite order the second time. Numbers below are the median latency of one AllReduce,")
    o.append("averaged over the passes and over the ranks. v1 prints only the mean, so its numbers are means.")
    o.append("Everything is read from the logs by `make_final_results.py`.")
    o.append("")
    o += table([[v, "`{}`".format(f), a] for v, f, a in VERS], ["Version", "Folder", "What it is"])
    o.append("")
    if lat:
        t32 = lat.get(32, (None,))[0]
        o.append("**Network floor:** one 32 B RDMA write takes {} one way (about {} round trip); large messages "
                 "move at up to {} MB/s. See `floor/RESULTS.md`.".format(
                     fmt(t32), fmt(2 * t32) if t32 else "-", "{:,.0f}".format(max(bw.values())) if bw else "-"))
        o.append("")

    o += ["## Side by side, 3 ranks", ""]
    for s, label in SIZES:
        if not any(get(data[v], p, 3, s) for v in vs for p, _ in PROGS):
            continue
        o += ["### {}".format(label), ""]
        rows = [[name] + [acell(get(data[v], p, 3, s)) for v in vs] for p, name in PROGS]
        o += table(rows, ["Algorithm"] + vs)
        o.append("")
    o.append("The ring has no versions, so it is compared in the next table and in `baselines/RESULTS.md`.")
    o.append("")

    o += ["## Against the ring baseline (v6 programs, 3 ranks)", "",
          "Speed-up = ring time / program time (above 1x means faster than the ring).", ""]
    cmp_progs = [("Baseline_Barrier", "Baseline (barrier)"), ("LL", "LL"), ("Sentinel", "Sentinel"),
                 ("Twoshot_LL", "Two-shot LL"), ("Twoshot_Sentinel", "Two-shot Sentinel")]
    rows = []
    for s, label in SIZES:
        ring = avg(get(data["ring"], "Ring_AllReduce", 3, s))
        if ring is None:
            continue
        row = [label, fmt(ring)]
        for p, _ in cmp_progs:
            t = avg(get(data["v6"], p, 3, s))
            row.append("{} ({:.2f}x)".format(fmt(t), ring / t) if t else "-")
        rows.append(row)
    o += table(rows, ["Size", "Ring"] + [n for _, n in cmp_progs])
    o.append("")

    o += ["## v3 to v6 for the Two-shot programs, 3 ranks", "",
          "Change relative to v3 (negative = faster).", ""]
    rows = []
    for p in ("Twoshot_LL", "Twoshot_Sentinel"):
        for s, label in SIZES:
            base = avg(get(data["v3"], p, 3, s))
            if base is None:
                continue
            row = [NAME[p], label, fmt(base)]
            for v in ("v4", "v5", "v6"):
                t = avg(get(data[v], p, 3, s))
                row.append("{} ({:+.1f}%)".format(fmt(t), 100 * (t - base) / base) if t else "-")
            rows.append(row)
    o += table(rows, ["Program", "Size", "v3", "v4", "v5", "v6"])
    o.append("")

    o += ["## Did every run finish?", ""]
    rows = []
    for v, _, _ in VERS:
        sts, checks = [], []
        for p in PASSES:
            for _, info in data[v][p].values():
                for r in ranks_of(info).values():
                    sts.append(status(r)); checks += r["checks"]
        rows.append([v, str(sts.count("ok")), str(sts.count("crashed")), str(sts.count("hung")),
                     str(sts.count("wrong result")), str(sts.count("skipped")),
                     "{} / {}".format(checks.count("PASSED"), checks.count("FAILED"))])
    o += table(rows, ["Version", "ranks ok", "crashed", "hung", "wrong result", "skipped", "PASSED / FAILED lines"])
    o.append("")
    o.append("FAILED lines in v3 to v6 come from the negative test, which must fail. Crashes and hangs are listed")
    o.append("with their error messages in each folder's `RESULTS.md`, section \"Correctness and errors\".")
    o.append("")
    return o


def write(path, lines):
    with open(path, "w", encoding="utf-8") as fh:
        fh.write("\n".join(lines) + "\n")
    print("wrote", path)


if __name__ == "__main__":
    fl, lat, bw = floor_file()
    write(os.path.join(OUT, "floor", "RESULTS.md"), fl)
    for v, f, _ in VERS:
        write(os.path.join(OUT, f, "RESULTS.md"), version_file(v))
    write(os.path.join(OUT, "SUMMARY.md"), summary(lat, bw))
