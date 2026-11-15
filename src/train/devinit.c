// SPDX-License-Identifier: GPL-2.0-or-later
/* Copyright (C) 2026 Thalles Campagnani */
/*
 * DevInit (SequenceCtrl[0]) — docsClean/07 "Complete DevInit hardware
 * initialization sequence" (docsClean 19ab858).
 *
 * Implemented:
 *   phase 0  0x20090, byte[1] bit 0 branch, PHY version decode (0xc00ee),
 *            signature canary, delay multiplier (kept in struct devinit);
 *   phase 1  PHY block enable (reference clock), MemResetL pre-reset state;
 *   phase 2  delay shift from 0xc0080, commit strobe;
 *   phase 3  3.1 QBPll read-back and the 0x800004cc descriptors,
 *            3.2 0x2008b/0x20072/0x20073, 3.3 0x20011 and 0x01<r>0a3,
 *            3.4 DQ block reset, 3.5 DRAM reset / PLL sequencing,
 *            3.6 per-rank DQ calibration (result in 0x01<r>0ad),
 *            3.7 period latch and frequency buckets;
 *   phase 6  6.1 ACSM dividers and one-shot NOP flush, block disable;
 *            6.2 MemResetL release and the one-shot MRW burst;
 *            6.3 lane state, channel commit, 0x01<r>0a3 restore, 0x000a4.
 * Not implemented: the ZQ calibration MPC burst of step 6.2 (question J2).
 *
 * Delays follow the two-tier model of docsClean/07: delay_dfi_clks(n) waits
 * (n + 1) >> shift RTC ticks (one dummy read below 16), delay_ps_or_clks()
 * converts picoseconds or a scaled minimum to DFI clocks.
 */
#include <stdint.h>
#include "common/log.h"
#include "common/msgblk.h"
#include "hal/mmio.h"
#include "hal/timer.h"
#include "train/devinit_calc.h"
#include "train/train.h"

#define CSR_PACE	0x20024u	/* delay_dfi_clks() dummy read */
#define CSR_CAL_CTRL	0x20011u
#define CSR_CNT_EN	0x200e3u
#define CSR_CNT_CFG	0x200e5u
#define CSR_TCA_CNT	0x300bfu
#define CSR_TDQ_CNT	0x100e0u
#define CSR_TCA		0x2000au	/* + pstate */
#define CSR_TDQ		0x20017u	/* + pstate */
#define CSR_BUCKET	0x20012u	/* + pstate */
#define CSR_BUCKET_A	0x20186u	/* + pstate */
#define CSR_BUCKET_B	0x20187u	/* + pstate */
#define CSR_BUCKET_SRC	0x100d5u
#define CSR_CLKSEL	0x200a2u	/* [1:0]: 0 gated, 1 reference, 2 PHY PLL */
#define CSR_PLLRST	0x20078u	/* 1 assert, 2 release, 0 normal */
#define CSR_MEMRESETL	0x20060u
#define CSR_UCCLK	0xc0080u
#define CSR_STICKY	0xc0086u

/* ACSM sequencer (docsClean/07 "ACSM command sequencer") */
#define CSR_ACSM_RUN	0x20020u
#define CSR_ACSM_DONE	0x20021u	/* ACSM_DONE: 1 = done/idle, 0 = running */
#define CSR_ACSM_START	0x20022u	/* + pstate */
#define CSR_ACSM_STOP	0x20023u	/* + pstate */
#define CSR_ACSM_MODE	0x20027u
#define CSR_ACSM_BURST	0x20041u	/* + pstate */
#define ACSM_SRAM_ARC	0x90082000u
#define ACSM_POLL_MAX	25000u
#define ACSM_NOP_HW3	0x2b00u		/* fixed 2-unit inter-MRW spacing */

/* group 0x01 rank r register xx, lane l, and the write-only all-rank alias */
#define RANK_CSR(r, xx)		(0x10000u | ((uint32_t)(r) << 12) | (xx))
#define LANE_CSR(r, l, xx)	(RANK_CSR(r, xx) | ((uint32_t)(l) << 8))
#define BCAST_CSR(xx)		RANK_CSR(0xf, xx)

