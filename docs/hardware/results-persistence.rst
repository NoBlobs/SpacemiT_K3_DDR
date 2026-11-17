.. SPDX-License-Identifier: CC-BY-SA-4.0

Training results and persistence
================================

Per controller, collected by the host after the PMU run [spec: :dc:`09`,
/10]:

========== ========== =============================================
Offset     Size       Content
========== ========== =============================================
``0x0000`` ``0x0140`` message block
``0x0140`` ``0x2864`` ``phypara``: 5170 halfwords, 69 ordered segments
``0x29a4`` ``0x2000`` ACSM SRAM, 4096 halfwords
========== ========== =============================================

The ``phypara`` order is generated from the appendix by
``tools/gen_phypara.py``; ``tests/test_phypara.c`` checks 5170 entries,
segment 48 = ``0x9002e``–``0x9062d`` and that all indices are unique.

Before training the ACSM SRAM holds random power-on data (different on each
controller, almost no zeros); the SPL pre-training flow does not initialise it
[:exp:`002`].

Envelope ``DDRT`` (``0x9800`` bytes, CRC-32 over ``[0x08, 0x9800)``) lives in SPI
``bootinfo + 0x10000`` on the reference board: a 128-byte header, then one
record per controller (message block, ``phypara``, ACSM image) and a zero
tail from ``0x93c8``. In the SPL it is ``struct ddr_info_t`` at
``DDR_TRAINING_INFO_BUFF = 0xC08D0000``.

Reference envelopes
-------------------

:dc:`appendix-reference-envelope` (SPI cold boot) and
:dc:`appendix-reference-envelope-fdl` (nine USB/FDL captures ``E1``–``K3``)
publish complete 6400 MT/s envelopes of this board as hex blocks.
``tools/gen_ref_envelope.py`` rebuilds the ``0x9800``-byte binary from an
appendix and checks the CRC the appendix states (SPI zero-padded
``0xa16d989b``, ``E1`` ``0xfa1e9f98``). They are development fixtures: a
result of the reference training, not of ours.

Differences between a USB/FDL and the SPI capture [spec: :dc:`14`]:
``0x20011 = 0x006f`` (FDL) / ``0x000f`` (SPI); ``TrainedVREFCA = 0x3f`` /
``0x27``; ``0xc0086 = 1`` / ``0``; ``0x030001 = 0x0004`` or ``0`` /
``0x003e``. Read-delay medians are the same (61–63 taps) when the RX ODT is
the default 60 Ω.

Using a saved result
--------------------

* Writing a saved result into the PHY after a full DevInit gives working DRAM
  in about 3 boots of 10, whichever envelope is used [:exp:`011`,
  :exp:`012`].
* In the quickboot order it works in 10 boots of 10
  (:doc:`quickboot-sequence`) [:exp:`019`].
* DevInit leaves ``0x20011 = 0x006f``, ``0x20060 = 3``, ``0xc0086 = 1`` and
  period values within 2 counts of every FDL envelope, so our DevInit ends in
  the same state as the reference DevInit for these registers [:exp:`010`,
  :exp:`012`].
* U-Boot proper saves whatever valid envelope it finds in the buffer to the
  boot private partition (``save_ddr_training_info()``). An experiment that
  puts a fixture there must clear the header before leaving the SPL
  [:exp:`014`].

Our board's SPI holds no envelope of ours; quickboot with a result of our own
training is the last step of the project.
