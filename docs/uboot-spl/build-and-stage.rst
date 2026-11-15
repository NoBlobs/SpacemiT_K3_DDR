.. SPDX-License-Identifier: GPL-2.0-or-later

Build and stage
===============

.. code-block:: sh

   make                          # PMU firmware images in build/<fw>/
   make fsbl FW=probe            # FSBL with the probe firmware
   tools/bench.sh build/fsbl-probe/FSBL.bin probe-run   # stage + log

``tools/build_fsbl.sh`` builds U-Boot **in tree** (the vendor packaging in
``board/spacemit/k3/config.mk`` only supports in-tree builds), merges the
fragment, sets ``K3_DDR_LP5_FREE_FW_PREFIX`` and copies ``FSBL.bin``,
``.config``, the U-Boot revision and SHA-256 sums to ``build/fsbl-<fw>/``.

Host toolchain notes
   ``riscv64-linux-gnu-`` cross compiler. U-Boot host tools use the OpenSSL
   ENGINE API removed in OpenSSL 4; the script uses the ``openssl-3``
   compatibility package (``/usr/include/openssl-3``, ``/usr/lib/openssl-3``).
