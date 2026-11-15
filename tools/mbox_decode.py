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
          0x25: "probe IMEM", 0x26: "csrdump start", 0x27: "csrdump phypara",
          0x28: "csrdump acsm", 0x29: "csrscan", 0x2a: "pllprobe", 0x30: "train start"}
MAJORS.update({0x40 + b: f"stage bit {b}" for b in range(16)})


def bcr(w):
    return ", ".join(f"aux[{w[i]:#05x}]={w[i+1]:#010x}" for i in range(0, len(w) - 1, 2))


def rtc(w):
    return f"tag={w[0]} ctrl={w[1]:#x} count={(w[3] << 32) | w[2]}"


def dmem(w):
    return " ".join(f"{x >> 16:03x}:{x & 0xffff:04x}" for x in w)


PLL_REGS = [0x200c3, 0x200c5, 0x200c6, 0x200c7, 0x200c9, 0x200ca, 0x200cc,
            0x200cd, 0x200cf, 0x300bf, 0x100e0, 0x20047, 0x200e3, 0x20011]

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
            0x0815: ("ISA", isa),
            0x0820: ("CSRDUMP", lambda w: "phypara n=%d nz=%d crc=%#x acsm n=%d nz=%d crc=%#x ticks=%d/%d" % tuple(w[:8])),
            0x0821: ("CSRSCAN", lambda w: "0x%05x+%d: " % (w[0], w[1]) + " ".join(
                "%04x" % ((w[2 + k // 2] >> (16 * (k & 1))) & 0xffff) for k in range(w[1]))),
            0x0822: ("PLLPROBE", lambda w: "tag=%#04x t=%dus " % (w[0], w[1]) + " ".join(
                "%05x=%04x" % (r, (w[2 + k // 2] >> (16 * (k & 1))) & 0xffff) for k, r in enumerate(PLL_REGS))),
            0x0830: ("DEVINIT_CAL", lambda w: ("e3=%#x e5=%#x tca=%#x(%d) tdq=%#x(%d) 2*tca=%d n=%d freqval=%d n=%d 0x100d5=%#x ratio=%d"
                                               % (w[0], w[1], w[2], w[2], w[3], w[3], w[4], w[5], w[6], w[7], w[8], w[9]))),
            0x0831: ("DEVINIT_STATE", lambda w: "phase=%d 0xc0086=%#x 0x20011=%#x 0x2000a=%#x 0x20017=%#x 0x20012=%#x 0x20186=%#x 0x20187=%#x" % tuple(w[:8])),
            0x0832: ("DEVINIT_WATCH", lambda w: "t=%.3fus 0x300bf=%#x 0x100e0=%#x 0x100d5=%#x" % (w[0] / 400, w[1], w[2], w[3])),
            0x0833: ("DEVINIT_DMEM", lambda w: " ".join("%#010x=%04x" % (w[i], w[i + 1]) for i in range(0, len(w) - 1, 2))),
            0x0834: ("DEVINIT_INPUT", lambda w: ("byte0=%#x MsgMisc=%#x Pstate=%#x PllBypassEn=%d DRAMFreq=%d byte8=%#x "
                                                 "Quickboot=%d csA=%d csB=%d 0xc0080=%#x ranks=%d" % tuple(w[:11]))),
            0x0835: ("DEVINIT_ACSM", lambda w: "tag=%#x polls=%d busy=%#x" % tuple(w[:3]) + (" " + " ".join("%#x" % x for x in w[3:]) if len(w) > 3 else "")),
            0x0836: ("DEVINIT_CAL", lambda w: ("e3=%#x e5=%#x pre 300bf=%#x 100e0=%#x | e3(hi)=%#x latched 300bf=%#x(%d) 100e0=%#x(%d) "
                                               "100d5=%#x" % (w[0], w[1], w[2], w[3], w[4], w[5], w[5] & 0x1ff, w[6], w[6] & 0x1ff, w[7]))
                     + ((" f_lo=%d f_hi=%d 20012=%#06x 20186=%#06x" % tuple(w[8:12])) if len(w) >= 12 else "")),
            0x0837: ("DEVINIT_STATE", lambda w: ("tag=%#04x t=%dus c0086=%#x 20011=%#x 20060=%#x 2000a=%#x 20017=%#x "
                                                 "20012=%#x 20186=%#x 20187=%#x 100d5=%#x 100a3=%#x" % tuple(w[:12]))),
            0x0838: ("DEVINIT_LATCH", lambda w: ("tag=%#04x wait|200a2=%d e3=%#x e5=%#x pre 300bf=%#x 100e0=%#x -> 300bf=%#x(%d) 100e0=%#x(%d)"
                                                 % (w[0], w[1], w[2], w[3], w[4], w[5], w[6], w[6] & 0x1ff, w[7], w[7] & 0x1ff))),
            0x0839: ("DEVINIT_REGS", lambda w: ("tag=%#04x t=%dus 200c9=%#x 20021=%#x 200e0=%#x 200e1=%#x 200e2=%#x 200a2=%#x "
                                                "200f8=%#x 20078=%#x 20061=%#x 200c3=%#x 20047=%#x" % tuple(w[:13]))),
            0x083a: ("DEVINIT_PHASE0", lambda w: ("byte403=%#x dmem424=%#06x dmem426=%#06x | 9002e=%#x 9002f=%#x 90029=%#x 9002a=%#x "
                                                  "| written lo=%#x hi=%#x | mult=%d 0x8000040c=%#x" % tuple(w[:11]))),
            0x083b: ("DEVINIT_DESC", lambda w: "desc[%d] csr=%#07x val=%#x replaced=%d" % tuple(w[:4])),
            0x083c: ("DEVINIT_DQCAL", lambda w: "rank=%d k=%d d5=%#x lane0 d0..d4=%#x %#x %#x %#x %#x d5now=%#x" % tuple(w[:9])),
            0x083d: ("DEVINIT_ACSMTRY", lambda w: "tag=%#x busy before=%d start=%d poll=%d polls=%d after-stop=%d" % tuple(w[:6])),
            0x083e: ("DEVINIT_DQCAL", lambda w: "rank=%d d5=%#x k[l0..8]=%s fallbacks=%d 0x01r0ad=%d" % (
                w[0], w[1], "".join(str((w[2] >> (3 * l)) & 7) for l in range(9)), w[3], w[4])),
            0x083f: ("DEVINIT_PHASE0", lambda w: ("ver=%d dmem424=%#06x dmem426=%#06x c00ee=%#x branch=%d | read-back lo=%#x hi=%#x "
                                                  "| mult=%d pair=%#x" % (w[0], w[1], w[2], w[3], w[4], w[7], w[8], w[9], w[10])))}


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
