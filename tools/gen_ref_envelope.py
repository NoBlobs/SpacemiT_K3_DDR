#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Thalles Campagnani
"""Build the reference DDRT envelope fixture from docsClean.

usage: gen_ref_envelope.py <appendix-reference-envelope*.md> <out.bin>

Each appendix (the SPI capture, or one of the USB/FDL captures E1..K3)
publishes a 6400 MT/s reference envelope as seven hex blocks:
header (128 B), then per controller the message block (320 B), phypara
(5170 u16) and ACSM SRAM (4096 u16). The fixture is the 0x9800-byte DDRT
object with a zero tail and the CRC-32 over [0x08, 0x9800) recomputed.
For development and test only (experiments 011, 012).
"""
import re
import struct
import sys
import zlib

ENV_SIZE = 0x9800
REC_SIZE = 0x49a4
MSG_SIZE = 0x140
PHYPARA = 5170
ACSM = 4096
CRC_RE = re.compile(r"zlib\.crc32\(buf\[0x08:0x9800\]\)[^0-9]*(0x[0-9a-f]{8})")


def values(block):
    out = []
    for line in block.splitlines():
        if ":" in line:
            out += [int(t, 16) for t in line.split(":", 1)[1].split()]
    return out


def main(src, dst):
    text = open(src).read()
    m = CRC_RE.search(text) or re.search(r"\(`(0x[0-9a-f]{8})`\)", text)
    if not m:
        sys.exit("no zero-padded CRC found in %s" % src)
    expected = int(m.group(1), 16)
    blocks = re.findall(r"```text\n(.*?)\n```", text, re.S)
    if len(blocks) != 7:
        sys.exit("expected 7 hex blocks, found %d" % len(blocks))
    env = bytearray(ENV_SIZE)
    hdr = bytes(values(blocks[0]))
    if len(hdr) != 0x80:
        sys.exit("header is %d bytes" % len(hdr))
    env[0:0x80] = hdr
    for ctl in range(2):
        msg, phy, acsm = (values(b) for b in blocks[1 + 3 * ctl:4 + 3 * ctl])
        if (len(msg), len(phy), len(acsm)) != (MSG_SIZE, PHYPARA, ACSM):
            sys.exit("DDR%d sizes %d/%d/%d" % (ctl, len(msg), len(phy), len(acsm)))
        rec = bytes(msg) + struct.pack("<%dH" % PHYPARA, *phy) + struct.pack("<%dH" % ACSM, *acsm)
        assert len(rec) == REC_SIZE
        off = 0x80 + ctl * REC_SIZE
        env[off:off + REC_SIZE] = rec
    crc = zlib.crc32(env[0x08:]) & 0xffffffff
    if crc != expected:
        sys.exit("CRC %#010x, docsClean says %#010x" % (crc, expected))
    struct.pack_into("<I", env, 0x04, crc)
    open(dst, "wb").write(env)
    print("%s: %d bytes, crc %#010x" % (dst, len(env), crc))


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    main(sys.argv[1], sys.argv[2])
