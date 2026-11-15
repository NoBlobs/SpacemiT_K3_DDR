.. SPDX-License-Identifier: GPL-2.0-or-later

Mailbox (PMU side)
==================

``src/hal/mailbox.c``:

Handshake polarity is the measured one (:doc:`../hardware/mailbox`,
:exp:`experiment 000 <000>`), not the one in :dc:`docsClean BR-09 <13>`.

``mbox_major(code)``
   one 16-bit item with the full handshake.
``mbox_word(w)``
   one 32-bit item (low to ``0x90180064``, high to ``0x90180068``).
``mbox_stream(id, payload, len)``
   major ``0x08``, header ``(id << 16) | len``, ``len`` words.

``src/common/log.c`` adds the ``HdtCtrl``/``MsgMisc[4]`` filter
(``log_stream``, ``log_words``). The probe streams unfiltered.

Our major progress codes
------------------------

===================== ===============================================
Code                  Meaning
===================== ===============================================
``0x20``              probe start
``0x21``–``0x25``     probe steps (BCR, RTC, message block, ISA, IMEM)
``0x30``              training firmware start
``0x40+n``            start of the stage of ``SequenceCtrl`` bit ``n``
===================== ===============================================

They do not reproduce the reference major sequence; the host treats any
unknown major as progress.
