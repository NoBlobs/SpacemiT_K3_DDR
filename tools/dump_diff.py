#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Thalles Campagnani
"""Diff two phypara CSV dumps (k,index,value) produced by dump_extract.py.

usage: dump_diff.py <before.csv> <after.csv>
Prints one line per changed CSR, grouped by CSR group, and a summary.
"""
import csv
import sys
from collections import Counter


def load(path):
    with open(path) as f:
        return {int(r["index"], 16): int(r["value"], 16) for r in csv.DictReader(f)}


def main(a_path, b_path):
    a, b = load(a_path), load(b_path)
    changed = sorted(i for i in a if i in b and a[i] != b[i])
    groups = Counter(i >> 16 for i in changed)
    for i in changed:
        print(f"0x{i:06x}  0x{a[i]:04x} -> 0x{b[i]:04x}")
    print(f"# {len(changed)} changed of {len(a)}; by group: "
          + ", ".join(f"0x{g:02x}:{n}" for g, n in sorted(groups.items())))


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    main(sys.argv[1], sys.argv[2])
