.. SPDX-License-Identifier: CC-BY-SA-4.0

Mailbox and streaming
=====================

[spec: :dc:`08`, :dc:`13` BR-09]

======================= ============ ==============
Function                Host index   PMU address
======================= ============ ==============
status / busy (bit 0)   ``0xd0004``  ``0x90180008``
acknowledge / doorbell  ``0xd0031``  ``0x90180066``
code / low half         ``0xd0032``  ``0x90180064``
high half               ``0xd0034``  ``0x90180068``
======================= ============ ==============

Host receive [spec]: wait ``UCT_SHADOW[0] = 0``, read, ACK = write 0 to
``0xd0031``, wait ``UCT_SHADOW[0] = 1``, write 1.

PMU send [:exp:`000`]: at rest the PMU status ``0x90180008[0]`` (host index
``0xc0004``) reads **1** and ``0x90180066`` (``0xc0033``) reads **1**. The send
sequence that matches the host side is:

#. wait status bit 0 = 1 (host ready, ``DCT_WRITE_PROT = 1``);
#. write low (``0x90180064``) and high (``0x90180068``) halves;
#. ``0x90180066 = 0`` (item available, host sees ``UCT_SHADOW[0] = 0``);
#. wait status bit 0 = 0 (host ACK, ``DCT_WRITE_PROT = 0``);
#. ``0x90180066 = 1``.

.. note::

   :dc:`13` BR-09 gives the opposite polarity for both the status wait and
   the ``0x90180066`` writes; a firmware following it waits forever
   (:lr:`000-probe-first-boot-mailbox/readme.md`). The corrected sequence
   above is confirmed for majors and streaming on both PMUs
   (:lr:`001-probe-core-mailbox-rtc/readme.md`).

Host ``UCT_SHADOW`` (``0xd0004``) reads ``3`` at rest; bit 1 is not understood
[open].

Majors: ``0x07`` done, ``0x08`` stream introducer, full-word ``0xff`` fatal (no
ACK). Any other value is progress.

Streaming: after ``0x08`` the host reads ``j`` (= header low half); ``j + 1``
words follow, the first being the header ``(id << 16) | len`` itself.

Terminal pulse: PMU writes ``0x90040238`` ``1 -> 0`` on completion, ``4 -> 0`` on
fatal.
