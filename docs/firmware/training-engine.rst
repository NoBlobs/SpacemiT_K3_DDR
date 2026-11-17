.. SPDX-License-Identifier: GPL-2.0-or-later

Training engine
===============

Status: **engine implemented and unit-tested natively; not yet used by a
stage**: the RdDQS probe gets no PASS/FAIL information from the PHY yet
(:doc:`stages`).

``src/train/eye.h`` is pure code on an explicit context (no MMIO, no global
state), after the three-level model of :dc:`07`:

=======  ================================================================
Level    Functions
=======  ================================================================
1        ``eye_samples()`` (``base × 2^BitTimeControl[2:0]`` transfers),
         ``eye_point()``: PASS only with zero errors in all transfers
2 (1D)   ``eye_scan_1d()``, ``eye_longest_run()`` (longest contiguous PASS
         window; shorter islands ignored), ``eye_win_center()``,
         ``eye_win_ok()``
2 (2D)   ``eye_scan_2d()``, ``eye_pick_2d()``: the PASS point with the
         highest ``delay_margin × wd + voltage_margin × wv`` (both weights
         0 → 1)
3        ``eye_margin()`` = ``min(c − L, R − c)``, ``eye_group_margin()``
         (minimum over the lanes of a group)
=======  ================================================================

A stage supplies the transfer-and-compare callback (``eye_eval_fn``: bit
errors of one transfer, negative to abort) and the scan axes
(``struct eye_axis``: first coordinate, step, sample count; at most 128
delay × 64 voltage samples). Scan boundaries count as failing coordinates
just outside the scan, so a margin is never larger than what was observed
(our policy; :dc:`07` leaves truncated eyes open).

``tests/test_eye.c`` checks it on synthetic elliptical eyes: centre and
margins, isolated PASS islands, an intermittently failing point, hardware
abort, an eye cut by the scan edge, coarse steps, and the weighted 2D pick.

Real data
---------

[:exp:`015`]: a host scan of the per-bit read delays ``0x01<r><l>4e/4f``
(``r`` = data byte, ``l`` = bit) on working DRAM gives windows of 82–122
taps (median about 100) with the trained value within a few taps of the
centre. A host write to these CSRs takes effect immediately. These
windows are the level 2 input a RdDQS stage has to reproduce.

Differences from the reference to resolve when a stage uses it [spec:
:dc:`07`]: the reference 2D centre is a weighted centroid over the passing
rows (``X_c``, ``Y_c``; weight ``n``, ``n²`` or a tier by
``Train2DMisc[12:11]``), not the best-score point of ``eye_pick_2d()``; its
1D selection keeps the longest PASS island, as ``eye_longest_run()`` does; its
margins are ``min`` over lanes accumulated per rank into the message block
(``0x037``–``0x03e``, ``0x052``–``0x059``: ``+0`` vertical, ``+1``
horizontal).
