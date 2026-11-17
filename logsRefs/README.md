<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# logsRefs — experiment reference logs

One directory per experiment whose result matters for the documentation,
numbered in order: `000-<name>`, `001-<name>`, …

Each directory contains:

- `readme.md` — filled from [TEMPLATE.md](TEMPLATE.md);
- `serial.log` — raw console capture with host timestamps (`tools/serial_log.py`);
- `mbox.txt` — decoded mailbox stream (`tools/mbox_decode.py`);
- `FSBL.sha256`, firmware `*.sha256`, `uboot.rev`, `.config` used.

Pages in `docs/` cite these as `logsRefs/NNN-<name>/readme.md`. Negative results
that close a hypothesis are recorded too. Runs without a relevant finding stay
in `build/bench/` (not versioned).

| # | Experiment | Status | Main finding |
|---|---|---|---|
| 000 | [probe-first-boot-mailbox](000-probe-first-boot-mailbox/readme.md) | done | PMU starts at `0x1fc` via vector table; PMU-side mailbox polarity is inverse of docsClean BR-09 |
| 001 | [probe-core-mailbox-rtc](001-probe-core-mailbox-rtc/readme.md) | done | Mailbox fix confirmed; BCRs (RF16, barrel shifter, div, mpy, 64 KiB CCMs); RTC ≈ 400 MHz; IMEM data-readable |
| 002 | [csr-readout-pmu-vs-host](002-csr-readout-pmu-vs-host/readme.md) | done | PMU reads all 5170 phypara CSRs + ACSM identically to the host; pre-training baseline; ACSM is random before training |
| 003 | [devinit-clock-period-counters](003-devinit-clock-period-counters/readme.md) | done (negative) | docsClean/07 calibration alone: period counters stay 0; `0x20011` starts at `0x6f`; PLL lock status 0 |
| 004 | [csr-range-scan](004-csr-range-scan/readme.md) | done | 1792 CSRs of groups 0x02/0x04/0x0c read safely; host-table values confirmed; 46 reset/status values; ACSM control range all 0 |
| 005 | [pll-public-registers](005-pll-public-registers/readme.md) | done (negative) | DDR4-PHY PLL registers 0x200c5/c7/ca/cc do not exist on the K3; writes to unimplemented CSRs are ignored |
| 006 | [devinit-phases-1-4](006-devinit-phases-1-4/readme.md) | done (partial) | Phases 1–4 run as specified; counters enable but read a constant 0x240; 0x1f0xx broadcasts to all ranks |
| 007 | [devinit-0041e77](007-devinit-0041e77/readme.md) | done (partial) | DevInit per 0041e77; bit 10 falling edge latches; counters saturate once 0x200a2 bit 1 is set in step 3.5 (0x200a2=1 gives 0x250); buckets 0x7353/0x0073 match; ACSM busy stuck at 1 |
| 008 | [devinit-991bb0f](008-devinit-991bb0f/readme.md) | done (partial) | Phase 0 + 0x800004cc descriptors + documented delays: PHY PLL runs, T_ca = 0x84–0x86 (reference); 0x100d5 → 0x43 in step 3.6; ACSM still busy, MRW burst not sent |
| 009 | [devinit-19ab858](009-devinit-19ab858/readme.md) | done | DevInit per 19ab858 completes (status OK) on both PHYs; ACSM_DONE polarity confirmed; MRW burst executed; step 3.6 lanes 1–8 fall back to 5 |
| 010 | [devinit-8aa6257](010-devinit-8aa6257/readme.md) | done | Step 3.6 per rank selects k = 2 on all ranks (reference); ZQ MPC burst executed; `0xc0086` reads 1, not 3 |
| 011 | [refenv-host-restore](011-refenv-host-restore/readme.md) | done | Free DevInit + host restore of the docsClean reference envelope: DDRC up, memory test clean in 1 of 2 boots (other: +0x100 offset errors) |
| 012 | [refenv-fdl-e1-restore](012-refenv-fdl-e1-restore/readme.md) | done | All nine FDL envelopes (E1–K3) + docsClean/11 restore rules: memory test clean in 3 of 10 boots; failures are per byte lane (bits 8–15 a burst off, bits 16–23 `0xff`), with identical DevInit outputs |
| 013 | [host-assisted-quickboot](013-host-assisted-quickboot/readme.md) | done | Saved phypara before DRAM reset/MRW, free quickboot path with DLL recalibration, Pass 3 after: 16 GiB memory test clean in 3 of 3 boots once the rank-1 write-leveling `0x01r00d` (missing from the docsClean lists) is restored; later boots: 5 of 7 overall (see 015) |
| 014 | [uboot-on-free-quickboot](014-uboot-on-free-quickboot/readme.md) | done | U-Boot relocates and runs on DRAM brought up by the free quickboot path; 52 MB loaded and CRC-checked; Linux not started (no OpenSBI in the USB/FDL chain) |
| 015 | [host-read-eye-scan](015-host-read-eye-scan/readme.md) | done | Host scan of `0x01<r><l>4e/4f`: per-bit read windows ~100 taps wide, trained values near the centre, margin ≥ 24 taps; `0x200cf` stuck at `0x1f`; quickboot path clean in 5 of 7 boots |
| 016 | [quickboot-8110257](016-quickboot-8110257/readme.md) | done | DLL recalibration samples the reference code (`0x0c`/`0x0d`) with the 8110257 order; byte-lane failures remain. First RdDQS probe from our firmware: the entry-200 burst runs, but `0x01<d><l>b7` stays `0` at every delay (compare not armed). Host cannot rewrite the message block after a PMU run |
| 017 | [rddqs-compare-armed](017-rddqs-compare-armed/readme.md) | done (negative) | RdDQS probe with the eae6813 arming (pattern generator, pattern MRs, `MR46`, `0x01f001`): `0x01<d><l>b7` stays `0` even with write delays swept over 0..504; `0x000a4`, `0x200f8` and the reference clock do not change it |
| 018 | [quickboot-fsp-switch](018-quickboot-fsp-switch/readme.md) | done | Quickboot with the two-pass MR sequence and FSP switch of docsClean eae6813: 2 of 3 boots clean; the coarse per-boot misalignment remains |
| 019 | [e00a07a-probe-and-quickboot](019-e00a07a-probe-and-quickboot/readme.md) | done | Quickboot with the switch back to the direct clock before the DLL recalibration (docsClean e00a07a): 16 GiB memory test clean in 10 of 10 boots. RdDQS probe with the corrected preconditions, firmware-built burst and ACSM routing: `0x01<d><l>b7` still `0`; the probe leaves the memory failing |
