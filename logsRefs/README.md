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
