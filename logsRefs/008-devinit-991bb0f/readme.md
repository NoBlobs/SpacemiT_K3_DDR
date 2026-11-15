<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# 008 — DevInit per docsClean 991bb0f: the PHY PLL runs and the period measures 0x86

- **Date:** 2026-11-15
- **Goal:** run DevInit as specified in docsClean/07 (991bb0f) — phase 0,
  `0x800004cc` descriptors, two-tier delays, step 3.6, ACSM enable and the
  MRW burst — and check whether the period counters now measure.
- **Firmware:** `train` (`src/train/devinit.c`, `src/train/devinit_calc.h`).
  Deviations from the spec: the canary mismatch only logs (no abort); step
  3.6 evaluates and logs but writes no result to `0x01r02d` and skips the
  `0x1f004` rank bit (H2); no ZQ/MPC and no "post-reset state bit" (H4);
  the MRW burst runs only if the ACSM flush completed. Diagnostic latches
  (result unused) after step 3.5.12 (`0x56`), after 3.5 (`0x35`) and after
  3.6 (`0x36`). FSBL fragment `uboot.config`; `uboot.rev`; `run1/train.sha256`.
- **Configuration:** MT62F2G32D4DS, 2 ranks per channel, x16, 6400 MT/s,
  `DfiFreqRatio = 4`, `PllBypassEn = 0`, `MsgMisc = 0x01`, `Pstate = 0x30`.

## Run 1 (`run1/`)

| Item | PMU0 | PMU1 | Reference (docsClean) |
|---|---|---|---|
| DMEM canary `0x80000424/426` | `0x6745 / 0x00f2` | same | `0x6745 / 0x00f2` |
| descriptors at `0x800004cc` | `0x2006e=1`, `0x70060=0`, `0x70011=0` | same | same |
| latch after 3.5.12 (`0x200a2 = 2`) | `0x1486` (134) / `0x1643` (67) | `0x1685` / `0x1643` | — |
| **`0x2000a` (`T_ca`)** | **`0x86`** (134) | **`0x84`** (132) | `0x82`–`0x86` |
| `0x20017` (`T_dq`) | `0x43` (67) | `0x42` (66) | `0x40` (unchanged) |
| `0x100d5` → `0x20187` | `0x43` | `0x45` | `0x41` |
| `0x20012` / `0x20186` | `0x7353` / `0x0073` | same | same |
| ACSM busy before / after enable / after flush | 1 / 1 / 1 | 1 / 1 / 1 | 0 after the run |
| `0x90808` / `0x90813` | `0x3601` / `0x7b01` | `0x3601` / `0x7a01` | `0x3b01` / `0x8301` |
| `0x20060` at the end | 3 | 3 | 1 (3 in a DevInit-only run) |

- DevInit takes ~453 ms with logging (UART-bound); the waits themselves are
  the documented µs values.
- `0x100d5` reads `0x19f` after step 3.5 and **`0x43` after the step 3.6
  broadcast setup** (before the per-rank loop), as docsClean says.
- Step 3.6 lane-0 captures (all ranks, unchanged over `k = 0..4`):
  `d0 = 0`, `d1 = 0`, `d2 ≈ 0x41`, `d3 ≈ 0x85`, `d4 ≈ 0xca`; `d5 ≈ 0x43`.
  The step deltas `d3 − d2 ≈ d4 − d3 ≈ 0x44` match `d5` within 14 %, so the
  first matching step would be `k = 2` (match rule still to be confirmed, H2).
- The 6-entry ACSM flush never completed (`0x20021` stays `1` for 25,000
  polls), so the 23-command MRW burst (376 halfwords) was queued but not run.
- Phase 0 wrote the canary to `0x90029/0x9002a` (`byte[0x80000403] = 0`);
  `0x90029` reads back `0x145` (only 9 bits stored), `0x9002a` = `0xf2`.
