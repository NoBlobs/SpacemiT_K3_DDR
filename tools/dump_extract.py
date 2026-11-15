#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Thalles Campagnani
"""Extract the host-side phypara / ACSM dumps printed by the SPL
(CONFIG_K3_DDR_PMU_COMPARE_DUMP) from a serial log into CSV files:

  phypara-pmu<N>.csv   k,index,value   (index = PHY CSR index, hex)
  acsm-pmu<N>.csv      j,value

usage: dump_extract.py <serial.log> <out_dir>
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gen_phypara import parse  # noqa: E402

LINE = re.compile(r"PMU(\d) ([PA]) ([0-9a-f]{4}):((?: [0-9a-f]{4})+)")
APPENDIX = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                        "..", "docsClean", "appendix-phypara-order.md")


def main(log, out):
    order = [b + o for b, _, offs in parse(APPENDIX) for o in offs]
    vals = {}
    with open(log, errors="replace") as f:
        for line in f:
            m = LINE.search(line)
            if m:
                key = (m.group(1), m.group(2))
                start = int(m.group(3), 16)
                words = [int(x, 16) for x in m.group(4).split()]
                buf = vals.setdefault(key, {})
                for i, w in enumerate(words):
                    buf[start + i] = w
    os.makedirs(out, exist_ok=True)
    for (ctl, kind), buf in sorted(vals.items()):
        if kind == "P":
            path = os.path.join(out, f"phypara-pmu{ctl}.csv")
            with open(path, "w") as f:
                f.write("k,index,value\n")
                for k in range(len(order)):
                    f.write(f"{k},0x{order[k]:06x},0x{buf.get(k, 0):04x}\n")
        else:
            path = os.path.join(out, f"acsm-pmu{ctl}.csv")
            with open(path, "w") as f:
                f.write("j,value\n")
                for j in sorted(buf):
                    f.write(f"{j},0x{buf[j]:04x}\n")
        print(path, len(buf))


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    main(sys.argv[1], sys.argv[2])
