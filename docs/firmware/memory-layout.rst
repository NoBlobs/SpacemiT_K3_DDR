.. SPDX-License-Identifier: GPL-2.0-or-later

Memory layout
=============

Linker script: ``src/arch/pmu.ld``.

IMEM (``0x00000000``–``0x0000ffff``)
   ``0x000``–``0x1fb`` vector table (127 entries), ``0x1fc`` ``_start``, then
   ``.text``.

DMEM (``0x80000000``–``0x8000ffff``)

=============================== ==============================================
Range                           Use
=============================== ==============================================
``0x80000000``–``0x8000013f``   message block (host in/out) — 8/16-bit only
``0x80000140``–``0x800003ff``   reserved message area — untouched
``0x80000400``–``0x800009a3``   host parameter area — never cleared, not read
``0x800009a4``–``0x80003eff``   cleared by host, currently unused
``0x80003f00``–``0x80003f3f``   bring-up breadcrumbs (``src/hal/bcrumb.h``), dumped by the SPL on timeout
``0x80004000``–…                ``.rodata`` + ``.data`` (DMEM image), ``.bss``
…–``0x8000dfff``                limit for ``.bss`` (linker assert)
``0x8000e000``–``0x8000e0ff``   host scratch — never used
``0x8000e100``–``0x8000ffff``   stack (top ``0x80010000``)
=============================== ==============================================

Why ``.rodata``/``.data`` are in DMEM [design]: the host only loads IMEM in
the vendor flow and data reads from IMEM by ``ld`` are not documented. Our SPL
patch loads ``<fw>.dmem.bin`` at index ``0x5a000`` after the host DMEM clear,
so no copy loop is needed. Experiment 001 showed that IMEM is data-readable,
so constants could move to IMEM later; the DMEM image is kept for now.
