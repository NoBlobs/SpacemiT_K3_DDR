.. SPDX-License-Identifier: GPL-2.0-or-later

Quickboot
=========

Status: **the hardware part of quickboot runs in the free firmware and,
with the passes applied by the SPL, brings up 16 GiB at 6400 MT/s from a
reference training result: sampled memory test clean in 10 of 10 boots**
[:exp:`019`]. Not done: reading ``phypara`` from DMEM in the firmware, a
result of our own training, saving and loading the envelope.

Design
------

The reference quickboot firmware reads the saved ``phypara`` from DMEM
``0x58200`` and writes the CSRs itself, in three passes interleaved with the
hardware steps (:doc:`../hardware/quickboot-sequence`). Ours splits the work
[design]:

* the **SPL** writes the saved ``phypara`` to the CSRs before the PMU starts
  and repeats Pass 3 after it halts (:doc:`../uboot-spl/quickboot-host`);
* the **firmware** does everything that needs the PMU: resets, clock
  switching, ACSM bursts, DLL recalibration.

This keeps the firmware small and let each hardware step be tested alone.
Moving the passes into the firmware later needs only the pass tables.

Firmware path
-------------

``stage_devinit()`` calls ``quickboot_run()`` (``src/train/devinit.c``) when
the message block has ``Quickboot != 0``. It reuses the DevInit building
blocks but not the DevInit order: no period calibration, no step 3.6, no
``0x01<r>0a3`` mask, no ``tINIT`` wait.

==== ==================================================== ======================
Step Code                                                 Notes
==== ==================================================== ======================
1    ``devinit_phase0()``; delay shift from ``0xc0080``;  ``0x20090 = 1``, commit
     ``0x20009`` saved                                    strobe, ``0x2f052``
                                                          pulse, PHY signature
2    ``0x2006e = 1``, ``0x70060 = 0``, ``0x70011 = 0``;   startup script of the
     ``0x2008b``; ``0x20072/73``                          reference
3    ``devinit_phase1()``, ``dq_block_reset()``           direct clock, DQ reset
4    ``dram_reset_pll()``                                 as DevInit step 3.5,
                                                          400 µs PLL lock wait
5    ``devinit_phase6_1()``                               ACSM dividers, NOP
                                                          flush, PLL clock
6    ``qb_pulse_052()``; multiplier off; ``qb_mrw()``     mode registers, below
7    direct clock; ``0x2006e = 0``; per-rank step;        the step that made
     ``0x20072/73``; ACSM dividers; ``0x01f0b3/b4 = 0``   quickboot reliable
8    ``qb_dll_recal()``; commit strobe; ``0x000a4 = 1``   DLL code sampled while
                                                          ``0x2f0e0 = 1``
==== ==================================================== ======================

``qb_mrw()`` (ACSM one-shots, tags in ``DEVINIT_ACSM`` frames):

1. ``0x72`` — pass 0: ``MR16 = MR16_A0 | 0x41``, then the 18-entry table of
   the message block (MR19, 18, 1, 2, 3, 10, 11, 17, 12, 14, 15, 16, 20, 22,
   24, 37, 40, 41; MR12/14/15/17/24 per rank);
2. ``0x75`` — ``MR16 = MR16_A0 | 0x45`` and a 256-unit NOP, then 4 µs;
3. ``0x76`` — pass 1: the table with ``MR16 = (MR16_A0 | 0x45) & 0xbc``;
4. ``0x73`` — ZQCAL latch;
5. ``0x74`` — per rank ``MR40 = 0x40``, ``MR37 = 0x40``; ``MR28 = MR28_A0 | 2``
   when ``MR28_A0[5] = 0``; ``cmd 0x28``; NOP.

Differences from :dc:`11` [design]: the QBPll CSRs are not loaded from the
message block (the host already wrote the same values; the firmware logs
both); the DRAM reset is DevInit step 3.5 followed by the step 6.1 flush;
``0x01f001`` is not set (``MR22_A0[4] = 0`` here).

Logs: ``QB_DLL`` (``0x0842``) with the sampled code and restored values,
``QB_DLLPROBE`` (``0x0843``) along the path (:doc:`streaming-ids`).

Second start
------------

The host cannot rewrite the message block after a PMU run. If byte 0 of the
message block is ``0xa5`` (our private flag, set by the SPL at the initial
load), ``quickboot_run()`` clears it and writes ``SequenceCtrl = 0x0008``
before returning, so the next start of the firmware runs RdDQS alone
(:doc:`stages`). ``main()`` accepts a ``SequenceCtrl`` without DevInit when
``Quickboot != 0``.

History
-------

How the working order was found; details in each experiment record and in
:doc:`../hardware/quickboot-sequence`.

* [:exp:`011`, :exp:`012`]: full DevInit, then the saved result written by
  the host: 1 of 3 and 3 of 10 boots clean, whichever of ten reference
  envelopes; identical DevInit outputs in good and bad boots.
* [:exp:`013`]: quickboot order. Upper 8 GiB one beat off until
  ``0x01<r>00d`` (rank-1 write leveling) was restored too; then 3 of 3.
* [:exp:`014`]: U-Boot relocates and runs on that DRAM; 52 MB loaded and
  CRC-checked. Linux not started (no OpenSBI in the USB/FDL chain).
* [:exp:`015`]: further boots fail (5 clean of 7). Host read-eye scan:
  windows about 100 taps, trained values centred, so not a read-delay
  problem. ``0x200cf`` always ``0x1f``.
* [:exp:`016`]: DLL code sampled while ``0x2f0e0 = 1`` gives the reference
  ``0x0c``/``0x0d``; ``0x2f052`` pulses, ZQCAL, MR40/MR37/MR28 added;
  failures remain.
* [:exp:`018`]: two MR passes with the FSP switch and ``cmd 0x28``: 2 of 3.
* [:exp:`019`]: direct clock before the DLL recalibration: **10 of 10**.