#define MAX_RANKS	4
#define PHY_SIG_LO	0x6745u
#define PHY_SIG_HI	0x00f2u

struct devinit {
	struct train_ctx *c;
	uint8_t ps;		/* Pstate & 0x0f */
	uint8_t pstate;		/* raw byte[0x80000004] */
	uint8_t ratio;
	uint8_t shift;		/* byte[0x80000404] */
	uint8_t mult;		/* byte[0x80000b68] */
	uint8_t mult_en;	/* byte[0x80000a80] */
	uint8_t nranks;		/* word[0x8000040c] */
	uint8_t ver;		/* byte[0x80000403], PHY IP version code */
	int dram_init;		/* Quickboot == 0 && byte[0] bit 7 == 0 */
	uint16_t a3[MAX_RANKS];	/* DMEM 0x80000430 + 2r in the reference */
	uint64_t t0;
};

/* Message-block offset of MR<ma>_A0 (A1 = +1); 0 if not in the burst. */
static uint32_t mr_offset(unsigned int ma)
{
	switch (ma) {
	case 1: return MB_MR1_A0;
	case 2: return MB_MR2_A0;
	case 3: return MB_MR3_A0;
	case 10: return MB_MR10_A0;
	case 11: return MB_MR11_A0;
	case 12: return MB_MR12_A0;
	case 14: return MB_MR14_A0;
	case 15: return MB_MR15_A0;
	case 16: return MB_MR16_A0;
	case 17: return MB_MR17_A0;
	case 18: return MB_MR18_A0;
	case 19: return MB_MR19_A0;
	case 20: return MB_MR20_A0;
	case 22: return MB_MR22_A0;
	case 24: return MB_MR24_A0;
	case 37: return MB_MR37_A0;
	case 40: return MB_MR40_A0;
	case 41: return MB_MR41_A0;
	default: return 0;
	}
}

static void delay_dfi_clks(const struct devinit *d, uint32_t n)
{
	uint32_t ticks = delay_scaled(n, d->shift);

	if (ticks < 16) {
		(void)csr_rd(CSR_PACE);
		return;
	}
	timer_delay_ticks(ticks);
}

static void delay_ps_or_clks(const struct devinit *d, uint32_t ps, uint32_t min_clks)
{
	int en = d->mult_en && !(MB_GET(MsgMisc) & 0x08);

	delay_dfi_clks(d, ps_or_clks(ps, min_clks, d->c->freq, d->ratio, d->mult, en));
}

static void commit_strobe(void)
{
	csr_wr(0x20028, 1);
	csr_wr(0x2f0a5, 1);
	csr_wr(0x2f077, 0);
}

static uint32_t elapsed_us(const struct devinit *d)
{
	return (uint32_t)(timer_now() - d->t0) / RTC_TICKS_PER_US;
}

static void report_state(const struct devinit *d, uint32_t tag)
{
	log_words(0, LOG_ID_DEVINIT_STATE2, 12, tag, elapsed_us(d),
		  csr_rd(CSR_STICKY), csr_rd(CSR_CAL_CTRL), csr_rd(CSR_MEMRESETL),
		  csr_rd(CSR_TCA + d->ps), csr_rd(CSR_TDQ + d->ps),
		  csr_rd(CSR_BUCKET + d->ps), csr_rd(CSR_BUCKET_A + d->ps),
		  csr_rd(CSR_BUCKET_B + d->ps), csr_rd(CSR_BUCKET_SRC),
		  csr_rd(RANK_CSR(0, 0x0a3)));
}

