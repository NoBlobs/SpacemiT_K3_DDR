// SPDX-License-Identifier: GPL-2.0-or-later
/* Copyright (C) 2026 Thalles Campagnani */
/*
 * M1 probe firmware. Runs no training. Streams (unfiltered) facts about the
 * PMU core and its environment, then completes with major 0x07:
 *   - IDENTITY and ARCv2 build configuration registers;
 *   - RTC control/counter samples around a fixed busy loop (host timestamps
 *     each frame, giving the RTC rate);
 *   - message block as seen from the PMU (8/16-bit reads);
 *   - hardware multiply/divide and delay-slot self tests;
 *   - optional last step: data read from IMEM (may fault -> 0xff).
 *
 * Major progress codes 0x20.. are ours; the host only logs them.
 */
#include <stdint.h>
#include "arch/aux.h"
#include "common/log.h"
#include "common/msgblk.h"
#include "hal/mailbox.h"
#include "hal/mmio.h"
#include "hal/timer.h"
#include "hal/bcrumb.h"

#define PROBE_VERSION	0x00010000u
#define FW_ID_PROBE	0x50524f42u	/* "PROB" */

#ifndef PROBE_READ_IMEM
#define PROBE_READ_IMEM	1
#endif

#define MAJOR_PROBE_START	0x20
#define MAJOR_PROBE_BCR		0x21
#define MAJOR_PROBE_RTC		0x22
#define MAJOR_PROBE_MB		0x23
#define MAJOR_PROBE_ISA		0x24
#define MAJOR_PROBE_IMEM	0x25

static void stream_aux_range(uint32_t first, uint32_t last)
{
	uint32_t buf[30];
	uint32_t a;
	uint16_t n = 0;

	for (a = first; a <= last; a++) {
		buf[n++] = a;
		buf[n++] = aux_read(a);
		if (n == 30) {
			mbox_stream(LOG_ID_PROBE_BCR, buf, n);
			n = 0;
		}
	}
	if (n)
		mbox_stream(LOG_ID_PROBE_BCR, buf, n);
}

static void probe_bcr(void)
{
	uint32_t id[4] = { AUX_IDENTITY, aux_read(AUX_IDENTITY),
			   AUX_STATUS32, aux_read(AUX_STATUS32) };

	mbox_major(MAJOR_PROBE_BCR);
	mbox_stream(LOG_ID_PROBE_BCR, id, 4);
	stream_aux_range(AUX_BCR_FIRST, AUX_BCR_LAST);
	stream_aux_range(AUX_BCR2_FIRST, AUX_BCR2_LAST);
}

static void stream_rtc(uint32_t tag)
{
	uint64_t t = timer_now();
	uint32_t w[4] = { tag, aux_read(AUX_RTC_CTRL), (uint32_t)t, (uint32_t)(t >> 32) };

	mbox_stream(LOG_ID_PROBE_RTC, w, 4);
}

static void probe_rtc(void)
{
	uint32_t i;

	mbox_major(MAJOR_PROBE_RTC);
	stream_rtc(0);			/* state before we touch it */
	timer_init();
	stream_rtc(1);
	for (i = 0; i < 4; i++) {	/* host time between frames / tick delta */
		timer_delay_ticks(1u << 22);
		stream_rtc(2 + i);
	}
	timer_delay_ticks(8);		/* short path: PHY bus pacing */
	stream_rtc(6);
}

static void probe_msgblk(void)
{
	uint32_t buf[30];
	uint32_t off;
	uint16_t n = 0;

	mbox_major(MAJOR_PROBE_MB);
	for (off = 0; off < MB_SIZE; off += 2) {
		buf[n++] = (off << 16) | mb_rd16(off);
		if (n == 30) {
			mbox_stream(LOG_ID_PROBE_DMEM, buf, n);
			n = 0;
		}
	}
	if (n)
		mbox_stream(LOG_ID_PROBE_DMEM, buf, n);
	/* byte read path must agree with the halfword path */
	buf[0] = mb_rd8(MB_MsgMisc);
	buf[1] = mb_rd8(MB_HdtCtrl);
	buf[2] = mb_rd16(MB_DRAMFreq);
	buf[3] = mb_rd16(MB_SequenceCtrl);
	mbox_stream(LOG_ID_PROBE_DMEM, buf, 4);
}

static uint32_t dbnz_count(uint32_t n)
{
	uint32_t c = 0;

	/* The delay-slot add executes on every iteration, including the last. */
	__asm__ volatile (
		"1:\n\t"
		"dbnz.d %1, 1b\n\t"
		"add %0, %0, 1\n\t"
		: "+r"(c), "+r"(n));
	return c;
}

static void probe_isa(void)
{
	volatile uint32_t a = 0x89abcdefu, b = 0x12345678u, c = 5, d = 4;
	volatile int32_t s = -7, t = 2;
	uint64_t p = (uint64_t)a * b;
	uint32_t add3r;
	uint32_t w[10];

	__asm__ volatile ("add3 %0, %1, %2" : "=r"(add3r) : "r"(0), "r"(1024));
	mbox_major(MAJOR_PROBE_ISA);
	w[0] = (uint32_t)p;		/* expect 0x2236d88f... low */
	w[1] = (uint32_t)(p >> 32);
	w[2] = c / d;			/* 1 (libgcc fast-path hang case) */
	w[3] = c % d;			/* 1 */
	w[4] = (uint32_t)(s / t);	/* -3 */
	w[5] = (uint32_t)(s % t);	/* -1 */
	w[6] = dbnz_count(5);		/* 5 */
	w[7] = add3r;			/* 0x2000 */
	w[8] = a / 0x100u;
	w[9] = PROBE_VERSION;
	mbox_stream(LOG_ID_PROBE_ISA, w, 10);
}

static void probe_imem(void)
{
	uint32_t w[3];
	uint32_t base = 0;

	/* hide the constant address 0 from the compiler (intentional access) */
	__asm__ ("" : "+r"(base));
	mbox_major(MAJOR_PROBE_IMEM);
	w[0] = rd16(base);		/* low half of reset vector */
	w[1] = rd16(base + 0x1fc);
	w[2] = *(volatile uint32_t *)base;
	mbox_stream(LOG_ID_PROBE_MEM, w, 3);
}

int main(void)
{
	uint32_t boot[3] = { FW_ID_PROBE, PROBE_VERSION, 0 };

	bcrumb(BC_MAIN, 1);
	mbox_major(MAJOR_PROBE_START);
	mbox_stream(LOG_ID_BOOT, boot, 3);
	probe_bcr();
	probe_rtc();
	probe_msgblk();
	probe_isa();
	if (PROBE_READ_IMEM)
		probe_imem();
	fw_done();
}
