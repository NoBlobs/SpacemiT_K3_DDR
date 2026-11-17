<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# 018 — Quickboot with the two-pass MR sequence and FSP switch: 2 of 3 boots clean

- **Date:** 2026-11-16
- **Goal:** test whether the mode-register sequence of docsClean eae6813
  (quickboot step 5) removes the boot-to-boot failures of the free
  quickboot path.
- **Setup:** host-assisted quickboot, envelope `E1`, as experiment 013 with
  the 8110257 fixes of experiment 016; no second PMU run.
- **Firmware** (`qb_mrw()` in `src/train/devinit.c`):
  1. pass 0: `MR16 = MR16_A0 | 0x41` first, then the 18-MR table (its
     `MR16` entry with the same value);
  2. `MR16 = MR16_A0 | 0x45`, a 256-unit NOP, 4 µs wait;
  3. pass 1: the table with `MR16 = (MR16_A0 | 0x45) & 0xbc`;
  4. ZQCAL MPC; per-rank `MR40 = 0x40`, `MR37 = 0x40`;
     `MR28 = MR28_A0 | 2`; the `cmd 0x28` slot `c028 0000 0000 0010`;
     NOP `0x1e`.

  Not done: the clock-enable step after the tail (`0x200a2 |= 1`,
  `0x200f8 = 1`), because selecting the reference clock there is not
  followed by a documented switch back.

## Results

| Run | Memory test | `test_pattern` |
|---|---|---|
| 1 | **0 errors** | 0 |
| 2 | **0 errors** | 0 |
| 3 | 406,090 errors (19.4 %), 60,423 unstable | 11 |

All ACSM one-shots complete in every run (table 5 polls, FSP switch 12,
ZQCAL 3, tail 4); `0x2004e = 0x0c`/`0x0d`. Run 3 errors are almost all in the upper 8 GiB
(8,500–16,200 of 32,768 words per block; rank 1), reading data shifted by
8 bytes (`0x302000100` reads `0x02000108`); the lower 8 GiB has 0 or 1
error per block (a word reading `0x00000000`).

## Conclusion

The FSP-switch sequence does not remove the failures: 2 clean boots of 3,
in line with the earlier rate. The run 3 pattern (a one-beat shift on
rank 1 only, rank 0 nearly clean) is again a coarse per-boot misalignment,
here of rank 1, not a per-bit delay problem. The quickboot path still lacks something the
reference does on every boot, or does it in a different order; the passes
applied by the host before the PMU start (instead of inside the PMU after
phase 0) and the undone clock-enable step are the known differences
(question Y1).
