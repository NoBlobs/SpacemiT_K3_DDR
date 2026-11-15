<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# 003 — DevInit clock-period calibration: the period counters stay at 0

- **Date:** 2026-11-15
- **Goal:** implement the DevInit master clock-period calibration described in
  docsClean/07 and check the measured periods (`T_ca ≈ 400`, `T_dq = 256` at
  6400 MT/s per docsClean/07 and /14).
- **Claims tested:** docsClean/07 "DevInit master clock-period calibration":
  save `0x20011`, clear bits [6:5], trigger at `0x200e3`, read
  `T_ca = 0x300bf & 0x1ff` and `T_dq = 0x100e0 & 0x1ff`; docsClean/07 step 4
  "restore the original `0x20011` value (`0x0041`)".
- **Firmware:** `train` with `src/train/devinit.c`; run 1 waits 0x20 RTC
  ticks (80 ns) after writing `0x200e3 = 1`; run 2 records every change of
  (`0x200e3`, `0x300bf`, `0x100e0`) in four phases (idle; after clearing
  `0x20011[6:5]`; with `0x200e3 = 1` held; after restoring `0x200e3`).
  `train-run2.sha256`.
- **FSBL:** fragment `configs/k3_freefw_devinit.config` (`K3_DDR_PMU_STOP_AFTER_FW`,
  `K3_DDR_PMU_HOST_DUMP`); `uboot.config`, `uboot.rev`, `fsbl.sha256`.
- **Configuration:** MT62F2G32D4DS, 2 ranks, x16, 6400 MT/s.

## Procedure

`make fsbl FW=train FSBL_FRAG=configs/k3_freefw_devinit.config`, power cycle,
`tools/bench.sh build/fsbl-train/FSBL.bin devinit-cal|devinit-watch 120`.
Runs in `run1-80ns/` and `run2-watch/` (`serial.log`, `mbox.txt`, host dump
CSV in `run2-watch/dump/`).

## Result

Identical on PMU0 and PMU1, both runs:

```
DEVINIT_STATE: 0xc0086=0x1 0x100a3=0xb33 0x20060=0x2
DEVINIT_WATCH: tag=0x0  trig=0x0 tca=0x0 tdq=0x0     (idle)
DEVINIT_WATCH: tag=0x1  trig=0x0 tca=0x0 tdq=0x0     (0x20011 [6:5] cleared)
DEVINIT_WATCH: tag=0x2  trig=0x1 tca=0x0 tdq=0x0     (0x200e3 = 1)
DEVINIT_WATCH: tag=0x102 t≈7.9 ms trig=0x1 tca=0x0 tdq=0x0
DEVINIT_WATCH: tag=0x3  trig=0x0 tca=0x0 tdq=0x0     (0x200e3 restored)
DEVINIT_WATCH: tag=0x103 t≈7.9 ms trig=0x0 tca=0x0 tdq=0x0
DEVINIT_CAL: ctrl=0x6f ...
PMU0 fatal 0xff, plllockstatus 0x0
```

- `0x200e3` is read/write (reads back the value written).
- `0x300bf` and `0x100e0` read 0 at every sample, up to ~7.9 ms after the
  trigger (the observation window is dominated by the first mailbox frame,
  so only the first and last samples of each phase were taken).
- `0x20011` reads `0x006f` before calibration (the host pre-training table
  writes `0x006f`), not `0x0041` as docsClean/07 states for the restored
  value.
- `0x200c9` ("plllockstatus") reads 0 after the run on both controllers.
- The firmware aborted (`TRAIN_FAIL`), restored `0x20011` and `0x200e3`; the
  host dump shows **0 changed** `phypara` registers versus the experiment 002
  baseline.
- `0xc0086 = 1` and `0x20060 = 2` at PMU start (both written by the host
  table), `0x100a3 = 0x0b33`.

## Conclusion

- **Not reproduced:** with the sequence of docsClean/07 alone the period
  counters do not count. Something the specification does not describe
  must run before the measurement.
- **Hypothesis (to test):** the PHY PLL is not locked at PMU start
  (`0x200c9 = 0`); without its clocks the period counters cannot count. The
  host pre-training table writes `0xc00f1`–`0xc00f7` and `0xc00f9`–`0xc00ff`,
  seven registers each, matching the seven `QBPll*` message-block outputs.
- **Partial refutation:** docsClean/07's "original `0x20011` value
  (`0x0041`)" does not match the value present on this board before DevInit
  (`0x006f`, from the host table). Possibly `0x0041` is the value after a
  successful run.
- **Next:** read-only scan of groups `0x02` and `0x0c` (experiment 004) to
  find PLL control/status registers and their state at PMU start.

## Documentation updated

- `docs/firmware/devinit.rst` — status of step 1.
- `docs/hardware/phy-csr-map.rst` — `0x200e3`, `0x300bf`, `0x100e0`, `0x200c9`.
