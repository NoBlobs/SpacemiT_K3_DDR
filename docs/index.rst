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

Current status
==============

================================ ==========================================
Part                             State
================================ ==========================================
PMU runtime, mailbox, streaming  working [:exp:`000`, :exp:`001`]
DevInit (``SequenceCtrl[0]``)    working, matches the reference final state
                                 [:exp:`010`, :exp:`012`]
Quickboot hardware sequence      working with a reference result and the
                                 passes applied by the SPL: 16 GiB memory
                                 test clean in 10 of 10 boots [:exp:`019`];
                                 U-Boot runs on it [:exp:`014`]
Training engine (eye selection)  implemented, unit-tested, not yet used
RdDQS                            probe only: the PHY compare never reports
                                 an error [:exp:`016`–:exp:`019`]
Other training stages            not started
Own training result, envelope    not started
================================ ==========================================

Start with :doc:`hardware/quickboot-sequence` for what works on the board and
:doc:`firmware/stages` for what blocks an independent training.

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
