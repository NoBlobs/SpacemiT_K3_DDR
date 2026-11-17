.. SPDX-License-Identifier: CC-BY-SA-4.0

PHY CSR map
===========

Index decomposition [spec: :dc:`02`]:

.. code-block:: text

   group  = i >> 16          rank = (i >> 12) & 0xf
   lane   = (i >> 8) & 0xf   offset = i & 0xff

================= ============== ======================================================
Group             ARC base       Role [spec]
================= ============== ======================================================
``0x01``          ``0x90020000`` per-dbyte / per-bit DQ timing, VREF, ODT, eye centres
``0x02``          ``0x90040000`` PHY engine control, DevInit clock period, MemReset
``0x03``          ``0x90060000`` CA training (LPCA), CA TX drive
``0x04``          ``0x90082000`` ACSM SRAM (``0x41000``)
``0x05``          ``0x900a0000`` IMEM/DMEM host windows
``0x07``          ``0x900e0000`` PHY pattern generator (training), max read latency
``0x09``          ``0x90120000`` PHY / P-state configuration
``0x0a``          ``0x90140000`` P-state SRAM
``0x0c``/``0x0d`` ``0x90180000`` mailbox, clocks (``0xc0080``), mux (``0xd0000``), reset (``0xd0099``)
``0x11``          ``0x90220000`` live DQ mirror
================= ============== ======================================================

Discovered by experiment
------------------------

PMU-side reads [:exp:`002`]: every register of the ``phypara`` set (5170, all
groups below) and the ACSM SRAM read the same from the PMU
(``0x90000000 | index << 1``, ACSM at ``0x90082000 + 2*j``) as from the host,
on both controllers, without stalling. A PMU CSR read takes about 0.5 µs.

State seen by the PMU when it starts (after the SPL pre-training tables),
identical on PMU0 and PMU1:

============ ======= ========== ==============================================
Group        Read    Non-zero   Notes
============ ======= ========== ==============================================
``0x01``     1468    1116       ``0x100a3 = 0x0b33`` already set by the host tables
``0x02``     338     69
``0x03``     152     78
``0x07``     101     0
``0x09``     1693    8
``0x0c``     57      13
``0x11``     1180    1024       non-zero although "never written by host tables"
``0x12``     52      17
``0x13``     88      32
``0x19``     41      0
============ ======= ========== ==============================================

Full values: :lr:`002-csr-readout-pmu-vs-host/baseline/phypara-pmu0.csv`
(``k,index,value``), the reference for later diffs against trained states.
Compared with the trained sample quoted in :dc:`09`, only ``0x100d9`` differs
among the first twelve values (128 before training, 156 trained).

Write behaviour (R/W masks, side effects) is still [open].

Range scans
-----------

[:exp:`004`] read every index of ``0x20000``–``0x203ff``, ``0x40000``–``0x401ff``
and ``0xc0000``–``0xc00ff`` from the PMU at start: no read stalls; every
register the host pre-training table writes reads back its table value; the
ACSM control range ``0x40000``–``0x401ff`` is all zero; 46 non-zero registers
are not written by any host table (reset/status values, listed in
:lr:`004-csr-range-scan/readme.md`). ``0x20047`` differs between the two
controllers (``0x00bd``/``0x00ff``) and looks like a live status.

Individual registers
--------------------

Detailed pages: :doc:`phy-clock-dll` (clock mode, DLL, strobes), :doc:`acsm`
(sequencer), :doc:`dq-data-path` (group ``0x01``).

.. list-table::
   :header-rows: 1
   :widths: 22 18 60

   * - Index
     - Value
     - Notes
   * - ``0x20011``
     - ``0x006f`` (host table)
     - saved and masked ``& 0xff9f`` in DevInit step 3.3, restored in step 6.3 [:exp:`010`]
   * - ``0x200e3``
     - ``0x0000``
     - bit 10 falling edge latches the period counters [:exp:`007`]
   * - ``0x300bf``, ``0x100e0``
     - ``0x0240`` unlatched
     - CA / DQ period counters; ``0x84``–``0x87`` with the PLL running [:exp:`008`]
   * - ``0x200c9``
     - ``0x0000``
     - never polled by the PMU [spec]
   * - ``0xc0086``
     - ``0x0001`` (host table)
     - host-side ECC control; the full-training PMU never touches it [spec, :exp:`010`]
   * - ``0x20060``
     - ``0x0002`` (host table)
     - ``MemResetL``; ``3`` after DevInit with ``Misc = 0`` [:exp:`010`]
   * - ``0x01<r>00f``
     - ``0x1086`` (host table)
     - ``0x1006`` after step 3.5, ``0x1004`` after step 3.6; the host writes ``0x1089`` after training [:exp:`010`]
   * - ``0x01<r>0ad``
     - ``2``
     - step 3.6 selection on all dbytes [:exp:`010`]
   * - ``0x01<r>0af``
     - ``0x3a``–``0x3f``
     - copy of ``0x01<r>0d6`` in step 3.6 [:exp:`010`]
   * - ``0x200cf``
     - ``0x001f`` idle
     - DLL code, live only while ``0x2f0e0 = 1``: ``0x0c`` / ``0x0d`` [:exp:`016`]
   * - ``0x2004e``
     - ``0x0c`` / ``0x0d``
     - stored DLL code (PHY0 / PHY1) [:exp:`019`]
   * - ``0x20314``, ``0x20315``
     - ``0x36``, ``0x7a``
     - read-only calibration outputs, source of the ACSM dividers [:exp:`012`]
   * - ``0x030084``, ``0x01000a``
     - ``1`` (final state)
     - ACSM routing; ``0`` while the PMU sends commands [:exp:`019`]
   * - ``0x2007e``
     - ``0x33`` (final state)
     - ``(cs_b << 2) | cs_a`` per channel; ``0`` during RdDQS [spec]
   * - ``0x000a4``
     - ``0`` (final state)
     - ``1`` written at the end of DevInit [:exp:`017`]
   * - ``0xd00e7``
     - ``0x0080`` before
     - ``0x0600`` written by the host at the end of quickboot [:exp:`016`]
   * - ``0xc00f1``–``0xc00f7``
     - host table
     - seven values; candidate PHY PLL settings (matches the seven ``QBPll*`` outputs) [open]

PHY version: ``0xc00ee & 0xfff = 0x240`` decodes to version code 9; the
signature ``0x6745`` / ``0x00f2`` is written to and read back from
``0x9002e`` / ``0x9002f`` [:exp:`008`].