/* Diagnostic latch (result unused); restores 0x200e3/0x200e5. */
static void latch_probe(const struct devinit *d, uint32_t tag)
{
	uint16_t e3 = csr_rd(CSR_CNT_EN), e5 = csr_rd(CSR_CNT_CFG);
	uint16_t pa = csr_rd(CSR_TCA_CNT), pd = csr_rd(CSR_TDQ_CNT), a, b;

	csr_wr(CSR_CNT_CFG, e5 & 0xffe0);
	csr_wr(CSR_CNT_EN, e3 | 0x0800);
	csr_wr(CSR_CNT_EN, e3 | 0x0c00);
	delay_dfi_clks(d, 10);
	csr_wr(CSR_CNT_EN, e3 | 0x0800);
	a = csr_rd(CSR_TCA_CNT);
	b = csr_rd(CSR_TDQ_CNT);
	csr_wr(CSR_CNT_EN, e3);
	csr_wr(CSR_CNT_CFG, e5);
	log_words(0, LOG_ID_DEVINIT_LATCH, 8, tag, csr_rd(CSR_CLKSEL), e3, e5, pa, pd, a, b);
}

/* PHY IP version code from 0xc00ee & 0xfff (docsClean/07 phase 0). */
static uint8_t phy_version(uint16_t v)
{
	v &= 0x0fff;
	if (v >= 0x0101 && v <= 0x0105)
		return (uint8_t)(v - 0x0101 + 2);
	if (v == 0x0110)
		return 7;
	if (v == 0x0200)
		return 8;
	if (v == 0x0240)
		return 9;
	return 10;
}

/* Phase 0 — top-level PMU startup. */
static int devinit_phase0(struct devinit *d)
{
	uint16_t m_lo = rd16(0x80000424), m_hi = rd16(0x80000426);
	uint16_t ee = csr_rd(0xc00ee);
	uint32_t lo_idx;
	int branch = mb_rd8(0x001) & 0x01;	/* byte[0x80000001] bit 0 (question J1) */
	uint16_t r_lo, r_hi;

	csr_wr(0x20090, 1);
	if (branch) {
		csr_wr(0x3f01a, 0);
		csr_wr(BCAST_CSR(0x01a), 0);
		csr_wr(0x2001a, 0);
		csr_wr(0x200aa, 0);
		csr_wr(0x2001a, 3);
	}
	commit_strobe();
	if (branch) {
		csr_wr(0x2f052, 1);
		delay_dfi_clks(d, 10);
		csr_wr(0x2f052, 0);
	}
	d->ver = phy_version(ee);
	lo_idx = d->ver ? 0x9002eu : 0x90029u;
	csr_wr(lo_idx, PHY_SIG_LO);
	csr_wr(lo_idx + 1, PHY_SIG_HI);
	r_lo = csr_rd(lo_idx);
	r_hi = csr_rd(lo_idx + 1);

	d->mult_en = 1;
	d->mult = clk_mult(d->c->freq, d->ratio);
	log_words(0, LOG_ID_DEVINIT_PHASE0B, 11, d->ver, m_lo, m_hi, ee, branch, 0, 0,
		  r_lo, r_hi, d->mult, lo_idx);
	/* the reference aborts with 0xff on a mismatch; we log and continue */
	return 0;
}
/* Phase 1 — PHY block enable on the reference clock. */
static void devinit_phase1(struct devinit *d)
{
	csr_wr(CSR_CLKSEL, (uint16_t)((csr_rd(CSR_CLKSEL) & 0xfffc) | 0x0001));
	delay_dfi_clks(d, 4);
	delay_dfi_clks(d, 0x1f);
	csr_wr(0x200f8, 1);
	if (d->dram_init)
		csr_wr(CSR_MEMRESETL, 2);
}

/* Phase 2 — delay shift and configuration commit. */
static void devinit_phase2(struct devinit *d)
{
	d->shift = delay_shift(csr_rd(CSR_UCCLK));
	commit_strobe();
}

/* Step 3.1 — the three 6-byte [u32 csr_idx, u16 val] descriptors at 0x800004cc. */
static void apply_descriptors(void)
{
	static const uint32_t expect[3] = { 0x2006e, 0x70060, 0x70011 };
	static const uint16_t expect_val[3] = { 0x0001, 0x0000, 0x0000 };
	unsigned int i, k;

	for (i = 0; i < 3; i++) {
		uint32_t a = 0x800004ccu + 6 * i, idx = 0;
		uint16_t v;

		for (k = 0; k < 4; k++)
			idx |= (uint32_t)rd8(a + k) << (8 * k);
		v = (uint16_t)(rd8(a + 4) | (rd8(a + 5) << 8));
		/* apply what the host pre-loaded only if it names the documented CSR */
		if (idx != expect[i]) {
			log_words(0, LOG_ID_DEVINIT_DESC, 4, i, idx, v, 1);
			idx = expect[i];
			v = expect_val[i];
		} else {
			log_words(0, LOG_ID_DEVINIT_DESC, 4, i, idx, v, 0);
		}
		csr_wr(idx, v);
	}
}

