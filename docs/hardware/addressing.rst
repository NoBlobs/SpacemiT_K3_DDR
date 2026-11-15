.. SPDX-License-Identifier: CC-BY-SA-4.0

Addressing and access rules
===========================

Three address views that must never be mixed [spec: :dc:`02`]:

1. **DDRC/CFG byte offsets** — ``DDRC_BASE + off``; host 32-bit accesses.
2. **Host DPHY word index** ``i`` — ``DPHY_BASE + 4*i``; each 32-bit host word
   carries one 16-bit PHY CSR in its low half.
3. **ARC addresses** inside the PMU — ``0x0000xxxx`` IMEM, ``0x8000xxxx`` DMEM,
   ``0x90xxxxxx`` PHY CSRs / mailbox.

Conversions
-----------

.. code-block:: text

   ARC CSR address  = 0x90000000 | (i << 1)
   ARC DMEM address = 0x80000000 + 2 * (i - 0x58000)     (0x58000 <= i < 0x60000)
   CSR index        = base | (rank << 12) | (lane << 8) | offset   (OR, never +)

Firmware helpers: ``PHY_CSR_ARC()``, ``PHY_IDX()``, ``DMEM_ARC()`` in
``src/hal/mmio.h``.

Windows
-------

=========== ============== ===================================================
Host index  ARC            Use
=========== ============== ===================================================
``0x41000`` via CSRs       ACSM SRAM, 4096 halfwords
``0x50000`` ``0x00000000`` PMU IMEM, 64 KiB
``0x58000`` ``0x80000000`` message block (``0x140`` bytes used, ``0x400`` reserved)
``0x58200`` ``0x80000400`` host parameter area (full training: up to ``0x584d1``)
``0x584d2`` ``0x800009a4`` cleared by the host up to ``0x5ffff`` before release
``0x5a000`` ``0x80004000`` **our** DMEM image (.rodata/.data), loaded by our SPL patch [design]
``0x5f000`` ``0x8000e000`` transient host scratch (256 bytes) — unused by us
``0x60000`` ``0x80010000`` DMEM end, initial stack top
=========== ============== ===================================================

Access widths
-------------

On the PMU side, DMEM shared with the host and every ``0x90xxxxxx`` register
are 16-bit: only ``volatile`` 16-bit (or 8-bit for byte fields) accesses
[spec: :dc:`12`, BR-03]. Our own stack and ``.bss`` in DMEM are accessed
with ordinary 32-bit loads/stores by the compiler; whether that is safe in
every DMEM range is checked by experiment [open].

.. warning::

   Never access DPHY/PMU MMIO from Linux (``/dev/mem``): reported to stall the
   SoC until a power cycle [spec: :dc:`14`]. Re-verification is scheduled
   for the later validation phase.
