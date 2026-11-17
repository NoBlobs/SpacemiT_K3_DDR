.. SPDX-License-Identifier: CC-BY-SA-4.0

Quickboot hardware sequence
===========================

The order of hardware operations that brings the DRAM up from a saved
training result. Specified by :dc:`11`; this page records the order that
works on the board and what each deviation caused. Our implementation splits
it between the SPL and the PMU firmware
(:doc:`../uboot-spl/quickboot-host`, :doc:`../firmware/quickboot`).

The saved result
----------------

5170 ``phypara`` halfwords in 69 segments, the message block and the ACSM
image (:doc:`results-persistence`). The reference quickboot firmware applies
``phypara`` in three passes [spec: :dc:`11`]:

====== ================ ==================================================
Pass   Segments         Content
====== ================ ==================================================
1      0–25 (860 hw)    per-dbyte control, read delays ``4e``–``51``, write
                        leveling ``0c``/``0d``, ``ad``/``af``, RX ODT
                        ``48/4a/4b``, CA group ``0x06``, 33 CA bases of
                        group ``0x03``, group ``0x02`` list (period
                        ``0x2000a/12/17``), ``0x908ff``, ``0xc00f0``–``ff``,
                        ``0xc0086``
2      26–37 (808 hw)   5 CA bases (``0x30001/02/05/06/08``) and the write
                        delays ``0x10``–``0x13``, ``0x15/16``, ``0x20/21``,
                        ``0x24``–``0x2b``, ``0x5a``–``0x67``
3      38–68 (3502 hw)  everything else: 384 bases in groups ``0x00``–``0x0c``,
                        including the ACSM dividers ``0x90808/13`` and
                        ``0x01004c/4d``
====== ================ ==================================================

Classification by CSR for group ``0x01`` (``off = index & 0xff``): Pass 2 =
``0x10–0x13, 0x15, 0x16, 0x20, 0x21, 0x24–0x2b, 0x5a–0x67``; Pass 1 =
``0x00, 03, 04, 06, 0c, 0d, 14, 2e, 30, 33, 35, 38, 3a–3c, 3e, 40, 42, 43, 48,
4a, 4b, 4e–51, 56, 57, 80–87, a3, a5, ad, af, d0–d4, d9``; Pass 3 = the
remaining 75 bases.

Registers never restored by a host [spec: :dc:`11`; :exp:`012`]: the runtime
controls ``0x20020``, ``0x20027``, ``0x20028``, ``0x2002a``, ``0x20031``,
``0x20041``, ``0x20060``, ``0x20078`` (they already hold the right values
after the PMU run), ``0xc0080`` and the ``0xd0xxx`` block.

Order that works
----------------

Clean in 10 of 10 boots [:exp:`019`]:

1. Pre-training host tables; DMEM message block from the saved result with
   ``SequenceCtrl = 1``, ``Quickboot = 1`` and the trained values copied into
   the MR shadows (``TrainedVREFCA → MR12``, ``TrainedVREFDQ → MR14``,
   ``TrainedVREFDQU → MR15``, ``TrainedDRAMDFE → MR24``, ``TrainedDRAMDCA →
   MR30``).
2. **Pass 1 and Pass 2 in the PHY before any reset** (ours: all three passes
   written by the host before the PMU starts, which :dc:`11` allows because
   nothing later overwrites a Pass 3 CSR).
3. Phase 0: ``0x20090 = 1``, commit strobe, ``0x2f052`` pulse, PHY signature
   check; ``0x20009`` saved.
4. Startup script ``0x2006e = 1``, ``0x70060 = 0``, ``0x70011 = 0``;
   ``0x2008b``; ``0x20072/73 = CsPresent``; **no** ``0x01<r>0a3`` mask;
   DQ block reset (``0x20303``–``0x20320``).
5. DRAM reset and PLL sequence with the 400 µs lock wait; ACSM dividers and
   NOP flush; switch to the PLL clock.
6. Mode registers, no ``tINIT`` wait: pass 0 with ``MR16 | 0x41``; ``MR16 |
   0x45`` and a wait (frequency-set-point switch); pass 1 with
   ``(MR16 | 0x45) & 0xbc``; ZQCAL latch; per-rank ``MR40 = 0x40`` and
   ``MR37 = 0x40``; ``MR28 = MR28_A0 | 2``; ``cmd 0x28``; NOP.
7. **Back to the direct clock**; ``0x2006e = 0``; per-rank step
   (``0x20072/73 = 1 << r``, ``0x2007a = 0x2007e = 0``); ``0x20072/73 =
   CsPresent``; ACSM dividers from ``0x20314/15``; ``0x01f0b3 = 0x01f0b4 =
   0``.
8. DLL recalibration (:doc:`phy-clock-dll`), ``0x9f02d = 1``, ``0x2f009``.
9. Pass 3; ACSM dividers ``0x90808/13`` recomputed; ``0x20037`` bit 6
   cleared; ACSM image restored; ``0xd00e7 = 0x0600``; ``0xc0080 = 2``;
   ``0xd0000 = 1``; DDRC activation.

What each deviation caused
--------------------------

.. list-table::
   :header-rows: 1
   :widths: 65 35

   * - Deviation
     - Result
   * - saved result written after a full DevInit (``Quickboot = 0``), SPI or FDL envelope, with or without live/kept registers [:exp:`011`, :exp:`012`]
     - 3 clean boots of 10, random byte-lane failures
   * - quickboot order, ``0x01<r>00d`` left at reset [:exp:`013` runs 1–2]
     - rank 1 one beat off, every boot
   * - quickboot order, DLL code read after ``0x2f0e0 = 0`` [:exp:`013`–:exp:`015`]
     - ``0x2004e = 0x1f`` written by the PMU (the host restored the saved code afterwards); 5 clean of 7
   * - DLL sampled correctly, single MR pass [:exp:`016`]
     - byte-lane failures remain
   * - two MR passes with the FSP switch, DLL recalibration on the PLL clock [:exp:`018`]
     - 2 clean of 3
   * - direct clock before the DLL recalibration [:exp:`019`]
     - **10 clean of 10**

Reference envelopes of the same board differ little from boot to boot
(``0x2000a`` within 2 counts, read-delay medians 61–63 taps over nine FDL
captures and the SPI capture, [spec: :dc:`14`]), and the result did not depend
on which of the nine was used [:exp:`012`]. A restored result is therefore
sensitive to the order of operations, not to small differences in the saved
values.

Not reproduced from the reference: the firmware-side deserialization of
``phypara`` from DMEM ``0x58200`` (the host writes the CSRs instead), the
710-halfword table at DMEM ``0x5aa00`` (only its three used fields are
needed, and they are constants in our code), and multi-P-state restore.
