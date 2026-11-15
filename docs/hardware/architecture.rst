.. SPDX-License-Identifier: CC-BY-SA-4.0

System architecture
===================

Two complete DDR instances [spec: :dc:`01`]:

======== ============== ============== ==============
Instance DDRC           CFG/DPLL       DPHY
======== ============== ============== ==============
0        ``0xcb000000`` ``0xcb600000`` ``0xcb800000``
1        ``0xcc000000`` ``0xcc600000`` ``0xcc800000``
======== ============== ============== ==============

``CFG = DDRC + 0x600000``, ``DPHY = DDRC + 0x800000``. The SPL initialises
instance 0 then instance 1, each with its own PHY, PMU run and result.

Who does what
-------------

Host (U-Boot SPL, RISC-V)
   part/rate selection, DPLL, DDRC timings and address map, PHY tables,
   loading the PMU images, mailbox service, result collection, DDRC
   activation, memory smoke test. See :doc:`../uboot-spl/role`.

PMU (ARCv2 EM inside each DPHY)
   DRAM device initialisation and all PHY training. This is what this
   project implements. See :doc:`pmu-arc-core`.

Full training vs. quickboot
---------------------------

Full training: the PMU runs the stages selected by ``SequenceCtrl``; the host
then saves message block + 5170 ``phypara`` halfwords + 4096 ACSM halfwords per
controller (``0x49a4`` bytes). Quickboot: a different PMU program restores a
saved result (about 9 ms vs. about 5.6 s) [spec: :dc:`01`, :dc:`11`].

Reference board
---------------

SpacemiT K3 Pico-ITX, 16 GiB LPDDR5 ``MT62F2G32D4DS`` (2 ranks, x16) per
controller pair, 6400 MT/s [spec: :dc:`14`]. Board access:
:doc:`board-access`.
