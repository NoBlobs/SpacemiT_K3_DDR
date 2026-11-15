/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright (C) 2026 Thalles Campagnani */
/*
 * K3 DDR PHY PMU (ARCv2 EM) memory layout -- docsClean/02, /12, /13.
 *
 * IMEM 0x00000000-0x0000ffff: loaded by the host SPL at PHY index 0x50000.
 *   0x000-0x1fb  vector table (127 words, entry 0 = reset -> 0x1fc)
 *   0x1fc        startup
 * DMEM 0x80000000-0x8000ffff:
 *   0x80000000-0x8000013f message block (host in/out, 16/8-bit access only)
 *   0x80000400-0x800009a3 host-supplied parameter area (never cleared)
 *   0x80004000-...        our initialised data (loaded by SPL from the DMEM
 *                         image at PHY index 0x5a000) followed by .bss
 *   0x8000e000-0x8000e0ff transient host scratch -- never used
 *   0x8000e100-0x8000ffff stack (top 0x80010000)
 */
OUTPUT_FORMAT("elf32-littlearc")
OUTPUT_ARCH(arc)
ENTRY(__vectors)

MEMORY
{
	IMEM (rx)  : ORIGIN = 0x00000000, LENGTH = 0x10000
	DMEM (rw)  : ORIGIN = 0x80004000, LENGTH = 0x0a000
	STACK (rw) : ORIGIN = 0x8000e100, LENGTH = 0x01f00
}

__stack_top    = 0x80010000;
__stack_limit  = 0x8000e100;
__dmem_img_idx = 0x5a000;	/* host PHY index of DMEM image (0x80004000) */

SECTIONS
{
	.text : {
		KEEP(*(.vectors))
		. = 0x1fc;
		KEEP(*(.text.startup0))
		*(.text .text.*)
	} > IMEM

	/* Constant data and initialised data live in DMEM and are loaded by
	 * the host as a separate image: IMEM may not be readable by ld. */
	.rodata : ALIGN(4) {
		__dmem_img_start = .;
		*(.rodata .rodata.*)
	} > DMEM

	.data : ALIGN(4) {
		*(.data .data.*)
		*(.sdata .sdata.*)
		. = ALIGN(4);
		__dmem_img_end = .;
	} > DMEM

	.bss (NOLOAD) : ALIGN(4) {
		__bss_start = .;
		*(.bss .bss.*)
		*(.sbss .sbss.*)
		*(COMMON)
		. = ALIGN(4);
		__bss_end = .;
	} > DMEM

	/DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) *(.ARC.attributes) }
}

ASSERT(__bss_end <= 0x8000e000, "DMEM overflows into host scratch 0x8000e000")
ASSERT(SIZEOF(.text) <= 0x10000, "IMEM image larger than 64 KiB")