/* Step 3.4 — DQ block reset on 0x20303..0x20320. */
static void dq_block_reset(struct devinit *d)
{
	uint16_t u303 = csr_rd(0x20303);

	csr_wr(0x20310, 1);
	csr_wr(0x20303, (uint16_t)((u303 & 0x00ef) | 0x0010));
	csr_wr(0x20310, 0);
	csr_wr(0x2030d, 1);
	csr_wr(0x2030d, 0);
	csr_wr(0x20311, 1);
	delay_ps_or_clks(d, 130000000u, 0);
	csr_wr(0x20311, 0);
	csr_wr(0x20320, 0);
	csr_wr(0x20320, 1);
	csr_wr(0x20320, 0);
	csr_wr(0x20303, (uint16_t)(u303 & 0x00ff));
}

/* Steps 13 and 14 of the normal branch (shared with bypass). */
static void pll_tail(struct devinit *d)
{
	csr_wr(CSR_PLLRST, 0);
	csr_wr(CSR_CLKSEL, 0);
	csr_wr(0x20200, 0);
	delay_ps_or_clks(d, 0, 0x20);
	csr_wr(0x200f8, 0);
	csr_wr(CSR_CLKSEL, 2);
}

/* Step 3.5 — DRAM reset and PHY PLL/reset sequencing. */
static void dram_reset_pll(struct devinit *d)
{
	uint16_t u0f = csr_rd(RANK_CSR(0, 0x00f) + d->ps);

	csr_wr(BCAST_CSR(0x00f) + d->ps, (uint16_t)((u0f & 0xff78) | 0x0006));
	csr_wr(0x20061, 1);
	if (!MB_GET(PllBypassEn)) {
		delay_dfi_clks(d, 4);
		csr_wr(0x20049, 0x01f0);
		csr_wr(0x20061, 0);
		csr_wr(0x200e1, 0);
		csr_wr(0x20310, 0);
		csr_wr(0x200e0, 0);
		csr_wr(0x200c3, 0);
		csr_wr(0x200e2, 3);
		delay_ps_or_clks(d, 1000000u, 0);
		csr_wr(0x200e2, 2);
		delay_ps_or_clks(d, 500000u, 0);
		csr_wr(0x200e2, 0);
		delay_ps_or_clks(d, 10000000u, 0);
		csr_wr(CSR_PLLRST, 1);
		csr_wr(0x20200, 0x0033);
		csr_wr(CSR_CLKSEL, 0);
		delay_dfi_clks(d, 6);
		csr_wr(CSR_PLLRST, 2);
		csr_wr(CSR_CLKSEL, 2);
		delay_ps_or_clks(d, 0, 10000);	/* PLL lock: 400 us at 6400/4 */
		latch_probe(d, 0x56);		/* experiment 008: PLL clock selected */
	} else {
		delay_dfi_clks(d, 4);
		csr_wr(0x20061, 0);
		csr_wr(0x200e1, 1);
		csr_wr(0x200e0, 1);
		csr_wr(0x20310, 0);
		csr_wr(0x200c3, 0);
		delay_ps_or_clks(d, 10000000u, 0);
		csr_wr(CSR_PLLRST, 1);
		csr_wr(0x20200, 0x0033);
		csr_wr(CSR_CLKSEL, 0);
		delay_ps_or_clks(d, 0, 0x20);
		csr_wr(CSR_PLLRST, 2);
		csr_wr(CSR_CLKSEL, 2);
		delay_ps_or_clks(d, 0, 1024);
	}
	pll_tail(d);
}

