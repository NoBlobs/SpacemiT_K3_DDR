/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright (C) 2026 Thalles Campagnani */
/*
 * Pure DevInit computations (no MMIO), shared with the native unit tests.
 * docsClean/07 (991bb0f): frequency buckets, the two-tier delay model and
 * the ACSM MRW encoding.
 */
#ifndef TRAIN_DEVINIT_CALC_H
#define TRAIN_DEVINIT_CALC_H

#include <stdint.h>

struct freq_buckets {
	uint16_t f_lo, f_hi;
	uint16_t r20012;	/* (bucket3 << 12) | (fine_hi << 8) | coarse | fine_lo */
	uint16_t r20186;	/* (bucket3 << 4) | fine_hi */
};

/* Number of the five thresholds strictly exceeded by x (0..5). */
static inline unsigned int freq_interval(uint32_t x)
{
	static const uint16_t thr[5] = { 0x014b, 0x0213, 0x031f, 0x0707, 0x0897 };
	unsigned int n = 0;

	while (n < 5 && x > thr[n])
		n++;
	return n;
}

static inline unsigned int freq_fine(unsigned int n)
{
	return n > 3 ? 3 : n;
}

/* Returns -1 when DfiFreqRatio is 0 (division impossible). */
static inline int freq_buckets(uint16_t dram_freq, uint8_t ratio, struct freq_buckets *b)
{
	uint32_t f, f_lo, f_hi;
	unsigned int n_lo, n_hi;

	if (!ratio)
		return -1;
	f = dram_freq / ratio;
	f_lo = ratio == 2 ? 2 * f : f;
	f_hi = f * 6u;
	n_lo = freq_interval(f_lo);
	n_hi = freq_interval(f_hi);
	b->f_lo = (uint16_t)(f_lo > 0xffff ? 0xffff : f_lo);
	b->f_hi = (uint16_t)(f_hi > 0xffff ? 0xffff : f_hi);
	b->r20012 = (uint16_t)(((n_hi + 2) << 12) | (freq_fine(n_hi) << 8) |
			       ((n_lo + 2) << 4) | freq_fine(n_lo));
	b->r20186 = (uint16_t)(((n_hi + 2) << 4) | freq_fine(n_hi));
	return 0;
}

/* delay_dfi_clks(n): rtc_ticks = (n + 1) >> shift, shift = 1 & ~(0xc0080 >> 2). */
static inline uint8_t delay_shift(uint16_t ucclk)
{
	return (uint8_t)(1u & ~((unsigned int)ucclk >> 2));
}

static inline uint32_t delay_scaled(uint32_t n, uint8_t shift)
{
	return (n + 1) >> shift;
}


/* byte[0x80000b68] = ceil(DRAMFreq / (DfiFreqRatio * 220)) (8 at 6400/4). */
static inline uint8_t clk_mult(uint16_t dram_freq, uint8_t ratio)
{
	uint32_t d = (uint32_t)ratio * 220u;

	return d ? (uint8_t)((dram_freq + d - 1) / d) : 0;
}

/*
 * delay_ps_or_clks(ps, min_clks) -> DFI clocks for delay_dfi_clks():
 * dfi = ((ps * DRAMFreq / 2e6) + ratio + 1) / ratio; floor c = min * mult * 4
 * when the multiplier is enabled (byte[0x80000a80] = 1), else min + 1.
 * 32-bit only: ps above 200,000 is converted in ns to avoid overflow.
 */
static inline uint32_t ps_or_clks(uint32_t ps, uint32_t min_clks, uint16_t dram_freq,
				  uint8_t ratio, uint8_t mult, int mult_en)
{
	uint32_t v, dfi, c;

	if (!ratio)
		return min_clks + 1;
	if (ps <= 200000u)
		v = ps * dram_freq / 2000000u;
	else
		v = (ps / 1000u) * dram_freq / 2000u;
	dfi = (v + ratio + 1u) / ratio;
	c = (mult_en && mult) ? min_clks * mult * 4u : min_clks + 1u;
	return dfi > c ? dfi : c;
}

/* ACSM LPDDR5 MRW entries (hw[0] of MRW-1 and MRW-2). */
static inline uint16_t acsm_mrw1(unsigned int cs, unsigned int ma)
{
	return (uint16_t)(((cs & 3u) << 14) | ((ma & 0x7fu) << 7) | 0x0058u);
}

static inline uint16_t acsm_mrw2(unsigned int cs, unsigned int op)
{
	return (uint16_t)(((cs & 3u) << 14) | ((op & 0x7fu) << 7) | ((op & 0x80u) >> 1) | 0x0008u);
}

/* ACSM NOP pair (8 halfwords): repeat count r = ceil(cycles / div) - 1. */
static inline unsigned int acsm_nop_count(unsigned int cycles, unsigned int div)
{
	return (cycles + div - 1u) / div - 1u;
}

static inline uint16_t acsm_nop_hw3(unsigned int r)
{
	return r ? (uint16_t)(((r & 7u) << 12) | 0x0b00u) : 0;
}

static inline uint16_t acsm_nop_hw7(unsigned int r)
{
	return (uint16_t)((r & 0xf8u) << 7);
}

/* ZQ MPC, variant 1: first half (cmd 0x25) and second half (cmd 0x26). */
static inline uint16_t acsm_mpc25_hw0(unsigned int cs, unsigned int op)
{
	unsigned int f = ((op >> 1) & 0x40u) | 0x30u;

	return (uint16_t)(((cs & 3u) << 14) | (f << 7) | f);
}

static inline uint16_t acsm_cs_hw3(unsigned int cs)
{
	return (cs & 2u) ? 0x0010u : 0;
}

static inline uint16_t acsm_mpc26_hw0(unsigned int cs, unsigned int op)
{
	return (uint16_t)(((cs & 3u) << 14) | ((op & 0x7fu) << 7) | (op & 0x7fu));
}

/*
 * Step 3.6 step selection: first k in 0..4 with 2*d[k] >= d5 and
 * |d[k+1] - d[k] - d5| <= d5*14/100, where d[5] = 0x3ff; else 5.
 */
static inline unsigned int dq_step_select(const uint16_t d[5], uint16_t d5)
{
	unsigned int k, tol = d5 * 14u / 100u;

	for (k = 0; k < 5; k++) {
		int next = k < 4 ? d[k + 1] : 0x3ff;
		int diff = next - (int)d[k] - (int)d5;

		if (diff < 0)
			diff = -diff;
		if (2u * d[k] >= d5 && (unsigned int)diff <= tol)
			return k;
	}
	return 5;
}

#endif
