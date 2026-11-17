.. SPDX-License-Identifier: CC-BY-SA-4.0

DQ data path: group ``0x01`` registers, address map, eye
========================================================

What the experiments established about the per-byte / per-bit registers of
group ``0x01``, about how memory addresses map to controllers and ranks, and
about the size of the read eye. Register names are ours; the PHY vendor names
are not known.

Index fields of group ``0x01``
------------------------------

.. code-block:: text

   0x01<r><l><off>     r = (i >> 12) & 0xf   l = (i >> 8) & 0xf   off = i & 0xff

* ``r`` is the **data byte** (dbyte 0..3 of the 32-bit bus of one controller),
  not a memory rank. :dc:`07` calls it "rank" in several places. ``r = 0xf``
  is the write-only broadcast to all four dbytes.
* ``l`` is the **bit** inside the byte (0..7) and 8 is the ninth lane
  (DMI / strobe). ``l = 0xf`` is the write-only lane broadcast.
* For the per-bit read delays, CSR ``0x01<r><l>4e`` and ``4f`` move exactly
  data bit ``8r + l`` of the 32-bit word, in 124 of 124 scans; lane 8 has no
  effect on data [:exp:`015`].

Even/odd register pairs
-----------------------

Many offsets come in pairs. Two different meanings were found:

=================== ============================== ===========================
Pair                Meaning                        Evidence
=================== ============================== ===========================
``0x0c`` / ``0x0d`` write leveling, memory rank 0  [:exp:`013`]: with ``0x0d`` at
                    / rank 1                       reset (``0``) only the upper
                                                   8 GiB (rank 1) is shifted by
                                                   one 8-byte beat
``0x4e`` / ``0x4f`` per-bit read delay, **two      [:exp:`015`]: both act on
                    capture phases** (not ranks)   every address; ``4e`` windows
                                                   sit about 10 taps higher
``0x50`` / ``0x51`` pre-training seeds of the read [spec: :dc:`07`]; ``0x35`` in
                    delay, not used on LPDDR5      the reference result
``0x10``–``0x13``   write DQ delays: ``10/12``     [spec: :dc:`07`, :dc:`11`];
                    rank 0, ``11/13`` rank 1       trained ``0x13b``–``0x14d``
``0x24`` / ``0x25`` receive-enable (gate) delay,   [spec: :dc:`07`]
                    rank 0 / rank 1
``0x26``–``0x29``   write DQ delays by rank        [spec: :dc:`07`]
``0x2a`` / ``0x2b`` write-leveling sweep register, [spec: :dc:`07`]
                    rank 0 / rank 1
=================== ============================== ===========================

Reset values that matter [:exp:`013`, :exp:`019`]: ``0x01<r>00c`` and
``0x01<r>00d`` reset to ``0x0000`` (trained ``0x97``–``0x9c``; the pre-training
tables do not write them); ``0x01<r>019``, ``01b``, ``022``, ``023`` reset to
``0x001f``, the value also found after training; ``0x01<r><l>80`` reads ``0``.

Read eye measured from the host
-------------------------------

[:exp:`015`]: on working DRAM the host stepped each ``0x01<r><l>4e`` and
``4f`` over 0..126 (step 2) with a read-only compare of pre-written data.

* A host write to these CSRs takes effect immediately, with the DDRC active
  and without a commit strobe (``MICRO_CONT_MUX_SEL = 0`` around the access).
* Windows are 82–122 taps wide, median about 100, which agrees with the 2D
  contours of the reference (:dc:`07`: 70–124, median 102).
* The trained values (43–76) are within a few taps of the window centre; the
  smallest distance to an edge is 24 taps.

  ========= ======================= =================== ==========================
  PHY, CSR  Closed windows of 32    Width min/med/max   Trained − centre
  ========= ======================= =================== ==========================
  0, ``4e`` 22 (10 open at 126)     88 / 99 / 108       −13 / −4 / +1
  0, ``4f`` 27 (2 open at 0)        82 / 100 / 114      −3 / +2 / +8
  1, ``4e`` 26 (4 open at 126)      88 / 99 / 122       −12 / −3 / +3
  1, ``4f`` 26 (6 open at 0)        88 / 98 / 108       −9 / 0 / +9
  ========= ======================= =================== ==========================

* Four scans showed no error at any value (``0x01014f``, ``0x01024f``,
  ``0x01034f`` on PHY0, ``0x01034e`` on PHY1) [open].

``tools/eyescan_stats.py`` produces this summary from a serial log.

Compare-result block ``0xb0``–``0xbf``
--------------------------------------

Per dbyte and lane [spec: :dc:`07`; values: :exp:`016`, :exp:`017`]:

.. list-table::
   :header-rows: 1
   :widths: 20 80

   * - Offset
     - Role / observed value
   * - ``0xb0``
     - burst generator mode; ``0x34`` during a sweep at ratio 4 (``0x0004`` in the final state)
   * - ``0xb1``
     - ``1`` during a sweep (``0x0180`` in the final state)
   * - ``0xb3``, ``0xb4``
     - ``1`` during a sweep; cleared before the quickboot DLL recalibration
   * - ``0xb5``
     - VREF: ``(vref << 10) | 0x4001`` written through the dbyte broadcast during a 2D sweep; ``0x0001`` in the final state
   * - ``0xb7``
     - 16-bit compare result, ``0`` = PASS [spec]. **Never non-zero in our runs** [:exp:`016`–:exp:`019`, open]
   * - ``0xb9``, ``0xba``
     - read ``1`` on lane 0
   * - ``0xbb``
     - sticky clear: write ``0x3ff`` then ``0`` through ``0x01f0bb``
   * - ``0xbe``
     - ``1`` on lane 0, ``0`` on the others in the final state

Address map of the 16 GiB
-------------------------

Board: two controllers, each with a 32-bit LPDDR5 bus and a
``MT62F2G32D4DS`` package (two ranks); DRAM starts at ``0x1_0200_0000``
(``CONFIG_SYS_SDRAM_BASE``) [spec, :exp:`011`].

* **Ranks split the address space in halves**: the lower 8 GiB is rank 0 of
  both controllers and the upper 8 GiB is rank 1 [:exp:`013`: a rank-1-only
  register error fails exactly blocks 32–63 of 64].
* **The two controllers are interleaved inside each region**, finer than
  4 KiB: a read-delay error on one bit of either PHY produces errors in
  every 256 MiB block [:exp:`015`].
* A 64-byte block is one burst: an error of one beat shows as data from
  8 bytes earlier with the first two words of each 64-byte block wrong
  [:exp:`013`].

Failure signatures
------------------

Seen with the sampled memory test of the SPL (:doc:`../uboot-spl/quickboot-host`):

.. list-table::
   :header-rows: 1
   :widths: 55 45

   * - Signature
     - Cause found
   * - upper 8 GiB only, every word = data written 8 bytes earlier, stable on re-read
     - ``0x01<r>00d`` (rank-1 write leveling) not restored [:exp:`013`]
   * - one or two byte lanes (bits 8–15, 16–23, 16–31) reading ``0xff``, ``0x00`` or the byte of another burst; different lane per boot
     - saved result restored after a full DevInit, or quickboot without the direct-clock switch before the DLL recalibration [:exp:`012`, :exp:`015`–:exp:`018`]
   * - about 30 % of all words wrong, mostly unstable on re-read
     - PHY left in a different state by the RdDQS probe (clock switches, ACSM routing) [:exp:`019`]
   * - all 64 blocks, one data bit
     - that bit's ``4e``/``4f`` outside its window [:exp:`015`]
