<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# 002 — PHY CSR read-out: PMU side vs. host side, pre-training baseline

- **Date:** 2026-11-15
- **Goal:** check that the PMU reaches every PHY CSR the host serializes as
  `phypara`, plus the ACSM SRAM, at the addresses predicted by
  `ARC = 0x90000000 + 2 * index`; record the PHY state the PMU sees when it
  starts (after the SPL pre-training tables, before any training).
- **Claims tested:** docsClean/02 (host index ↔ ARC address in all CSR groups,
  ACSM window `0x90082000`), docsClean/09 and appendix-phypara-order (69
  segments, 5170 registers).
- **Firmware:** `csrdump` (`src/csrdump/main.c`); hashes in `csrdump.sha256`.
  Read-only: it writes no PHY register.
- **FSBL:** U-Boot `289dc2b7bd` + branch `k3-free-ddr-fw` changes, fragment
  `configs/k3_freefw_csrdump.config` (adds `K3_DDR_PMU_COMPARE_DUMP`);
  `uboot.config`, `uboot.rev`, `fsbl.sha256`.
- **Configuration:** MT62F2G32D4DS, 2 ranks, x16, 6400 MT/s; vendor SPL
  pre-training tables applied as usual.

## Procedure

1. The PMU reads the 5170 `phypara` CSRs in serialization order with 16-bit
   loads at `0x90000000 + 2*index`, then the 4096 ACSM halfwords at
   `0x90082000 + 2*j`, storing them at DMEM `0x80008000` (host index
   `0x5c000`). It streams counts, non-zero counts, CRC-32 and RTC ticks, then
   `0x07`.
2. After the PMU stops, the SPL runs its normal result collection
   (`save_snps_ddrc_training_result`: sets `0x20037` bit 6, mux 0, clocks 3,
   reads message / phypara / ACSM), compares every value with the PMU copy in
   DMEM and prints all host values.
3. `tools/bench.sh build/fsbl-csrdump/FSBL.bin csrdump 120`;
   `tools/dump_extract.py serial.log baseline` writes the CSV files.

## Result

```
PMU0 S 0820 8: 00001432 00000935 9f88d30d 00001000 00001000 041fb4bf 0010ed23 000cddb4
PMU0 C phypara 0 diffs, acsm 0 diffs
PMU1 S 0820 8: 00001432 00000935 9f88d30d 00001000 00000fff e40d1be3 0010ed23 000cdd60
PMU1 C phypara 0 diffs, acsm 0 diffs
```

- **phypara:** 5170 registers read from the PMU side, all equal to the host
  read on both controllers. 2357 non-zero; identical on PMU0 and PMU1 (same
  CRC `0x9f88d30d`). No read stalled the bus.
- **ACSM:** 4096 halfwords, PMU read = host read, but the contents look like
  random power-on SRAM: 4096 / 4095 non-zero, no position equal between the
  two controllers.
- **Timing:** 1 109 283 RTC ticks for the 5170 CSR reads (≈ 2.77 ms, ≈ 536 ns
  per read including the DMEM store and CRC); 843 188 ticks for 4096 ACSM
  reads (≈ 515 ns each).
- **Non-zero registers per group** (pre-training):

  | Group | Read | Non-zero |
  |---|---:|---:|
  | `0x01` | 1468 | 1116 |
  | `0x02` | 338 | 69 |
  | `0x03` | 152 | 78 |
  | `0x07` | 101 | 0 |
  | `0x09` | 1693 | 8 |
  | `0x0c` | 57 | 13 |
  | `0x11` | 1180 | 1024 |
  | `0x12` | 52 | 17 |
  | `0x13` | 88 | 32 |
  | `0x19` | 41 | 0 |

- First values: `0x100a3 = 0x0b33`, `0x10056 = 0`, `0x100d9 = 0x0080`, `0`,
  `1 … 7`, `0x0b3f`. The trained sample in docsClean/09 starts
  `2867, 0, 156, 0, 1 … 7, 2879`: only `0x100d9` differs (128 here, 156
  trained).

Files: `serial.log` (complete host dump), `mbox.txt`,
`baseline/phypara-pmu{0,1}.csv` (`k,index,value`),
`baseline/acsm-pmu{0,1}.csv`.

## Conclusion

- **Confirmed:** `ARC = 0x90000000 + 2*index` for every `phypara` group
  (`0x01`, `0x02`, `0x03`, `0x07`, `0x09`, `0x0c`, `0x11`, `0x12`, `0x13`,
  `0x19`) and ACSM at `0x90082000 + 2*j`; all these reads are safe from the
  PMU before training.
- **Confirmed:** the 69-segment table yields 5170 readable registers.
- **New:** before training the ACSM SRAM holds random power-on data; nothing
  in the SPL pre-training flow initializes it.
- **New:** `0x100a3 = 0x0b33` (and `0x0b3f` at the next rank group) is already
  present before the PMU runs, i.e. written by the host tables, not by
  DevInit. docsClean/02 labels it "DevInit status/config"; a value of 0 after
  a run (docsClean/04 failure signature) would therefore mean the firmware
  cleared it.
- **New:** group `0x11` ("live DQ mirror, never written by host tables" in
  docsClean/02) already has 1024 non-zero registers before training.
- **New:** a PMU-side CSR read costs about 0.5 µs.
- `baseline/phypara-pmu0.csv` is the reference pre-training state for later
  diffs against our own training results.

## Documentation updated

- `docs/hardware/phy-csr-map.rst` — group table with counts, confirmed mapping.
- `docs/hardware/results-persistence.rst` — ACSM before training.
- `docs/uboot-spl/integration.rst` — `K3_DDR_PMU_COMPARE_DUMP`.
- `docs/firmware/streaming-ids.rst` — `0x0820`.
