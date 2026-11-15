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
========== ===================== ==========================================
