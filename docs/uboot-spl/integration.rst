.. SPDX-License-Identifier: GPL-2.0-or-later

Integration of the free firmware
================================

Kconfig (``drivers/ddr/spacemit/k3/Kconfig``)
---------------------------------------------

``K3_DDR_LP5_FREE_FW``
   use the free firmware. Requires ``K3_DDR_LPDDR4X=n`` (single-type build,
   uncompressed tables) and not ``DDR_TRAINING_UPDATE_FSBL``.
``K3_DDR_LP5_FREE_FW_PREFIX``
   path prefix of ``<prefix>.imem.bin`` and ``<prefix>.dmem.bin``.
``K3_DDR_PMU_MBOX_LOG``
   log every major and stream frame with a µs timestamp.
``K3_DDR_PMU_MBOX_TIMEOUT_MS``
   bound for each mailbox wait (default 30000; 0 = unbounded).
``K3_DDR_PMU_STOP_AFTER_FW``
   experiments: run the PMU on both controllers, stop it, print the elapsed
   time and ``hang()`` without activating the DDRC.
``K3_DDR_PMU_COMPARE_DUMP``
   with the ``csrdump`` firmware: after the PMU stops, run the normal result
   collection, compare ``phypara``/ACSM with the PMU's copy at DMEM index
   ``0x5c000`` and print every host value (``PMU<n> P``/``A`` lines, extracted by
   ``tools/dump_extract.py``).

Fragments: ``configs/k3_freefw.config`` (training),
``configs/k3_freefw_probe.config`` (probe experiments) and
``configs/k3_freefw_csrdump.config`` (CSR read-out comparison).

Code changes
------------

``lpddr5_free_fw.S``
   ``.incbin`` of both images, wrapped as ``phy_init_config`` linear tables:
   ``k3_free_fw_imem_table`` (base ``0x50000``) and ``k3_free_fw_dmem_table``
   (base ``0x5a000`` = ARC ``0x80004000``).
``lpddr5_pre_training_table.c``
   the vendor firmware arrays are compiled out; the IMEM slot of
   ``lp5_pre_train_table[]`` points to ``k3_free_fw_imem_table``.
``lpddr5_init.c``
   after the DMEM clear, loads ``k3_free_fw_dmem_table``; a non-``0x07`` mailbox
   result aborts the controller and sets ``k3_pmu_failed``.
``ddr_init.c``
   forces full training (no free quickboot firmware yet); halts with a message
   if any PMU run failed, so DDRC ports are never enabled on an untrained PHY.
``lpddr_common.c``
   new mailbox receiver, see :doc:`mailbox-host`.

The SPL still writes the vendor DMEM table at ``0x58000``–``0x584d1``; the free
firmware reads only the documented message block there.
