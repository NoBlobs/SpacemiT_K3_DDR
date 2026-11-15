<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# 009 — DevInit per docsClean 19ab858: completes, MRW burst executed by the ACSM

- **Date:** 2026-11-15
- **Goal:** apply docsClean 19ab858 (PHY version decode, canary pair,
  `ACSM_DONE` polarity, one-shot ACSM runs, step 3.6 selection rule and
  `0x01r0ad`, tINIT4/5, routing writes, channel-commit pulse) and send the
  LPDDR5 MRW burst.
- **Firmware:** `train` (`src/train/devinit.c`). Deviations: canary
  mismatch only logs; `0x01f002 + rank` restored after step 3.6 (not stated,
  J3); the ZQ calibration `MPC` burst is not sent (J2); diagnostic latch after
  step 3.5.12 kept. FSBL fragment `uboot.config`; `uboot.rev`;
  `run1/train.sha256`.
- **Configuration:** MT62F2G32D4DS, 2 ranks per channel, x16, 6400 MT/s,
  `DfiFreqRatio = 4`, message-block `byte[0] = 0x00`, `byte[1] = 0x01`.

## Run 1 (`run1/`)

| Item | PMU0 | PMU1 |
|---|---|---|
| `0xc00ee` → version code | `0x240` → 9 | same |
| canary pair / read-back | `0x9002e/f` = `0x6745 / 0x00f2` | same |
| phase 0 branch (`byte[1] & 1`) | taken | taken |
| `T_ca` (`0x2000a`) | `0x86` | `0x85` |
| step 3.6 `k` per lane 0..8 (all ranks) | `2 5 5 5 5 5 5 5 5` | same |
| `0x01r0ad` written | `5` (was `2` before DevInit) | same |
| NOP flush: `0x20021` first read / polls | `1` / 0 | same |
| **MRW burst: `0x20021` first read / polls** | **`0` / 5** | **`0` / 5** |
| `0x20060` at the end | 3 | 3 |
| DevInit status | **0 (OK)** | **0 (OK)** |

- The MRW burst (23 commands, 376 halfwords, `0x20023 = 0x5d`) reads
  `ACSM_DONE = 0` right after `0x20020 = 1` and `1` five polls later: the
  ACSM really executes it. The NOP flush is already done at the first read,
  as docsClean says.
- ACSM SRAM in the host dump holds the burst; decoded: `MR19=0x00 MR18=0x0d
  MR1=0xb0 MR2=0xbb MR3=0x06 MR10=0x58 MR11=0x34 MR17(r0)=0x28 MR17(r1)=0x00
  MR12=0x27 MR14=0x28 MR15=0x28 MR16=0x00 MR20=0x02 MR22=0x00 MR24=0x00
  MR37=0x40 MR40=0x40 MR41=0x00` (values from this board's message block;
  `MR17_A1 = 0` looks suspicious).
- Step 3.6: lane 0 selects `k = 2` (as the docsClean example), lanes 1–8
  fall back to `5`, so the per-rank result (last lane) is `5`; the host table
  had left `0x01r0ad = 2`. Lane 1–8 captures were not logged.
- phypara diff against experiment 008 run 1 (PMU0, 18 entries):
  `0x01r0ad 2→5`, `0x01r00a 1→0` (routing `0x1f00a = 0`),
  `0x03r084 1→0` (`0x3f084 = 0`), `0x20023 = 0x5d`, `0x20031 1→0`,
  `0x9002e/f = 0x6745/0xf2`, `0x90029/a` back to `0`.
- DevInit takes ~259 ms with logging; the training firmware still ends with
  `0xff` because the later stages are stubs.

## Conclusion

- **Confirmed:** `0x20021` is `ACSM_DONE` (active high); one-shot runs need
  no `0x20031`; the MRW burst executes.
- **Confirmed:** version decode `0xc00ee = 0x240` → 9 selects the 16-bit
  canary pair `0x9002e/f`, which reads back correctly.
- **Different:** step 3.6 lanes 1–8 never satisfy the rule, so `0x01r0ad`
  becomes `5` instead of the reference `2` (question J3).
- **Not yet verified:** that the DRAM accepted the mode registers (no MRR in
  the reference flow; needs a later stage or a host-side check).

## Documentation updated

- `docs/firmware/devinit.rst`, `docs/questions.md` (J3 data).
