.. SPDX-License-Identifier: GPL-2.0-or-later

Build and toolchain
===================

Toolchain
   Synopsys ARC GNU prebuilt ``arc-2026.03-release``
   (``arc_gnu_2026.03_prebuilt_elf32_le_linux_install.tar.xz``, GCC 15.2.0,
   from github.com/foss-for-mips-arc-processors/toolchain), extracted in
   ``tools/toolchain/`` (git-ignored, not committed):

   .. code-block:: sh

      cd tools/toolchain
      curl -LO https://github.com/foss-for-mips-arc-processors/toolchain/releases/download/arc-2026.03-release/arc_gnu_2026.03_prebuilt_elf32_le_linux_install.tar.xz
      tar -xJf arc_gnu_2026.03_prebuilt_elf32_le_linux_install.tar.xz

   The Makefile looks first in ``tools/toolchain/*/*/bin``, then in ``PATH``
   (``arc-elf32-``, ``arc-snps-elf-``, ``arc-zephyr-elf-``); override with
   ``CROSS_COMPILE=``. No libgcc object is linked into the current images.

Flags (compile **and** link)
   ``-std=gnu11 -mcpu=em -mrf16 -mlittle-endian -mcode-density
   -mbarrel-shifter -mmpy-option=wlh5 -mdiv-rem -mno-sdata``, plus ``-Os
   -ffreestanding -fno-builtin -fno-tree-loop-distribute-patterns``. Code
   density, barrel shifter, multiplier and div/rem are confirmed by the BCRs
   (:exp:`experiment 001 <001>`); NORM and SWAP are absent, so ``-mnorm``/``-mswap`` stay off.

Libcalls
   ``src/arch/libcalls.c`` defines ``__udivsi3``, ``__divsi3``, ``__umodsi3``,
   ``__modsi3``, ``__umulsi3_highpart``, ``__muldi3``, ``memset``, ``memcpy`` with
   hardware instructions; it is linked before ``-lgcc`` and never with LTO.

Outputs (``build/<fw>/``)
   ``<fw>.elf``, ``<fw>.map``, ``<fw>.imem.bin`` (``.text``),
   ``<fw>.dmem.bin`` (``.rodata`` + ``.data``), ``<fw>.sha256``.

Targets
   ``make``, ``make check`` (``tools/audit.sh``: no ``r16``–``r25``, no libgcc
   division bodies, ``_start`` at ``0x1fc``, vector 0 = ``0x1fc``, data inside
   ``0x80004000``–``0x8000e000``, size limit), ``make test`` (native tests),
   ``make fsbl FW=<fw>``.
