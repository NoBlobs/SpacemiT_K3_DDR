.. SPDX-License-Identifier: CC-BY-SA-4.0

Message block
=============

320 bytes at ARC ``0x80000000`` (host index ``0x58000``), little-endian, 293
fields [spec: :dc:`05`, :dc:`appendix-message-block-fields`]. The firmware never
maps it to a C struct: ``tools/gen_msgblk.py`` turns the appendix into
``MB_<field>`` offsets, sizes and directions, and ``tests/test_msgblk.c`` checks
them against the facts in the chapters.

Inputs used by the firmware
---------------------------

``SequenceCtrl`` (``0x010``)
   stage mask: 0 DevInit, 1 WrLvl, 2 RxEn, 3 RdDQS, 4 WrDq, 6 DRAM DCA,
   7 PHY RD DCA, 8 PHY WR DCA, 9 MxRdLat, 10 DRAM DFE, 12 LPCA. Bits 5, 11,
   13–15 reserved.
``DRAMFreq`` (``0x006``)
   data rate in MT/s (``0x1900`` = 6400).
``CsPresentChA/B``, ``EnabledDQsChA/B``, ``X8Mode``
   topology; ``X8Mode`` valid LP5 values ``0x0``, ``0xa``, ``0xf``.
``HdtCtrl`` (``0x012``), ``MsgMisc[4]``
   streaming filter: a frame is sent when its level is lower than ``HdtCtrl``.
MR inputs ``0x05a``–``0x0cd``
   per A0/A1/B0/B1.

Outputs written by the firmware
-------------------------------

``PmuRevision`` (our firmware version), ``CsTestFail``, CDDs, trained VREFs,
DFE/DCA, four margins per rank, ``QBPll*``.

What the SPL writes there
-------------------------

The vendor SPL fills ``0x58000``–``0x584d1`` from its table ``training_table_54``
and patches a few fields (PDDS ``0x58031``/``0x58032``, 2D enable ``0x58067``).
Values seen by the PMU [:exp:`001`]: ``MsgMisc = 0x01``, ``Pstate`` byte ``0x30``
(meaning unknown), ``DRAMFreq = 0x1900``, ``DfiFreqRatio = 4``,
``Train2DMisc = 0x0004``, ``SequenceCtrl = 0x125f`` (bits 0–4, 6, 9, 12),
``HdtCtrl = 4``, ``DFIMRLMargin = 1``, ``CATrainOpt = 0x8b``, ``X8Mode = 0``,
``EnabledDQsChA/B = 16``, ``CsPresentChA/B = 3``. Full dump in
:lr:`001-probe-core-mailbox-rtc/mbox.txt`.
