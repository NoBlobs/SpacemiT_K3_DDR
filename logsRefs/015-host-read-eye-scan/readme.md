<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# 015 — Read-eye scan from the host: per-bit windows of ~100 taps, trained values near the centre; quickboot path clean in 5 of 7 boots

- **Date:** 2026-11-16
- **Goal:** with the DRAM working (experiment 013 path, envelope `E1`),
  measure the real read eye of each DQ bit from the host, to learn what
  `0x01<r><l>4e`/`4f` are and to have real data for the training engine.
- **Firmware:** as experiment 013; run 3 adds read-outs of `0x200cf`,
  `0x2004e`, `0x20049`, `0x200c3` along the quickboot path and for 120 ms
  after the step 6 DLL recalibration (sent as ID `0x0822` in run 3,
  `0x0843` from now on).
- **SPL** (`uboot.config`, option `K3_DDR_REF_EYESCAN`): after a clean
  memory test, pre-write 32 MiB (512 KiB in each of 64 blocks, one every
  256 MiB), then for each of the 144 CSRs `0x01<r><l>4e` and `4f`
  (r = 0..3, l = 0..8, both PHYs): write 0, 2, …, 126, read 4 KiB of every
  block at each value (a chunk not read recently, so the data cache cannot
  hide errors), restore the trained value. Host writes use
  `MICRO_CONT_MUX_SEL = 0` … `1` around each access, no commit strobe.
  Then the 16 GiB memory test again, halt.
- **Analysis:** `tools/eyescan_stats.py run3/serial.log`.

## Results

| Run | Memory test before the scan | Scan |
|---|---|---|
| 1 | 1,111,050 errors (53 %), bits 16–31, all blocks, many unstable | not run |
| 2 | 240,326 errors (11.5 %), mostly bits 16–23 reading `0xff` | not run |
| 3 | **0 errors** | 144 scans; memory test clean again afterwards |

Firmware and all logged values are identical in the three runs.

Scan (run 3):

- **A host write takes effect immediately**, with the DDRC active and no
  commit strobe.
- **`r` is the data byte and `l` the bit:** CSR `0x01<r><l>4e/4f` moves
  exactly data bit `8r + l` of the 32-bit word (0 mismatches in 124
  scans). Lane 8 has no effect on data (16 of 16 scans).
- **Both `4e` and `4f` act on all 64 blocks**, so they are not a rank-0 /
  rank-1 pair (unlike `0x0c`/`0x0d`); and a bit of either PHY fails in
  every block, so the two controllers are interleaved inside each block
  (finer than 4 KiB), not split by address range.
- **Windows** (delay taps, step 2; "closed" = both edges inside 0..126):

  | PHY, CSR | Closed windows | Width min/med/max | Trained − centre min/med/max | Smallest margin left/right |
  |---|---|---|---|---|
  | 0, `4e` | 22 of 32 (10 open at 126) | 88 / 99 / 108 | −13 / −4 / +1 | 24 / 47 |
  | 0, `4f` | 27 of 32 (2 open at 0, 3 no effect) | 82 / 100 / 114 | −3 / +2 / +8 | 39 / 36 |
  | 1, `4e` | 26 of 32 (1 open at 0, 4 at 126, 1 no effect) | 88 / 99 / 122 | −12 / −3 / +3 | 36 / 47 |
  | 1, `4f` | 26 of 32 (6 open at 0) | 88 / 98 / 108 | −9 / 0 / +9 | 35 / 37 |

  The ~100-tap width agrees with the reference 2D contours (docsClean/07:
  70–124, median 102). The trained values (43–76) sit within a few taps of
  the centre; the `4e` windows lie about 10 taps higher than the `4f` ones.
- Four scans (`0x01014f`, `0x01024f`, `0x01034f` on PHY0, `0x01034e` on
  PHY1) show no error at any value; not explained.

DLL read-out (run 3, both PHYs): `0x200cf` reads `0x1f` at every point,
before the DQ reset, after the PLL sequence, after step 6 and for 120 ms
after it. `0x20049` goes `0x3f0 → 0x1f0 → 0x3f0` and `0x200c3` `1 → 0 → 1`
as written. `0x2004e` holds the envelope value (`0x0c`/`0x0d`) until
step 6 overwrites it with `0x1f`; the host Pass 3 then restores `0x0c`.

## Conclusion

- The per-bit read delays have 24 taps or more of margin on each side, so
  the boots that fail are not caused by marginal `4e`/`4f` values. The
  failures are whole byte lanes, which points at a coarser per-boot
  alignment.
- The free quickboot path is now clean in **5 of 7 boots** (013 runs 3–5,
  014, 015 run 3; 015 runs 1–2 fail). Better than a restore after DevInit
  (3 of 10), not reliable.
- `0x200cf` never moves from `0x1f`: the step 6 recalibration probably
  does nothing in our flow (questions S2, V1).
