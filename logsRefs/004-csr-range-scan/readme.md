<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# 004 — Read-only scan of CSR groups 0x02, 0x04 (control) and 0x0c

- **Date:** 2026-11-15
- **Goal:** map which registers exist and what they hold when the PMU starts,
  in the groups involved in DevInit (clock period, MemReset, PLL candidates,
  ACSM start registers), to understand why the period counters of
  experiment 003 stay at 0.
- **Claims tested:** reads of these ranges are safe (no stall); docsClean/07
  ACSM start registers `0x40020`, `0x400fd`, `0x40100`.
- **Firmware:** `csrscan` (`src/csrscan/main.c`), read-only;
  `csrscan.sha256`.
- **FSBL:** fragment `configs/k3_freefw_csrscan.config` (stop after the PMU,
  no host dump); `uboot.config`, `uboot.rev`, `fsbl.sha256`.
- **Configuration:** MT62F2G32D4DS, 2 ranks, x16, 6400 MT/s.

## Procedure

The PMU reads every index of `0x20000–0x203ff`, `0x40000–0x401ff` and
`0xc0000–0xc00ff` (1792 registers), storing the index in breadcrumb slots
14/15 before each read, and streams the values (`LOG_ID_CSRSCAN 0x0821`).
`tools/scan_extract.py serial.log scan host-pretable-drank-x16-6400.csv`
adds the value the SPL pre-training table writes (extracted from the GPL SPL
source with `tools/spl_tables.py`, file `host-pretable-drank-x16-6400.csv`).

## Result

- All 1792 reads completed on both PMUs; run time 2.2 s (mostly UART).
- 101 registers are non-zero. Every register the host pre-training table
  writes reads back exactly the written value (including `0xc00f1`–`0xc00ff`).
- `0x40000–0x401ff` reads all zero (including `0x40020`, `0x400fd`,
  `0x40100`).
- `0x200c9` reads 0; `0x200e3`, `0x300bf`/`0x100e0` (not scanned) see 003.
- PMU0 and PMU1 are identical except `0x20047` (`0x00bd` vs `0x00ff`), which
  looks like a live/analog status.
- 46 non-zero registers are **not written by any host table** (reset values
  or hardware status):

  | Index | Value | Index | Value | Index | Value |
  |---|---|---|---|---|---|
  | `0x2000d` | `0x0006` | `0x20072` | `0x0003` | `0x20305` | `0x3ffd` |
  | `0x20016` | `0x0007` | `0x20073` | `0x0003` | `0x20306` | `0x0005` |
  | `0x2001c` | `0x0003` | `0x20077` | `0x0001` | `0x2030e` | `0x0188` |
  | `0x20021` | `0x0001` | `0x20078` | `0x0003` | `0x2030f` | `0x0010` |
  | `0x20047` | `0x00bd`/`0x00ff` | `0x20088` | `0x0124` | `0x20310` | `0x0001` |
  | `0x20049` | `0x01f0` | `0x200a2` | `0x0001` | `0x20318` | `0x0140` |
  | `0x2004a` | `0x2c18` | `0x200a4` | `0x03a2` | `0x20319`–`0x2031f` | `0x0180`/`0x0130`/`0x0188` |
  | `0x2004b` | `0x3c1f` | `0x200aa` | `0x0001` | `0x20323` | `0x0003` |
  | `0x20057` | `0x0098` | `0x200c3` | `0x0001` | `0x20330` | `0x0033` |
  | `0x2005a` | `0x0020` | `0x200c6` | `0x0010` | `0xc0004` | `0x0001` (mailbox status) |
  | `0x2005b` | `0x0020` | `0x200cd` | `0x0001` | `0xc0033` | `0x0001` (mailbox doorbell) |
  | `0x20071` | `0x000a` | `0x200cf` | `0x001f` | `0xc0035` | `0x0001` |
  | | | `0x200ee` | `0x0821` | `0xc00ee` | `0x0240` |
  | | | `0x200f8` | `0x0001` | `0xc00ef` | `0xffff` |

  (`0x20072`/`0x20073` are also written by the post-training table.)

Full data: `scan/scan-pmu{0,1}.csv` (`index,value,host_table`).

## Conclusion

- **Confirmed:** reading every index of these three ranges from the PMU is
  safe before training.
- **New:** the ACSM control range `0x40000–0x401ff` is all zero at PMU start;
  the registers docsClean/07 names for "ACSM init" hold 0.
- **New:** reset/status values of 46 registers not covered by any host table
  (table above), the reference for later write experiments.
- **Open:** nothing in these ranges identifies a PLL enable or a running clock
  for the period counters; `0x200c9` stays 0. The next step needs either
  documentation of the PHY clocking/calibration registers or write
  experiments.

## Documentation updated

- `docs/hardware/phy-csr-map.rst` — ranges scanned, register table.
