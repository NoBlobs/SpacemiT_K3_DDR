.. SPDX-License-Identifier: GPL-2.0-or-later

Verification and clean-room record
==================================

Automatic checks
----------------

``make check``
   ELF audit (``tools/audit.sh``).
``make test``
   native tests: message block layout vs. docsClean, ``phypara`` order
   (69 segments, 5170 unique indices).

Hardware checks
---------------

Every hardware run is staged through FDL and logged; runs with relevant
findings become ``logsRefs/NNN-<name>/`` records (see :lr:`README.md`).

Clean-room record
-----------------

* 2026-11-15 — while locating the boundaries of the vendor firmware arrays in
  ``lpddr5_pre_training_table.c`` to exclude them from the build, two commands
  printed a few bytes of those arrays (the first three entries of
  ``lp5_training_fw_table`` and a few bytes at an array tail, the latter
  inside a subagent). They were not analysed and nothing in this project uses
  them. The vector table layout (entry 0 = ``0x1fc``) had already been chosen
  from the public ARCv2 ISA and :dc:`12`. Edits to that file are now made
  by line structure only, without displaying array contents.