- Host phypara diff against experiment 007 run 2 (PMU0, 33 entries) includes
  `0x01r0d2..d4` (step 3.6 captures), `0x2000a`, `0x20017`, `0x20031 = 1`,
  `0x20042 = 0x108`, `0x20044 = 2`, `0x20090 = 1`, `0x20187`, `0x90029`,
  `0x9002a`, `0x90808`, `0x90813`, `0x9080b/c = 0xf`, and
  `0x20321/0x20322 = 0x36/0x7b` (equal to `0x20314/0x20315` this boot).
  `0x01r0ac` ends at `0` instead of the saved `1`.

## Run 2 (`run2-acsm-probe/`)

Same firmware; in step 6.1, after the dividers and `0x20031 = 1`, the ACSM is
started (`0x20022 = 0`, `0x20023 = 1`, `0x20020 = 1`, 2,000 polls,
`0x20020 = 0`) after each cumulative pre-step, and `0x20020`–`0x2003f` and
`0x90800`–`0x9081f` are dumped before and after:

| Tag | Pre-step (cumulative) | `0x20021` before / after start / after polls / after stop |
|---|---|---|
| `a0` | as left by the enable | 1 / 1 / 1 / 1 |
| `a1` | `0x20031 = 0` | 1 / 1 / 1 / 1 |
| `a2` | `0x20031 = 1` + 50 ns wait | 1 / 1 / 1 / 1 |
| `a3` | `0x200f8 = 0` | 1 / 1 / 1 / 1 |
| `a4` | `0x2002a = 1` | 1 / 1 / 1 / 1 |
| `a5` | commit strobe triad | 1 / 1 / 1 / 1 |
| `a6` | `0x20041 = 1` | 1 / 1 / 1 / 1 |
| `a7` | `0x20060 = 3` + tINIT4 | 1 / 1 / 1 / 1 |
| `a8` | `0x20027 = 0x8000` | 1 / 1 / 1 / 1 |
| `a9` | `0x200a2 = 1` (reference clock) | 1 / 1 / 1 / 1 |

Identical on both PMUs. Register windows (PMU0; PMU1 differs only in
`0x90808 = 0x3701`, `0x90813 = 0x7d01`):

```
0x20020+: 0000 0001 012c 0000 0000 0000 0000 0000 0001 0000 0000 0000 083f 083f 081f 081f
0x20030+: 083f 0001 0000 0000 0000 100c 100c 045c 1b20 1018 1018 0428 2f2c 1004 1004 0414
0x90800+: 0000 004d 0001 0000 0000 0000 0000 0000 3601 7000 1300 000f 000f 0000 0000 0000
0x90810+: 0000 0000 0000 7b01 0000 ...
```

- `0x20021` reads `1` in every state, with `0x20031` off or on, `0x20020`
  0 or 1, and either clock selected: it never reacts.
- `0x20031` reads back `1`; `0x20022`/`0x20023` read back what was written.
- `0x20314/0x20315` (and so `0x90808/0x90813`) vary per PHY and boot
  (`0x36/0x7b`, `0x37/0x7d`).

## Conclusion

- **Confirmed:** with phase 0, the `0x800004cc` descriptors and the
  documented delays, the PHY PLL runs: the period latch gives `0x84`–`0x86`,
  the reference values. (Experiment 007 lacked phase 0 and the descriptors;
  longer waits alone did not help, so one of those writes is required.)
- **Confirmed:** `0x100d5` is updated by hardware in step 3.6 (`0x19f` →
  `0x43`); host DMEM holds the canary and the three descriptors.
- **Different from the reference:** `T_dq` is `0x42`–`0x43`, not an
  unchanged `0x40`; the ACSM dividers come out as `0x3601`/`0x7b01` because
  `0x20314/0x20315` read `0x36/0x7b` here (they look like ZQ codes; the
  reference has `0x3b/0x83`).
- **Open:** the ACSM busy flag stays `1` even after `0x20031 = 1` and the
  divider setup, so no DRAM command has been sent yet (question H9). Run 2:
  none of nine further pre-steps changes it; `0x20021` behaves like a
  constant, not a busy flag that follows `0x20020`.

## Documentation updated

- `docs/firmware/devinit.rst`, `docs/questions.md` (H2 data, H7, H9).
