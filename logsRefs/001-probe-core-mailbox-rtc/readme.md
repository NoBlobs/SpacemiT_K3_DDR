<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# 001 — Probe: mailbox polarity confirmed, PMU core configuration, RTC rate

- **Date:** 2026-11-15
- **Goal:** confirm the PMU-side mailbox sequence derived in experiment 000 and
  collect the probe data: ARCv2 build configuration registers, RTC rate,
  message block as seen by the PMU, ISA self tests, IMEM data readability.
- **Claims tested:** docsClean/13 BR-09 (mailbox), BR-06 (RTC), BR-07
  (multiply/divide), delay-slot and scaled-arithmetic semantics (docsClean/12);
  docsClean/12 64 KiB IMEM, RF16.
- **Firmware:** probe built with the corrected mailbox (`probe.imem.bin`
  SHA-256 `8dcadd16…`, `probe.dmem.bin` `1461fff8…`), working tree on `81ea2b5`.
- **FSBL:** U-Boot `289dc2b7bd` + branch `k3-free-ddr-fw` changes, fragment
  `k3_freefw_probe.config`; `fsbl.sha256`, `uboot.config`, `uboot.rev`.
- **Configuration:** MT62F2G32D4DS, 2 ranks, x16, 6400 MT/s.

## Procedure

Power cycle, then `tools/bench.sh build/fsbl-probe/FSBL.bin probe-mbox-fixed 60`.
Decoded stream in `mbox.txt`; raw log `serial.log`.

## Result

Both PMUs ran every probe step and ended with major `0x07`; the SPL printed
`PMU experiment finished in 936ms`. PMU0 and PMU1 data are identical except
for timing.

### Mailbox

Every major, stream introducer, header and payload word arrived and was ACKed
with the sequence: wait status `0x90180008[0] = 1`, write data,
`0x90180066 = 0`, wait status bit 0 = 0, `0x90180066 = 1`.

### Core identity and build configuration registers (AUX, `lr`)

Non-zero values only; field decoding follows the public ARCv2 PRM and is an
interpretation:

| AUX | Register (ARCv2 PRM) | Value | Interpretation |
|---|---|---|---|
| `0x004` | IDENTITY | `0x00000043` | ARCVER `0x43`: ARCv2 EM family |
| `0x00a` | STATUS32 | `0x00000800` | only the Z flag set at that point |
| `0x060` | BCR_VER | `0x00000002` | |
| `0x068` | VECBASE_AC_BUILD | `0x00000010` | |
| `0x06e` | RF_BUILD | `0x00000302` | bit 9 (E) = 1: 16-entry register file (RF16) |
| `0x074` | DCCM_BUILD | `0x00000904` | size code 9 (consistent with 64 KiB DMEM) |
| `0x075` | TIMER_BUILD | `0x00000404` | RTC present, no TIMER0/TIMER1 |
| `0x078` | ICCM_BUILD | `0x00000904` | size code 9 (consistent with 64 KiB IMEM) |
| `0x07b` | MULTIPLY_BUILD | `0x00020d06` | 32×32 multiplier present |
| `0x07e` | (0x7e BCR) | `0x00000002` | |
| `0x07f` | BARREL_BUILD | `0x00000303` | full barrel shifter |
| `0x0c1` | ISA_CONFIG | `0x12043002` | D (div/rem) = 1, code density = 2, 32-bit addresses, no LL64, no atomics |
| `0x0c7` | (0xc7 BCR) | `0x00001203` | not interpreted |
| `0x0f1` | (0xf1 BCR) | `0x00000101` | not interpreted |
| `0x0f3` | IRQ_BUILD | `0x00040401` | 4 interrupts, 4 external |

NORM (`0x7d`) and SWAP (`0x7c`) build registers read 0.

### RTC

- Before `timer_init()`: `RTC_CTRL = 0xc0000000` (A0/A1 set, counter disabled),
  count 0. After writing 2 then 1: `RTC_CTRL = 0xc0000001`.
- Count deltas vs. SPL microsecond timestamps over three consecutive
  intervals of 2²² ticks plus mailbox traffic: **399.85–399.91 ticks/µs** on both
  PMUs, i.e. the RTC runs at ~400 MHz.

### ISA self tests (all as expected)

`0x89abcdef * 0x12345678` = `0x09ca39e0_e242d208`; `5/4 = 1`, `5%4 = 1`;
`-7/2 = -3`, `-7%2 = -1`; `dbnz.d` loop of 5 executes the delay-slot `add` 5
times; `add3 r, 0, 1024` = `0x2000`.

### IMEM as data

`ldh [0x0]` = `0x01fc`, `ldh [0x1fc]` = `0x200a`, `ld [0x0]` = `0x000001fc`: IMEM
is readable with ordinary loads, 16 and 32 bits, without exception.

### Message block as written by the vendor SPL table

`MsgMisc = 0x01`, `Pstate` byte = `0x30`, `DRAMFreq = 0x1900`,
`DfiFreqRatio = 0x04`, `BitTimeControl = 0`, `Train2DMisc = 0x0004`,
`SequenceCtrl = 0x125f` (bits 0–4, 6, 9, 12), `HdtCtrl = 0x04`,
`DFIMRLMargin = 0x01`, `CATrainOpt = 0x8b`, `X8Mode = 0`,
`EnabledDQsChA/B = 16`, `CsPresentChA/B = 3`, MR inputs from `0x05a`, `0x0e8 = 0x002e`,
`0x0ea = 0x0050`, `VrefInc = 1`, `0x103 = 0x43`, `0x104 = 0x3f`. Byte and
halfword reads agree.

## Conclusion

- **Confirmed:** the PMU-side mailbox sequence of experiment 000 (inverse of
  docsClean BR-09) works for majors and streaming.
- **Confirmed:** RF16, hardware 32×32 multiply with high part, hardware
  div/rem, code density, barrel shifter; docsClean delay-slot and `add3`
  semantics.
- **New:** RTC ≈ 400 MHz (400 ticks/µs). The firmware now uses
  `RTC_TICKS_PER_US = 400`.
- **New:** IMEM is data-readable from the PMU. Constants could live in IMEM;
  the separate DMEM image is kept for now.
- **Open:** the `Pstate` byte is `0x30` in the vendor table, which does not fit
  "target P-state" with bit 7 as DMA mode; meaning unknown.

## Documentation updated

- `docs/hardware/pmu-arc-core.rst` — BCR table, RTC rate, IMEM readability.
- `docs/hardware/mailbox.rst` — sequence confirmed.
- `docs/hardware/message-block.rst` — values written by the SPL.
- `docs/firmware/build-toolchain.rst` — `-mbarrel-shifter`.
