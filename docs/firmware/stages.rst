.. SPDX-License-Identifier: GPL-2.0-or-later

Training stages
===============

``src/train/main.c`` validates the message block (DevInit bit set, reserved
bits clear, rate 4800/6400, ``CsPresent`` and ``X8Mode`` legal), writes
``PmuRevision`` and runs the enabled stages in dependency order:

WrLvl (1) → RxEn (2) → RdDQS (3) → WrDq (4) → MxRdLat (9) → DRAM DCA (6) →
PHY RD DCA (7) → PHY WR DCA (8) → DRAM DFE (10) → LPCA (12).

Each stage gets major ``0x40 + bit`` and a ``LOG_ID_STAGE`` frame. All stages are
stubs today (``src/train/stages.c``). The sweep and eye-selection code they
will share is ready (:doc:`training-engine`).

:dc:`07` reports that ``SequenceCtrl = 0x0009`` (DevInit + RdDQS) alone gives
read delays within 1–2 taps of a full run, so RdDQS is the first stage to
implement once its register sequence is specified (questions U1–U5).

RdDQS probe
-----------

``src/train/rddqs.c`` holds the first code for ``SequenceCtrl[3]``: the
hardware transfer-and-compare primitive of :dc:`07` (burst configuration,
one-shot run of the burst at ACSM entry 200, per-lane result
``0x01<d><l>b7``, sticky clear through ``0x01f0bb``), used as a probe that
streams raw results (log ``0x0844``) and restores what it changed. It does
not build the burst or program the pattern generator yet. [:exp:`016`]: the
burst runs to done, the result register reads ``0`` at every delay, and no
register in ``0xb0``–``0xbf`` reacts; the compare is not armed (questions
W1–W4).

A stage may run without DevInit when ``Quickboot != 0``. With the private
flag ``0xa5`` in message byte 0 the quickboot path ends by writing
``SequenceCtrl = 0x0008``, so that a second start of the firmware runs RdDQS
alone (the host cannot rewrite the message block after a PMU run).

[:exp:`017`]: with the arming of docsClean eae6813 (pattern generator group
``0x07``, pattern mode registers MR31–MR34, ``0x01f001 = 1``, ``MR46 = 4``,
``0x200c0/c1 = 8``, ``0x01f056 = 1``) the result still reads ``0`` at every
read delay and every write delay (``0x10``–``0x13`` over 0..504). The stage
code keeps this arming and its exit sequence; something that training sets
before RdDQS is still missing (questions X1–X3).

[:exp:`019`]: with the preconditions of docsClean e00a07a (PLL clock mode,
``0x200c0/c1 = 0``, ``0x2007a/7e = 0``, pass 1 pair ``0x7f011 = 0x16`` /
``0x7f060 = 0x88``), the burst written by the firmware itself at ACSM entry
200 and the ACSM routing ``0x3f084 = 0`` / ``0x01f00a = 0``, the result is
still ``0`` everywhere, and the memory test fails after the probe. The probe
is not safe to run before DDRC activation yet (questions Z1–Z3).
