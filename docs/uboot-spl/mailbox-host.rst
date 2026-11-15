.. SPDX-License-Identifier: GPL-2.0-or-later

Host mailbox receiver
=====================

``major_message_all()`` in ``lpddr_common.c`` was rewritten for this project:

* every wait on ``UCT_SHADOW`` is bounded by ``K3_DDR_PMU_MBOX_TIMEOUT_MS``;
* typed result: ``PMU_MBOX_OK`` (``0x07``), ``PMU_MBOX_FATAL`` (full-word
  ``0xff``: not ACKed, PMU stopped, ``plllockstatus`` printed),
  ``PMU_MBOX_TIMEOUT`` (PMU stopped, mux back to host);
* with ``K3_DDR_PMU_MBOX_LOG`` each item is printed:

.. code-block:: text

   PMU0 M 20 @123456                      major 0x20 at t = 123456 us
   PMU0 S 0810 4: 00000004 ... @123500    stream id 0x0810, 4 payload words

``tools/mbox_decode.py`` parses these lines from a serial log.

The quickboot and LPDDR4X callers are unchanged and still ignore the result.
