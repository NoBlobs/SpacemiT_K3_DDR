.. SPDX-License-Identifier: CC-BY-SA-4.0

DDRC, DPLL and host ports
=========================

Programmed by the host SPL; the PMU firmware does not touch these blocks, but
its inputs (``DRAMFreq``, ``DfiFreqRatio``) must agree with them
[spec: :dc:`03`].

============ ============ ========================== ============
Data rate    DPLL         ``CFG+0x18`` source select ``DRAMFreq``
============ ============ ========================== ============
4800 MT/s    3000 MHz     ``7 << 19``                ``0x12c0``
6400 MT/s    3200 MHz     ``2 << 19``                ``0x1900``
============ ============ ========================== ============

Sequence: program DPLL, poll ``CFG+0x1c[0]`` (lock), clear ``CFG+0x18[21:16]``
and set the source, kick ``CFG+0x18[25]`` and wait for it to clear, set bit 0;
before the PMU runs, release DCLK with ``CFG+0x18[22]`` (and bit 1).

Before releasing the PMU the SPL sets ``dfimisc = 0x00010014`` and
``pwrctl |= 1 << 11``. After training: ``dfimisc`` ``0x00010034`` (wait
``dfistat[0]``), ``0x00010015``, clear ``pwrctl``, wait ``stat[1:0] == 01``, restore
ZQ/refresh, ``pwrctl = 0x10``, then enable the five ports (``0x20090`` …
``0x24090``) [spec: :dc:`06` stage 8].
