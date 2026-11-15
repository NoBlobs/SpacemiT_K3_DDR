#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Thalles Campagnani
"""Extract the PHY CSR writes performed by the vendor U-Boot SPL tables
(GPL host source) as an index -> value map.

Parses `static const phy_init_config training_table_N = { base, count, linear,
{ {{a, b}}, ... } };` definitions and the `<list>[]` arrays that order them.
Tables based in IMEM/DMEM (0x50000-0x5ffff: firmware image and vendor DMEM
image) are skipped and never printed.

usage: spl_tables.py <table.c> <list_name> [override_array] > writes.csv
"""
import re
import sys

TABLE = re.compile(r"static const phy_init_config (\w+) = \{\s*(0x[0-9a-fA-F]+|\d+)\s*,\s*(\d+)\s*,\s*(true|false)\s*,\s*\{(.*?)\}\s*\};", re.S)
PAIR = re.compile(r"\{\{\s*(0x[0-9a-fA-F]+|\d+)\s*,\s*(0x[0-9a-fA-F]+|\d+)\s*\}\}")
OVR = re.compile(r"\{\s*(0x[0-9a-fA-F]+)\s*,\s*(0x[0-9a-fA-F]+|\d+)\s*\}")
SKIP_LO, SKIP_HI = 0x50000, 0x60000


def parse(path, list_name, override=None):
    src = open(path, encoding="utf-8", errors="replace").read()
    tables = {}
    for m in TABLE.finditer(src):
        name, base, count, linear, body = m.groups()
        base, count = int(base, 0), int(count)
        if SKIP_LO <= base < SKIP_HI:
            continue
        pairs = [(int(a, 0), int(b, 0)) for a, b in PAIR.findall(body)]
        writes = []
        if linear == "true":
            vals = [v for p in pairs for v in p][:count]
            writes = [(base + i, v) for i, v in enumerate(vals)]
        else:
            writes = [(base + a, b) for a, b in pairs[:count]]
        tables[name] = writes
    lm = re.search(r"\b%s\[\]\s*=\s*\{(.*?)\};" % re.escape(list_name), src, re.S)
    order = re.findall(r"&(\w+)", lm.group(1)) if lm else sorted(tables)
    out = {}
    for name in order:
        for i, v in tables.get(name, []):
            out[i] = v
    if override:
        om = re.search(r"\b%s\[\]\s*=\s*\{(.*?)\};" % re.escape(override), src, re.S)
        for a, b in OVR.findall(om.group(1)):
            a, b = int(a, 0), int(b, 0)
            if a == 0:
                break
            if SKIP_LO <= a < SKIP_HI:
                continue
            if b != 0xdeadbeef:
                out[a] = b
    return out


if __name__ == "__main__":
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    w = parse(sys.argv[1], sys.argv[2], sys.argv[3] if len(sys.argv) > 3 else None)
    print("index,value")
    for i in sorted(w):
        print(f"0x{i:06x},0x{w[i]:04x}")
