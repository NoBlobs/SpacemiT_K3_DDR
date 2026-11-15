.. SPDX-License-Identifier: GPL-2.0-or-later

DevInit (``SequenceCtrl[0]``)
=============================

Status: **not implemented** (stub returns ``TRAIN_NOT_IMPLEMENTED``; the
training firmware then ends with ``0xff`` and the SPL halts before DDRC
activation).

Plan
   JEDEC LPDDR5 power-up and initialisation (reset, tINIT, MRW of the MR
   inputs from the message block, ZQ, WCK2CK) issued through the PHY command
   path, after the clock-period calibration described in :dc:`07`.
   Oracle: MRR of MR5–MR8 per channel/rank (Micron manufacturer ID, density
   and I/O width consistent with ``MT62F2G32D4DS``).

Inputs to read first: :dc:`07` (``DevInit`` master clock-period
calibration), :dc:`02` group ``0x02``, :dc:`14` failure signatures.
