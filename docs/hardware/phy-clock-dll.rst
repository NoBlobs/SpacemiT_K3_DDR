.. SPDX-License-Identifier: CC-BY-SA-4.0

PHY clock modes and DLL recalibration
=====================================

Registers of group ``0x02`` that select the PHY clock and recalibrate the
DLL, with what we measured on the board.

Clock selection
---------------

.. list-table::
   :header-rows: 1
   :widths: 15 85

   * - Register
     - Meaning
   * - ``0x200a2``
     - bits ``[1:0]``: ``1`` = direct (reference / bypass) clock, ``2`` = PHY PLL clock, ``0`` = gated [spec: :dc:`07`; :exp:`007`: with ``1`` the period counters latch ``0x250``, with ``2`` and a running PLL ``0x84``–``0x86``]
   * - ``0x200f8``
     - ``1`` together with the direct clock, ``0`` with the PLL clock
   * - ``0x20078``
     - PLL reset: ``1`` assert, ``2`` release, ``0`` normal

Neither ``0x200a2`` nor ``0x200f8`` is part of ``phypara``: a restored result
does not set the clock mode [spec: :dc:`07`].

Two fixed sequences are used [spec: :dc:`07`, :dc:`11`]:

.. code-block:: text

   to direct clock:  0x200a2 = (v & 0xfffc) | 1; delay_dfi_clks(4);
                     0x200f8 = 1;               delay_dfi_clks(0x1f)
   to PLL clock:     delay_dfi_clks(0x1f); 0x200f8 = 0;
                     delay_dfi_clks(4);    0x200a2 = (v & 0xfffc) | 2

* DevInit starts on the direct clock, locks the PLL in step 3.5 (400 µs wait)
  and switches to the PLL clock (:doc:`../firmware/devinit`).
* **Quickboot switches to the PLL clock before the MR bursts and back to the
  direct clock before the DLL recalibration, and ends in direct-clock mode.**
  Without the switch back, the memory test fails in about one boot of three
  with whole byte lanes or rank 1 one beat off; with it, 10 boots of 10 are
  clean [:exp:`018`, :exp:`019`].
* A training burst needs the PLL clock mode [spec: :dc:`07`].
* Switching PLL → direct → PLL *after* the DLL recalibration and then
  restoring the direct clock leaves the memory failing (about 30 % of the
  words) [:exp:`019`, open: which step is not reversible].

Period counters and frequency buckets (``0x2000a``, ``0x20017``, ``0x20012``,
``0x20186``, ``0x20187``) are described in :doc:`../firmware/devinit`.
Quickboot does not measure the period: it restores ``0x2000a/12/17`` from
the saved result [spec: :dc:`11`].

ACSM clock multiplier
---------------------

``0x20042`` and ``0x20044`` are set by DevInit step 6.3 (``0x20042 = mult |
0x100``, ``0x20044 = 2`` at ratio 4) and are ``0`` in a final result. With
``0x20042 = 0x0108`` the same 20-entry ACSM burst takes 12 times longer to
report done (48 polls instead of 4) [:exp:`016`]. They belong to write
leveling, not to RdDQS [spec: :dc:`07`].

ACSM dividers ``0x90808`` / ``0x90813`` are ``(0x20314 << 8) | 1`` and
``(0x20315 << 8) | 1``; ``0x20314/15`` are read-only calibration outputs that
change by a few counts from boot to boot (``0x36``–``0x37``, ``0x79``–``0x7d``)
[:exp:`012`]. A restored result must recompute them after the DLL
recalibration [spec: :dc:`11`].

DLL recalibration (quickboot step 6)
------------------------------------

Sequence [spec: :dc:`11`; confirmed :exp:`016`, :exp:`019`]:

.. code-block:: text

   delay_ps_or_clks(0, 0x40)            (multiplier off: 65 ticks)
   0x2f310 = 1;  0x2f0f8 = 1
   u = 0x20049 & 0x01f0
   0x2f049 = u | 0x0400;  wait 1 ns
   0x2f0e0 = 1;           wait 1 us
   code = 0x200cf                        <- only valid here
   0x2f0c3 = 1;  0x2f0e0 = 0;  0x2f049 = u | 0x0200
   0x2004e + pstate = code & 0x1f
   0x9f02d = 1   (PHY version 8..10; a level, never cleared)
   0x2f009 + pstate = value of 0x20009 saved at the start

Measured:

* ``0x200cf`` is live **only while ``0x2f0e0 = 1``**. Read at any other time
  (before the DQ reset, after the PLL sequence, after the recalibration, up
  to 120 ms later, or from the host) it returns ``0x1f`` [:exp:`015`].
* Sampled at the right moment it gives ``0x0c`` on PHY0 and ``0x0d`` on PHY1,
  the reference values, in every boot (10 of 10) [:exp:`016`, :exp:`019`].
* ``0x20049`` reads ``0x3f0`` before the PLL sequence, ``0x1f0`` after DevInit
  step 3.5 writes ``0x01f0``, and ``0x3f0`` again after the recalibration;
  ``0x200c3`` reads ``1``, ``0``, ``1`` at the same points [:exp:`015`].
* ``0x2004e`` is in ``phypara``. A host restore must not overwrite the value
  sampled in the current boot (same value in our runs, so no effect was
  seen) [:exp:`016`].
* Nothing is polled after ``0x2f0c3 = 1`` [spec].

Commit strobes
--------------

.. list-table::
   :header-rows: 1
   :widths: 40 60

   * - Strobe
     - Use
   * - ``0x20028 = 1``, ``0x2f0a5 = 1``, ``0x2f077 = 0``
     - configuration commit after a logical block of writes; strict order, no wait between them [spec: :dc:`07`]
   * - ``0x2f0a0 = 1``, wait, ``0``
     - channel commit (after routing or channel selection changes)
   * - ``0x2f052 = 1``, ``delay_dfi_clks(10)``, ``0``
     - only when ``MsgMisc[0] = 1``; five sites in quickboot [spec: :dc:`11`]

A host write of a per-bit read delay needs none of them [:exp:`015`].
