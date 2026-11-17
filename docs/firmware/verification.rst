.. SPDX-License-Identifier: GPL-2.0-or-later

Verification and clean-room record
==================================

Automatic checks
----------------

``make check``
   ELF audit (``tools/audit.sh``).
``make test``
   native tests (``tests/``, host compiler):

   * ``test_msgblk`` — message block layout against docsClean;
   * ``test_phypara``, ``test_phypara_fw`` — ``phypara`` order (69 segments,
     5170 unique indices) and the compact firmware table;
   * ``test_devinit_calc`` — frequency buckets, two-tier delays, ACSM
     encoders (MRW, NOP pairs, MPC), step 3.6 selection;
   * ``test_eye`` — the training engine on synthetic eyes.

Tools
-----

.. list-table::
   :header-rows: 1
   :widths: 35 65

   * - Tool
     - Purpose
   * - ``tools/bench.sh``
     - stage an FSBL through FDL, capture the serial console with timestamps, decode the mailbox stream
   * - ``tools/serial_log.py``
     - the serial capture (stops at a regular expression)
   * - ``tools/mbox_decode.py``
     - decode ``PMU<n> M`` / ``S`` lines into named frames
   * - ``tools/gen_ref_envelope.py``
     - build a ``DDRT`` fixture from a docsClean appendix and check its CRC
   * - ``tools/eyescan_stats.py``
     - summarise the host read-eye scan of a serial log
   * - ``tools/gen_phypara.py``
     - ``phypara`` order tables from the docsClean appendix
   * - ``tools/gen_msgblk.py``
     - message block field offsets (``msgblk_fields.h``)
   * - ``tools/dump_extract.py``, ``dump_diff.py``, ``scan_extract.py``
     - CSV files from the ``csrdump`` / ``csrscan`` experiments
   * - ``tools/build_fsbl.sh``
     - in-tree U-Boot build with a config fragment
   * - ``tools/audit.sh``
     - ELF audit

Hardware checks
---------------

The SPL-side checks used with a restored result (sampled 16 GiB memory test,
read-eye scan) are described in :doc:`../uboot-spl/quickboot-host`.


Every hardware run is staged through FDL and logged; runs with relevant
findings become ``logsRefs/NNN-<name>/`` records (see :lr:`README.md`).

Clean-room record
-----------------

* 2026-11-15 — **source policy extended** (decision of the project owner):
  public open-source code of other SoCs using the same Synopsys DWC LPDDR4/5
  PHY (NXP i.MX U-Boot, TF-A ``drivers/nxp/ddr``) may be read for host-side
  register names, semantics and initialization tables. The PMU firmware
  binaries shipped with those projects are never downloaded, opened or used.
  Facts taken from them are labelled ``[public: <project> <file>]`` in this
  documentation.

* 2026-11-15 — the page summarising those public sources was **removed**:
  the only use of public values (DDR4-PHY PLL registers, experiment
  :exp:`005`) was refuted on the K3, and neither the training firmware nor the
  U-Boot patch uses anything from public code; DevInit follows :dc:`07` and our
  measurements only. The ``[public]`` evidence label is no longer used.

* 2026-11-15 — while locating the boundaries of the vendor firmware arrays in
  ``lpddr5_pre_training_table.c`` to exclude them from the build, two commands
  printed a few bytes of those arrays (the first three entries of
  ``lp5_training_fw_table`` and a few bytes at an array tail, the latter
  inside a subagent). They were not analysed and nothing in this project uses
  them. The vector table layout (entry 0 = ``0x1fc``) had already been chosen
  from the public ARCv2 ISA and :dc:`12`. Edits to that file are now made
  by line structure only, without displaying array contents.
