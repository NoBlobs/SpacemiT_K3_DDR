// SPDX-License-Identifier: GPL-2.0-or-later
/* Copyright (C) 2026 Thalles Campagnani */
/*
 * RdDQS (SequenceCtrl[3]) — first step: the hardware transfer-and-compare
 * primitive of docsClean/07 (3cff3d8), used as a probe (experiment 016).
 *
 * The PHY compares the data itself. Per candidate point:
 *   write the delay in lane-broadcast form (0x01<dbyte>f<reg>), run the
 *   training burst compiled at ACSM entry 200 as a one-shot, read each
 *   lane's 16-bit result from 0x01<dbyte><lane>b7, clear the sticky flags
 *   through 0x01f0bb (0x3ff, then 0).
 *
 * Arming (docsClean e00a07a, Pattern = 0, 2D pass 1): PLL clock mode
 * (0x200f8 = 0, 0x200a2[1:0] = 2), 0x20072/73 = CsPresent, 0x2007a/7e = 0;
 * pattern generator 0x7f061..64 = 0xffff, 0x7f011 = 0x16, 0x7f065 = 0,
 * 0x7f0d6/d7 = 0x100, 0x7f060 = 0x88;
 * 0x2006e = 0, 0x01f056 = 1, 0x01f0a7 = 0, 0x200c0/c1 = 0, 0x20079 = 0x40,
 * 0x01f0b3/b4/b1 = 1, 0x01f0b0 = 0x34; DRAM pattern MR33 = 0x3c, MR34 = 0x5a,
 * MR31 = MR32 = 0; training mode 0x01f001 = 1 and MR46 = 4; result 0 = PASS.
 *
 * The firmware writes the documented 20-entry write-then-read burst at ACSM
 * entry 200 itself, for one rank at a time. It sweeps registers, streams the
 * raw results and restores what it changed. It selects nothing yet.
 */
#include <stdint.h>
#include "common/log.h"
#include "common/msgblk.h"
#include "hal/mmio.h"
#include "hal/timer.h"
#include "train/devinit_calc.h"
#include "train/train.h"

#define BURST_ENTRY	200u
#define BURST_END	219u	/* reference result: entries 200..219 */
#define NLANES		9u
#define POLL_MAX	25000u

#define DB_CSR(d, l, xx)	(0x10000u | ((uint32_t)(d) << 12) | ((uint32_t)(l) << 8) | (xx))

struct rdq {
	uint8_t ps;
	uint32_t polls;
};

/* One-shot run of the prebuilt burst. Returns 0 when the ACSM reports done. */
static int burst_run(struct rdq *q)
{
	uint32_t n = 0;
	uint16_t done;

	csr_wr(0x20022 + q->ps, BURST_ENTRY);
	csr_wr(0x20023 + q->ps, BURST_END);
	csr_wr(0x20027, 0);
	(void)csr_rd(0x20024);		/* delay_dfi_clks(6): below 16 ticks */
	csr_wr(0x20020, 1);
	done = csr_rd(0x20021);
	while (!done && n < POLL_MAX) {
		done = csr_rd(0x20021);
		n++;
	}
	csr_wr(0x20020, 0);
	q->polls = n;
	return done ? 0 : -1;
}

/* Read the 9 lane results of one dbyte, then clear the sticky flags. */
static void results(unsigned int d, uint16_t r[NLANES])
{
	unsigned int l;

	for (l = 0; l < NLANES; l++)
		r[l] = csr_rd(DB_CSR(d, l, 0xb7));
	csr_wr(DB_CSR(0xf, 0, 0xbb), 0x03ff);
	csr_wr(DB_CSR(0xf, 0, 0xbb), 0);
}

static void report(uint32_t tag, uint32_t x, const struct rdq *q, int rc, const uint16_t r[NLANES])
{
	log_words(0, LOG_ID_RDDQS_PROBE, 8, tag, x, (uint32_t)(rc ? 0x80000000u : 0) | q->polls,
		  ((uint32_t)r[0] << 16) | r[1], ((uint32_t)r[2] << 16) | r[3],
		  ((uint32_t)r[4] << 16) | r[5], ((uint32_t)r[6] << 16) | r[7], r[8]);
}

