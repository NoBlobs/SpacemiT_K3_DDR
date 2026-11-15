.. SPDX-License-Identifier: GPL-2.0-or-later

======================================================
SpacemiT K3 — free LPDDR5 PMU firmware: documentation
======================================================

This is the project documentation. It is written by the clean team and
complements the read-only clean-room specification in :dc:`docsClean/ <README>`:

* facts copied from :dc:`docsClean/ <README>` are cited by file and section;
* facts discovered by our own experiments cite the experiment record in
  ``logsRefs/NNN-<name>/readme.md``;
* where an experiment contradicts :dc:`docsClean/ <README>`, this documentation wins and
  says so explicitly.

.. toctree::
   :maxdepth: 2
   :caption: Contents

   hardware/index
   uboot-spl/index
   firmware/index

Evidence labels
===============

``[spec]``
   Taken from :dc:`docsClean/ <README>` (not yet re-verified by us).
``[exp NNN]``
   Established by experiment ``logsRefs/NNN-*``.
``[design]``
   A decision of this project, not a hardware fact.
``[open]``
   Unknown; to be measured.
