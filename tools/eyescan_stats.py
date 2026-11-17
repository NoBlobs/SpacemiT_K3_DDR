#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Thalles Campagnani
"""Summarise the host read-eye scan lines of a serial log (experiment 015).

usage: eyescan_stats.py <serial.log>

Line format (SPL, K3_DDR_REF_EYESCAN):
  ES <phy> <csr> orig <hex> win <l>..<r> row <hex64> blk <hex64> bits <hex32> errs <n>
"""
import re
import statistics as st
import sys

LINE = re.compile(r"ES (\d) (\w{5}) orig (\w+) win (-?\d+)\.\.(-?\d+) row (\w{16}) "
                  r"blk (\w{16}) bits (\w{8}) errs (\d+)")
TOP = 126


def main(path):
    rows = []
    for line in open(path, errors="replace"):
        m = LINE.search(line)
        if m:
            c, idx, orig, l, r, row, blk, bits, errs = m.groups()
            rows.append(dict(c=int(c), idx=int(idx, 16), orig=int(orig, 16), l=int(l), r=int(r),
                             row=int(row, 16), blk=int(blk, 16), bits=int(bits, 16), errs=int(errs)))
    print("%d scans" % len(rows))
    for c in (0, 1):
        for reg in (0x4e, 0x4f):
            s = [x for x in rows if x["c"] == c and (x["idx"] & 0xff) == reg and ((x["idx"] >> 8) & 0xf) < 8]
            eff = [x for x in s if x["errs"]]
            closed = [x for x in eff if x["l"] > 0 and x["r"] < TOP]
            print("PHY%d 0x%02x: %d bits, no effect %d, open left %d, open right %d, closed %d" % (
                c, reg, len(s), len(s) - len(eff), sum(x["l"] == 0 for x in eff),
                sum(x["r"] == TOP for x in eff), len(closed)))
            if not eff:
                continue
            if closed:
                w = [x["r"] - x["l"] + 2 for x in closed]
                off = [x["orig"] - (x["l"] + x["r"]) / 2 for x in closed]
                print("   closed: width %d/%d/%d (min/med/max), trained - centre %.0f/%.0f/%.0f" % (
                    min(w), st.median(w), max(w), min(off), st.median(off), max(off)))
            print("   trained %d..%d, margin to left edge >= %d, to right edge >= %d" % (
                min(x["orig"] for x in eff), max(x["orig"] for x in eff),
                min(x["orig"] - x["l"] for x in eff), min(x["r"] - x["orig"] for x in eff)))
            bad = [x for x in eff if x["bits"] != 1 << (((x["idx"] >> 12) & 0xf) * 8 + ((x["idx"] >> 8) & 0xf))]
            print("   data-bit mismatches %d, scans not failing in all blocks %d" % (
                len(bad), sum(x["blk"] != (1 << 64) - 1 for x in eff)))
    dmi = [x for x in rows if ((x["idx"] >> 8) & 0xf) == 8]
    print("lane 8 scans with effect: %d of %d" % (sum(1 for x in dmi if x["errs"]), len(dmi)))


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    main(sys.argv[1])
