#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Thalles Campagnani
"""Split a PMU ELF into the two images loaded by the host SPL.

  <out>.imem.bin  .text, loaded at PHY index 0x50000 (ARC 0x00000000)
  <out>.dmem.bin  .rodata + .data, loaded at PHY index 0x5a000 (ARC 0x80004000)

Both are little-endian and padded to a whole number of halfwords, because the
host writes one 16-bit value per 32-bit PHY register slot.
"""
import argparse
import hashlib
import os
import subprocess
import sys

IMEM_MAX = 0x10000
DMEM_BASE = 0x80004000
DMEM_MAX = 0x8000e000 - DMEM_BASE


def objcopy(tool, elf, out, sections):
    cmd = [tool, "-O", "binary"]
    for s in sections:
        cmd += ["-j", s]
    subprocess.run(cmd + [elf, out], check=True)
    with open(out, "rb") as f:
        data = f.read()
    if len(data) % 2 or not data:
        data += b"\0" * (2 - len(data) % 2)
        with open(out, "wb") as f:
            f.write(data)
    return data


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--objcopy", default="arc-elf32-objcopy")
    ap.add_argument("elf")
    ap.add_argument("out")
    a = ap.parse_args()

    imem = objcopy(a.objcopy, a.elf, a.out + ".imem.bin", [".text"])
    dmem = objcopy(a.objcopy, a.elf, a.out + ".dmem.bin", [".rodata", ".data"])
    if len(imem) > IMEM_MAX:
        sys.exit(f"IMEM image {len(imem)} > {IMEM_MAX}")
    if len(dmem) > DMEM_MAX:
        sys.exit(f"DMEM image {len(dmem)} > {DMEM_MAX}")
    if len(imem) < 0x200 or imem[0:4] != (0x1fc).to_bytes(4, "little"):
        sys.exit("reset vector entry 0 does not point to 0x1fc")
    with open(a.out + ".sha256", "w") as f:
        for p in (a.out + ".imem.bin", a.out + ".dmem.bin", a.elf):
            with open(p, "rb") as g:
                f.write(f"{hashlib.sha256(g.read()).hexdigest()}  {os.path.basename(p)}\n")
    print(f"{a.out}: imem {len(imem)} B ({len(imem)//2} hw), dmem {len(dmem)} B")


if __name__ == "__main__":
    main()
