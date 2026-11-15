.. SPDX-License-Identifier: GPL-2.0-or-later

Role of the SPL in this project
===============================

All DDR code lives in ``drivers/ddr/spacemit/k3/``. Per controller,
``init_snps_lp5_ddrc()`` (``lpddr5_init.c``) does:

#. DDRC timing/DFI/address-map setup, DCLK release, ``dfimisc = 0x00010014``;
#. ``phyinit_lp5_pre_training()``: applies the PHY tables
   ``lp5_pre_train_table[]`` with topology and I/O overrides
   (``lpddr_training_table_init()`` in ``lpddr_common.c``). One entry of that
   list is the PMU IMEM image (index ``0x50000``) and another is the DMEM table
   at ``0x58000`` (message block + parameter area). Then it clears
   ``0x584d2``–``0x5ffff``;
#. ``pwrctl |= 1 << 11``, mux = 1, ``MICRO_RESET`` 9, 1, 0;
#. ``major_message_all()`` until ``0x07``;
#. ``MICRO_RESET = 1``, mux = 0, ``phyinit_lp5_training()`` (post-run table),
   ``save_snps_ddrc_training_result()`` (message, ``phypara``, ACSM);
#. DDRC activation and port enable.

``spacemit_ddr_probe()`` (``ddr_init.c``) picks quickboot vs. training, runs
both controllers, then ``test_pattern()`` on DRAM.

What the host tables write
--------------------------

``tools/spl_tables.py`` extracts, from the GPL table sources, the PHY CSR writes
of the host tables (skipping the IMEM/DMEM images). For dual-rank x16 at
6400 MT/s:

* **pre-training** (``lp5_pre_train_table`` + ``phy_override_pre_seq_lp5_drank_x16_6400``):
  457 registers — group ``0x01`` 324, ``0x02`` 52, ``0x03`` 56, ``0x0c`` 14,
  ``0x09`` 9, ``0x0d`` 2 (including ``0xc00f1``–``0xc00ff``);
* **post-training** (``lp5_train_table`` + ``phy_override_seq_lp5_drank_x16_6400``),
  applied after the PMU run: 4047 registers — ACSM SRAM ``0x41000``–``0x410b7``
  and ``0x41388``–``0x41c57`` (2440), ``0x9002e``–``0x9050d`` and
  ``0x9070c``–``0x9071b`` (1317 in group ``0x09``), group ``0x02`` 175, ``0x07``
  73, plus a few control registers (``0xd0000 = 1``, ``0xd00e7 = 0x0600``,
  ``0xc0080 = 2``).

So the mission-mode ACSM program is supplied by the host after training; during
the PMU run the ACSM SRAM still holds power-on data.

What we do **not** use
   the vendor PMU firmware arrays (``lp5_training_fw*``, ``lp5_quickboot_*``):
   never opened, excluded from the build in free-firmware mode.
