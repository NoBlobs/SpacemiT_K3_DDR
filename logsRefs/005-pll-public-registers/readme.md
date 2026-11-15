<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# 005 — DDR4-PHY PLL registers do not exist on the K3 LPDDR5 PHY

- **Date:** 2026-11-15
- **Goal:** test whether the PHY PLL is the reason the DevInit period
  counters stay at 0 (experiment 003), using the PLL registers named in the
  public NXP DDR4 PHY code (TF-A `phy-gen2`).
- **Claims tested:** [public] `PllCtrl2 0x200c5`, `PllCtrl1 0x200c7 = 0x21`,
  `PllTestMode 0x200ca = 0x24`, `PllCtrl4 0x200cc = 0x17f`,
  `PllPwrDn 0x200c3`; `0x200c9` = PLL lock status (docsClean/08).
- **Firmware:** `pllprobe` (`src/pllprobe/main.c`), **first experiment that
  writes PHY registers**; `pllprobe.sha256`.
- **FSBL:** fragment `configs/k3_freefw_pllprobe.config` (stop after the PMU);
  `uboot.config`, `uboot.rev`, `fsbl.sha256`.
- **Configuration:** MT62F2G32D4DS, 2 ranks, x16, 6400 MT/s.

## Procedure

Stages, each followed by a snapshot of `0x200c3 c5 c6 c7 c9 ca cc cd cf`,
`0x300bf`, `0x100e0`, `0x20047`, `0x200e3`, `0x20011` (stream `0x0822`):

0. read state;
1. write `0x200c5 = 0x19`, `0x200c7 = 0x21`, `0x200ca = 0x24`,
   `0x200cc = 0x17f`, watch 50 µs;
2. write `0x200c3 = 0`, watch 2 ms;
3. clear `0x20011[6:5]`, `0x200e3 = 1`, watch 2 ms; `0x200e3 = 0`, watch
   500 µs; restore `0x20011`.

(Each snapshot costs ~11 ms of mailbox/UART time, so a "watch" sees only the
first and last sample.)

## Result

Identical on both PMUs except `0x20047` (PMU0 `0x00bd`; PMU1 `0x00dd` in this
boot, `0x00ff` in experiment 004 — it varies per controller and per boot):

```
tag=0x00 200c3=0001 200c5=0000 200c6=0010 200c7=0000 200c9=0000 200ca=0000 200cc=0000 200cd=0001 200cf=001f 300bf=0000 100e0=0000 200e3=0000 20011=006f
tag=0x21 (after writing c5/c7/ca/cc)  ... identical: c5/c7/ca/cc still read 0000
tag=0x22 (after 200c3=0)              200c3=0000, everything else unchanged
tag=0x23 (0x200e3=1, 0x20011=0x0f)    200e3=0001 20011=000f, counters 0000
tag=0x05 (restored)                   20011=006f
```

No stall; the PMU completed with `0x07`; total 242 ms.

## Conclusion

- **Refuted for the K3:** `0x200c5`, `0x200c7`, `0x200ca`, `0x200cc` are not
  implemented on this LPDDR5 PHY (writes are ignored, reads return 0), so the
  DDR4-PHY PLL programming of the public code does not apply.
- **New:** writes to unimplemented indices are silently ignored and do not
  stall the PHY bus.
- **New:** `0x200c3` is writable (1 → 0), but clearing it alone starts
  nothing observable: `0x200c9` and both period counters stay 0.
- **New:** `0x20047` is a live value that differs per controller and per boot.
- **Corollary:** `0xc00f0`–`0xc00ff` (host-written, `0xc00f7 = 0xc00ff = 0xf000`)
  match the public `DfiFreqXlat` pattern (`vec7 = 0xf000`), not a PLL.
- **Next:** find the register map of the DWC **LPDDR5** PHY in public code
  (NXP i.MX95 System Manager, `nxp-imx/imx-sm`).

## Documentation updated

- `docs/hardware/public-dwc-phy.rst` — K3 column for the PLL registers (page
  removed later: nothing from public code is used by the firmware or the
  documentation).
- `docs/firmware/devinit.rst` — status.