#define ACSM_ARC	0x90082000u

static void pulse(uint32_t idx)
{
	csr_wr(idx, 1);
	timer_delay_ticks(0x20);
	csr_wr(idx, 0);
}

/* MRW entries at the start of ACSM SRAM, run as a one-shot. */
static int mrw_burst(struct rdq *q, unsigned int cs, const uint8_t *ma, const uint8_t *op, unsigned int n)
{
	static const uint16_t nop3 = 0x2b00;	/* 2-unit NOP pair (docsClean/07) */
	uint32_t a = ACSM_ARC, polls = 0;
	unsigned int i, k, cur = 0;
	uint16_t e[5][4], done;

	for (i = 0; i < n; i++) {
		e[0][0] = acsm_mrw1(cs, ma[i]);
		e[1][0] = acsm_mrw2(cs, op[i]);
		e[2][0] = e[3][0] = e[4][0] = 0;
		for (k = 0; k < 5; k++)
			e[k][1] = e[k][2] = e[k][3] = 0;
		e[2][3] = nop3;
		e[3][3] = acsm_nop_hw7(acsm_nop_count(17, 8));
		e[4][3] = 0;		/* padding slot */
		for (k = 0; k < 4; k++, cur += 4) {
			wr16(a + 2 * cur, e[k][0]);
			wr16(a + 2 * cur + 2, e[k][1]);
			wr16(a + 2 * cur + 4, e[k][2]);
			wr16(a + 2 * cur + 6, e[k][3]);
		}
	}
	csr_wr(0x20022 + q->ps, 0);
	csr_wr(0x20023 + q->ps, (uint16_t)((cur >> 2) - 1));
	csr_wr(0x20027, 0);
	(void)csr_rd(0x20024);
	csr_wr(0x20020, 1);
	done = csr_rd(0x20021);
	while (!done && polls < POLL_MAX) {
		done = csr_rd(0x20021);
		polls++;
	}
	csr_wr(0x20020, 0);
	q->polls = polls;
	return done ? 0 : -1;
}

/*
 * The Pattern = 0 write-then-read burst, 20 ACSM entries from entry 200
 * (docsClean/07: the sequence of reference entries 200..219, published in
 * the reference envelope appendix). hw0[15:14] is the chip select.
 */
static const uint16_t burst_p0[20][4] = {
	{ 0x0000, 0x0000, 0x0000, 0x0080 }, { 0x0000, 0x0000, 0x0000, 0x0000 },
	{ 0x0000, 0x0000, 0x0000, 0x2b00 }, { 0x0000, 0x0000, 0x0000, 0x0400 },
	{ 0x8000, 0x0000, 0x0000, 0x0410 }, { 0x801c, 0x00d0, 0x0000, 0x0010 },
	{ 0x8060, 0x0084, 0x0000, 0x7f10 }, { 0x0000, 0x0000, 0x0000, 0x0000 },
	{ 0x8000, 0x0000, 0x0000, 0x0410 }, { 0x0000, 0x0000, 0x0000, 0x0000 },
	{ 0x0000, 0x0000, 0x0000, 0x3f00 }, { 0x0000, 0x0000, 0x0000, 0x0400 },
	{ 0x8000, 0x0000, 0x0000, 0x0410 }, { 0x802c, 0x0d00, 0x0000, 0x0010 },
	{ 0x8020, 0x0803, 0x0000, 0x7f10 }, { 0x0000, 0x0000, 0x0000, 0x0000 },
	{ 0x8000, 0x0000, 0x0000, 0x2e10 }, { 0x0000, 0x0000, 0x0000, 0x0400 },
	{ 0x0000, 0x0000, 0x0000, 0x0500 }, { 0x0000, 0x0000, 0x0000, 0x0000 },
};

