// SPDX-License-Identifier: GPL-2.0-or-later
/* Copyright (C) 2026 Thalles Campagnani */
/*
 * CSR read-out firmware (experiment 002). Reads, from the PMU side, every
 * PHY CSR the host serializes as phypara (5170, docsClean appendix order) and
 * the 4096 ACSM SRAM halfwords, without writing any PHY register. Values are
 * stored in DMEM at a fixed place so the SPL can compare them with its own
 * host-side reads of the same registers:
 *
 *   ARC 0x80008000 (host index 0x5c000): phypara[5170], then acsm[4096]
 *
 * Streams a summary (counts, non-zero counts, CRC-32, RTC ticks) and ends
 * with major 0x07.
 */
#include <stdint.h>
#include "common/log.h"
#include "common/phypara.h"
#include "hal/bcrumb.h"
#include "hal/mailbox.h"
#include "hal/mmio.h"
#include "hal/timer.h"

#define FW_ID_CSRDUMP	0x43535244u	/* "CSRD" */
#define CSRDUMP_VERSION	0x00010000u

#define DUMP_BUF	0x80008000u	/* host index 0x5c000 */
#define DUMP_ACSM	(DUMP_BUF + 2 * PHYPARA_COUNT)
#define ACSM_COUNT	4096
#define ACSM_ARC	0x90082000u	/* host index 0x41000 */

#define MAJOR_CSRDUMP_START	0x26
#define MAJOR_CSRDUMP_PHYPARA	0x27
#define MAJOR_CSRDUMP_ACSM	0x28

extern char __bss_end[];

static uint32_t crc32_update(uint32_t crc, uint16_t v)
{
	int i;

	crc ^= v;			/* little-endian halfword, 2 bytes */
	for (i = 0; i < 16; i++)
		crc = (crc >> 1) ^ (0xedb88320u & -(crc & 1));
	return crc;
}

struct summary {
	uint32_t count, nonzero, crc;
};

static void dump_phypara(struct summary *s)
{
	uint32_t k = 0, seg, j;

	s->crc = 0xffffffffu;
	for (seg = 0; seg < PHYPARA_NSEG; seg++) {
		const struct phypara_seg *g = &phypara_segs[seg];

		bcrumb(12, (uint16_t)seg);
		for (j = 0; j < g->count; j++, k++) {
			uint16_t v = csr_rd(phypara_index(g, (uint16_t)j));

			wr16(DUMP_BUF + 2 * k, v);
			s->nonzero += v != 0;
			s->crc = crc32_update(s->crc, v);
		}
	}
	s->count = k;
	s->crc = ~s->crc;
}

static void dump_acsm(struct summary *s)
{
	uint32_t j;

	s->crc = 0xffffffffu;
	for (j = 0; j < ACSM_COUNT; j++) {
		uint16_t v = rd16(ACSM_ARC + 2 * j);

		bcrumb(13, (uint16_t)j);
		wr16(DUMP_ACSM + 2 * j, v);
		s->nonzero += v != 0;
		s->crc = crc32_update(s->crc, v);
	}
	s->count = j;
	s->crc = ~s->crc;
}

int main(void)
{
	uint32_t boot[3] = { FW_ID_CSRDUMP, CSRDUMP_VERSION, DUMP_BUF };
	struct summary p = { 0 }, a = { 0 };
	uint64_t t0, t1, t2;
	uint32_t w[8];

	bcrumb(BC_MAIN, 1);
	timer_init();
	mbox_major(MAJOR_CSRDUMP_START);
	mbox_stream(LOG_ID_BOOT, boot, 3);
	if ((uint32_t)__bss_end > DUMP_BUF) {
		w[0] = __LINE__;
		w[1] = (uint32_t)__bss_end;
		mbox_stream(LOG_ID_ASSERT, w, 2);
		fw_fatal();
	}

	mbox_major(MAJOR_CSRDUMP_PHYPARA);
	t0 = timer_now();
	dump_phypara(&p);
	t1 = timer_now();
	mbox_major(MAJOR_CSRDUMP_ACSM);
	dump_acsm(&a);
	t2 = timer_now();

	w[0] = p.count;
	w[1] = p.nonzero;
	w[2] = p.crc;
	w[3] = a.count;
	w[4] = a.nonzero;
	w[5] = a.crc;
	w[6] = (uint32_t)(t1 - t0);
	w[7] = (uint32_t)(t2 - t1);
	mbox_stream(LOG_ID_CSRDUMP, w, 8);
	fw_done();
}
