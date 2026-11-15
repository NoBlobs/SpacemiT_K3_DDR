// SPDX-License-Identifier: GPL-2.0-or-later
/* Copyright (C) 2026 Thalles Campagnani */
/*
 * PHY PLL bring-up probe (experiment 005).
 *
 * Hypothesis (experiments 003/004 + register names from public DDR4-PHY code,
 * refuted by this experiment): the PHY PLL is powered down at PMU start
 * (0x200c3 PllPwrDn = 1) and unprogrammed, so the clock-period counters of
 * DevInit cannot count. Stages:
 *   0  read state
 *   1  program PllCtrl2 0x200c5 = 0x19, PllCtrl1 0x200c7 = 0x21,
 *      PllTestMode 0x200ca = 0x24, PllCtrl4 0x200cc = 0x17f (public values)
 *   2  PllPwrDn 0x200c3 = 0, watch the registers for 2 ms
 *   3  period measurement of docsClean/07 (clear 0x20011[6:5], 0x200e3 = 1)
 * After each stage the register set below is streamed (LOG_ID_PLLPROBE),
 * with the RTC time. Every change seen while watching is streamed too.
 */
#include <stdint.h>
#include "common/log.h"
#include "hal/bcrumb.h"
#include "hal/mailbox.h"
#include "hal/mmio.h"
#include "hal/timer.h"

#define FW_ID_PLLPROBE	0x504c4c50u	/* "PLLP" */
#define MAJOR_PLLPROBE	0x2a

static const uint32_t regs[] = {
	0x200c3, 0x200c5, 0x200c6, 0x200c7, 0x200c9, 0x200ca, 0x200cc,
	0x200cd, 0x200cf, 0x300bf, 0x100e0, 0x20047, 0x200e3, 0x20011,
};
#define NREGS (sizeof(regs) / sizeof(regs[0]))

static uint64_t t0;

static void snapshot(uint32_t tag, uint16_t *v)
{
	uint32_t w[2 + (NREGS + 1) / 2];
	unsigned int i;

	w[0] = tag;
	w[1] = (uint32_t)(timer_now() - t0) / RTC_TICKS_PER_US;	/* 32-bit: wraps after 10.7 s */
	for (i = 0; i < NREGS; i++) {
		v[i] = csr_rd(regs[i]);
		if (i & 1)
			w[2 + i / 2] |= (uint32_t)v[i] << 16;
		else
			w[2 + i / 2] = v[i];
	}
	mbox_stream(LOG_ID_PLLPROBE, w, (uint16_t)(2 + (NREGS + 1) / 2));
}

/* Poll the set for 'us' microseconds; stream when anything changes (max 12). */
static void watch(uint32_t tag, uint32_t us)
{
	uint16_t prev[NREGS], cur[NREGS];
	uint64_t end;
	unsigned int i, events = 0;
	int changed;

	snapshot(tag, prev);
	end = timer_now() + (uint64_t)us * RTC_TICKS_PER_US;
	while (timer_now() < end && events < 12) {
		changed = 0;
		for (i = 0; i < NREGS; i++) {
			cur[i] = csr_rd(regs[i]);
			changed |= cur[i] != prev[i];
		}
		if (changed) {
			snapshot(tag | 0x10, prev);
			events++;
		}
	}
	snapshot(tag | 0x20, prev);
}

int main(void)
{
	uint32_t boot[3] = { FW_ID_PLLPROBE, 0x00010000u, NREGS };
	uint16_t v[NREGS], ctrl;

	bcrumb(BC_MAIN, 1);
	timer_init();
	t0 = timer_now();
	mbox_major(MAJOR_PLLPROBE);
	mbox_stream(LOG_ID_BOOT, boot, 3);

	snapshot(0x00, v);

	bcrumb(12, 1);
	csr_wr(0x200c5, 0x19);
	csr_wr(0x200c7, 0x21);
	csr_wr(0x200ca, 0x24);
	csr_wr(0x200cc, 0x17f);
	watch(0x01, 50);

	bcrumb(12, 2);
	csr_wr(0x200c3, 0);
	watch(0x02, 2000);

	bcrumb(12, 3);
	ctrl = csr_rd(0x20011);
	csr_wr(0x20011, ctrl & 0xff9f);
	csr_wr(0x200e3, 1);
	watch(0x03, 2000);
	csr_wr(0x200e3, 0);
	watch(0x04, 500);
	csr_wr(0x20011, ctrl);
	snapshot(0x05, v);

	bcrumb(12, 4);
	fw_done();
}
