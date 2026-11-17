.. SPDX-License-Identifier: GPL-2.0-or-later

Board access (bench setup)
==========================

Serial console
   ``/dev/ttyUSB0``, 115200 8N1 (``SERIAL_PORT=/dev/ttyUSB0`` for
   ``tools/bench.sh``, whose default is ``/dev/ttyUSB1``). The SPL
   prints PMU mailbox traffic here when ``CONFIG_K3_DDR_PMU_MBOX_LOG=y``.

USB OTG
   connected to the host; used by the BootROM USB download mode (FDL /
   fastboot).

SPI NOR
   ``w25q64dw``, 8 MiB. Never written by this project: every test is a RAM
   staging through the BootROM FDL mode, entered at power-up:

   .. code-block:: sh

      fastboot stage build/fsbl-probe/FSBL.bin
      fastboot continue

   The free-firmware SPL ignores any envelope in flash: it trains, or with
   ``K3_DDR_REF_ENVELOPE`` uses the embedded fixture
   (:doc:`../uboot-spl/quickboot-host`).

Boot chain through USB/FDL [:exp:`014`]
   1. BootROM FDL: ``fastboot stage FSBL.bin``, ``fastboot continue``; the SPL
      runs DDR init.
   2. SPL fastboot: ``fastboot stage u-boot.itb``, ``fastboot continue``.
   3. U-Boot proper enters ``fastboot 0``; ``fastboot continue`` leaves it and
      runs ``bootm 0x140000000`` (the USB boot mode command, for an image
      staged in RAM), then drops to the prompt if there is none.

   ``u-boot.itb`` built from the tree is a standalone U-Boot without OpenSBI
   (the vendor image ships it separately as ``fw_dynamic.itb``, loaded from
   storage in a normal boot), so a kernel started this way prints nothing
   after ``Starting kernel ...``. From the U-Boot prompt, ``fastboot 0`` plus
   ``fastboot stage`` loads a file at ``0x140000000``; Ctrl-C returns to the
   prompt.

Storage [:exp:`014`]
   UFS ``KINGSTON TY7B-128`` (119 GiB) with an empty GPT; no NVMe link; no
   SD or USB storage; SPI NOR ``w25q64dw`` (8 MiB). No system is installed.

Recovery
   After a PMU or bus hang only a power cycle recovers the board (done by a
   person), followed by FDL staging again.

   After an SPL run ends (including the deliberate ``hang()`` of the probe
   configuration) the USB device ``361c:1001`` may stay listed by ``fastboot
   devices``, but ``fastboot stage`` blocks: **every run needs a power cycle
   first** (observed 2026-11-15, second staging attempt).

Helper: ``tools/bench.sh`` stages an FSBL, captures the serial log with
timestamps and decodes the mailbox stream (``tools/mbox_decode.py``).
