.. SPDX-License-Identifier: CC-BY-SA-4.0

ACSM command sequencer
======================

The ACSM sends LPDDR5 commands from a small SRAM. DevInit, quickboot and the
training stages all use it [spec: :dc:`07`].

SRAM and control registers
--------------------------

* SRAM: 4096 halfwords at CSR ``0x41000`` (ARC ``0x90082000 + 2j``). One
  entry (slot) is 4 halfwords; a command descriptor is 8 halfwords (two
  slots). Entry *n* is at ARC ``0x90082000 + 8n``.
* Before training it holds random power-on data [:exp:`002`]. After a run the
  final image is part of the saved result (4096 halfwords).
* Host access: the SPL writes the whole image after the PMU halts
  (``MICRO_CONT_MUX_SEL = 0``, ``0xc0080 = 3``) and reads back the same 4096
  halfwords, 0 differences [:exp:`019`].

============ =====================================================
Register     Role
============ =====================================================
``0x20020``  run: ``1`` starts, ``0`` stops
``0x20021``  **done** flag: ``1`` = idle / done, ``0`` = running
             [:exp:`009`; earlier specs called it "busy"]
``0x20022``  first entry (+ pstate)
``0x20023``  last entry (+ pstate)
``0x20027``  mode; ``0`` for one-shot
``0x20031``  continuous-loop enable; ``0`` in DevInit
``0x20041``  burst enable (+ pstate); ``1`` while the PMU sends commands
``0x2002a``  ``1`` before a burst
============ =====================================================

One-shot run:

.. code-block:: text

   0x20022 + ps = first;  0x20023 + ps = last
   0x20027 = 0;  delay_dfi_clks(6)
   0x20020 = 1;  poll while 0x20021 == 0;  0x20020 = 0

Poll counts seen (CSR reads until done) [:exp:`010`, :exp:`016`–:exp:`019`]:
4-unit NOP flush 1; 23-entry MRW table 5; ZQCAL burst 3; ``MR16`` FSP switch
with a 256-unit NOP 12; 20-entry training burst 4 (48 with ``0x20042 =
0x108``).

Routing
-------

The ACSM drives the pads only with the routing DevInit sets before its
bursts [spec: :dc:`07` step 6.2]:

.. code-block:: text

   0x3f084 = 0;  0x01f00a = 0;  0x2002a = 1;  0x20041 + ps = 1

A final (mission-mode) state holds ``0x030084 = 1`` and ``0x01000a = 1``
[:exp:`019`: values of the reference result]. After a burst DevInit clears
``0x20041`` and ``0x01f00a`` and pulses ``0x2f0a0``.

Entry formats
-------------

Halfword 0 carries the chip select in bits ``[15:14]`` (``1`` = rank 0,
``2`` = rank 1, ``3`` = both) for every command.

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - Command
     - Slots (4 halfwords each)
   * - NOP, repeat count ``r`` (``r + 1`` units)
     - ``0 0 0 ((r & 7) << 12) | 0x0b00`` then ``0 0 0 (r & 0xf8) << 7``; ``r = ceil(cycles / div) − 1`` [:exp:`010`]
   * - MRW
     - MRW-1 slot, MRW-2 slot, then a NOP pair with repeat count 2 (``0x2b00``) [:exp:`009`]
   * - MPC ZQCAL latch
     - ``f870 0000 0000 0010``, ``0306 0000 0000 0000`` between NOPs [:exp:`010`]
   * - ``cmd 0x28`` (quickboot tail)
     - ``c028 0000 0000 0010`` [spec: :dc:`11`]

The encoders are in ``src/train/devinit_calc.h`` (unit-tested).

Training burst at entry 200
---------------------------

A sweep compiles its burst once at entry 200 (halfword 800, CSR ``0x41320``)
and runs entries 200..219 as a one-shot for each candidate point. For
``Pattern = 0`` it is a per-rank write-then-read burst [spec: :dc:`07`]. The
reference result holds it for rank 1; ours writes the same 20 slots with the
chip-select field of the wanted rank (``src/train/rddqs.c``):

.. code-block:: text

   200  0000 0000 0000 0080    201  0000 0000 0000 0000
   202  0000 0000 0000 2b00    203  0000 0000 0000 0400
   204  8000 0000 0000 0410    205  801c 00d0 0000 0010
   206  8060 0084 0000 7f10    207  0000 0000 0000 0000
   208  8000 0000 0000 0410    209  0000 0000 0000 0000
   210  0000 0000 0000 3f00    211  0000 0000 0000 0400
   212  8000 0000 0000 0410    213  802c 0d00 0000 0010
   214  8020 0803 0000 7f10    215  0000 0000 0000 0000
   216  8000 0000 0000 2e10    217  0000 0000 0000 0400
   218  0000 0000 0000 0500    219  0000 0000 0000 0000

The burst runs to done from our firmware, but the per-lane compare result
never reports an error [:exp:`016`–:exp:`019`, open].
