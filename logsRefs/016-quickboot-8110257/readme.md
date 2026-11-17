<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# 016 — Quickboot fixes of docsClean 8110257/3cff3d8 and a first RdDQS transfer-and-compare probe

- **Date:** 2026-11-16
- **Goal:** (a) apply the quickboot corrections of docsClean 8110257 and
  b2c6030; (b) run the hardware transfer-and-compare primitive of
  docsClean/07 (3cff3d8) from our own firmware, as the first step of an
  independent RdDQS.
- **Fixture:** USB/FDL envelope `E1`, host-assisted quickboot as
  experiment 013.
- **Firmware, quickboot path** (`quickboot_run()`):
  - step 6 samples `0x200cf` **while** `0x2f0e0 = 1`, then `0x2f0c3 = 1`,
    `0x2f0e0 = 0`, `0x2f049 = u | 0x200`, and stores the sample in
    `0x2004e`;
  - `0x2f052` pulse before the MRW burst (with `MsgMisc[0]`);
  - after the table burst: ZQCAL MPC, per-rank `MR40 = 0x40` and
    `MR37 = 0x40`, broadcast `MR28 = MR28_A0 | 2` (when `MR28_A0[5] = 0`).
    Not issued: the `MR16` setup and the `cmd 0x28` entry (encoding not
    specified).
- **SPL, quickboot:** `0x2f052` pulses after the pre-PMU writes (two) and
  after Pass 3; `0x2004e` keeps the value sampled in this boot.
- **Firmware, RdDQS probe** (`src/train/rddqs.c`, `SequenceCtrl[3]`): burst
  configuration of docsClean/07 (`0x20079 = 0x40`, `0x01f0b3 = 1`,
  `0x01f0b4 = 1`, `0x01f0b1 = 1`, `0x01f0b0 = 0x34`), ACSM routing
  (`0x2002a = 1`, `0x20041 = 1`), `0x2f0a0` pulse; per point: delay in
  lane-broadcast form, one-shot run of ACSM entries 200..219 (the burst of
  the restored reference ACSM image), read `0x01<d><l>b7`, clear through
  `0x01f0bb`. Everything changed is restored. The firmware does not build
  the burst or program the pattern generator (not specified).
- **SPL, probe** (`uboot.config`, option `K3_DDR_REF_PROBE`): after the
  quickboot PMU run and the Pass 3 / ACSM restore, start the PMU again,
  then restore again, finalize, activate the DDRC, memory test.

## Results

| Run | Build | Second PMU run | Memory test |
|---|---|---|---|
| 1 | quickboot fixes only | — | 166,284 errors (7.9 %): byte lane 1 reads another burst |
| 2 | + probe | repeated the quickboot path (SequenceCtrl not changed) | **0** |
| 3 | probe before finalization | repeated the quickboot path | 648,187 errors |
| 4 | PMU held in reset for the write | repeated the quickboot path | 1,048,576 errors |
| 5 | firmware selects RdDQS itself | RdDQS probe, full sweep | **0** |
| 6 | exploration round 1 | RdDQS probe, register dumps | 133,043 errors |
| 7 | exploration round 2 | RdDQS probe, register dumps | **0** |

Quickboot:

- **DLL recalibration now samples a real code:** `0x2004e = 0x0c` (PHY0)
  and `0x0d` (PHY1), the reference values; read later, `0x200cf` is back
  at `0x1f`.
- The ZQCAL and MR40/MR37/MR28 bursts complete (3 and 4 polls).
- Whole-byte-lane failures still occur (run 1, with no second PMU run).
  The runs with a second PMU run are not a clean reliability sample.

Host access to the message block after a PMU run:

- With the PMU stalled (`0xd0099 = 1`) the host reads DMEM but a write to
  `SequenceCtrl` does not land (reads back `0x0001`), before or after the
  `0xd00e7` finalization. With the PMU in reset (`0xd0099 = 9`) DMEM reads
  `0x0000`. The initial load, before the first start, works.
- Workaround: the host puts the private flag `0xa5` in message byte 0 at
  the initial load; the firmware clears it at the end of the quickboot path
  and writes `SequenceCtrl = 0x0008` itself.
- A second start of the same loaded firmware (`0xd0099 = 9, 1, 0`) works.

RdDQS probe (runs 5–7, both PHYs):

- The one-shot burst at entries 200..219 runs to done: 4 polls, or 48
  polls once `0x20042 = 0x0108` and `0x20044 = 2` are set (run 7).
- **`0x01<d><l>b7` reads `0x0000` for every lane at every delay**, including
  `0x4e`/`0x4f` = 0 and 126 where the host scan of experiment 015 fails.
- `0x0100b0..bf` and `0x0101b0..bf` are identical after a burst at the
  trained delay, at 0 and at 126, in all variants tried: the documented
  configuration; + `0x01f<l>be = 0x41`; + `0x70061..64 = 0xffff`; + the
  DevInit step 6.3 state (`0x2007a = 3`, `0x2007e = 0xf`, `0x20042`,
  `0x20044`); + `0x000a4 = 0`.
- State found before the probe: `0x20079 = 0`, `0x20022 = 0`,
  `0x20023 = 9`, `0x0100b0 = 4`, `0x0100b1 = 0x180`, `0x0100b5 = 1`,
  `0x70061..64 = 77aa 007e 77aa 007e`, `0x7f011 = 0`, `0x7f060 = 0`,
  `0x2007e = 0x33`, `0x20042 = 0x20044 = 0`.

## Conclusion

The framework for a stage of our own works: a second PMU start in the
restored state, the entry-200 burst as a one-shot, raw results streamed,
state restored, memory still good afterwards. The compare itself is not
armed: no result register reacts to the read delay. What arms it (pattern
generator, pattern mode registers, `MR46`, per-lane compare enable) needs
the register values of the reference (questions W1–W4).
