<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# 019 — Quickboot with the pre-DLL direct-clock switch: 16 GiB clean in 10 of 10 boots; RdDQS compare still silent

- **Date:** 2026-11-17
- **Goal:** apply docsClean e00a07a: (a) the quickboot order between the MR
  tail and step 6; (b) the corrected RdDQS preconditions.
- **Fixture:** USB/FDL envelope `E1`, host-assisted quickboot (Pass 1/2/3
  written by the host, hardware sequence by the free firmware).
- **Firmware, quickboot** (`quickboot_run()`), after the MR bursts of
  experiment 018 and before step 6:
  1. direct clock: `0x200a2 = (.. & 0xfffc) | 1`, `delay_dfi_clks(4)`,
     `0x200f8 = 1`, `delay_dfi_clks(0x1f)`;
  2. `0x2006e = 0`;
  3. per populated rank: `0x20072/73 = 1 << r`, `0x2007a = 0x2007e = 0`;
     then `0x20072/73 = CsPresent`, `0x90808/13` from `0x20314/15`;
  4. `0x01f0b3 = 0x01f0b4 = 0`;
  5. step 6 (DLL recalibration, sampled while `0x2f0e0 = 1`).

  The PMU run ends in direct-clock mode.
- **Firmware, RdDQS probe** (`src/train/rddqs.c`, runs 1–3): PLL clock
  mode, `0x20072/73 = CsPresent`, `0x2007a = 0x2007e = 0`, `0x200c0 =
  0x200c1 = 0`, pattern generator pass 1 pair `0x7f011 = 0x16` /
  `0x7f060 = 0x88`; from run 2 the firmware writes the 20-entry burst at
  ACSM entry 200 itself (per rank); from run 3 ACSM routing `0x3f084 = 0`,
  `0x01f00a = 0`.

## Results

| Run | Build | Memory test |
|---|---|---|
| 1 | quickboot + probe | 653,063 errors, 530,104 unstable |
| 2 | + firmware-built burst, ACSM read-back | 628,646 errors, 477,153 unstable |
| 3 | + ACSM routing in the probe | 702,635 errors, 545,029 unstable |
| 4–13 | quickboot only | **0 errors, ten consecutive boots** |

Quickboot (runs 4–13):

- The 16 GiB sampled memory test and `test_pattern` pass in **10 of 10**
  boots. Before this change the same path passed in about 2 of 3 boots
  (experiments 013–018), failing with whole-byte-lane or rank-1 one-beat
  shifts.
- `0x2004e` (DLL code sampled in step 6) is `0x0c` on PHY0 and `0x0d` on
  PHY1 in every boot.
- So the step that mattered is the switch back to the direct clock before
  the DLL recalibration, as docsClean e00a07a suggested.

RdDQS probe (runs 1–3):

- The host's ACSM restore is intact (0 of 4096 halfwords differ on
  read-back; entry 200 holds the reference burst). The firmware-built burst
  reads back correctly.
- At probe entry: `0x200f8 = 1`, `0x200a2 = 1` (direct clock, from the new
  quickboot ending), `0x2007a = 0`, `0x2007e = 0x33`, `0x20072 = 3`,
  `0x030084 = 1`, `0x01000a = 1`, `0x010080 = 0`, `0x20090 = 1`.
- `0x01<d><l>b7` is `0x0000` in every frame of all three runs (read delays
  `4e`/`4f` 0..124, write delays `0x10`–`0x13` 0..504, both ranks).
- The memory test fails after every probe run (about 30 % of the words,
  mostly unstable on re-read), while the same quickboot without the probe
  is clean ten times: the probe does not leave the PHY as it found it
  (suspects: the PLL / direct clock switches, the ACSM routing).

## Conclusion

The free quickboot path (reference envelope, host-assisted passes) is
reliable in this sample: 10 clean boots of 10. It remains a development
result: the envelope is a reference capture, not our own training.

The hardware compare is still not armed: with everything docsClean
describes in place, the bursts complete and no lane ever reports an error.
Questions Z1–Z3.
