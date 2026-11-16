<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# 010 — DevInit per docsClean 8aa6257: step 3.6 selects k = 2, ZQ MPC burst executed

- **Date:** 2026-11-15
- **Goal:** apply docsClean 8aa6257 — step 3.6 once per rank (no lane loop,
  rule `2·d[k] ≥ d5`, gate `0x01r057 + pstate`, latch and
  `0x01r0d6 → 0x01r0af`), 8-halfword NOP pairs (4-unit flush), the ZQ
  calibration `MPC` burst (variant 1) and `MsgMisc[7]` from `byte[1]`.
- **Firmware:** `train` (`src/train/devinit.c`, encoders in
  `src/train/devinit_calc.h` with unit tests). Deviations: canary mismatch
  only logs; diagnostic latch after step 3.5.12 kept. New logs `0x0840`
  (step 3.6 per rank) and `0x0841` (end-of-DevInit registers). FSBL fragment
  `uboot.config`; `uboot.rev`; `run1/train.sha256`.
- **Configuration:** MT62F2G32D4DS, 2 ranks per channel, x16, 6400 MT/s,
  `DfiFreqRatio = 4`, `MsgMisc = 0x01`, USB/FDL boot path. Serial on
  `/dev/ttyUSB0` (`SERIAL_PORT=/dev/ttyUSB0`).

## Run 1 (`run1/`)

| Item | PMU0 | PMU1 | Reference (docsClean/09, SPI) |
|---|---|---|---|
| step 3.6 `d0..d4` (rank 0) | `0 0 0x41 0x86 0xcb` | `0 0 0x3f 0x83 0xc5` | `0 0 0x41 0x85 0xca` |
| `d5` per rank 0..3 | `0x44 0x45 0x44 0x43` | `0x44 0x43 0x42 0x43` | `0x41`–`0x43` |
| **`0x01r0ad`** (all ranks) | **`2`** | **`2`** | `2` |
| `0x01r0af` rank 0..3 | `0x3e 0x3e 0x3e 0x3f` | `0x3b 0x3d 0x3c 0x3c` | not given |
| flush (4-unit NOP): first read / polls | `0` / 1 | same | — |
| MRW burst (`0x178` hw): first / polls | `0` / 5 | same | — |
| **ZQ `MPC` burst (48 hw, `0x20023 = 11`)**: first / polls | **`0` / 3** | **same** | — |
| `T_ca` (`0x2000a`) / `0x20187` | `0x86` / `0x43` | `0x85` / `0x44` | `0x82` / `0x41` (DDR0) |
| `0x20011` at the end | `0x000f` | `0x000f` | `0x000f` (SPI), `0x006f` (FDL, after training) |
| `0x20060` at the end | `3` | `3` | `3` with `Misc[0] = 0` (DevInit only) |
| **`0xc0086` at the end** | **`1`** | **`1`** | **`3` after DevInit** |
| DevInit status | 0 (OK) | 0 (OK) | — |

- phypara diff against experiment 009 run 1 (PMU0, 22 entries):
  `0x01r0ad 5→2`, `0x01r0af 0→0x3e..0x3f`, `0x01r00f 0x1006→0x1004`,
  `0x01r0d3/d4 → 0` (captures cleared by the second `0x01r0ac` pulse),
  `0x20023 0x5d→0x0b` (last run is the ZQ burst), `0x20187` ±1.
  PMU1 additionally shows the per-boot `0x20314/5` variation.

## Conclusion

- **Confirmed:** step 3.6 with one capture per rank selects `k = 2` on all
  eight ranks, the reference result; the lane loop was the J3 cause.
- **Confirmed:** the 48-halfword ZQ `MPC` burst executes (done after 3 polls).
- **Different:** `0xc0086` reads `1` at the end of DevInit, docsClean/14
  says `3` (question L1). `0x01r00f` ends at `0x1004` (the old code restored
  `0x1006`; question L2).
- **Not yet verified:** that the DRAM accepted the MRW/ZQ commands.

## Documentation updated

- `docs/firmware/devinit.rst`, `docs/firmware/streaming-ids.rst`,
  `docs/questions.md`.