/*
 * Step 3.6 — per-rank DQ calibration (docsClean/07 19ab858). The channel bits
 * cleared in 0x01f002 + rank are restored at the end (not stated, question J3).
 */
static void dq_rank_cal(struct devinit *d)
{
	uint16_t old_0f, old_ac, d5, v, saved02[MAX_RANKS], dd[5];
	unsigned int r, l, k, sel, fallback, tol;
	uint32_t ks;

	if (d->c->freq < 1600)
		return;
	old_0f = csr_rd(RANK_CSR(0, 0x00f) + d->ps);
	old_ac = csr_rd(RANK_CSR(0, 0x0ac) + d->ps);
	csr_wr(BCAST_CSR(0x0d9), 0x1000);
	csr_wr(BCAST_CSR(0x00f) + d->ps, (uint16_t)((old_0f & 0xfff8) | 0x0006));
	for (r = 0; r < d->nranks; r++) {
		saved02[r] = csr_rd(0x10002u + r);
		csr_wr(0x1f002u + r, (uint16_t)(saved02[r] & ~(r < 2 ? 1u : 2u)));
	}
	csr_wr(BCAST_CSR(0x0ac) + d->ps, (uint16_t)(old_ac & 0xfffe));
	csr_wr(BCAST_CSR(0x00f) + d->ps, (uint16_t)(csr_rd(RANK_CSR(0, 0x00f) + d->ps) | 0x0005));
	delay_dfi_clks(d, 0x3000);
	csr_wr(BCAST_CSR(0x00f) + d->ps,
	       (uint16_t)((csr_rd(RANK_CSR(0, 0x00f) + d->ps) & 0xfffa) | 0x0004));

	for (r = 0; r < d->nranks; r++) {
		ks = 0;
		fallback = 0;
		d5 = 0;
		for (l = 0; l <= 8; l++) {
			for (k = 0; k < 5; k++)
				dd[k] = csr_rd(LANE_CSR(r, l, 0x0d0 + k));
			v = csr_rd(RANK_CSR(r, 0x0ac));
			csr_wr(RANK_CSR(r, 0x0ac), v | 1);
			csr_wr(RANK_CSR(r, 0x0ac), (uint16_t)(v & ~1u));
			d5 = csr_rd(RANK_CSR(r, 0x0d5));
			tol = d5 * 14u / 100u;
			sel = 5;
			for (k = 0; k < 4; k++) {
				uint16_t delta = (uint16_t)(dd[k + 1] - dd[k]);
				unsigned int diff = delta > d5 ? delta - d5 : d5 - delta;

				if (dd[k] > 0 && diff <= tol) {
					sel = k;
					break;
				}
			}
			if (sel == 5)
				fallback++;
			ks |= (uint32_t)sel << (3 * l);
			csr_wr(RANK_CSR(r, 0x0ad), (uint16_t)sel);
		}
		log_words(0, LOG_ID_DEVINIT_DQCAL2, 5, r, d5, ks, fallback, csr_rd(RANK_CSR(r, 0x0ad)));
	}
	delay_dfi_clks(d, ((csr_rd(RANK_CSR(d->nranks - 1, 0x014)) & 0x01fe) + 4u) * 160u);
	csr_wr(BCAST_CSR(0x00f) + d->ps, old_0f);
	csr_wr(BCAST_CSR(0x0ac) + d->ps, old_ac);
	for (r = 0; r < d->nranks; r++)
		csr_wr(0x1f002u + r, saved02[r]);
}
/* Step 3.7 — master clock-period calibration and frequency buckets. */
static enum train_status clock_period(struct devinit *d, uint16_t *tca)
{
	struct freq_buckets b;
	uint16_t old_e3 = csr_rd(CSR_CNT_EN), old_e5 = csr_rd(CSR_CNT_CFG);
	uint16_t pre_ca = csr_rd(CSR_TCA_CNT), pre_dq = csr_rd(CSR_TDQ_CNT);
	uint16_t hi_e3, ca, dq, src;
	unsigned int r;

