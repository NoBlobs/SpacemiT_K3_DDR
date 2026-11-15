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

Empty. Each family scan (``logsRefs/NNN-csr-scan-*``) adds a table here with
reset value, R/W mask and observed side effects.
