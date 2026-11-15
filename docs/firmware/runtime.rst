.. SPDX-License-Identifier: GPL-2.0-or-later

Runtime
=======

Startup (``src/arch/start.S``)
   ``sp = 0x80010000``, ``fp = 0``, zero ``.bss``, call ``main()``. ``main`` must
   end in ``fw_done()`` or ``fw_fatal()``.

Exceptions
   vectors 1–15 → ``__exc_entry``: fresh stack, read ``ECR``/``ERET``/``EFA``,
   call ``fw_exception()`` (``src/common/exception.c``), which stores them in
   ``fw_diag`` (``.bss``, magic ``"DIAG"``), streams ``LOG_ID_EXCEPTION`` and
   ends with ``0xff``. Interrupts 16–126 do the same with bit 31 of the cause
   set.

MMIO (``src/hal/mmio.h``)
   ``rd16``/``wr16``/``rd8``/``wr8`` on ``volatile`` pointers; ``csr_rd(idx)`` /
   ``csr_wr(idx, v)`` use ``0x90000000 | idx << 1``.

Timer (``src/hal/timer.c``)
   ``timer_init()`` enables the RTC if needed (write 2 then 1 to AUX
   ``0x103``); ``timer_now()`` re-reads the high word until it is stable;
   ``timer_delay_ticks(n)`` uses ``n`` dummy reads of ``0x90040048`` below 16
   ticks, else polls the 64-bit counter. ``timer_delay_us()`` uses the measured
   ``RTC_TICKS_PER_US = 400`` (:exp:`experiment 001 <001>`).

Terminal states
   ``fw_done()``: pulse ``0x90040238`` 1→0, major ``0x07``, halt (``flag 1``).
   ``fw_fatal()``: pulse 4→0, publish ``0xff`` without waiting for ACK, halt.
   The pulse is sent before the code because the host may stop the PMU as
   soon as it sees the code [design].