	csr_wr(CSR_CNT_CFG, old_e5 & 0xffe0);
	csr_wr(CSR_CNT_EN, old_e3 | 0x0800);
	csr_wr(CSR_CNT_EN, old_e3 | 0x0c00);
	delay_dfi_clks(d, 10);
	hi_e3 = csr_rd(CSR_CNT_EN);
	csr_wr(CSR_CNT_EN, old_e3 | 0x0800);	/* bit 10 falling edge latches */
	ca = csr_rd(CSR_TCA_CNT);
	dq = csr_rd(CSR_TDQ_CNT);
	csr_wr(CSR_TCA + d->ps, ca & 0x01ff);
	csr_wr(CSR_TDQ + d->ps, dq & 0x01ff);
	csr_wr(CSR_CNT_EN, old_e3);
	csr_wr(CSR_CNT_CFG, old_e5);
	*tca = ca;

	src = csr_rd(CSR_BUCKET_SRC);
	if (freq_buckets(d->c->freq, d->ratio, &b)) {
		log_words(0, LOG_ID_DEVINIT_CAL2, 8, old_e3, old_e5, pre_ca, pre_dq,
			  hi_e3, ca, dq, src);
		return TRAIN_FAIL;
	}
	csr_wr(CSR_BUCKET + d->ps, b.r20012);
	csr_wr(CSR_BUCKET_A + d->ps, b.r20186);
	csr_wr(CSR_BUCKET_B + d->ps, src);
	for (r = 0; r < d->nranks; r++)
		csr_wr(RANK_CSR(r, 0x0d9), 0x009c);

	log_words(0, LOG_ID_DEVINIT_CAL2, 12, old_e3, old_e5, pre_ca, pre_dq, hi_e3,
		  ca, dq, src, b.f_lo, b.f_hi, b.r20012, b.r20186);
	return TRAIN_OK;
}

/* Phase 3 — steps 3.1 to 3.7. */
static enum train_status devinit_phase3(struct devinit *d, uint16_t *tca)
{
	static const uint32_t qbpll_src[7] = {
		0x20088, 0x20089, 0x2008a, 0x2008c, 0x2004a, 0x2004b, 0x2004c,
	};
	unsigned int i;

	/* 3.1 */
	if (d->pstate & 0x20)
		for (i = 0; i < 7; i++)
			mb_wr16(MB_QBPllUPllProg0 + 2 * i, csr_rd(qbpll_src[i]));
	apply_descriptors();
	/* 3.2 */
	csr_wr(0x2008b, (d->pstate & 0x80) ? 0 : (d->pstate & 1));
	csr_wr(0x20072, d->c->cs_a);
	csr_wr(0x20073, d->c->cs_b);
	/* 3.3 */
	if (!MB_GET(Quickboot)) {
		csr_wr(CSR_CAL_CTRL, csr_rd(CSR_CAL_CTRL) & 0xff9f);
		for (i = 0; i < d->nranks; i++)
			d->a3[i] = csr_rd(RANK_CSR(i, 0x0a3));
		for (i = 0; i < d->nranks; i++)
			csr_wr(RANK_CSR(i, 0x0a3), csr_rd(RANK_CSR(i, 0x0a3)) & 0x000c);
	}
	report_state(d, 0x33);
	/* 3.4 */
	dq_block_reset(d);
	report_state(d, 0x34);
	/* 3.5 */
	dram_reset_pll(d);
	report_state(d, 0x35);
	latch_probe(d, 0x35);
	/* 3.6 */
	dq_rank_cal(d);
	latch_probe(d, 0x36);
	/* 3.7 */
	return clock_period(d, tca);
}

/*
 * One-shot ACSM run over instructions 0..(cur >> 2) - 1. 0x20021 is
 * ACSM_DONE (1 = done): poll while it reads 0. Returns 0 when done.
 */
static int acsm_oneshot(const struct devinit *d, uint16_t cur, uint32_t tag)
{
	uint16_t first, done;
	uint32_t n = 0;

	csr_wr(CSR_ACSM_MODE, 0);
	delay_dfi_clks(d, 6);
	csr_wr(CSR_ACSM_STOP + d->ps, (uint16_t)((cur >> 2) - 1));
	csr_wr(CSR_ACSM_RUN, 1);
	first = csr_rd(CSR_ACSM_DONE);
	done = first;
	while (!done && n < ACSM_POLL_MAX) {
		done = csr_rd(CSR_ACSM_DONE);
		n++;
	}
	csr_wr(CSR_ACSM_RUN, 0);
	log_words(0, LOG_ID_DEVINIT_ACSM, 5, tag, n, done, first, cur);
	return done ? 0 : -1;
}

