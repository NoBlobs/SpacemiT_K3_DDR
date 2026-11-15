#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Thalles Campagnani
#
# Build the vendor U-Boot FSBL with a free PMU firmware embedded.
#   build_fsbl.sh <uboot_dir> <fw_prefix> [fragment] [out_dir]
#
# <fw_prefix>.imem.bin / .dmem.bin must exist. The vendor FSBL packaging
# (board/spacemit/k3/config.mk) only works for in-tree builds, so U-Boot is
# built in <uboot_dir>; FSBL.bin and the .config are copied to <out_dir>
# (default build/fsbl-<fw name>).
set -eu
UBOOT=$1
PREFIX=$2
FRAG=${3:-configs/k3_freefw.config}
FW=$(basename "$PREFIX")
OUT=${4:-$(pwd)/build/fsbl-$FW}
CROSS=${UBOOT_CROSS_COMPILE:-riscv64-linux-gnu-}

for s in imem dmem; do
	[ -f "$PREFIX.$s.bin" ] || { echo "missing $PREFIX.$s.bin" >&2; exit 1; }
done
mkdir -p "$OUT"

# U-Boot host tools need the OpenSSL ENGINE API, removed in OpenSSL 4.
# Use the openssl-3 compatibility package when present (Arch: openssl-3).
set -- ARCH=riscv CROSS_COMPILE="$CROSS"
if [ -d /usr/include/openssl-3 ] && [ -d /usr/lib/openssl-3 ]; then
	set -- "$@" HOSTCFLAGS=-I/usr/include/openssl-3 \
		"HOSTLDFLAGS=-L/usr/lib/openssl-3 -Wl,-rpath,/usr/lib/openssl-3"
fi

make -C "$UBOOT" "$@" k3_defconfig >/dev/null
( cd "$UBOOT" && scripts/kconfig/merge_config.sh -m .config "$FRAG" >/dev/null )
"$UBOOT/scripts/config" --file "$UBOOT/.config" \
	--set-str K3_DDR_LP5_FREE_FW_PREFIX "$PREFIX"
make -C "$UBOOT" "$@" olddefconfig >/dev/null
make -C "$UBOOT" "$@" -j"$(nproc)"

for o in K3_DDR_LPDDR4X K3_DDR_LP5_FREE_FW K3_DDR_LP5_FREE_FW_PREFIX \
	K3_DDR_PMU_MBOX_LOG K3_DDR_PMU_MBOX_TIMEOUT_MS K3_DDR_PMU_STOP_AFTER_FW \
	K3_DDR_PMU_HOST_DUMP K3_DDR_PMU_COMPARE_DUMP; do
	grep -E "^(# )?CONFIG_$o[= ]" "$UBOOT/.config" || true
done
grep -q k3_free_fw_imem_table "$UBOOT/spl/u-boot-spl.map" ||
	{ echo "free firmware not linked into SPL" >&2; exit 1; }
cp "$UBOOT/FSBL.bin" "$UBOOT/.config" "$OUT/"
git -C "$UBOOT" rev-parse HEAD > "$OUT/uboot.rev"
git -C "$UBOOT" diff HEAD --stat >> "$OUT/uboot.rev"
( cd "$OUT" && sha256sum FSBL.bin "$PREFIX.imem.bin" "$PREFIX.dmem.bin" | tee fsbl.sha256 )
