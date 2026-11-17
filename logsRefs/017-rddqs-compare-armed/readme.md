<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# 017 — RdDQS probe with the eae6813 arming: the hardware compare still reports nothing

- **Date:** 2026-11-16
- **Goal:** arm the PHY transfer-and-compare with the register state of
  docsClean eae6813 (`Pattern = 0`) and see `0x01<d><l>b7` react to a
  delay.
- **Setup:** as experiment 016 run 5 (host-assisted quickboot with envelope
  `E1`, then a second PMU start running `SequenceCtrl[3]` alone, then the
  post-PMU restore and the memory test).
- **Firmware** (`src/train/rddqs.c`):
  - generator: `0x2006e = 0`, `0x01f056 = 1`, `0x01f0a7 = 0`,
    `0x200c0 = 0x200c1 = 8`, `0x20079 = 0x40`, `0x01f0b3 = 0x01f0b4 =
    0x01f0b1 = 1`, `0x01f0b0 = 0x34`; `0x20042`, `0x20044`, `0x2007a` left
    as found (0);
  - pattern generator: `0x7f061..64 = 0xffff`, `0x7f065 = 0`,
    `0x7f011 = 0x16`, `0x7f060 = 0x8c`, `0x7f0d6 = 0x7f0d7 = 0x100`
    (`0x07<lane>024..2b` already `0x5a3c`);
  - ACSM routing `0x2002a = 1`, `0x20041 = 1`; MRW bursts from entry 0:
    `MR33 = 0x3c`, `MR34 = 0x5a`, `MR31 = MR32 = 0` (both ranks), then
    `0x01f001 = 1` and `MR46 = 0x04`; `0x2f0a0` pulse;
  - the write-then-read burst at ACSM entries 200..219 of the restored
    reference image (chip select `hw0[15:14] = 2`, rank 1; patched to `1`
    for rank 0), one-shot per point, `0x01<d><l>b7` read, `0x01f0bb =
    0x3ff → 0`;
  - exit: `0x01f0b0 = 0x01f0b1 = 0`, `0x01f056 = 0`, `0x200c0/c1 = 0`,
    `0x20079 = 0`, `0x7f011 &= ~0x10`, `0x200a3 = 1 → 0`, `MR46 = 0`,
    `0x01f001 = 0`, then everything saved is written back.

## Results

| Run | Probe | Memory test after |
|---|---|---|
| 1 | sweeps: `0x4e`/`0x4f` 0..124 (all dbytes, rank 1; dbyte 0, rank 0); `0x11`/`0x13` and `0x10`/`0x12` 0..504 (dbyte 0) | **0 errors** |
| 2 | variants: + `0x000a4 = 1`; + `0x200f8 = 1`; + reference clock (`0x200a2 = ..01`), each with trained values, `0x13 = 0`, `0x13 = 0x1f8`, `0x4e = 0` | 1,488,420 errors |

- All ACSM one-shots reach done (MRW bursts 1 poll, training burst 4).
- **`0x01<d><l>b7 = 0x0000` in every frame of both runs** (460 sweep points
  × 9 lanes in run 1), including write delays far from the trained
  `0x13b`–`0x14d`.
- `0x0100b0..bf` after a burst is the same in every variant and at every
  delay: `0034 0001 0000 0001 0001 0001 0000 0000 0000 0001 0001 0000 0000
  0000 0001 0000`.
- State found at entry: `0x000a4 = 0`, `0x200f8 = 1`, `0x200a2 = 2`,
  `0x20078 = 0`, `0x2006e = 0`; `0x70065 = 0x01ff`, `0x70011 = 0x0010`,
  `0x70060 = 0x008c`, `0x700d6/d7 = 0x0100`.
- Run 2 selected the reference clock for its last variant and switched
  back; the memory test failed afterwards (not separated from the known
  boot-to-boot failures).

## Conclusion

The documented arming is not sufficient in our state: no error is ever
latched, even with write delays that cannot work, so either the burst does
not drive the data path (the ACSM runs but DQ is not written or read) or
the comparator is not enabled. Since our state comes from a quickboot
restore and not from a full DevInit followed by the earlier stages, a
register that training sets before RdDQS is probably missing. Questions
X1–X3.