/* Write the burst for chip select cs (1 = rank 0, 2 = rank 1). Returns mismatches on read-back. */
static unsigned int burst_build(unsigned int cs)
{
	unsigned int e, k, bad = 0;

	for (e = 0; e < 20; e++) {
		uint32_t a = ACSM_ARC + 8 * (BURST_ENTRY + e);

		for (k = 0; k < 4; k++) {
			uint16_t v = burst_p0[e][k];

			if (k == 0 && (v & 0xc000))
				v = (uint16_t)((v & 0x3fff) | (cs << 14));
			wr16(a + 2 * k, v);
			bad += rd16(a + 2 * k) != v;
		}
	}
	return bad;
}

/* Sweep one register of one dbyte in lane-broadcast form. */
static int sweep(struct rdq *q, unsigned int d, unsigned int reg, unsigned int from,
		 unsigned int step, unsigned int n, uint32_t tag)
{
	uint16_t sv[NLANES], r[NLANES];
	unsigned int l, i;
	int rc, fails = 0;

	for (l = 0; l < NLANES; l++)
		sv[l] = csr_rd(DB_CSR(d, l, reg));
	report(tag | 0x40, (d << 16) | (reg << 8), q, 0, sv);
	for (i = 0; i < n; i++) {
		unsigned int v = from + i * step;

		csr_wr(DB_CSR(d, 0xf, reg), (uint16_t)v);
		rc = burst_run(q);
		fails += rc != 0;
		results(d, r);
		report(tag, (d << 28) | (reg << 16) | v, q, rc, r);
	}
	for (l = 0; l < NLANES; l++)
		csr_wr(DB_CSR(d, l, reg), sv[l]);
	return fails;
}

enum train_status stage_rddqs(struct train_ctx *c)
{
	static const uint8_t pat_ma[4] = { 33, 34, 31, 32 }, pat_op[4] = { 0x3c, 0x5a, 0x00, 0x00 };
	static const uint8_t m46 = 46, on = 0x04, off = 0x00;
	static const uint32_t gsave[] = {
		0x20079, 0x20022, 0x20023, 0x2002a, 0x20041, 0x2006e, 0x200c0, 0x200c1,
	};
	uint16_t s84 = csr_rd(0x30084), s0a[4];
	uint16_t sf8 = csr_rd(0x200f8), sa2 = csr_rd(0x200a2), s7a = csr_rd(0x2007a),
		 s7e = csr_rd(0x2007e), s72 = csr_rd(0x20072), s73 = csr_rd(0x20073);
	static const uint16_t g7[] = { 0x061, 0x062, 0x063, 0x064, 0x065, 0x011, 0x060, 0x0d6, 0x0d7 };
	/* 2D pass 1 pair: 0x7f011 = 0x16, 0x7f060 = 0x88 (docsClean e00a07a) */
	static const uint16_t g7v[] = { 0xffff, 0xffff, 0xffff, 0xffff, 0x0000, 0x0016, 0x0088, 0x0100, 0x0100 };
	static const uint8_t breg[] = { 0xb0, 0xb1, 0xb3, 0xb4, 0x56, 0xa7, 0x01 };
	struct rdq q = { .ps = c->pstate & 0x0f };
	uint16_t sg[8], s7[9], sb[4][7], r[NLANES];
	unsigned int d, l, k, i;
	int rc, fails = 0;

	for (k = 0; k < 8; k++)
		sg[k] = csr_rd(gsave[k] + (k == 1 || k == 2 || k == 4 ? q.ps : 0));
	for (k = 0; k < 9; k++)
		s7[k] = csr_rd(0x70000u | g7[k]);
	for (d = 0; d < 4; d++)
		for (k = 0; k < 7; k++)
			sb[d][k] = csr_rd(DB_CSR(d, 0, breg[k]));
	log_words(0, LOG_ID_RDDQS_PROBE, 8, 0x01, sg[0], ((uint32_t)sg[1] << 16) | sg[2],
		  ((uint32_t)sg[3] << 16) | sg[4], ((uint32_t)sb[0][0] << 16) | sb[0][1],
		  ((uint32_t)sb[0][2] << 16) | sb[0][3], csr_rd(DB_CSR(0, 0, 0xb5)),
		  csr_rd(DB_CSR(0, 0, 0xbb)));
	log_words(0, LOG_ID_RDDQS_PROBE, 8, 0x04, ((uint32_t)s7[0] << 16) | s7[1],
		  ((uint32_t)s7[2] << 16) | s7[3], ((uint32_t)s7[4] << 16) | s7[5],
		  ((uint32_t)s7[6] << 16) | s7[7], s7[8], ((uint32_t)sg[5] << 16) | sg[6],
		  ((uint32_t)sb[0][4] << 16) | sb[0][5]);

