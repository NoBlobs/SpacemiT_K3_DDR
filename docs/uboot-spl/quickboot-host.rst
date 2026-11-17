.. SPDX-License-Identifier: GPL-2.0-or-later

Host-assisted quickboot and experiment hooks
============================================

The SPL side of the quickboot experiments (``K3_DDR_REF_*`` options,
:doc:`integration`). All of it is development code: it uses a reference
training result published in ``docsClean`` as a fixture, tests the memory and
normally halts. The hardware order it follows is in
:doc:`../hardware/quickboot-sequence`; the PMU side is in
:doc:`../firmware/quickboot`.

Flow with ``K3_DDR_REF_QUICKBOOT``
----------------------------------

``ddr_init.c``
   copies the fixture (``lpddr5_ref_env.S``, ``.incbin`` of
   ``K3_DDR_REF_ENVELOPE_PATH``) over ``DDR_TRAINING_INFO_BUFF`` and prints
   its header. Whatever the boot left there is not used.

``lpddr5_init.c``, per controller
   1. normal DDRC and PHY setup, pre-training tables, free firmware loaded;
   2. ``k3_ref_load_message()``: the fixture's message block to DMEM with
      ``SequenceCtrl = 1``, ``Quickboot = 1`` and the trained VREF / DFE / DCA
      values copied into ``MR12``, ``MR14``, ``MR15``, ``MR24``, ``MR30``
      (byte 0 = ``0xa5`` with ``K3_DDR_REF_PROBE``);
   3. ``k3_qb_pre_pmu()``: every ``phypara`` entry of the fixture written to
      its CSR, in serialization order, with the commit triad at each change
      of CSR group; then two ``0x2f052`` pulses (when ``MsgMisc[0]``),
      ``0x20090 = 1``, ``0x20060 = 3``, ``0xc0086 = 1``;
   4. PMU start (``0xd0000 = 1``, ``0xd0099 = 9, 1, 0``), mailbox service until
      major ``0x07``, PMU stop;
   5. ``k3_qb_post_pmu()``: Pass 3 entries written again, ``0x2004e`` kept at
      the value the PMU sampled, ``0x2f052`` pulse, ``0x90808/13`` recomputed
      from ``0x20314/15``, commit triad, ``0x20037`` bit 6 cleared, ACSM image
      written and read back, ``0xd00e7 = 0x0600``, ``0xc0080 = 2``,
      ``0xd0000 = 1``;
   6. DDRC activation as in the vendor flow.

Entries not written in step 3: ``0xc0080``, the ``0xd0xxx`` block and the
eight runtime controls ``0x20020/27/28/2a/31/41/60/78``.
``k3_qb_pass()`` classifies each CSR as Pass 1, 2 or 3 of :dc:`11`; group
``0x01`` offsets in none of its lists are treated as Pass 1.

``ddr_init.c``, after both controllers
   ``k3_ref_memtest()`` and the vendor ``test_pattern()``; then the training
   info header is cleared (see below) and the SPL halts, or continues with
   ``K3_DDR_REF_BOOT`` if both tests are clean.

Memory test
-----------

``k3_ref_memtest()`` writes and reads back 64 KiB at the start of every
256 MiB over the whole part (64 blocks for 16 GiB), with the address as
pattern and then its complement. It prints the error count, how many wrong
words change on a second read, errors per block and per data bit, and the
first 24 failing words. The per-block and per-bit counts are what identify
a rank, a byte lane or a single bit (:doc:`../hardware/dq-data-path`). It is
a sampled test, not a full memory test.

The SPL tiny ``printf`` has no ``%llu``/``%llx``: 64-bit values are printed as
two ``%08x`` halves or as ``unsigned long`` with ``%lx``.

Never saving the fixture
------------------------

U-Boot proper writes a valid envelope found in ``DDR_TRAINING_INFO_BUFF`` to
the boot private partition (``save_ddr_training_info()`` in
``board/spacemit/k3/k3.c``). With ``K3_DDR_REF_ENVELOPE`` the SPL therefore
clears the 128-byte header of the buffer after the memory test, in every
path, so the fixture is never written to flash [:exp:`014`].

Hooks
-----

``K3_DDR_REF_BOOT`` [:exp:`014`]
   continue to U-Boot when the memory test and ``test_pattern`` report no
   error; halt otherwise.

``K3_DDR_REF_EYESCAN`` [:exp:`015`]
   ``k3_ref_eyescan()``: pre-writes 512 KiB in each of the 64 blocks, then for
   each ``0x01<r><l>4e`` and ``4f`` of both PHYs writes 0, 2, …, 126, reads
   4 KiB of every block at each value and restores the trained value. Each
   point reads a chunk not read recently (128 chunks, alternating halves per
   scan) so the data cache cannot hide an error. Output: one ``ES`` line per
   CSR with the passing window, the pass bitmap, the blocks and data bits
   that failed. ``tools/eyescan_stats.py`` summarises a log.

``K3_DDR_REF_PROBE`` [:exp:`016`]
   after step 5 without the finalization (``0xd00e7``, clocks, mux), starts
   the PMU a second time; the firmware, having seen the flag ``0xa5``, runs
   RdDQS alone. Then step 5 is repeated in full. The host cannot change
   ``SequenceCtrl`` itself at that point
   (:doc:`../hardware/addressing`).

Earlier variant: ``K3_DDR_REF_ENVELOPE`` without ``K3_DDR_REF_QUICKBOOT``
runs a full DevInit (``Quickboot = 0``) and then
``k3_ref_restore_training_result()`` writes the saved result with the
keep-live / blocklist rules of :dc:`11`. Kept as the record of experiments
:exp:`011` and :exp:`012` (3 clean boots of 10).

Fragments
---------

============================================ =======================================
``configs/…``                                Use
============================================ =======================================
``k3_freefw_refenv.config``                  011: DevInit + restore, SPI fixture
``k3_freefw_refenv_e1.config``               012: DevInit + restore, FDL fixture E1
``k3_freefw_qb_e1.config``                   013, 018, 019: quickboot, test, halt
``k3_freefw_qb_e1_boot.config``              014: quickboot, continue to U-Boot
``k3_freefw_qb_e1_eyescan.config``           015: quickboot, read-eye scan
``k3_freefw_qb_e1_probe.config``             016, 017, 019: second PMU run (RdDQS)
============================================ =======================================

.. code-block:: sh

   python3 tools/gen_ref_envelope.py docsClean/appendix-reference-envelope-fdl-e1.md \
           build/envelope/ref6400-fdl-e1.bin
   make fsbl FW=train FSBL_FRAG=configs/k3_freefw_qb_e1.config
   SERIAL_PORT=/dev/ttyUSB0 tools/bench.sh build/fsbl-train/FSBL.bin qb-run 120 halting
