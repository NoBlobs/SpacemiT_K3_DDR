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
