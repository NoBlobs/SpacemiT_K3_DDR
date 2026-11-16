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
 *            3.6 per-rank DQ calibration (0x01<r>0ad, 0x01<r>0af),
 *            3.7 period latch and frequency buckets;
 *   phase 6  6.1 ACSM dividers and one-shot NOP flush, block disable;
 *            6.2 MemResetL release and the one-shot MRW burst;
 *            6.3 lane state, channel commit, 0x2006e = 0, restore of 0x20011,
 *                0x01<r>0a3, 0x20072/73 and 0x90808/13, 0x000a4.
 *            6.2 also sends the ZQ calibration MPC burst (variant 1).
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
	int dram_init;		/* Quickboot == 0 && MsgMisc[7] == 0 */
	uint16_t a3[MAX_RANKS];	/* DMEM 0x80000430 + 2r in the reference */
	uint16_t cal_ctrl;	/* 0x20011 saved in step 3.3 (DMEM 0x8000042a) */
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
	int branch = MB_GET(MsgMisc) & 0x01;	/* MsgMisc[0] */
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

/* Pulse bit 0 of 0x01r0ac. */
static void pulse_0ac(unsigned int r)
{
	uint16_t v = csr_rd(RANK_CSR(r, 0x0ac));

	csr_wr(RANK_CSR(r, 0x0ac), v | 1);
	csr_wr(RANK_CSR(r, 0x0ac), (uint16_t)(v & ~1u));
}

/* Gate pulse around a rank-calibration latch (r = 0xf: broadcast, read from rank 0). */
static void dq_gate_latch(struct devinit *d, unsigned int r, uint32_t gate, uint16_t gbit)
{
	unsigned int rr = r == 0xf ? 0 : r;

	csr_wr(RANK_CSR(r, gate), (uint16_t)(csr_rd(RANK_CSR(rr, gate)) | gbit));
	csr_wr(RANK_CSR(r, 0x00f) + d->ps,
	       (uint16_t)((csr_rd(RANK_CSR(rr, 0x00f) + d->ps) & ~3u) | 1u));
	delay_dfi_clks(d, ((uint32_t)(csr_rd(RANK_CSR(rr, 0x014) + d->ps) << 1) & 0x01feu) * 160u + 160u);
	csr_wr(RANK_CSR(r, gate), (uint16_t)(csr_rd(RANK_CSR(rr, gate)) & ~gbit));
	csr_wr(RANK_CSR(r, 0x00f) + d->ps, (uint16_t)(csr_rd(RANK_CSR(rr, 0x00f) + d->ps) & ~1u));
}

/*
 * Step 3.6 — per-rank DQ calibration (docsClean/07 8aa6257): one capture per
 * rank (sub-block 0, no lane loop), result in 0x01r0ad, 0x01r0d6 -> 0x01r0af.
 */
