<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# 011 — Free DevInit + host restore of the docsClean reference envelope: DRAM works (1 of 2 boots)

- **Date:** 2026-11-15
- **Goal:** check whether a free DevInit followed by a host-side restore of
  a full-training result gives working DRAM, using the 6400 MT/s reference
  envelope published in `docsClean/appendix-reference-envelope.md`
  (6e0642a) as a development fixture.
- **Fixture:** `tools/gen_ref_envelope.py` → `build/envelope/ref6400.bin`
  (0x9800 bytes, CRC `0xa16d989b` = the docsClean value). Captured on
  **this same board** (same unit, same USB FDL + serial debug wiring), but
  the envelope itself comes from an **SPI** cold boot (docsClean/09, /14);
  our runs boot via **USB/FDL**.
- **Firmware:** `train` with the Step 6.3 restores of docsClean 6e0642a
  (`0x20011` from the step 3.3 save, `0x2006e = 0`, `0x20072/73`,
  `0x90808/13`).
- **SPL** (`uboot.config`, `uboot.rev`; option `K3_DDR_REF_ENVELOPE`):
  1. copy the fixture over the training-info buffer (whatever the boot
     left there, including the board's own SPI envelope, is not used);
  2. message block = the fixture's block with `SequenceCtrl = 1`,
     `Quickboot = 0` and the trained VREF/DFE/DCA copied into
     MR12/14/15/24/30 (as the vendor `init_snps_ddrc_quick()` does);
  3. run the free PMU firmware (DevInit only, full DRAM init with the
     trained MR values);
  4. host writes the 5,170 `phypara` values straight to the PHY CSRs in
     serialization order (skipping `0xc0086`), clears `0x20037` bit 6,
     writes the 4,096 ACSM halfwords, `0xd00e7 = 0x600`,
     `0xc0080 = 2`, `0xd0000 = 1` (post-PMU part of the vendor quickboot);
  5. normal DDRC activation, memory test, vendor `test_pattern`, halt.
  No vendor PMU image runs; nothing is written to storage.

## Runs

| Run | DevInit (both PMUs) | DDRC activation | Memory test | `test_pattern` |
|---|---|---|---|---|
| `run1/` | OK, `0x20011 = 0x6f` | OK (599 ms) | **396,611 errors** in 2,097,152 words | **fail** |
| `run2/` | OK, `0x20011 = 0x6f` | OK | **0 errors** in 2,097,152 words, 0 unstable | **0 errors** |
| `run3/` (keep boot-specific registers) | OK | OK | **153,440 errors** (7.3 %), 434 unstable | **11 errors** |

- Memory test: 64 KiB every 256 MiB over all 16 GiB (from
  `0x1_0200_0000`), address pattern and its inverse, both controllers and
  ranks through the address map.
- Run 1 (first version of the test, which only printed the first errors):
  wrong words read the value of the address **+0x100**
  (`0x02000100` instead of `0x02000000`), from the first block on; about
  19 % of the words. Run 2 used the same restore with the extended test
  (per-block and per-bit counts, re-read) and found no error.
- DevInit captures differ by ±1 between the boots (normal); the restored
  `phypara`/ACSM are identical (same fixture).

Run 3 kept 63 registers from this boot's DevInit instead of the capture
(`0x2000a`, `0x20017`, `0x20187`, `0x20314/15`, `0x20321/22`,
`0x90808/13`, `0x01r0ad/af/d0..d6` and their `0x11` mirrors); the SPL prints
current vs. reference (`PMU<n> K` lines): e.g. `0x2000a 0x86/0x82`,
`0x20321 0x36/0x3b`, `0x01r0af 0x3e..0x3f/0x39..0x3b`. The failure is
different from run 1: **only data bits 16–23 (one byte lane) are wrong**
(≈153,420 errors on each of those bits, ≤ 9 on any other bit), the byte
mostly reads `0xff`, every 256 MiB block is affected, and the wrong value is
mostly stable on re-read (written or captured wrong, not noise).

## Conclusion

- **Confirmed:** free DevInit + host-side `phypara`/ACSM restore + DDRC
  activation gives working DRAM on this board in at least one boot: no
  vendor quickboot PMU image is required (question M3 partly answered by
  the hardware).
- **Not reliable:** 1 of 3 boots passes. Run 1 failed with a +0x100
  address offset pattern, run 3 (boot-specific registers kept) with one byte
  lane reading `0xff`; keeping the boot-specific registers did not help. The board is the same unit, so the likely causes are:
  the fixture is an SPI-boot capture while we boot via FDL (docsClean/14:
  different operating point), boot-specific registers (period counters,
  ACSM dividers, step 3.6 results) overwritten with the capture's values,
  and the unspecified restore order/skipped entries (M3).
- **Development fixture only:** these trained values are not ours; the
  product still needs the free training stages.

## Documentation updated

- `docs/questions.md` (M3 data), `logsRefs/README.md`.
