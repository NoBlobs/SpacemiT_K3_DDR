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

What we do **not** use
   the vendor PMU firmware arrays (``lp5_training_fw*``, ``lp5_quickboot_*``):
   never opened, excluded from the build in free-firmware mode.
