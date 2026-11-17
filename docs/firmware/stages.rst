.. SPDX-License-Identifier: GPL-2.0-or-later

Training stages
===============

``src/train/main.c`` validates the message block (DevInit bit set, reserved
bits clear, rate 4800/6400, ``CsPresent`` and ``X8Mode`` legal), writes
``PmuRevision`` and runs the enabled stages in dependency order:

WrLvl (1) → RxEn (2) → RdDQS (3) → WrDq (4) → MxRdLat (9) → DRAM DCA (6) →
PHY RD DCA (7) → PHY WR DCA (8) → DRAM DFE (10) → LPCA (12).

Each stage gets major ``0x40 + bit`` and a ``LOG_ID_STAGE`` frame. DevInit is
implemented (:doc:`devinit`); RdDQS holds the transfer-and-compare probe
below; the other stages are stubs that report "not implemented"
(``src/train/stages.c``), so a full training run ends with ``0xff``. The
sweep and eye-selection code they will share is ready
(:doc:`training-engine`).

Execution order in the reference [spec: :dc:`07`, e00a07a]: bit-aligned passes
with retraining loops (``DisableTrainingLoop`` suppresses them), the 2D
refinements of WrDq and RdDQS after the 1D stages, MxRdLat last; LPCA runs
inside the DevInit tail. Our dispatcher runs each enabled stage once, in the
order above [design].

:dc:`07` reports that ``SequenceCtrl = 0x0009`` (DevInit + RdDQS) alone gives
read delays within 1–2 taps of a full run, so RdDQS is the first stage to
implement. Its register sequence is now specified by :dc:`07`; see below
for what the board does with it.

RdDQS (``SequenceCtrl[3]``): transfer-and-compare probe
--------------------------------------------------------

Status: **the stage runs the hardware transfer-and-compare primitive of**
:dc:`07` **as a probe; the PHY never reports an error, so nothing is trained
yet** [:exp:`016`, :exp:`017`, :exp:`019`].

``src/train/rddqs.c`` implements, for ``Pattern = 0`` (2D pass 1):

1. save every register it changes;
2. PLL clock mode (``0x200f8 = 0``, ``0x200a2[1:0] = 2``);
3. ``0x20072/73 = CsPresent``, ``0x2007a = 0x2007e = 0``, ``0x2f0a0`` pulse;
4. burst generator: ``0x2006e = 0``, ``0x01f056 = 1``, ``0x01f0a7 = 0``,
   ``0x200c0 = 0x200c1 = 0``, ``0x20079 = 0x40``, ``0x01f0b3 = 0x01f0b4 =
   0x01f0b1 = 1``, ``0x01f0b0 = 0x34`` (``0x30`` at ratio 2);
5. pattern generator: ``0x7f061..64 = 0xffff``, ``0x7f065 = 0``,
   ``0x7f011 = 0x16``, ``0x7f060 = 0x88``, ``0x7f0d6 = 0x7f0d7 = 0x100``;
6. ACSM routing ``0x3f084 = 0``, ``0x01f00a = 0``, ``0x2002a = 1``,
   ``0x20041 = 1``;
7. MRW one-shots from entry 0: ``MR33 = 0x3c``, ``MR34 = 0x5a``,
   ``MR31 = MR32 = 0``; ``0x01f001 = 1``; ``MR46 = 4``; ``0x2f0a0`` pulse;
8. the 20-entry write-then-read burst written at ACSM entry 200 for one
   rank (``burst_build()``, :doc:`../hardware/acsm`);
9. per point (``sweep()``): the candidate value in lane-broadcast form
   (``0x01<d>f<reg>``), one-shot of entries 200..219, read ``0x01<d><l>b7``
   for the nine lanes, ``0x01f0bb = 0x3ff``, then ``0``; the lanes are
   restored after each sweep;
10. exit: ``0x01f0b0 = 0x01f0b1 = 0``, ``0x01f056 = 0``, ``0x200c0/c1 = 0``,
    ``0x20079 = 0``, ``0x7f011 &= ~0x10``, ``0x200a3 = 1``, ``0``, ``MR46 = 0``,
    ``0x01f001 = 0``, then every saved value written back and the clock mode
    as found.

It streams raw frames (``RDDQS_PROBE``, ``0x0844``): the state found, each
MRW run, and for every point the nine ``b7`` words. The sweeps are ``0x4e``
and ``0x4f`` 0..124 (all dbytes on rank 1, dbyte 0 on rank 0) and the write
delays ``0x11``/``0x13`` and ``0x10``/``0x12`` 0..504 on dbyte 0.

Results so far:

* every ACSM one-shot reaches done (MRW 1 poll, burst 4 polls);
* ``b7`` is ``0`` at every point, including write delays far from the trained
  ``0x13b``–``0x14d``, with and without ``0x000a4 = 1``, ``0x01f<l>be =
  0x41``, ``0x200c0/c1 = 8``, ``0x20042/44`` set, or the direct clock;
* the memory test fails after a probe run that switches the clock mode,
  while the same quickboot without the probe is clean [:exp:`019`]. The
  probe must not run before DDRC activation in a boot that is meant to work.

What is missing is not known (questions Z1–Z3 in ``questions.md``). The
selection code the stage will use is ready (:doc:`training-engine`).

A stage may run without DevInit when ``Quickboot != 0`` (:doc:`quickboot`,
"Second start").
