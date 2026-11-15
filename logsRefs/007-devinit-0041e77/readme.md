<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# 007 — DevInit per docsClean 0041e77: the counters latch, but saturate after step 3.5

- **Date:** 2026-11-15
- **Goal:** run DevInit as restructured in docsClean/07 (0041e77) and check
  the period-counter latch (bit 10 falling edge of `0x200e3`).
- **Firmware:** `train` (`src/train/devinit.c`, `src/train/devinit_calc.h`):
  phases 1, 2, 3 (3.1–3.5, 3.6 entry/exit only, 3.7), 6.1 and 6.2 steps 1–3;
  no MRW burst, no 6.3. `delay(n)` assumes 1 RTC tick per scaled unit.
  FSBL fragment `uboot.config`; `uboot.rev`; `train.sha256` per run.
- **Configuration:** MT62F2G32D4DS, 2 ranks per channel, x16, 6400 MT/s,
  `DfiFreqRatio = 4`, `PllBypassEn = 0`, `0xc0080 = 3` (delay shift 1).
- **Claims tested:** docsClean/07 0041e77 phases, latch strobe, frequency
  buckets, ACSM NOP flush.

## Run 1 (`run1-sequence/`)

Message-block inputs as seen by the PMU: `byte[0] = 0`, `MsgMisc = 0x01`,
`Pstate = 0x30`, `byte[8] = 4`, `Quickboot = 0`, `CsPresentChA/B = 3`.

```
CAL: e3=0 e5=0, pre 0x300bf=0x400 0x100e0=0x400, e3(high)=0xc00,
     latched 0x300bf=0x17ff 0x100e0=0x17ff (& 0x1ff = 511), 0x100d5=0x19a
     f_lo=1600 f_hi=6400 0x20012=0x7353 0x20186=0x0073
ACSM: 0x20021 still 1 after 25000 polls
```

- Timing: step 3.4 (`delay(130000000)`) 176 ms; whole DevInit 558 ms.
- Host phypara diff against experiment 006 (PMU0, 27 entries):
  `0x01r00f 0x0001→0x1006`, `0x01r0ac 0x000f→0x0001`, `0x01r0d9 0→0x009c`
  (r = 0..3), `0x2000a`/`0x20017 0x40→0x1ff`, `0x20012 0xa1a1→0x7353`,
  `0x20186 0xa1→0x73`, `0x20187 0x40→0x19a`, `0x20023 0→1`, `0x2002a 0→1`,
  `0x20060 2→3`, `0x20072`/`0x20073 0→3`, `0x20078 3→0`, `0x2007a 3→0`,
  `0x2007e 0xf→0`, and **`0x20321 0→1`, `0x20322 0→0xff`** (not written by
  the firmware: side effect of step 3.4).

## Run 2 (`run2-latch-probe/`)

Same firmware plus a test latch (strobe, read, restore `e3`/`e5`, result
unused) at several points, with an optional extra wait between the rising and
falling edge of bit 10. Identical on both PMUs:

| Point | Wait (RTC ticks) | Before | After latch |
|---|---|---|---|
| before phase 1, after phase 2, after 3.3, after 3.4 | 0 / 400 | `0x0000` | `0x0240` |
| after 3.5, after 3.6 | 0, 4, 40, 400, 4000 | `0x0400` | `0x17ff` |

- `0x20021` (ACSM busy) already reads `1` **before** the ACSM is started.
- Host dump identical to run 1 except `0x20187` (`0x19f`, copied from the
  per-boot `0x100d5`).

## Run 3 (`run3-step35-probe/`)

Step 3.5 (normal branch, `PllBypassEn = 0`) probed after each write group:
registers `0x200c9 0x20021 0x200e0–e2 0x200a2 0x200f8 0x20078 0x20061
0x200c3 0x20047`, then a test latch. Afterwards step 3.5 is repeated with all
long waits ×10 and ×100 (the ×100 pass takes ~1.46 s), probed at the end.

