// SPDX-License-Identifier: GPL-2.0-or-later
/* Copyright (C) 2026 Thalles Campagnani */
/*
 * PMU side of the host mailbox (docsClean/08, /13 BR-09). Every register is
 * 16 bits wide. Waits are unbounded: if the host stops servicing, nobody is
 * left to report to.
 */
#include "hal/mailbox.h"
#include "hal/mmio.h"
#include "hal/timer.h"
#include "hal/bcrumb.h"

#define MBOX_STATUS	0x90180008u	/* bit 0 */
#define MBOX_LO		0x90180064u
#define MBOX_DOORBELL	0x90180066u
#define MBOX_HI		0x90180068u
#define TERM_PULSE	0x90040238u	/* 1->0 done, 4->0 fatal */

static uint16_t mbox_sent;

/*
 * PMU send (logsRefs/000: polarity measured on hardware, inverse of
 * docsClean BR-09). At rest the status 0x90180008[0] mirrors the host's
 * DCT_WRITE_PROT = 1 and 0x90180066 reads 1.
 *   1. wait status[0] == 1   (host ready)
 *   2. write data            (0x90180064 [, 0x90180068])
 *   3. 0x90180066 = 0        (item available; host sees UCT_SHADOW[0] = 0)
 *   4. wait status[0] == 0   (host ACK: DCT_WRITE_PROT = 0)
 *   5. 0x90180066 = 1        (host then sees UCT_SHADOW[0] = 1, writes 1)
 */
static void mbox_put(uint16_t lo, uint16_t hi, int has_hi)
{
	uint16_t st = rd16(MBOX_STATUS);

	if (!mbox_sent)
		bcrumb(BC_MBOX_ST0, st);
	bcrumb(BC_MBOX_WAIT, 1);
	while (!(st & 1)) {
		bcrumb(BC_MBOX_ST1, st);
		st = rd16(MBOX_STATUS);
	}
	wr16(MBOX_LO, lo);
	if (has_hi)
		wr16(MBOX_HI, hi);
	wr16(MBOX_DOORBELL, 0);
	bcrumb(BC_MBOX_WAIT, 2);
	while ((st = rd16(MBOX_STATUS)) & 1)
		bcrumb(BC_MBOX_ST1, st);
	wr16(MBOX_DOORBELL, 1);
	bcrumb(BC_MBOX_WAIT, 0);
	bcrumb(BC_MBOX_SENT, ++mbox_sent);
}

void mbox_major(uint16_t code)
{
	mbox_put(code, 0, 0);
}

void mbox_word(uint32_t w)
{
	mbox_put((uint16_t)w, (uint16_t)(w >> 16), 1);
}

void mbox_stream(uint16_t id, const uint32_t *payload, uint16_t len)
{
	uint16_t i;

	mbox_major(MBOX_MAJOR_STREAM);
	mbox_word(((uint32_t)id << 16) | len);
	for (i = 0; i < len; i++)
		mbox_word(payload[i]);
}

static __attribute__((noreturn)) void halt(void)
{
	for (;;)
		__asm__ volatile ("flag 1\n\tnop\n\tnop\n\tnop");
}

/* The pulse is issued before the terminal code: once the host sees the code
 * it may stop the PMU at any time (MICRO_RESET = 1). */
void fw_done(void)
{
	wr16(TERM_PULSE, 1);
	timer_delay_ticks(0x20);
	wr16(TERM_PULSE, 0);
	mbox_major(MBOX_MAJOR_DONE);
	halt();
}

void fw_fatal(void)
{
	wr16(TERM_PULSE, 4);
	timer_delay_ticks(0x20);
	wr16(TERM_PULSE, 0);
	/* The host does not ACK 0xff: publish it without waiting for the ACK. */
	while (!(rd16(MBOX_STATUS) & 1))
		;
	wr16(MBOX_LO, MBOX_MAJOR_FATAL);
	wr16(MBOX_DOORBELL, 0);
	halt();
}
