<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# 012 — Host restore of the USB/FDL envelopes (E1–K3) with the docsClean/11 rules: 3 of 10 boots

- **Date:** 2026-11-15
- **Goal:** repeat experiment 011 with the envelope that matches our boot
  path (USB/FDL, 60 Ω RX ODT) and the host-side restore rules of
  docsClean/11 (4b64572), to tell a restore-procedure problem from an
  operating-point mismatch.
- **Fixtures:** `tools/gen_ref_envelope.py docsClean/appendix-reference-envelope-fdl-<x>.md`
  → `build/envelope/ref6400-fdl-<x>.bin` for all nine USB/FDL envelopes
  (`E1`–`E3`, `G1`–`G3`, `K1`–`K3`; each 0x9800 bytes, CRC = the docsClean
  value). Same board unit, same USB/FDL boot path. `uboot.config` is the E1
  fragment; the other runs differ only in `K3_DDR_REF_ENVELOPE_PATH`.
- **Firmware:** `train`, unchanged from experiment 011 (`inputs.sha256`).
- **SPL** (`uboot.config`, `uboot.rev`): as experiment 011, with the
  restore rewritten per docsClean/11:
  1. keep live: `0x2000a`, `0x20017`, `0x20187`, `0x20011`, `0xc0086`;
     `0x90808/13 = (0x20314/15 << 8) | 1` from the live registers;
  2. never write: `0x20020/27/28/2a/31/41/60/78`;
  3. everything else from the envelope (trained delays, `0x01r0ad/0af`,
     `0x01r00f = 0x1089`, `0x01r057`), with the commit triad
     `0x20028 = 1`, `0x2f0a5 = 1`, `0x2f077 = 0` after each of the 69
     `phypara` groups (question N2: the group granularity is our choice);
  4. ACSM image, `0xD00E7 = 0x600`, `UCCLK = 2`, `MUX_SEL = 1`, DDRC
     activation, 16 GiB sampled memory test, `test_pattern`, halt.

## Results

`run1`, `run2`: envelope E1; `env-<x>`: envelope `<x>`, one boot each.

| Boot | Envelope | Memory test (2,097,152 words) | Failing bits | Unstable on re-read | `test_pattern` |
|---|---|---|---|---|---|
| run1 | E1 | **0** | — | — | **0** |
| run2 | E1 | 554,165 (26.4 %) | 8–23 | 34,532 | 11 |
| env-e2 | E2 | 195,973 (9.3 %) | 16–23 | 13,921 | 11 |
| env-e3 | E3 | 177,903 (8.5 %) | 16–23 | 7,325 | 11 |
| env-g1 | G1 | 373,073 (17.8 %) | 8–15 | 26,644 | 11 |
| env-g2 | G2 | 359,530 (17.1 %) | 8–15 | 21,671 | 11 |
| env-g3 | G3 | 260,813 (12.4 %) | 8–15 | 181,207 | **0** |
| env-k1 | K1 | **0** | — | — | **0** |
| env-k2 | K2 | 153,772 (7.3 %) | 16–23 | 72 | 11 |
| env-k3 | K3 | **0** | — | — | **0** |

Errors on other bits are ≤ 33 per bit in every boot.

- **3 of 10 boots clean** (E1 run 1, K1, K3). The other boots fail on one
  or both of byte lanes 1 (bits 8–15) and 2 (bits 16–23). Bytes 0 and 3
  never fail. Errors are spread over all 64 blocks (all 16 GiB).
- **Error shape** (the pattern is the address): on bits 8–15 the lane
  returns the byte of another word in the same 1 KiB (`0x102000300` reads
  `0x02000000`; `0x102000140` reads `0x02000040`), i.e. data from an
  earlier/later burst. On bits 16–23 it reads `0xff` (`0x112000a44` reads
  `0x12ff0a44`), `0x00`, or a single bit flipped (`0x04`, `0x20`, `0x80`).
  Mostly stable on re-read, except G3 (69 % unstable: marginal).
- **DevInit outputs** (`0x20011 = 0x006f`, `0x20060 = 3`, Step 3.6
  `k = 2`, `d0`–`d5`, `0x01r0af`) are the same in passing and failing boots,
  within 1.
- **Kept/recomputed vs envelope:** `0x2000a` within 2; `0x90813` live
  `0x7a01`–`0x7d01` vs envelope `0x7901`–`0x8201`. The clean K1 and K3
  boots have the largest `0x90813` difference (up to `0x0700`), so the
  difference is not what decides the result.

## Conclusion

The docsClean/11 restore with a matching USB/FDL envelope gives working
DRAM in 3 of 10 boots, and which envelope is used makes no visible
difference. The failure is per byte lane on the read path: one lane is a
burst off (bits 8–15) or captures nothing (`0xff`, bits 16–23). Everything
DevInit sets or measures is the same in passing and failing boots. So the
saved coarse read timing (RxEn / DQS gating, read latency) of a lane only
fits some boots: something in the PHY/DRAM clock alignment changes from
boot to boot, the saved values cannot carry it, and a plain host restore
does not re-align it (question N5).

docsClean 23ce4af confirms it (docsClean/11, inferred): only the
`lp5_quickboot` PMU image re-aligns RxEn / DQS gating, resets the DLL /
FIFO pointers and runs the DFI handshake; a host-only restore works in
~3 of 10 boots by luck. It also corrects two details of this restore:
`0x01r0ad`/`0x01r0af` must stay live from Step 3.6, and the commit triad
goes once per logical CSR block, not per segment (questions N2, N4).
