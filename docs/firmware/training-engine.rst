.. SPDX-License-Identifier: GPL-2.0-or-later

Training engine
===============

Status: **not implemented**.

Planned ``src/train/eye.c``: generic 1D/2D sweep on an explicit context,
``2^BitTimeControl`` comparisons per point, interior centre, combined score
``delay_margin * wd + voltage_margin * wv`` (both weights 0 → 1), margin
``min(c - L, R - c)``, bounded iterations. Unit-tested natively on synthetic
eyes before use on hardware.
