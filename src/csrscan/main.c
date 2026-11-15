// SPDX-License-Identifier: GPL-2.0-or-later
/* Copyright (C) 2026 Thalles Campagnani */
/*
 * Read-only CSR range scan (experiment 004). Reads every index of the ranges
 * below from the PMU side, without writing any PHY register, and streams
 * the values. Before each read the index is stored in breadcrumb slots 14/15
 * (ARC 0x80003f1c/0x80003f1e = host index 0x59f8e/0x59f8f) so a read that
 * stalls the bus is identified by the SPL post-mortem dump.
 */
#include <stdint.h>
#include "common/log.h"
#include "hal/bcrumb.h"
#include "hal/mailbox.h"
#include "hal/mmio.h"
#include "hal/timer.h"

#define FW_ID_CSRSCAN	0x4353434eu	/* "CSCN" */
#define CSRSCAN_VERSION	0x00010000u
#define MAJOR_CSRSCAN	0x29
#define PER_FRAME	28		/* halfwords per stream frame */

static const struct { uint32_t first, count; } ranges[] = {
	{ 0x20000, 0x400 },	/* group 0x02: engine, clock period, MemReset, PLL? */
	{ 0x40000, 0x200 },	/* group 0x04 control (ACSM start 0x40020/0x400fd/0x40100) */
	{ 0xc0000, 0x100 },	/* group 0x0c: mailbox, clocks, 0xc00f1..ff */
};

static void scan_range(uint32_t first, uint32_t count)
{
	uint32_t buf[2 + PER_FRAME / 2];
	uint32_t i, n;

	for (i = 0; i < count; i += PER_FRAME) {
		n = count - i < PER_FRAME ? count - i : PER_FRAME;
		buf[0] = first + i;
		buf[1] = n;
		for (uint32_t j = 0; j < n; j++) {
			uint32_t idx = first + i + j;
			uint16_t v;

			bcrumb(14, (uint16_t)idx);
			bcrumb(15, (uint16_t)(idx >> 16));
			v = csr_rd(idx);
			if (j & 1)
				buf[2 + j / 2] |= (uint32_t)v << 16;
			else
				buf[2 + j / 2] = v;
		}
		mbox_stream(LOG_ID_CSRSCAN, buf, (uint16_t)(2 + (n + 1) / 2));
	}
}

int main(void)
{
	uint32_t boot[3] = { FW_ID_CSRSCAN, CSRSCAN_VERSION, 0 };
	unsigned int r;

	bcrumb(BC_MAIN, 1);
	timer_init();
	mbox_major(MAJOR_CSRSCAN);
	mbox_stream(LOG_ID_BOOT, boot, 3);
	for (r = 0; r < sizeof(ranges) / sizeof(ranges[0]); r++)
		scan_range(ranges[r].first, ranges[r].count);
	bcrumb(14, 0xffff);
	fw_done();
}
