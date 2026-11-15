#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Thalles Campagnani
#
# Pre-deployment audit of a PMU ELF (docsClean/13 BR-10):
#   1. no instruction references r16-r25;
#   2. no libgcc division / 64-bit multiply bodies; our overrides linked;
#   3. vector entry 0 = 0x1fc and _start at 0x1fc;
#   4. .rodata/.data/.bss inside 0x80004000-0x8000e000;
#   5. .text <= 64 KiB (<= 0x6800 for quickboot images, QB=1).
set -eu
elf=$1
OBJDUMP=${OBJDUMP:-arc-elf32-objdump}
NM=${NM:-arc-elf32-nm}
fail=0
err() { echo "AUDIT FAIL [$elf]: $*" >&2; fail=1; }

bad=$($OBJDUMP -d "$elf" | grep -E '^\s+[0-9a-f]+:' | grep -E '\br(1[6-9]|2[0-5])\b' || true)
[ -z "$bad" ] || { err "r16-r25 used:"; printf '%s\n' "$bad" | head -20 >&2; }

syms=$($NM "$elf")
for sym in __udivmodsi4 __divmodsi4 __umoddi3 __udivdi3 __divdi3; do
	if printf '%s\n' "$syms" | grep -qw "$sym"; then err "libgcc helper $sym linked"; fi
done

start=$(printf '%s\n' "$syms" | awk '$3=="_start"{print $1}')
[ "$start" = "000001fc" ] || err "_start at 0x$start, expected 0x1fc"
vec0=$($OBJDUMP -s -j .text --start-address=0 --stop-address=4 "$elf" | awk '/^ 0000/{print $2}')
[ "$vec0" = "fc010000" ] || err "vector entry 0 is $vec0, expected fc010000"

layout=$($OBJDUMP -h "$elf" | awk -v qb="${QB:-0}" '
	$2 ~ /^\.(bss|data|rodata)$/ {
		vma = strtonum("0x" $4); sz = strtonum("0x" $3)
		if (sz && (vma < 0x80004000 || vma + sz > 0x8000e000))
			printf "%s at 0x%x+0x%x outside DMEM window\n", $2, vma, sz
	}
	$2 == ".text" { t = strtonum("0x" $3); lim = qb == "1" ? 0x6800 : 0x10000
		if (t > lim) printf ".text 0x%x > 0x%x\n", t, lim }')
[ -z "$layout" ] || err "$layout"

[ $fail -eq 0 ] && echo "audit ok: $elf"
exit $fail