static void dq_rank_cal(struct devinit *d)
{
	uint16_t dd[5], d5, gbit;
	uint32_t gate;
	unsigned int r, k, sel;

	if (d->c->freq < 1600)
		return;
	if (d->ver >= 4) {
		gate = 0x057u + d->ps;
		gbit = 1;
	} else {
		gate = d->ver >= 2 ? 0x056u : 0x014u + d->ps;
		gbit = 2;
	}

	csr_wr(BCAST_CSR(0x0d9), 0x1000);
	csr_wr(BCAST_CSR(0x00f) + d->ps,
	       (uint16_t)((csr_rd(RANK_CSR(0, 0x00f) + d->ps) & 0xfff8) | 0x0006));
	csr_wr(BCAST_CSR(gate), (uint16_t)(csr_rd(RANK_CSR(0, gate)) & ~gbit));
	csr_wr(BCAST_CSR(0x0ac), (uint16_t)(csr_rd(RANK_CSR(0, 0x0ac)) & ~1u));
	csr_wr(BCAST_CSR(0x00f) + d->ps, (uint16_t)(csr_rd(RANK_CSR(0, 0x00f) + d->ps) | 0x0005));
	delay_dfi_clks(d, 0x3000);
	csr_wr(BCAST_CSR(0x00f) + d->ps,
	       (uint16_t)((csr_rd(RANK_CSR(0, 0x00f) + d->ps) & ~0x0005u) | 0x0004));

	for (r = 0; r < d->nranks; r++) {
		for (k = 0; k < 5; k++)
			dd[k] = csr_rd(RANK_CSR(r, 0x0d0 + k));
		pulse_0ac(r);
		d5 = csr_rd(RANK_CSR(r, 0x0d5));
		sel = dq_step_select(dd, d5);
		csr_wr(RANK_CSR(r, 0x0ad), (uint16_t)sel);
		dq_gate_latch(d, r, gate, gbit);
		pulse_0ac(r);
		csr_wr(RANK_CSR(r, 0x0af), csr_rd(RANK_CSR(r, 0x0d6)));
		log_words(0, LOG_ID_DEVINIT_DQCAL3, 7, r, d5,
			  ((uint32_t)dd[0] << 16) | dd[1], ((uint32_t)dd[2] << 16) | dd[3], dd[4],
			  csr_rd(RANK_CSR(r, 0x0ad)), csr_rd(RANK_CSR(r, 0x0af)));
	}
	dq_gate_latch(d, 0xf, gate, gbit);
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
		d->cal_ctrl = csr_rd(CSR_CAL_CTRL + d->ps);
		csr_wr(CSR_CAL_CTRL + d->ps, d->cal_ctrl & 0xff9f);
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

/* NOP pair with repeat count r (r + 1 units), docsClean/07 8aa6257. */
static void acsm_nop(uint16_t *cur, unsigned int r)
{
	while (r > 255) {
		acsm_put(cur, 0, 0, 0, acsm_nop_hw3(255));
		acsm_put(cur, 0, 0, 0, acsm_nop_hw7(255));
		r -= 255;
	}
	acsm_put(cur, 0, 0, 0, acsm_nop_hw3(r));
	acsm_put(cur, 0, 0, 0, acsm_nop_hw7(r));
}

/* One MRW (MRW-1, MRW-2) followed by the fixed NOP pair. */
static void acsm_mrw(uint16_t *cur, unsigned int cs, unsigned int ma, unsigned int op)
{
	acsm_put(cur, acsm_mrw1(cs, ma), 0, 0, 0);
	acsm_put(cur, acsm_mrw2(cs, op), 0, 0, 0);
	acsm_nop(cur, acsm_nop_count(17, 8));	/* fixed inter-MRW spacing (0x2b00) */
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
	acsm_nop(&cur, acsm_nop_count(4, 1));	/* 4-unit NOP flush */
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
	int fast = MB_GET(MsgMisc) & 0x02;	/* MsgMisc[1]: fast tINIT */
	uint16_t cur;
	unsigned int i, r;
	int rc, zq;

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
	acsm_nop(&cur, 0);			/* 1-unit NOP */
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

	/* ZQ calibration MPC burst (step 6.2.5) */
	acsm_reset_ptr(d, &cur);
	if (!(MB_GET(MsgMisc) & 0x08)) {
		/* variant 1 (byte[0x80000a80] == 1 is fixed in phase 2) */
		acsm_nop(&cur, acsm_nop_count(5, 1));
		acsm_put(&cur, acsm_mpc25_hw0(0x0f, 0x86), 0, 0, acsm_cs_hw3(0x0f));
		acsm_put(&cur, acsm_mpc26_hw0(0, 0x86), 0, 0, acsm_cs_hw3(0));
		acsm_nop(&cur, acsm_nop_count(16, 1));
		acsm_nop(&cur, acsm_nop_count(16, 1));
		acsm_nop(&cur, acsm_nop_count(1, 1));
		acsm_nop(&cur, acsm_nop_count(10, 1));
		zq = acsm_oneshot(d, cur, 0x65);
	} else {
		/* variant 0: single MPC (cmd 0x19) */
		acsm_put(&cur, (uint16_t)((3u << 14) | (0x06u << 7) | 0x40u | 0x30u), 0, 0, acsm_cs_hw3(3));
		acsm_put(&cur, 0, 0, 0, 0);
		zq = acsm_oneshot(d, cur, 0x65);
		delay_ps_or_clks(d, 30000, 0x1e);
	}
	if (zq)
		rc = zq;
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
	csr_wr(0x2006e, 0);
	if (!MB_GET(Quickboot)) {
		csr_wr(CSR_CAL_CTRL + d->ps, d->cal_ctrl);
		for (r = 0; r < d->nranks; r++)
			csr_wr(RANK_CSR(r, 0x0a3), d->a3[r]);
		csr_wr(0x20072, d->c->cs_a);
		csr_wr(0x20073, d->c->cs_b);
		if (d->ver >= 8) {
			csr_wr(0x90808, (uint16_t)((csr_rd(0x20314) << 8) | 1));
			csr_wr(0x90813, (uint16_t)((csr_rd(0x20315) << 8) | 1));
		}
	}
	csr_wr(0x000a4, 1);
	report_state(d, 0x63);
}

/*
 * Quickboot (Quickboot != 0) — docsClean/11 "quickboot PMU internal execution
 * sequence" (066c550), hardware part only. The host writes the saved phypara
 * before the PMU starts (Pass 1 and 2, and Pass 3 early, which docsClean
 * allows) and re-applies Pass 3 and 0x90808/13 after the PMU halts. No period
 * calibration, no step 3.6, no 0x01<r>0a3 mask, no tINIT wait.
 */
static int qb_mrw(struct devinit *d)
{
	static const uint8_t order[] = { 19, 18, 1, 2, 3, 10, 11, 17, 12, 14, 15, 16,
					 20, 22, 24, 37, 40, 41 };
	uint16_t cur;
	unsigned int i, r;
	int rc;

	csr_wr(0x3f084, 0);
	csr_wr(BCAST_CSR(0x00a), 0);
	csr_wr(0x2002a, 1);
	csr_wr(CSR_ACSM_BURST + d->ps, 1);
	acsm_reset_ptr(d, &cur);
	acsm_nop(&cur, 0);
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
	rc = acsm_oneshot(d, cur, 0x72);
	csr_wr(CSR_ACSM_BURST + d->ps, 0);
	csr_wr(BCAST_CSR(0x00a), 0);
	channel_commit(d);
	return rc;
}

/* docsClean/11 step 6 — hardware DLL / clock recalibration. */
static void qb_dll_recal(struct devinit *d, uint16_t r20009)
{
	uint16_t u = csr_rd(0x20049) & 0x01f0;

	delay_ps_or_clks(d, 0, 0x40);		/* multiplier off: 65 ticks */
	csr_wr(0x2f310, 1);
	csr_wr(0x2f0f8, 1);
	if (!MB_GET(PllBypassEn)) {
		csr_wr(0x2f049, u | 0x0400);
		delay_ps_or_clks(d, 1000u, 0);
		csr_wr(0x2f0e0, 1);
		delay_ps_or_clks(d, 1000000u, 0);
		csr_wr(0x2f049, u | 0x0200);
		csr_wr(0x2f0c3, 1);
		csr_wr(0x2f0e0, 0);
		csr_wr(0x2004e + d->ps, csr_rd(0x200cf) & 0x001f);
	} else {
		csr_wr(0x2f0c3, 1);
	}
	csr_wr(d->ver ? 0x9f02du : 0x9f028u, 1);	/* level, never cleared */
	csr_wr(0x2f009 + d->ps, r20009);
	log_words(0, LOG_ID_QB_DLL, 8, r20009, csr_rd(0x20049), csr_rd(0x200cf),
		  csr_rd(0x2004e + d->ps), csr_rd(RANK_CSR(0, 0x0ad)), csr_rd(RANK_CSR(0, 0x0af)),
		  csr_rd(CSR_TCA + d->ps), csr_rd(CSR_MEMRESETL));
}

static enum train_status quickboot_run(struct devinit *d)
{
	static const uint32_t qbpll_csr[7] = {
		0x20088, 0x20089, 0x2008a, 0x2008c, 0x2004a, 0x2004b, 0x2004c,
	};
	uint16_t r20009;
	unsigned int i;
	int flush_ok, mrw_ok;

	/* step 1: phase 0, commit strobe and canary (as DevInit), 0x20009 */
	if (devinit_phase0(d))
		return TRAIN_FAIL;
	d->shift = delay_shift(csr_rd(CSR_UCCLK));
	r20009 = csr_rd(0x20009);
	/*
	 * QBPll: the reference loads 0x20088..8c / 0x2004a..4c from message
	 * 0x130..0x13d; here the host already wrote the saved phypara values.
	 * Log both and keep the CSRs.
	 */
	for (i = 0; i < 7; i++)
		log_words(0, LOG_ID_PROBE_CSR, 4, qbpll_csr[i], csr_rd(qbpll_csr[i]),
			  0x58000u + ((MB_QBPllUPllProg0 + 2 * i) >> 1),	/* message halfword */
			  mb_rd16(MB_QBPllUPllProg0 + 2 * i));
	report_state(d, 0x71);

	/* step 4: startup script, 0x2008b, 0x20072/73 (no 0x01r0a3 mask), DQ block reset */
	csr_wr(0x2006e, 1);
	csr_wr(0x70060, 0);
	csr_wr(0x70011, 0);
	csr_wr(0x2008b, (d->pstate & 0x80) ? 0 : (d->pstate & 1));
	csr_wr(0x20072, d->c->cs_a);
	csr_wr(0x20073, d->c->cs_b);
	devinit_phase1(d);			/* reference clock, 0x200f8 = 1 */
	dq_block_reset(d);

	/* step 5: DRAM reset / PLL sequencing with lock wait, clock switch */
	dram_reset_pll(d);
	flush_ok = !devinit_phase6_1(d);	/* ACSM flush; 0x200f8 = 0, PLL clock */

	/* no tINIT; multiplier off before MRW */
	d->mult_en = 0;
	mrw_ok = !qb_mrw(d);
	csr_wr(0x2006e, 0);
	report_state(d, 0x75);

	/* step 6 */
	qb_dll_recal(d, r20009);
	commit_strobe();
	csr_wr(0x000a4, 1);
	report_state(d, 0x76);
	return flush_ok && mrw_ok ? TRAIN_OK : TRAIN_PARTIAL;
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
	d.dram_init = !MB_GET(Quickboot) && !(MB_GET(MsgMisc) & 0x80);
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
	if (MB_GET(Quickboot))
		return quickboot_run(&d);
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
	log_words(0, LOG_ID_DEVINIT_END, 5, csr_rd(0xc0086), csr_rd(CSR_MEMRESETL),
		  csr_rd(0x20011), csr_rd(0x2000a), csr_rd(0x20187));

	/* period latched (not the seed, not saturated) and both ACSM runs done */
	if ((tca & 0x01ff) != 0x0040 && (tca & 0x01ff) != 0x01ff && flush_ok && mrw_ok)
		return TRAIN_OK;
	return TRAIN_PARTIAL;
}
