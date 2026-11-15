.. SPDX-License-Identifier: GPL-2.0-or-later

DevInit (``SequenceCtrl[0]``)
=============================

Status: **implemented per docsClean/07 (19ab858); DevInit completes with
status OK on both PHYs: PLL running, period ``0x84``–``0x86``, MRW burst
executed by the ACSM** [:exp:`009`]. Open: the ZQ calibration ``MPC`` burst
(J2) and step 3.6 lanes 1–8 (J3). The training firmware still ends with
``0xff`` because the later stages are stubs.

Sequence (docsClean 19ab858)
----------------------------

=========  ==============================================================  =====================
Step       Content                                                         State
=========  ==============================================================  =====================
0          ``0x20090 = 1``, ``byte[1] & 1`` branch, version decode        done
           (``0xc00ee``), canary ``0x9002e/f``, delay multiplier
1          ``0x200a2[1:0] = 1`` (reference clock), ``0x200f8``,            done
           ``0x20060 = 2``
2          delay shift from ``0xc0080``; commit strobe triad               done
3.1–3.3    ``QBPll*``, ``0x800004cc`` descriptors, ``0x2008b``,            done
           ``0x20072/73``, ``0x20011``, ``0x01r0a3``
3.4        DQ block reset on ``0x20303``–``0x20320``                       done
3.5        DRAM reset / PLL sequencing, 400 µs lock wait                   done, PLL runs
3.6        per-rank DQ calibration, result in ``0x01r0ad``                lanes 1–8 fall back (J3)
3.7        period latch, frequency buckets, ``0x01r0d9 = 0x9c``            done, ``0x84``–``0x86``
6.1        ACSM dividers, one-shot NOP flush, block disable                done
6.2        tINIT3, ``0x20060 = 3``, tINIT4/5, routing, MRW burst           done; ZQ ``MPC`` (J2) missing
6.3        masks, ``0x20042``, ``0x20044``, channel commit,               done
           ``0x01r0a3`` restore, ``0x000a4 = 1``
=========  ==============================================================  =====================

The pure computations (frequency buckets, two-tier delays, ACSM MRW
encoding) are in ``src/train/devinit_calc.h`` with unit tests
(``tests/test_devinit_calc.c``) checked against the tables of :dc:`07`.
``delay_dfi_clks(n)`` waits ``(n + 1) >> shift`` RTC ticks (one dummy read of
``0x20024`` below 16); ``delay_ps_or_clks(ps, min)`` converts picoseconds, or
``min × 32`` DFI clocks at 6400/4, and calls it.

Period calibration
------------------

The counters ``0x300bf``/``0x100e0`` latch on the **falling edge of
``0x200e3`` bit 10**: ``0x200e5 &= 0xffe0``; ``0x200e3 = old | 0x800``;
``old | 0xc00``; ``delay_dfi_clks(10)``; ``old | 0x800``; read; restore. The frequency
buckets depend only on ``DRAMFreq``/``DfiFreqRatio`` (6400 MT/s, ratio 4 →
``0x20012 = 0x7353``, ``0x20186 = 0x0073``); ``0x20187`` is copied from
``0x100d5``.

Hardware history:

* [:exp:`003`]: writing ``0x200e3 = 1`` leaves both counters at 0.
* [:exp:`005`]: the public DDR4-PHY PLL registers do not exist on the K3.
* [:exp:`006`]: the b03a788 enable (``|= 0x0c00`` without the falling edge)
  leaves the unlatched seed ``0x240``.
* [:exp:`007`]: with the falling edge the counters latch. Before step 3.5 the
  latch returns ``0x240``; after step 3.5 it returns ``0x17ff`` (saturated),
  independent of a 0–10 µs window. Reference: ``0x82``–``0x86``. The
  frequency buckets match the reference. ``0x100d5`` stays at its per-boot
  value (≈ ``0x19a``–``0x1bf``), not ``0x41``. Questions G7, G10.
  Run 3 places the change at the step 3.5 writes ``0x20078 = 2``,
  ``0x200a2 = 2`` (waits ×10 or ×100 do not help). Run 4: ``0x20078[0]`` holds
  the counters at ``0x240``; ``0x200a2[1]`` saturates them; ``0x200a2 = 1``
  gives ``0x250`` (80). Hypothesis: ``0x200a2`` selects the measured clock, and
  the source selected by bit 1 is not running here.
* [:exp:`008`]: with phase 0 (``0x20090 = 1``, signature canary), the
  ``0x800004cc`` descriptors (``0x2006e = 1``, ``0x70060 = 0``,
  ``0x70011 = 0``) and the documented delays (400 µs PLL lock wait) the latch
  gives ``0x86``/``0x84`` on the two PHYs and ``0x100d5`` becomes ``0x43``
  during step 3.6. ``T_dq`` reads ``0x42``–``0x43`` (reference: unchanged
  ``0x40``).

ACSM
----

Control registers ``0x20020``–``0x20027`` and the 4-halfword entry format are
in :dc:`07`. [:exp:`007`]: the busy flag ``0x20021`` reads ``1`` before the
first start and stays ``1``. [:exp:`008`]: still ``1`` after the documented
clock dividers (``0x90808``/``0x90813``) and ``0x20031 = 1`` (question H9);
the 23-command MRW burst built from the message block is queued in ACSM SRAM
but not started. The reference firmware never issues MRR (:dc:`07`).
[:exp:`009`]: ``0x20021`` is ``ACSM_DONE`` (1 = done); DevInit runs the ACSM in
one-shot mode with ``0x20031 = 0``. The flush reads done at once; the
23-command MRW burst reads ``0`` right after ``0x20020 = 1`` and done five
polls later.
Oracle for the DRAM initialisation: MRR of MR5–MR8 per rank (Micron ID,
density and width consistent with ``MT62F2G32D4DS``) and ``0xc0086 = 3``
after DevInit (:dc:`14`).
