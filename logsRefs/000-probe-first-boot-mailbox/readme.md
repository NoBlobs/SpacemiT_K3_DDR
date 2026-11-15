<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# 000 — First boot of the probe firmware: reset vector and mailbox polarity

- **Date:** 2026-11-15
- **Goal:** run the M1 probe firmware on both PMUs and see whether it starts
  and talks over the mailbox.
- **Claims tested:**
  - docsClean/12 and /13 BR-01: effective startup at `0x1fc`, reached from a
    reset entry at `0x0`;
  - docsClean/13 BR-09: PMU send = wait `0x90180008[0] == 0`, write data,
    `0x90180066 = 1`, wait `0x90180008[0] == 1`, `0x90180066 = 0`.
- **Firmware:** working tree on top of `81ea2b5` (uncommitted); hashes in
  `probe.sha256` (run 2 build with breadcrumbs).
- **FSBL:** U-Boot `289dc2b7bd` + uncommitted branch `k3-free-ddr-fw` changes,
  fragment `configs/k3_freefw_probe.config` (`uboot.config`), hashes in
  `fsbl.sha256`.
- **Configuration:** MT62F2G32D4DS, 2 ranks, x16, 6400 MT/s; message block as
  written by the vendor SPL table; mailbox timeout 10 s.

## Procedure

1. Run 1 (`serial-first.log`): probe firmware without diagnostics,
   `tools/bench.sh build/fsbl-probe/FSBL.bin probe`.
2. Power cycle (by hand).
3. Run 2 (`serial-bcrumb.log`): same probe with DMEM breadcrumbs at ARC
   `0x80003f00` (host index `0x59f80`), and an SPL dump after timeout of the
   mailbox registers, the breadcrumbs and the first words of IMEM / our DMEM
   image.

## Result

Run 1: the SPL prints `LPDDR5 total size: 16 GB, data rate: 6400 MT/s`, then
both controllers time out with no mailbox item (`last code 0x0`).

Run 2, identical on PMU0 and PMU1:

```
PMU0 D regs: d0004=0003 d0031=0001 d0032=0000 d0034=0000 c0004=0001 c0032=0000 c0033=0001 c0034=0000 d0000=0000 d0099=0001 c0080=0003
PMU0 D bcrumb: c0de 0001 0001 0000 0001 0001 0000 ...
PMU0 D imem: 01fc 0000 0258 0000 0258 0000 0258 0000 @1fc: 200a 0f80 8000 3f00
PMU0 D dmem5a000: 4f42 5052 0000 0001 ...
```

Breadcrumb slots: 0 = `c0de` (`_start` ran), 1 = `0001` (`main` entered),
2 = `0001` (status `0x90180008` before the first send), 3 = `0` (no item sent),
4 = `1` (stuck in "wait free"), 5 = `0001` (status while waiting).

The IMEM and DMEM images read back as built (vector entry 0 = `0x1fc`, our
`.rodata` "PROB" string at `0x80004000`).

## Conclusion

- **Confirmed:** a 127-entry ARCv2 vector table whose entry 0 holds the
  address `0x1fc` starts the PMU at `0x1fc`; `.bss` zeroing, stack, DMEM 16-bit
  stores and the SPL-loaded DMEM image at index `0x5a000` all work.
- **Confirmed:** host index `i` ↔ ARC `0x90000000 + 2*i` for the mailbox block:
  the PMU's `0x90180008` is host index `0xc0004`, `0x90180066` is `0xc0033`.
- **Refuted (polarity):** docsClean/13 BR-09 has both PMU-side polarities
  inverted. At rest, with the host idle (`DCT_WRITE_PROT` `0xd0031 = 1`), the
  PMU status `0x90180008[0]` reads **1** and `0x90180066` reads **1**. A
  firmware that waits for status 0 before sending waits forever. Hypothesis,
  consistent with the host side in docsClean/08: PMU send = wait status bit 0
  = 1, write data, `0x90180066 = 0`, wait status bit 0 = 0 (host wrote
  `DCT_WRITE_PROT = 0`), `0x90180066 = 1`. To be confirmed by experiment 001.
- Host `UCT_SHADOW` (`0xd0004`) reads `3` at rest: bit 0 = 1 matches "no item
  pending"; bit 1 meaning unknown.

## Documentation updated

- `docs/hardware/mailbox.rst` — PMU-side polarity.
- `docs/hardware/pmu-arc-core.rst` — reset vector confirmed.
- `docs/firmware/mailbox-pmu.rst`, `docs/firmware/memory-layout.rst`.
