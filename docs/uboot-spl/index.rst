.. SPDX-License-Identifier: GPL-2.0-or-later

=============
U-Boot SPL
=============

The first-stage loader (vendor U-Boot SPL, packaged as ``FSBL.bin``) is the
host side of the PMU contract: it prepares clocks, DDRC and PHY, loads the PMU
firmware, services the mailbox and activates the controllers.

Tree: ``/home/thalles/0-git/NoBlobs/u-boot_SpacemiT_K3``, branch
``k3-free-ddr-fw`` (based on ``k3-br-v1.0.y``).

.. toctree::
   :maxdepth: 1

   role
   integration
   mailbox-host
   build-and-stage
