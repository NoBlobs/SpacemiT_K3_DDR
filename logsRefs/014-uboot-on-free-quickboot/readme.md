<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# 014 — U-Boot runs on DRAM brought up by the free quickboot path

- **Date:** 2026-11-15
- **Goal:** after experiment 013 (16 GiB memory test clean in 3 of 3
  boots), let the boot continue past the SPL and use the DRAM for real.
- **Fixture / firmware:** as experiment 013 (USB/FDL envelope `E1`, free
  firmware quickboot path, host-assisted passes).
- **SPL** (`uboot.config`, option `K3_DDR_REF_BOOT`): after the 16 GiB
  sampled memory test and `test_pattern`, continue only if both are clean.
  The training-info header in `DDR_TRAINING_INFO_BUFF` is **cleared**
  first, so U-Boot's `save_ddr_training_info()` never writes the fixture to
  the boot private partition (SPI flash).
- **Procedure** (USB/FDL, nothing written to storage):
  1. `fastboot stage FSBL.bin`, `continue`: SPL, DDR init, memory test;
  2. SPL fastboot: `fastboot stage u-boot.itb` (built from the same tree),
     `continue`;
  3. U-Boot fastboot: `continue` → `bootm 0x140000000` (the USB boot mode
     command) fails with no kernel there and drops to the prompt;
  4. from the prompt: `fastboot 0` + `fastboot stage` three times to load a
     vendor Bianbu 6.18.3 initrd (15.9 MB), DTB and kernel `Image`
     (36.2 MB) into RAM, `crc32` of each, `booti`.

## Results

- SPL: memory test 0 errors, `test_pattern` 0 errors, header cleared.
- **U-Boot 2022.10 relocates and runs**: `DRAM: 16 GiB`, 693 devices
  bound, SPI NOR, UFS (`KINGSTON TY7B-128`, empty GPT), USB host scan; the
  prompt stayed responsive for more than 9 minutes.
- **52 MB loaded over fastboot and read back correctly**: `crc32` in
  U-Boot of the initrd (`e461a4db`), DTB (`7e1a7ad4`) and kernel
  (`c040623a`) equals the host files. `booti` then moved the kernel to
  `0x102200000` and the ramdisk and DTB to the top of the first 12 GiB
  (`0x3fae54000`).
- No `save DDR training info` line: nothing was written to flash.
- **Linux did not start**: nothing after `Starting kernel ...`. In the
  USB/FDL path the SPL loads only `u-boot.itb` (a standalone U-Boot, no
  OpenSBI: the vendor image ships OpenSBI separately as `fw_dynamic.itb`,
  loaded from storage in a normal boot), so there is no SBI firmware under
  the kernel. This is a boot-flow limit of the bench, not a memory fault.
  The board has no installed system (UFS empty, no NVMe link, no SD/USB
  storage).

## Conclusion

DRAM initialised by the free firmware (quickboot order, reference envelope)
is good enough to relocate and run U-Boot and to hold 52 MB of data intact
across the address space. Booting Linux on this bench additionally needs
OpenSBI in the USB/FDL chain (or a storage boot); that is outside the DDR
firmware. Work returns to the training stages.
