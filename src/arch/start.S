/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright (C) 2026 Thalles Campagnani */
/*
 * Reset vector table and startup for the K3 DDR PHY PMU (ARCv2 EM, RF16).
 *
 * ARCv2 vector tables hold handler addresses: entry 0 is the reset vector.
 * docsClean/12 requires effective startup at 0x1fc; 127 words of table end
 * exactly there. Only r0-r15, gp, fp, sp, r30, blink may be used.
 */
	.section .vectors, "ax"
	.global __vectors
__vectors:
	.word	_start			/* 0: reset */
	.rept	15
	.word	__exc_entry		/* 1-15: exceptions */
	.endr
	.rept	111
	.word	__irq_entry		/* 16-126: interrupts (unused) */
	.endr

	.section .text.startup0, "ax"
	.global _start
	.type	_start, @function
_start:					/* must be at 0x1fc */
	mov	r0, 0x80003f00		/* breadcrumb slot 0 = 0xc0de */
	mov	r1, 0xc0de
	sth	r1, [r0]
	mov	sp, __stack_top
	mov	fp, 0
	/* .bss is ours (>= 0x80004000); host data below it is never touched */
	mov	r0, __bss_start
	mov	r1, __bss_end
	mov	r2, 0
1:	cmp	r0, r1
	bhs	2f
	st.ab	r2, [r0, 4]
	b	1b
2:	bl	main
	mov	r0, 0
	mov	r1, 0
	mov	r2, 0
	mov	r3, sp
	b	fw_exception		/* main() must not return */
	.size	_start, . - _start

	.text
	.global __exc_entry
	.type	__exc_entry, @function
__exc_entry:
	mov	r3, sp
	mov	r0, 0x80003f0c		/* breadcrumb slot 6 = 0xdead */
	mov	r1, 0xdead
	sth	r1, [r0]
	mov	sp, __stack_top
	lr	r0, [0x403]		/* ECR */
	lr	r1, [0x400]		/* ERET */
	lr	r2, [0x404]		/* EFA */
	b	fw_exception
	.size	__exc_entry, . - __exc_entry

	.global __irq_entry
	.type	__irq_entry, @function
__irq_entry:
	mov	r3, sp
	mov	sp, __stack_top
	lr	r0, [0x403]
	or	r0, r0, 0x80000000	/* tag: unexpected interrupt */
	lr	r1, [0x400]
	lr	r2, [0x404]
	b	fw_exception
	.size	__irq_entry, . - __irq_entry
