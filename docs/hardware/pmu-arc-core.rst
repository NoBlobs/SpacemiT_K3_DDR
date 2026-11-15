.. SPDX-License-Identifier: CC-BY-SA-4.0

PMU processor (ARCv2 EM)
========================

[spec: :dc:`12`, :dc:`13`] unless marked otherwise.

* 32-bit little-endian ARCv2 EM, no MMU/FPU, 16/32-bit encodings,
  zero-overhead loops, ``dbnz``.
* **Reduced register file (RF16)**: ``r16``–``r25`` do not exist. Every compile
  and the link use ``-mrf16``.
* Hardware ``mpy``/``mpyu``/``mpymu`` and ``div``/``divu``/``rem``/``remu``;
  libgcc software helpers reportedly hang, so ``-mmpy-option=wlh5 -mdiv-rem``
  plus global overrides.
* 64 KiB IMEM; effective startup at ``0x1fc``; stack top ``0x80010000``.
* RTC: AUX ``0x103`` (control), ``0x104``/``0x105`` (count low/high), via
  ``lr``/``sr`` only. At reset ``RTC_CTRL = 0xc0000000`` (disabled); writing 2
  then 1 gives ``0xc0000001``. **Rate ≈ 400 ticks/µs (400 MHz)** [:exp:`001`].
* Exceptions: ``ERET`` AUX ``0x400``, ``ECR`` AUX ``0x403``.

Reset vector
------------

ARCv2 cores fetch the reset PC from entry 0 of the interrupt vector table,
which holds handler *addresses* (public ARCv2 ISA). The spec describes "a
reset branch at ``0x0`` targeting ``0x1fc``". Our image stores a 127-entry
vector table (``0x000``–``0x1fb``): entry 0 = ``0x1fc``, entries 1–15 = exception
handler, 16–126 = unexpected-interrupt handler [design]. **Confirmed**
[:exp:`000`]: the PMU starts at ``0x1fc`` with this layout on both controllers.

Build configuration registers
-----------------------------

Read by the probe on both PMUs [:exp:`001`]; decoding follows the public ARCv2
PRM and is an interpretation:

========= ================= ============== =======================================
AUX       Register          Value          Meaning
========= ================= ============== =======================================
``0x004`` IDENTITY          ``0x43``       ARCv2 EM family
``0x06e`` RF_BUILD          ``0x302``      16-entry register file (RF16)
``0x074`` DCCM_BUILD        ``0x904``      size code 9 (64 KiB DMEM)
``0x075`` TIMER_BUILD       ``0x404``      RTC only, no TIMER0/1
``0x078`` ICCM_BUILD        ``0x904``      size code 9 (64 KiB IMEM)
``0x07b`` MULTIPLY_BUILD    ``0x20d06``    32×32 multiplier
``0x07f`` BARREL_BUILD      ``0x303``      full barrel shifter
``0x0c1`` ISA_CONFIG        ``0x12043002`` div/rem, code density, no LL64/atomics
``0x0f3`` IRQ_BUILD         ``0x40401``    4 interrupts
========= ================= ============== =======================================

NORM and SWAP build registers read 0. Unexplained non-zero BCRs: ``0x060``
(2), ``0x068`` (``0x10``), ``0x07e`` (2), ``0x0c7`` (``0x1203``), ``0x0f1``
(``0x101``). Full list: :lr:`001-probe-core-mailbox-rtc/readme.md`.

Self tests [:exp:`001`]: 64-bit multiply, signed/unsigned div and rem, ``dbnz.d``
delay slot executed on every iteration, ``add3`` scaling ×8 — all as
:dc:`12` states.

IMEM as data
------------

The PMU can read IMEM with ``ldh`` and ``ld`` without exception [:exp:`001`].
