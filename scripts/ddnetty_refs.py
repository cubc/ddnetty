#!/usr/bin/env python3
"""Writes the DDNetty splits reference file of a map: the top players' runs with their
time checkpoint times, and the best time of anyone else in each segment between two
time checkpoints.

The times come from the public DDNet database (https://ddnet.org/stats/, ddnet.sqlite,
table "race": Map, Name, Time, Timestamp, cp1..cp25) or from a CSV with the columns
Name/nick, Time/tempo, Timestamp/data_utc and cp1..cpN / CP1..CPN.

    python3 scripts/ddnetty_refs.py --map Tutorial --db ddnet.sqlite \\
        --me gustavo,23,Verside,1000,loko,soap --exclude cheaters.json

The file goes to <out>/<map>.txt; put it in the client's ddnetty/splits/refs folder
(DDNetty -> Splits -> Target time: Top player -> Open folder).
"""

import argparse
import csv
import json
import os
import re
import sqlite3
import sys
from datetime import datetime, timezone

NUM_TIMECPS = 25
TAS_RE = re.compile(r"\btas\b", re.IGNORECASE)


def read_names(arg):
    """Names from a comma separated list, a JSON list or a text file (one per line)."""
    if not arg:
        return []
    if os.path.isfile(arg):
        with open(arg, encoding="utf-8") as f:
            text = f.read()
        try:
            data = json.loads(text)
            return [str(n) for n in data]
        except json.JSONDecodeError:
            return [line.rstrip("\r\n") for line in text.splitlines() if line.strip() and not line.startswith("#")]
    return [n for n in arg.split(",") if n]


def parse_date(value):
    if value is None:
        return ""
    if isinstance(value, (int, float)):
        return datetime.fromtimestamp(value, tz=timezone.utc).strftime("%Y-%m-%d")
    return str(value)[:10]


def from_sqlite(path, map_name):
    con = sqlite3.connect(path)
    cols = ", ".join(f"cp{i}" for i in range(1, NUM_TIMECPS + 1))
    for name, time, timestamp, *cps in con.execute(f"SELECT Name, Time, Timestamp, {cols} FROM race WHERE Map = ?", (map_name,)):
        yield name, float(time), parse_date(timestamp), [float(c or 0) for c in cps]


def from_csv(path, map_name):
    with open(path, newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        fields = {k.lower(): k for k in reader.fieldnames}
        name_col = fields.get("name") or fields.get("nick")
        time_col = fields.get("time") or fields.get("tempo")
        date_col = fields.get("timestamp") or fields.get("data_utc") or fields.get("date")
        map_col = fields.get("map")
        status_col = fields.get("status")
        cp_cols = [fields[f"cp{i}"] for i in range(1, NUM_TIMECPS + 1) if f"cp{i}" in fields]
        for row in reader:
            if map_col and row[map_col] != map_name:
                continue
            if status_col and row[status_col] and not row[status_col].startswith("ok"):
                continue  # rows another tool already marked (clones, cheaters, ...)
            cps = [float(row[c] or 0) for c in cp_cols]
            yield row[name_col], float(row[time_col]), parse_date(row[date_col]) if date_col else "", cps


def to_ms(seconds):
    return int(round(seconds * 1000))


def run_cps(cps, num_cps):
    """Run time at T0..T(num_cps-1) in ms, -1 where the database has none."""
    out = [to_ms(c) if c > 0 else -1 for c in cps[:num_cps]]
    out += [-1] * (num_cps - len(out))
    return out


def point_name(index, num_cps):
    if index < 0:
        return "S"
    if index >= num_cps:
        return "F"
    return f"T{index}"


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--map", required=True)
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--db", help="ddnet.sqlite from ddnet.org/stats")
    source.add_argument("--csv", help="CSV of finishes")
    parser.add_argument("--me", default="", help="your names (comma separated or a file): one player in the list, left out of the segment bests")
    parser.add_argument("--exclude", default="", help="names to leave out (comma separated, JSON list or text file)")
    parser.add_argument("--top", type=int, default=10, help="how many players (each with their best run)")
    parser.add_argument("--out", default=".", help="folder for <map>.txt")
    args = parser.parse_args()

    me = read_names(args.me)
    me_set = set(me)
    excluded = set(read_names(args.exclude))
    rows = from_sqlite(args.db, args.map) if args.db else from_csv(args.csv, args.map)

    runs = []
    for name, time, date, cps in rows:
        if name in excluded or TAS_RE.search(name):
            continue
        runs.append((name, time, date, cps))
    if not runs:
        sys.exit(f"no finishes of {args.map}")

    num_cps = 0
    for _, _, _, cps in runs:
        for i, c in enumerate(cps):
            if c > 0:
                num_cps = max(num_cps, i + 1)

    # best run of each player (your names count as one player)
    best = {}
    for name, time, date, cps in runs:
        player = me[0] if name in me_set else name
        if player not in best or time < best[player][1]:
            best[player] = (name, time, date, cps)
    top = sorted(best.values(), key=lambda r: r[1])[: args.top]

    # best of anyone else in each segment between consecutive points (S, T0.., F)
    seg = {}
    for name, time, date, cps in runs:
        if name in me_set:
            continue
        points = [0.0] + [c for c in cps[:num_cps]] + [time]
        for i in range(len(points) - 1):
            a, b = points[i], points[i + 1]
            if (i > 0 and a <= 0) or b <= 0 or b <= a:
                continue
            ms = to_ms(b - a)
            key = (i - 1, i)
            if key not in seg or ms < seg[key][0]:
                seg[key] = (ms, date, name)

    os.makedirs(args.out, exist_ok=True)
    path = os.path.join(args.out, f"{args.map}.txt")
    with open(path, "w", encoding="utf-8") as f:
        f.write("# ddnetty splits refs v2\n")
        f.write(f"# map {args.map}, {len(runs)} finishes")
        if excluded:
            f.write(f", {len(excluded)} names left out")
        f.write("\n# ref <ms> <date> <num cps> <cp ms>... <name>\n")
        for name, time, date, cps in top:
            values = " ".join(str(v) for v in run_cps(cps, num_cps))
            f.write(f"ref {to_ms(time)} {date or '-'} {num_cps} {values} {name}\n")
        f.write("# segbest <from> <to> <ms> <date> <name>   (best of anyone else)\n")
        for (a, b), (ms, date, name) in sorted(seg.items()):
            f.write(f"segbest {point_name(a, num_cps)} {point_name(b, num_cps)} {ms} {date or '-'} {name}\n")
    print(f"wrote {path}: {len(top)} runs, {len(seg)} segments, {num_cps} time checkpoints")


if __name__ == "__main__":
    main()
