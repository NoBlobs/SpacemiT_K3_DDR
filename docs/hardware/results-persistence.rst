.. SPDX-License-Identifier: CC-BY-SA-4.0

Training results and persistence
================================

Per controller, collected by the host after the PMU run [spec: :dc:`09`,
/10]:

========== ========== =============================================
Offset     Size       Content
========== ========== =============================================
``0x0000`` ``0x0140`` message block
``0x0140`` ``0x2864`` ``phypara``: 5170 halfwords, 69 ordered segments
``0x29a4`` ``0x2000`` ACSM SRAM, 4096 halfwords
========== ========== =============================================

The ``phypara`` order is generated from the appendix by
``tools/gen_phypara.py``; ``tests/test_phypara.c`` checks 5170 entries,
segment 48 = ``0x9002e``–``0x9062d`` and that all indices are unique.

Envelope ``DDRT`` (``0x9800`` bytes, CRC-32 over ``[0x08, 0x9800)``) lives in SPI
``bootinfo + 0x10000`` on the reference board. Our board's SPI is blank, so
every boot is a full training; quickboot with our own envelopes comes last.