| Tag | After | `0x200a2` | `0x20078` | idle → latch |
|---|---|---|---|---|
| `0x50` | `0x1f00f`, `0x20061 = 1` | 1 | 3 | `0x0000 → 0x0240` |
| `0x51` | `0x20049`…`0x200c3 = 0` | 1 | 3 | `0x0000 → 0x0240` |
| `0x52`–`0x54` | `0x200e2 = 3 / 2 / 0` + waits | 1 | 3 | `0x0000 → 0x0240` |
| `0x55` | `0x20078 = 1`, `0x20200 = 0x33`, `0x200a2 = 0` | 0 | 1 | `0x0000 → 0x0240` |
| `0x56` | `0x20078 = 2`, `0x200a2 = 2`, `delay(10000)` | 2 | 2 | **`0x0400 → 0x17ff`** |
| `0x57` | tail (`0x20078 = 0`, `0x200f8 = 0`, `0x200a2 = 2`) | 2 | 0 | `0x0400 → 0x17ff` |
| `0x5a` / `0x5b` | repeat with waits ×10 / ×100 | 2 | 0 | `0x0400 → 0x17ff` |

- The counter state changes in the group `0x20078 = 2`, `0x200a2 = 2`.
- Longer waits (×10, ×100) change nothing.
- `0x200c9` reads 0 and `0x20021` reads 1 throughout; `0x200e0`–`0x200e2`
  read back what was written; `0x20047` is constant within a run (PMU0
  `0xbd`, PMU1 `0xdd`). Otherwise identical on both PMUs.

## Run 4 (`run4-a2-matrix/`)

After step 3.5 (and its ×10/×100 repeats), every combination of
`0x200a2[1:0]` and `0x20078[1:0]` is written, followed by a 10 µs wait and a
test latch (tag `0x80 | a2 << 2 | r78`); both registers are restored after.
Identical on both PMUs. Latched value (raw):

| `0x200a2` \ `0x20078` | 0 | 1 | 2 | 3 |
|---|---|---|---|---|
| 0 | `0x17ff` | `0x0240` | **`0x0250`** | `0x0240` |
| 1 | **`0x0250`** | `0x0240` | **`0x0250`** | `0x0240` |
| 2 | `0x17ff` | `0x0240` | `0x17ff` | `0x0240` |
| 3 | `0x17ff` | `0x0240` | `0x17ff` | `0x0240` |

- `0x20078` bit 0 set: the latch always returns the seed `0x240` (idle `0`),
  so bit 0 holds the counters.
- `0x200a2` bit 1 set: saturated `0x17ff` (idle `0x400`).
- `0x200a2 = 1`, or `0x200a2 = 0` with `0x20078 = 2`: **`0x250`**
  (`& 0x1ff = 0x50` = 80), the first value that is neither the seed nor
  saturated. Reference is `0x82`–`0x86` with `0x200a2 = 2`.
- Restoring `0x200a2 = 2`, `0x20078 = 0` returns to `0x17ff` (tag `0x5c`).

## Conclusion

- **Confirmed:** the latch needs the bit 10 falling edge; the counter value
  changes on the strobe (0x0041e77 fixes F7's procedure).
- **Confirmed:** frequency buckets `0x7353` / `0x0073` match the reference.
- **Confirmed:** step 3.2 `0x20072`/`0x20073` = `CsPresentChA/B` (= 3, the
  host-table value).
- **New:** the result does not depend on the window (0 to 10 µs): the
  counters return a sampled measurement, not a time-window count.
- **New:** before step 3.5 the latch returns `0x240` (seed); after step 3.5
  (DRAM reset / PLL sequencing) it returns `0x17ff` — bit 12 plus all-ones
  low bits, i.e. **saturated**, as if the measured clock were not running.
  Reference: `0x82–0x86`.
- **New:** the ACSM busy flag is stuck at `1` before any run, consistent with
  an unclocked or held sequencer.
- **New:** step 3.4 sets `0x20321 = 1`, `0x20322 = 0xff` in hardware.
- **New (run 3):** the saturated state starts when `0x20078 = 2`,
  `0x200a2 = 2` are written in step 3.5, and does not depend on the length of
  the step 3.5 waits (×1, ×10, ×100). `0x200c9` is not a lock flag here.
- **New (run 4):** `0x20078[0]` holds the counters at the seed;
  `0x200a2[1]` (the value step 3.5 leaves, and the one the reference
  calibrates with) gives a saturated count; `0x200a2[1:0] = 1` gives a real
  count of `0x50`. Working hypothesis: `0x200a2` selects the clock that is
  measured, and the source selected by bit 1 (presumably the PHY PLL output)
  is not running on our board after step 3.5.
- **Open (questions G10–G11):** what state the PHY clock must be in after 3.5
  (PLL lock wait units, G2; status register to poll), and what `0x20021`
  should read before the ACSM start; `0x100d5` never reaches `0x41` (G7).

## Documentation updated

- `docs/firmware/devinit.rst`, `docs/questions.md` (G10, G11).
