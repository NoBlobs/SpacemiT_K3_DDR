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
``0x01``          ``0x90020000`` per-rank / per-lane DQ timing, VREF, ODT, eye centres
``0x02``          ``0x90040000`` PHY engine control, DevInit clock period, MemReset
``0x03``          ``0x90060000`` CA training (LPCA), CA TX drive
``0x04``          ``0x90082000`` ACSM SRAM (``0x41000``)
``0x05``          ``0x900a0000`` IMEM/DMEM host windows
``0x07``          ``0x900e0000`` max read latency
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

======================= ======================= ======================================================================================
Index                   Value at PMU start      Notes
======================= ======================= ======================================================================================
``0x20011``             ``0x006f`` (host table) calibration control; :dc:`07` says restore ``0x0041`` [:exp:`003`]
``0x200e3``             ``0x0000``              read/write; period-measurement trigger per :dc:`07` [:exp:`003`]
``0x300bf``             ``0x0000``              CA period counter; stays 0 after triggering [:exp:`003`]
``0x100e0``             ``0x0000``              DQ period counter; stays 0 after triggering [:exp:`003`]
``0x200c9``             ``0x0000`` (after run)  "PLL lock status" (:dc:`08`)
``0xc0086``             ``0x0001`` (host table) sticky status; 3 after DevInit per :dc:`14`
``0x20060``             ``0x0002`` (host table) changed by DevInit per :dc:`09`
``0xc00f1``–``0xc00f7`` host table              seven values; candidate PHY PLL settings (matches the seven ``QBPll*`` outputs) [open]
======================= ======================= ======================================================================================
