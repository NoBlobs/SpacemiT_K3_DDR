.. SPDX-License-Identifier: GPL-2.0-or-later

Board access (bench setup)
==========================

Serial console
   ``/dev/ttyUSB1``, 115200 8N1 (``picocom -b 115200 /dev/ttyUSB1``). The SPL
   prints PMU mailbox traffic here when ``CONFIG_K3_DDR_PMU_MBOX_LOG=y``.

USB OTG
   connected to the host; used by the BootROM USB download mode (FDL /
   fastboot).

SPI NOR
   blank. The BootROM therefore always enters FDL at power-up, and every test
   is a RAM staging, never a flash write:

   .. code-block:: sh

      fastboot stage build/fsbl-probe/FSBL.bin
      fastboot continue

   No ``DDRT`` envelope exists, so the SPL always runs full training.

SSH
   ``ssh thalles@<IP>`` once Linux is up; the IP is read on the serial console
   (``ip -4 addr``) and recorded here: *not yet known*.

Recovery
   After a PMU or bus hang only a power cycle recovers the board (done by a
   person), followed by FDL staging again.

   After an SPL run ends (including the deliberate ``hang()`` of the probe
   configuration) the USB device ``361c:1001`` may stay listed by ``fastboot
   devices``, but ``fastboot stage`` blocks: **every run needs a power cycle
   first** (observed 2026-11-15, second staging attempt).

Helper: ``tools/bench.sh`` stages an FSBL, captures the serial log with
timestamps and decodes the mailbox stream (``tools/mbox_decode.py``).
