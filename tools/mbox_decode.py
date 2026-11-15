#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Thalles Campagnani
"""Decode PMU mailbox lines printed by the SPL (CONFIG_K3_DDR_PMU_MBOX_LOG).

  PMU0 M 20 @123456
  PMU0 S 0810 4: 00000004 00000000 00000060 00000000 @123500

Prints one decoded line per item; IDs of this project (0x08xx) are expanded.
"""
import re
import sys

M = re.compile(r"PMU(\d) M ([0-9a-f]+) @(\d+)")
S = re.compile(r"PMU(\d) S ([0-9a-f]{4}) (\d+):((?: [0-9a-f]{8})*) @(\d+)")

MAJORS = {0x07: "DONE", 0x08: "STREAM", 0x20: "probe start", 0x21: "probe BCR",
          0x22: "probe RTC", 0x23: "probe msgblk", 0x24: "probe ISA",
          0x25: "probe IMEM", 0x30: "train start"}


def bcr(w):
    return ", ".join(f"aux[{w[i]:#05x}]={w[i+1]:#010x}" for i in range(0, len(w) - 1, 2))


def rtc(w):
    return f"tag={w[0]} ctrl={w[1]:#x} count={(w[3] << 32) | w[2]}"


def dmem(w):
    return " ".join(f"{x >> 16:03x}:{x & 0xffff:04x}" for x in w)


ISA_NAMES = ["mul64.lo", "mul64.hi", "5/4", "5%4", "-7/2", "-7%2",
             "dbnz.d(5)", "add3(0,1024)", "a/256", "version"]


def isa(w):
    return ", ".join(f"{n}={v:#x}" for n, v in zip(ISA_NAMES, w))


DECODERS = {0x0800: ("BOOT", lambda w: f"id={w[0]:#x} ver={w[1]:#x}"),
            0x0801: ("EXCEPTION", lambda w: "ecr=%#x eret=%#x efa=%#x sp=%#x" % tuple(w[:4])),
            0x0802: ("ASSERT", lambda w: f"line={w[0]} value={w[1]:#x}"),
            0x0803: ("STAGE", lambda w: f"bit={w[0]} status={w[1]}"),
            0x0810: ("BCR", bcr), 0x0811: ("RTC", rtc), 0x0813: ("DMEM", dmem),
            0x0814: ("MEM", lambda w: " ".join(f"{x:#x}" for x in w)),
            0x0815: ("ISA", isa)}


def main(path):
    last_rtc = {}
    with open(path, errors="replace") as f:
        for line in f:
            m = M.search(line)
            if m:
                ctl, code, t = int(m.group(1)), int(m.group(2), 16), int(m.group(3))
                print(f"{t:>12} PMU{ctl} major {code:#04x} {MAJORS.get(code, '')}")
                continue
            m = S.search(line)
            if not m:
                continue
            ctl, sid, n = int(m.group(1)), int(m.group(2), 16), int(m.group(3))
            words = [int(x, 16) for x in m.group(4).split()]
            t = int(m.group(5))
            name, fn = DECODERS.get(sid, (f"id {sid:#06x}", lambda w: " ".join(f"{x:08x}" for x in w)))
            extra = ""
            if sid == 0x0811 and len(words) >= 4:
                cnt = (words[3] << 32) | words[2]
                if ctl in last_rtc:
                    pc, pt = last_rtc[ctl]
                    if t > pt:
                        extra = f"  ~{(cnt - pc) / (t - pt):.3f} ticks/us"
                last_rtc[ctl] = (cnt, t)
            if len(words) != n:
                extra += f"  !! len {n} but {len(words)} words"
            print(f"{t:>12} PMU{ctl} {name}: {fn(words)}{extra}")


if __name__ == "__main__":
    main(sys.argv[1])
