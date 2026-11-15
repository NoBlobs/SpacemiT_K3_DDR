#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Thalles Campagnani
"""Extract csrscan stream frames (id 0x0821) from a serial log into CSV files
scan-pmu<N>.csv with columns index,value[,host_table].

usage: scan_extract.py <serial.log> <out_dir> [host_table.csv]
host_table.csv (from spl_tables.py) adds the value the host table writes.
"""
import csv
import os
import re
import sys

S = re.compile(r"PMU(\d) S 0821 (\d+):((?: [0-9a-f]{8})*)")


def main(log, out, table=None):
    host = {}
    if table:
        with open(table) as f:
            host = {int(r["index"], 16): int(r["value"], 16) for r in csv.DictReader(f)}
    vals = {}
    for line in open(log, errors="replace"):
        m = S.search(line)
        if not m:
            continue
        w = [int(x, 16) for x in m.group(3).split()]
        first, n = w[0], w[1]
        d = vals.setdefault(m.group(1), {})
        for k in range(n):
            d[first + k] = (w[2 + k // 2] >> (16 * (k & 1))) & 0xffff
    os.makedirs(out, exist_ok=True)
    for ctl, d in sorted(vals.items()):
        p = os.path.join(out, f"scan-pmu{ctl}.csv")
        with open(p, "w") as f:
            f.write("index,value,host_table\n")
            for i in sorted(d):
                h = f"0x{host[i]:04x}" if i in host else ""
                f.write(f"0x{i:06x},0x{d[i]:04x},{h}\n")
        print(p, len(d))


if __name__ == "__main__":
    if len(sys.argv) not in (3, 4):
        sys.exit(__doc__)
    main(*sys.argv[1:])
