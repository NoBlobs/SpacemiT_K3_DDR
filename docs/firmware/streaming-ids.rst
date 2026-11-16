.. SPDX-License-Identifier: GPL-2.0-or-later

Streaming IDs
=============

Our IDs live in ``0x0800``–``0x0fff``, outside the ``0x0000``–``0x02ff`` range seen
in reference captures, so logs from the two can never be confused
(``src/common/log.h``).

========== ===================== ==========================================
ID         Name                  Payload
========== ===================== ==========================================
``0x0800`` ``LOG_ID_BOOT``       firmware id (``PROB``/``TRAN``), version, 0
``0x0801`` ``LOG_ID_EXCEPTION``  ECR, ERET, EFA, SP
``0x0802`` ``LOG_ID_ASSERT``     source line, value
``0x0803`` ``LOG_ID_STAGE``      stage bit, status (0 ok, 1 fail, 2 n/i)
``0x0810`` ``PROBE_BCR``         (aux address, value) pairs
``0x0811`` ``PROBE_RTC``         tag, RTC ctrl, count low, count high
``0x0812`` ``PROBE_CSR``         (index, value) pairs
``0x0813`` ``PROBE_DMEM``        ``offset << 16 | halfword``; last frame: byte reads
``0x0814`` ``PROBE_MEM``         IMEM ``ldh`` @0, ``ldh`` @0x1fc, ``ld`` @0
``0x0815`` ``PROBE_ISA``         self-test results (see ``src/probe/main.c``)
``0x0820`` ``CSRDUMP``           phypara count, non-zero, CRC; ACSM count, non-zero, CRC; ticks phypara, ticks ACSM
``0x0821`` ``CSRSCAN``           first index, count, packed halfwords
``0x0822`` ``PLLPROBE``          tag, time µs, packed register values
``0x0830`` ``DEVINIT_CAL``       b03a788 calibration values (exp 006, retired)
``0x0831`` ``DEVINIT_STATE``     b03a788 phase state (exp 006, retired)
``0x0832`` ``DEVINIT_WATCH``     ticks, ``0x300bf``, ``0x100e0``, ``0x100d5`` (exp 006)
``0x0833`` ``DEVINIT_DMEM``      (ARC address, halfword) pairs (exp 006)
``0x0834`` ``DEVINIT_INPUT``     message-block inputs, ``0xc0080``, rank count
``0x0835`` ``DEVINIT_ACSM``      tag, poll iterations, final ``0x20021``
``0x0836`` ``DEVINIT_CAL2``      ``e3``, ``e5``, counters before, ``e3`` high, latched counters, ``0x100d5``, ``f_lo``, ``f_hi``, buckets
``0x0837`` ``DEVINIT_STATE2``    tag, µs, ``0xc0086``, ``0x20011``, ``0x20060``, calibration registers, ``0x100d5``, ``0x100a3``
``0x0838`` ``DEVINIT_LATCH``     tag, wait (exp 007) or ``0x200a2`` (exp 008), ``e3``, ``e5``, counters before and after a test latch
``0x0839`` ``DEVINIT_REGS``      tag, µs, step 3.5 registers (exp 007)
``0x083a`` ``DEVINIT_PHASE0``    canary DMEM/registers before and after, multiplier, ``0x8000040c``
``0x083b`` ``DEVINIT_DESC``      descriptor index, CSR, value, replaced flag
``0x083c`` ``DEVINIT_DQCAL``     rank, k, ``d5`` before, lane 0 ``d0``–``d4``, ``d5`` now
``0x083d`` ``DEVINIT_ACSMTRY``   tag, ``0x20021`` before/start/poll, polls, after stop (exp 008)
``0x083e`` ``DEVINIT_DQCAL2``    retired (lane loop, exp 009)
``0x083f`` ``DEVINIT_PHASE0B``   version code, canary DMEM, ``0xc00ee``, branch, read-back, multiplier, pair
``0x0840`` ``DEVINIT_DQCAL3``    rank, ``d5``, ``d0``–``d4``, ``0x01r0ad``, ``0x01r0af``
``0x0841`` ``DEVINIT_END``       ``0xc0086``, ``0x20060``, ``0x20011``, ``0x2000a``, ``0x20187``
``0x0842`` ``QB_DLL``            quickboot step 6: ``0x20009``, ``0x20049``, ``0x200cf``, ``0x2004e``, ``0x0100ad/af``, ``0x2000a``, ``0x20060``
========== ===================== ==========================================
