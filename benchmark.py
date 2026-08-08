#!/usr/bin/env python3
"""
Repeatedly run the compiler, parse per-phase timings from its log file,
and report median (plus min/max/mean/stddev) per phase.

Usage:
    python3 bench.py --runs 20 --cmd "./zcp-dev tests/program.z" --log out.log

    Lexing:       20.318 us
    Preprocessing:216.296 us
i.e. "<PhaseName>:<whitespace?><number> <unit>", unit one of ns/us/ms/s.
Adjust PHASE_LINE_RE below if your format differs.
"""

import argparse
import re
import statistics
import subprocess
import sys
from collections import defaultdict

# Matches:  "Lexing:   20.318 us"  /  "Preprocessing:216.296 us"
# group 1 = phase name (letters/spaces), group 2 = number, group 3 = unit
PHASE_LINE_RE = re.compile(r"^\s*([A-Za-z][A-Za-z ]*?)\s*:\s*([0-9]*\.?[0-9]+)\s*(ns|us|ms|s)\b")

# normalize every time to microseconds so phases with different units compare
UNIT_TO_US = {"ns": 1e-3, "us": 1.0, "ms": 1e3, "s": 1e6}


def parse_log(text):
    """Return dict {phase_name: time_in_us} for one run's log."""
    times = {}
    for line in text.splitlines():
        m = PHASE_LINE_RE.match(line)
        if not m:
            continue
        name = m.group(1).strip()
        value = float(m.group(2))
        unit = m.group(3)
        # Skip a "Total" line if present — we report per-phase; comment out to include.
        if name.lower() == "total":
            continue
        times[name] = value * UNIT_TO_US[unit]
    return times


def read_source(path):
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        return f.read()


def fmt_us(us):
    """Human-friendly: pick us or ms."""
    if us >= 1000:
        return f"{us/1000:.3f} ms"
    return f"{us:.3f} us"


def main():
    ap = argparse.ArgumentParser(description="Benchmark compiler phase timings (median over N runs).")
    ap.add_argument("--runs", type=int, default=20, help="number of runs (default 20)")
    ap.add_argument("--cmd", required=True,
                    help='the compiler command to run, e.g. "./zcp-dev tests/prog.z"')
    ap.add_argument("--log", required=True,
                    help="path to the log file the compiler writes each run")
    ap.add_argument("--warmup", type=int, default=1,
                    help="warmup runs to discard (default 1, to skip cold-cache first run)")
    ap.add_argument("--timeout", type=float, default=30.0,
                    help="per-run timeout in seconds (default 30)")
    args = ap.parse_args()

    cmd_parts = args.cmd.split()
    samples = defaultdict(list)   # phase -> [us, us, ...]
    total_runs = args.runs + args.warmup
    failures = 0

    for i in range(total_runs):
        is_warmup = i < args.warmup
        try:
            proc = subprocess.run(cmd_parts, capture_output=True, text=True,
                                  timeout=args.timeout)
        except subprocess.TimeoutExpired:
            print(f"run {i+1}: TIMEOUT", file=sys.stderr)
            failures += 1
            continue
        except FileNotFoundError:
            print(f"error: command not found: {cmd_parts[0]}", file=sys.stderr)
            sys.exit(1)

        if proc.returncode != 0:
            # compiler may exit nonzero on programs with errors; still try to read the log
            print(f"run {i+1}: exit code {proc.returncode} "
                  f"(continuing; stderr tail: {proc.stderr.strip()[-120:]})",
                  file=sys.stderr)

        try:
            log_text = read_source(args.log)
        except FileNotFoundError:
            print(f"run {i+1}: log file '{args.log}' not found", file=sys.stderr)
            failures += 1
            continue

        times = parse_log(log_text)
        if not times:
            print(f"run {i+1}: no phase timings parsed from log "
                  f"(check PHASE_LINE_RE against your format)", file=sys.stderr)
            failures += 1
            continue

        if is_warmup:
            continue
        for phase, us in times.items():
            samples[phase].append(us)

    if not samples:
        print("No samples collected. Check --cmd, --log, and the log format.", file=sys.stderr)
        sys.exit(1)

    # Preserve a sensible phase order if these names appear; unknown phases appended after.
    preferred = ["Lexing", "Preprocessing", "Parsing", "Analysis", "CodeGen", "Optimization"]
    ordered = [p for p in preferred if p in samples] + \
              [p for p in samples if p not in preferred]

    n = min(len(v) for v in samples.values())
    print(f"\n{args.runs} run(s) requested, {n} counted "
          f"({args.warmup} warmup discarded, {failures} failed)\n")

    header = f"{'Phase':<16}{'median':>12}{'min':>12}{'max':>12}{'mean':>12}{'stddev':>12}"
    print(header)
    print("-" * len(header))

    median_total = 0.0
    for phase in ordered:
        vals = samples[phase]
        med = statistics.median(vals)
        median_total += med
        lo = min(vals)
        hi = max(vals)
        mean = statistics.mean(vals)
        sd = statistics.pstdev(vals) if len(vals) > 1 else 0.0
        print(f"{phase:<16}{fmt_us(med):>12}{fmt_us(lo):>12}"
              f"{fmt_us(hi):>12}{fmt_us(mean):>12}{fmt_us(sd):>12}")

    print("-" * len(header))
    print(f"{'Total (medians)':<16}{fmt_us(median_total):>12}")
    print()


if __name__ == "__main__":
    main()