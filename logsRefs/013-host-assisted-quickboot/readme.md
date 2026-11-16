<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# 013 — Host-assisted quickboot with the free firmware: 16 GiB clean in 3 of 3 boots

- **Date:** 2026-11-15
- **Goal:** follow the quickboot order of docsClean/11 (066c550): saved
  `phypara` in the PHY **before** DQ reset, DRAM reset and MRW, then the
  DLL/clock recalibration, then Pass 3. Experiments 011/012 restored after
  a full DevInit and worked in 3 of 10 boots.
- **Fixture:** USB/FDL envelope `E1` (`build/envelope/ref6400-fdl-e1.bin`,
  CRC `0xfa1e9f98`). Same board, USB/FDL boot.
- **Firmware** (`train`, `inputs.sha256`): `stage_devinit` takes a new path
  when `Quickboot != 0` (`quickboot_run()` in `src/train/devinit.c`):
  1. phase 0 (commit strobe, canary), `0x20009` saved; QBPll CSRs logged
     against message `0x130..0x13d` (equal) and kept;
  2. startup script `0x2006e = 1`, `0x70060 = 0`, `0x70011 = 0`;
     `0x2008b`, `0x20072/73`; no `0x01r0a3` mask;
  3. reference clock (`0x200a2 |= 1`, `0x200f8 = 1`), DQ block reset;
  4. Step 3.5 DRAM reset / PLL sequencing with the 400 µs lock wait;
     Step 6.1 ACSM flush and switch to the PLL clock;
  5. no `tINIT`; multiplier off; MRW burst (18 MRs, trained MR12/14/15/24/30
     shadows). **Not done:** the extra MR6/MR7/MR40 writes (their values
     are not clear in docsClean/11), ZQ;
  6. step 6 DLL recalibration (`0x2f310`, `0x2f0f8`, `0x2f049`, `0x2f0e0`,
     `0x2f0c3`, `0x2004e = 0x200cf & 0x1f`, `0x9f02d = 1`, `0x2f009`),
     commit strobe, `0x000a4 = 1`.
- **SPL** (`uboot.config`, option `K3_DDR_REF_QUICKBOOT`):
  1. after the pre-training tables, write every saved `phypara` entry
     classified as quickboot Pass 1, 2 or 3 (docsClean/11 partition; runs
     1–2 skipped the 20 group-`0x01` entries outside the three lists, runs
     3–5 write them with Pass 1), except
     `UCCLK`, `0xd0xxx` and the 8 runtime CSRs; commit triad per logical
     CSR block; then `0x20090 = 1`, `0x20060 = 3`, `0xc0086 = 1`;
  2. message block from the envelope, `SequenceCtrl = 1`, `Quickboot = 1`,
     trained MR shadows;
  3. after the PMU halts: Pass 3 written again, `0x90808/13` recomputed
     from `0x20314/15`, `0x20037 &= ~BIT6`, ACSM image, `0xD00E7`,
     `UCCLK = 2`, `MUX_SEL = 1`; DDRC activation, 16 GiB sampled memory
     test, `test_pattern`, halt.

## Results

| Run | Unlisted group `0x01` entries | Blocks 0–31 (lower 8 GiB) | Blocks 32–63 (upper 8 GiB) | `test_pattern` |
|---|---|---|---|---|
| 1 | not written | **0 errors** | 1,048,576 of 1,048,576 words wrong | 0 |
| 2 | not written | **0 errors** | 1,048,576 of 1,048,576 words wrong | 0 |
| 3 | written before the PMU | **0 errors** | **0 errors** | 0 |
| 4 | written before the PMU | **0 errors** | **0 errors** | 0 |
| 5 | written before the PMU | **0 errors** | **0 errors** | 0 |

Runs 1–2 (identical word for word):

- **Upper half error shape:** every word reads the value written 8 bytes
  earlier, and the first two words of each 64-byte block read `0`
  (`0x302000000` → `0`, `…008` → `…000`, `…040` → `0`). All byte lanes
  shift together; nothing changes on re-read.
- **Cause:** 20 saved group-`0x01` entries are in none of the docsClean/11
  pass lists: `0x01r00d` (r = 0..3) and `0x01r019`, `0x01r01b`, `0x01r022`,
  `0x01r023`. `0x01r00d` is the rank-1 partner of the Pass 1 write-leveling
  delay `0x01r00c`; its reset value is `0x0000`, the envelope holds
  `0x97`–`0x9c`. The other 16 reset to `0x1f`, equal to the envelope. So
  rank 1 wrote one beat off.

Runs 3–5 (`0x01r00d` restored): memory test and `test_pattern` clean over
all 16 GiB in three consecutive boots.

All runs:

- **Firmware:** both PHYs complete; flush done after 1 poll, MRW burst after
  5. Step 6 reads `0x200cf = 0x1f`, so `0x2004e = 0x1f` (the field
  maximum; reference `0x0c`/`0x0d`), on both PHYs in every boot.
- **Live at PMU halt:** `0x0100ad = 2`, `0x0100af = 0x3f`/`0x3a`,
  `0x2000a = 0x87`/`0x85`, `0x20060 = 3`, `0x01r00c/0d` (runs 3–5) = the
  envelope values. `0x90813` recomputed `0x7a01`–`0x7d01` vs saved
  `0x7901`–`0x7a01`.

## Conclusion

The quickboot order works with the free firmware: saved `phypara` in the
PHY before DQ reset, DRAM reset and MRW, the step 6 DLL recalibration, then
Pass 3 and the recomputed ACSM dividers. With `0x01r00d` restored the
16 GiB sampled test is clean in 3 of 3 boots (011/012: 3 of 10, random
byte-lane failures). This is a development result: the envelope is a
docsClean reference capture of this board, not a result of our own
training, and the test halts after the memory test.

Open: the extra MR6/MR7/MR40 writes are not issued, and `0x200cf` reads
`0x1f` after step 6 (questions S1, S2); neither prevents a clean test.
docsClean/11 needs `0x01r00d` in Pass 1 (question T1).