static void acsm_reset_ptr(const struct devinit *d, uint16_t *cur)
{
	csr_wr(CSR_ACSM_START + d->ps, 0);
	csr_wr(CSR_ACSM_STOP + d->ps, 0);
	*cur = 0;
}

static void acsm_put(uint16_t *cur, uint16_t h0, uint16_t h1, uint16_t h2, uint16_t h3)
{
	uint32_t a = ACSM_SRAM_ARC + 2u * *cur;

	wr16(a, h0);
	wr16(a + 2, h1);
	wr16(a + 4, h2);
	wr16(a + 6, h3);
	*cur = (uint16_t)(*cur + 4);
}

/* One MRW (MRW-1, MRW-2) followed by the fixed NOP pair. */
static void acsm_mrw(uint16_t *cur, unsigned int cs, unsigned int ma, unsigned int op)
{
	acsm_put(cur, acsm_mrw1(cs, ma), 0, 0, 0);
	acsm_put(cur, acsm_mrw2(cs, op), 0, 0, 0);
	acsm_put(cur, 0, 0, 0, ACSM_NOP_HW3);
	acsm_put(cur, 0, 0, 0, 0);
}

static void channel_commit(const struct devinit *d)
{
	csr_wr(0x2f0a0, 1);
	delay_dfi_clks(d, 0x20);
	csr_wr(0x2f0a0, 0);
}

/* Step 6.1 — ACSM dividers, one-shot NOP flush, PHY block disable. */
static int devinit_phase6_1(struct devinit *d)
{
	uint16_t cur;
	int rc;

	if (d->ver >= 8) {
		csr_wr(0x90808, (uint16_t)((csr_rd(0x20314) << 8) | 1));
		csr_wr(0x90813, (uint16_t)((csr_rd(0x20315) << 8) | 1));
		csr_wr(0x9080b, csr_rd(0x9080b) & 0xff9f);
		csr_wr(0x9080c, csr_rd(0x9080c) & 0xff9f);
	}
	acsm_reset_ptr(d, &cur);
	acsm_put(&cur, 0, 0, 0, 0);
	acsm_put(&cur, 0, 0, 0, 0);
	rc = acsm_oneshot(d, cur, 0x61);

	delay_dfi_clks(d, 0x1f);
	delay_dfi_clks(d, 4);
	csr_wr(0x200f8, 0);
	csr_wr(CSR_CLKSEL, (uint16_t)((csr_rd(CSR_CLKSEL) & 0xfffc) | 0x0002));
	csr_wr(0x2002a, 1);
	report_state(d, 0x61);
	return rc;
}

