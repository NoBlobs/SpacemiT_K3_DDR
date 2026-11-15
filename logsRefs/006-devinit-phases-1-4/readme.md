<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# 006 — DevInit phases 1–4 (docsClean b03a788): counters enable but read a constant

- **Date:** 2026-11-15
- **Goal:** run DevInit phases 1–4 as specified in docsClean/07 (b03a788) and
  check the clock-period calibration.
- **Firmware:** `train` (`src/train/devinit.c`), stops before phase 5;
  FSBL fragment `configs/k3_freefw_devinit.config`; `uboot.config`,
  `uboot.rev`, `fsbl.sha256`, `train-run2.sha256`.
- **Claims tested:** docsClean/07 (b03a788) phases 1–4, counter enable
  `0x200e3 |= 0x0c00` / `0x200e5 &= 0xffe0`, expected `T_ca ≈ 0x190`,
  `T_dq = 0x100` at 6400 MT/s.

## Run 1 (`run1-10ticks/`)

- With `0x200e3 |= 0x0c00`, `0x200e5 &= 0xffe0` and a 10-RTC-tick (25 ns)
  wait, the counters are **no longer 0**: `0x300bf = 0x40`, `0x100e0 = 0x40`
  on both PMUs (spec expects ~`0x190` / `0x100`). `0x100d5` reads `0x19a` /
  `0x19f`. `2*T_ca = 128` falls outside the encoded buckets, so the firmware
  aborted (`TRAIN_FAIL`) after phase 3 calibration.
- `phypara` diff vs. experiment 002 baseline (PMU0, 19 registers):
  `0x1r00f 0x1086→0x0001`, `0x1r0ac 0→0x000f`, `0x1r0d9 0x80→0` for r = 0..3
  (the `0x1f0xx` writes act as a **broadcast to all four ranks**);
  `0x20011 0x6f→0x0f`; `0x20028 0→1`; `0x20072 3→0`, `0x20073 3→0`
  (written from `Pstate & 0xf` and `PllBypassEn`); `0x2007a 0→3`;
  `0x2007e 0→0xf`; and `0x20077 1→0`, which the firmware does **not** write
  (side effect).

## Run 2 (`run2-watch/`)

Same firmware plus a 2 ms watch of the counters right after the enable
(changes buffered with RTC timestamps) and a read of host DMEM words:

```
DEVINIT_DMEM:  0x80000458=0000 0x8000045a=0004 0x8000042a=0000 0x8000042c=0000
DEVINIT_WATCH: t=0.145us    0x300bf=0x240 0x100e0=0x240 0x100d5=0x19f   (PMU0)
DEVINIT_WATCH: t=2000.168us 0x300bf=0x240 0x100e0=0x240 0x100d5=0x19f   (PMU0)
DEVINIT_WATCH: t=0.145us    0x300bf=0x240 0x100e0=0x240 0x100d5=0x1bf   (PMU1)
```

- Both counters read **`0x240`** (bit 9 set, `& 0x1ff = 0x40`) from 145 ns to
  2 ms after the enable, never changing: a constant, not a measurement in
  progress.
- `0x100d5` is constant within a run but differs per controller and per boot
  (`0x19a`, `0x19f`, `0x1bf` ≈ 410–447): it behaves like a live period/delay
  measurement close to the ~400 the spec expects for `T_ca`.
- The delay divisor `0x8000045a` (docsClean/12) holds 4 from the host table.
- The host `phypara` dump is identical to run 1.

## Conclusion

- **Partly confirmed:** the counter enable of docsClean/07 b03a788 changes
  `0x300bf`/`0x100e0` from 0 to `0x240`, so the bits are right, but the value
  is a constant `0x40` after masking, not the expected ~`0x190`/`0x100`.
- **Confirmed:** phases 1–3 writes behave as specified (`0x20011 → 0x000f`,
  `0x2007a = 3`, `0x2007e = 0xf`); `0x1f0xx` writes broadcast to the four rank
  copies `0x1r0xx`.
- **New:** `0x20077` drops from 1 to 0 during phases 1–3 without being
  written by the firmware.
- **Open (sent to docsClean as F7):** the measurement probably needs a state
  not yet reached (phase 5/6 settings?), or the period lives elsewhere
  (`0x100d5`?). The calibration remains blocked until answered.

## Documentation updated

- `docs/firmware/devinit.rst`, `docs/questions.md` (F7).
