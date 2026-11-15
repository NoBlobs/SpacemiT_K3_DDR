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