/* Step 6.2 — MemResetL release and the LPDDR5 MRW burst (values from the message block). */
static int devinit_phase6_2(struct devinit *d)
{
	static const uint8_t order[] = { 19, 18, 1, 2, 3, 10, 11, 17, 12, 14, 15, 16,
					 20, 22, 24, 37, 40, 41 };
	int fast = mb_rd8(0x001) & 0x02;	/* byte[0x80000001] bit 1 (question J1) */
	uint16_t cur;
	unsigned int i, r;
	int rc;

	if (!d->dram_init)
		return 0;
	csr_wr(CSR_ACSM_BURST + d->ps, 0);
	csr_wr(0x2002a, 1);
	delay_ps_or_clks(d, fast ? 2000000u : 200000000u, 0);		/* tINIT3 */
	csr_wr(CSR_MEMRESETL, 3);
	delay_ps_or_clks(d, fast ? 2000000u : 2000000000u, 0);		/* tINIT4/5 */
	if (MB_GET(Misc) & 0x01)
		csr_wr(CSR_MEMRESETL, 1);

	csr_wr(0x3f084, 0);
	csr_wr(BCAST_CSR(0x00a), 0);
	csr_wr(0x2002a, 1);
	csr_wr(CSR_ACSM_BURST + d->ps, 1);
	acsm_reset_ptr(d, &cur);
	acsm_put(&cur, 0, 0, 0, 0);
	acsm_put(&cur, 0, 0, 0, 0);
	for (i = 0; i < sizeof(order); i++) {
		unsigned int ma = order[i];
		uint32_t off = mr_offset(ma);
		int per_rank = ma == 17 || ma == 12 || ma == 14 || ma == 15 || ma == 24;

		if (!per_rank) {
			acsm_mrw(&cur, 3, ma, mb_rd8(off));
			continue;
		}
		for (r = 0; r < 2; r++)
			if (d->c->cs_a & (1u << r))
				acsm_mrw(&cur, 1u << r, ma, mb_rd8(off + r));
	}
	rc = acsm_oneshot(d, cur, 0x62);
	/* ZQ calibration MPC burst: entry encoding not specified (question J2) */
	csr_wr(CSR_ACSM_BURST + d->ps, 0);
	csr_wr(BCAST_CSR(0x00a), 0);
	channel_commit(d);
	report_state(d, 0x62);
	return rc;
}

/* Step 6.3 — post-DevInit lane state. */
static void devinit_phase6_3(struct devinit *d)
{
	unsigned int r;

	csr_wr(0x2007a, (uint16_t)((d->c->cs_a ? 1 : 0) | (d->c->cs_b ? 2 : 0)));
	csr_wr(0x2007e, (uint16_t)((d->c->cs_b << 2) | d->c->cs_a));
	csr_wr(0x20042, (uint16_t)(d->mult | 0x0100));
	csr_wr(0x20044, d->ratio == 2 ? 3 : 2);
	channel_commit(d);
	if (!MB_GET(Quickboot))
		for (r = 0; r < d->nranks; r++)
			csr_wr(RANK_CSR(r, 0x0a3), d->a3[r]);
	csr_wr(0x000a4, 1);
	report_state(d, 0x63);
}

enum train_status stage_devinit(struct train_ctx *c)
{
	struct devinit d = { .c = c };
	enum train_status st;
	uint16_t tca = 0;
	int flush_ok, mrw_ok;

	d.t0 = timer_now();
	d.pstate = mb_rd8(0x004);
	d.ps = d.pstate & 0x0f;
	d.ratio = MB_GET(DfiFreqRatio);
	d.shift = 1;
	d.dram_init = !MB_GET(Quickboot) && !(mb_rd8(0x000) & 0x80);
	d.nranks = (uint8_t)((c->cs_a & 1) + ((c->cs_a >> 1) & 1) +
			     (c->cs_b & 1) + ((c->cs_b >> 1) & 1));
	if (!d.nranks)
		return TRAIN_FAIL;
	if (d.nranks > MAX_RANKS)
		d.nranks = MAX_RANKS;

	log_words(0, LOG_ID_DEVINIT_INPUT, 11, mb_rd8(0x000), MB_GET(MsgMisc), d.pstate,
		  MB_GET(PllBypassEn), c->freq, d.ratio, MB_GET(Quickboot), c->cs_a, c->cs_b,
		  csr_rd(CSR_UCCLK), d.nranks);
	report_state(&d, 0x00);
	if (devinit_phase0(&d))
		return TRAIN_FAIL;
	devinit_phase1(&d);
	devinit_phase2(&d);
	report_state(&d, 0x02);
	st = devinit_phase3(&d, &tca);
	report_state(&d, 0x37);
	if (st != TRAIN_OK)
		return st;
	flush_ok = !devinit_phase6_1(&d);
	mrw_ok = !devinit_phase6_2(&d);
	devinit_phase6_3(&d);

	/* period latched (not the seed, not saturated) and both ACSM runs done */
	if ((tca & 0x01ff) != 0x0040 && (tca & 0x01ff) != 0x01ff && flush_ok && mrw_ok)
		return TRAIN_OK;
	return TRAIN_PARTIAL;
}