	for (d = 0; d < 4; d++)
		s0a[d] = csr_rd(DB_CSR(d, 0, 0x0a));
	log_words(0, LOG_ID_RDDQS_PROBE, 8, 0x05, sf8, sa2, s7a, s7e, s72,
		  ((uint32_t)s84 << 16) | s0a[0],
		  ((uint32_t)csr_rd(DB_CSR(0, 0, 0x80)) << 16) | csr_rd(0x20090));

	/* PLL clock mode (quickboot ends in direct-clock mode; not in phypara) */
	timer_delay_ticks(64);
	csr_wr(0x200f8, 0);
	timer_delay_ticks(16);
	csr_wr(0x200a2, (uint16_t)((sa2 & 0xfffc) | 0x0002));
	timer_delay_ticks(64);

	/* active channels, burst generator and pattern generator (Pattern = 0) */
	csr_wr(0x20072, c->cs_a);
	csr_wr(0x20073, c->cs_b);
	csr_wr(0x2007a, 0);
	csr_wr(0x2007e, 0);
	pulse(0x2f0a0);
	csr_wr(0x2006e, 0);
	csr_wr(DB_CSR(0xf, 0, 0x56), 1);
	csr_wr(DB_CSR(0xf, 0, 0xa7), 0);
	csr_wr(0x200c0, 0);		/* K3 LPDDR5: 0, not 8 */
	csr_wr(0x200c1, 0);
	csr_wr(0x20079, 0x0040);
	csr_wr(DB_CSR(0xf, 0, 0xb3), 1);
	csr_wr(DB_CSR(0xf, 0, 0xb4), 1);
	csr_wr(DB_CSR(0xf, 0, 0xb1), 1);
	csr_wr(DB_CSR(0xf, 0, 0xb0), MB_GET(DfiFreqRatio) == 4 ? 0x34 : 0x30);
	for (k = 0; k < 9; k++)
		csr_wr(0x7f000u | g7[k], k == 6 && MB_GET(DfiFreqRatio) != 4 ? 0x0008 : g7v[k]);
	/*
	 * ACSM routing as DevInit step 6.2: the restored final state has
	 * 0x030084 = 1 and 0x01000a = 1 (mission mode), with which the ACSM
	 * commands never reach the pads (experiments 016-019).
	 */
	csr_wr(0x3f084, 0);
	csr_wr(DB_CSR(0xf, 0, 0x0a), 0);
	csr_wr(0x2002a, 1);
	csr_wr(0x20041 + q.ps, 1);

	/* DRAM pattern mode registers, then training mode */
	rc = mrw_burst(&q, 3, pat_ma, pat_op, 4);
	fails += rc != 0;
	log_words(0, LOG_ID_RDDQS_PROBE, 4, 0x06, 0, (uint32_t)rc, q.polls);
	csr_wr(DB_CSR(0xf, 0, 0x01), 1);
	rc = mrw_burst(&q, 3, &m46, &on, 1);
	fails += rc != 0;
	log_words(0, LOG_ID_RDDQS_PROBE, 4, 0x06, 1, (uint32_t)rc, q.polls);
	pulse(0x2f0a0);

	/* what the host left at entries 200 and 204..206, then our own burst (rank 1) */
	log_words(0, LOG_ID_RDDQS_PROBE, 8, 0x07,
		  ((uint32_t)rd16(ACSM_ARC + 8 * 200) << 16) | rd16(ACSM_ARC + 8 * 200 + 6),
		  ((uint32_t)rd16(ACSM_ARC + 8 * 204) << 16) | rd16(ACSM_ARC + 8 * 204 + 6),
		  ((uint32_t)rd16(ACSM_ARC + 8 * 205) << 16) | rd16(ACSM_ARC + 8 * 205 + 2),
		  ((uint32_t)rd16(ACSM_ARC + 8 * 206) << 16) | rd16(ACSM_ARC + 8 * 206 + 2),
		  ((uint32_t)rd16(ACSM_ARC + 8 * 213) << 16) | rd16(ACSM_ARC + 8 * 213 + 2),
		  ((uint32_t)rd16(ACSM_ARC + 8 * 216) << 16) | rd16(ACSM_ARC + 8 * 216 + 6), 0);
	l = burst_build(2);
	log_words(0, LOG_ID_RDDQS_PROBE, 4, 0x06, 3, l, 0);

	/* the burst (rank 1), nothing swept: three times */
	for (i = 0; i < 3; i++) {
		rc = burst_run(&q);
		fails += rc != 0;
		for (d = 0; d < 4; d++) {
			results(d, r);
			report(0x03, (i << 8) | d, &q, rc, r);
		}
	}

	/* rank 1: read delays 0x4e / 0x4f of every dbyte, then 0x11 / 0x13 of dbyte 0 */
	for (d = 0; d < 4 && fails < 8; d++) {
		fails += sweep(&q, d, 0x4e, 0, 4, 32, 0x10);
		fails += sweep(&q, d, 0x4f, 0, 4, 32, 0x10);
	}
	fails += sweep(&q, 0, 0x11, 0, 8, 64, 0x11);
	fails += sweep(&q, 0, 0x13, 0, 8, 64, 0x11);

	/* rank 0: the same burst with its chip select patched */
	l = burst_build(1);
	log_words(0, LOG_ID_RDDQS_PROBE, 4, 0x06, 2, l, 0);
	rc = burst_run(&q);
	fails += rc != 0;
	for (d = 0; d < 4; d++) {
		results(d, r);
		report(0x03, 0x1000 | d, &q, rc, r);
	}
	fails += sweep(&q, 0, 0x4e, 0, 4, 32, 0x12);
	fails += sweep(&q, 0, 0x4f, 0, 4, 32, 0x12);
	fails += sweep(&q, 0, 0x10, 0, 8, 64, 0x13);
	fails += sweep(&q, 0, 0x12, 0, 8, 64, 0x13);
	(void)burst_build(2);

	/* exit sequence (docsClean eae6813), then restore what was saved */
	csr_wr(DB_CSR(0xf, 0, 0xb0), 0);
	csr_wr(DB_CSR(0xf, 0, 0xb1), 0);
	csr_wr(DB_CSR(0xf, 0, 0x56), 0);
	csr_wr(0x200c0, 0);
	csr_wr(0x200c1, 0);
	csr_wr(0x20079, 0);
	csr_wr(0x7f011, csr_rd(0x70011) & (uint16_t)~0x0010u);
	csr_wr(0x200a3, 1);
	csr_wr(0x200a3, 0);
	rc = mrw_burst(&q, 3, &m46, &off, 1);
	fails += rc != 0;
	csr_wr(DB_CSR(0xf, 0, 0x01), 0);

	for (k = 0; k < 9; k++)
		csr_wr(0x7f000u | g7[k], s7[k]);
	for (d = 0; d < 4; d++)
		for (k = 0; k < 7; k++)
			csr_wr(DB_CSR(d, 0, breg[k]), sb[d][k]);
	for (k = 0; k < 8; k++)
		csr_wr(gsave[k] + (k == 1 || k == 2 || k == 4 ? q.ps : 0), sg[k]);
	csr_wr(0x20041 + q.ps, 0);
	for (d = 0; d < 4; d++)
		csr_wr(DB_CSR(d, 0, 0x0a), s0a[d]);
	csr_wr(0x3f084, s84);
	pulse(0x2f0a0);
	csr_wr(0x20072, s72);
	csr_wr(0x20073, s73);
	csr_wr(0x2007a, s7a);
	csr_wr(0x2007e, s7e);
	/* clock mode as found */
	csr_wr(0x200a2, sa2);
	timer_delay_ticks(16);
	csr_wr(0x200f8, sf8);
	timer_delay_ticks(64);
	log_words(0, LOG_ID_RDDQS_PROBE, 2, 0x7f, (uint32_t)fails);
	return TRAIN_OK;
}
