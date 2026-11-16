/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright (C) 2026 Thalles Campagnani */
/*
 * Training engine: candidate evaluation and eye construction, after the
 * three-level model of docsClean/07 ("Concrete three-level eye acceptance
 * model", a recommended design, not recovered reference behaviour).
 *
 *   level 1  one point (delay, vref): N transfers, PASS only with zero errors;
 *   level 2  longest contiguous PASS window on the delay axis and its centre,
 *            or the 2D point with the best weighted margin;
 *   level 3  margins min(c - L, R - c) and their minimum over a group.
 *
 * Pure code on an explicit context: no MMIO, no global state. A stage
 * supplies the transfer-and-compare callback. Scan boundaries count as
 * failing coordinates just outside the scan (our policy; docsClean/07 leaves
 * truncated eyes open), so a margin is never larger than what was observed.
 */
#ifndef TRAIN_EYE_H
#define TRAIN_EYE_H

#include <stdint.h>

#define EYE_MAX_DELAY	128	/* samples on the delay axis */
#define EYE_MAX_VREF	64	/* samples on the voltage axis */

/* One delay scan: bit i set = sample i passed. */
struct eye_row {
	uint64_t w[EYE_MAX_DELAY / 64];
};

/* Inclusive sample window; left > right means "no passing sample". */
struct eye_win {
	int16_t left, right;
};

/* A scan axis: sample i is the coordinate lo + i * step, i < n. */
struct eye_axis {
	uint16_t lo;
	uint8_t step;
	uint8_t n;
};

/*
 * One transfer of the training pattern at (delay, vref), both in axis
 * coordinates. Returns the number of bit errors, or a negative value to abort
 * the scan (hardware error).
 */
typedef int (*eye_eval_fn)(void *ctx, unsigned int delay, unsigned int vref);

static inline uint16_t eye_coord(const struct eye_axis *a, unsigned int i)
{
	return (uint16_t)(a->lo + i * a->step);
}

/* Transfers per point: base * 2^BitTimeControl[2:0]. */
static inline uint32_t eye_samples(uint32_t base, uint8_t bit_time_control)
{
	return base << (bit_time_control & 0x7);
}

static inline void eye_row_clear(struct eye_row *r)
{
	unsigned int i;

	for (i = 0; i < EYE_MAX_DELAY / 64; i++)
		r->w[i] = 0;
}

static inline void eye_row_set(struct eye_row *r, unsigned int i)
{
	if (i < EYE_MAX_DELAY)
		r->w[i >> 6] |= 1ull << (i & 63);
}

static inline int eye_row_get(const struct eye_row *r, unsigned int i)
{
	return i < EYE_MAX_DELAY && ((r->w[i >> 6] >> (i & 63)) & 1);
}

/* Level 1: 1 = PASS (zero errors in n transfers), 0 = FAIL, -1 = abort. */
static inline int eye_point(eye_eval_fn fn, void *ctx, unsigned int delay,
			    unsigned int vref, uint32_t n)
{
	uint32_t i;

	for (i = 0; i < n; i++) {
		int e = fn(ctx, delay, vref);

		if (e < 0)
			return -1;
		if (e)
			return 0;
	}
	return 1;
}

/* Scan the delay axis at one vref. Returns the number of PASS samples or -1. */
static inline int eye_scan_1d(eye_eval_fn fn, void *ctx, const struct eye_axis *a,
			      unsigned int vref, uint32_t n, struct eye_row *row)
{
	unsigned int i, cnt = a->n > EYE_MAX_DELAY ? EYE_MAX_DELAY : a->n;
	int pass = 0;

	eye_row_clear(row);
	for (i = 0; i < cnt; i++) {
		int r = eye_point(fn, ctx, eye_coord(a, i), vref, n);

		if (r < 0)
			return -1;
		if (r) {
			eye_row_set(row, i);
			pass++;
		}
	}
	return pass;
}

static inline int eye_win_width(struct eye_win w)
{
	return w.left > w.right ? 0 : w.right - w.left + 1;
}

/*
 * Level 2: longest contiguous PASS run among samples 0..n-1 (the first one
 * on a tie). Shorter runs, including isolated noise islands, are ignored.
 */
static inline struct eye_win eye_longest_run(const struct eye_row *r, unsigned int n)
{
	struct eye_win best = { 0, -1 };
	int start = -1;
	unsigned int i;

	if (n > EYE_MAX_DELAY)
		n = EYE_MAX_DELAY;
	for (i = 0; i <= n; i++) {
		int p = i < n && eye_row_get(r, i);

		if (p && start < 0)
			start = (int)i;
		if (!p && start >= 0) {
			if ((int)i - start > eye_win_width(best)) {
				best.left = (int16_t)start;
				best.right = (int16_t)(i - 1);
			}
			start = -1;
		}
	}
	return best;
}

/* Midpoint sample of a window (rounded down). */
static inline int eye_win_center(struct eye_win w)
{
	return (w.left + w.right) / 2;
}

/*
 * Margin of c inside w, in samples: distance to the nearest failing sample
 * (L = left - 1, R = right + 1). 0 if c is outside w.
 */
static inline int eye_margin(int c, struct eye_win w)
{
	int l, r;

	if (c < w.left || c > w.right)
		return 0;
	l = c - w.left + 1;
	r = w.right - c + 1;
	return l < r ? l : r;
}

/* Level 2 acceptance of a 1D window. */
static inline int eye_win_ok(struct eye_win w, unsigned int w_min)
{
	return eye_win_width(w) >= (int)w_min && eye_win_width(w) > 0;
}

/* A 2D scan: row[v] is the delay scan at voltage sample v. */
struct eye_2d {
	struct eye_row row[EYE_MAX_VREF];
	uint8_t nd, nv;
};

struct eye_pick {
	uint8_t found;
	uint8_t d, v;		/* selected sample indices */
	uint8_t dm, vm;		/* delay and voltage margins, in samples */
	uint32_t score;
};

/* Scan nv voltages x nd delays. Returns the number of PASS points or -1. */
static inline int eye_scan_2d(eye_eval_fn fn, void *ctx, const struct eye_axis *da,
			      const struct eye_axis *va, uint32_t n, struct eye_2d *e)
{
	unsigned int v;
	int total = 0;

	e->nd = da->n > EYE_MAX_DELAY ? EYE_MAX_DELAY : da->n;
	e->nv = va->n > EYE_MAX_VREF ? EYE_MAX_VREF : va->n;
	for (v = 0; v < e->nv; v++) {
		int r = eye_scan_1d(fn, ctx, da, eye_coord(va, v), n, &e->row[v]);

		if (r < 0)
			return -1;
		total += r;
	}
	return total;
}

/* Margins of point (d, v): distance to the nearest FAIL or scan boundary. */
static inline void eye_margins_2d(const struct eye_2d *e, unsigned int d, unsigned int v,
				  uint8_t *dm, uint8_t *vm)
{
	unsigned int l = 0, r = 0, u = 0, w = 0;

	while (d >= l + 1 && eye_row_get(&e->row[v], d - l - 1))
		l++;
	while (d + r + 1 < e->nd && eye_row_get(&e->row[v], d + r + 1))
		r++;
	while (v >= w + 1 && eye_row_get(&e->row[v - w - 1], d))
		w++;
	while (v + u + 1 < e->nv && eye_row_get(&e->row[v + u + 1], d))
		u++;
	*dm = (uint8_t)((l < r ? l : r) + 1);
	*vm = (uint8_t)((w < u ? w : u) + 1);
}

/*
 * Level 2 in 2D: the PASS point with the highest
 * delay_margin * wd + voltage_margin * wv (both weights 0 -> both 1).
 * Ties: larger smaller-margin, then lower v, then lower d.
 */
static inline struct eye_pick eye_pick_2d(const struct eye_2d *e, uint8_t wd, uint8_t wv)
{
	struct eye_pick best = { 0 };
	unsigned int d, v;

	if (!wd && !wv)
		wd = wv = 1;
	for (v = 0; v < e->nv; v++) {
		for (d = 0; d < e->nd; d++) {
			uint8_t dm, vm, lo, blo;
			uint32_t s;

			if (!eye_row_get(&e->row[v], d))
				continue;
			eye_margins_2d(e, d, v, &dm, &vm);
			s = (uint32_t)dm * wd + (uint32_t)vm * wv;
			lo = dm < vm ? dm : vm;
			blo = best.dm < best.vm ? best.dm : best.vm;
			if (best.found && (s < best.score || (s == best.score && lo <= blo)))
				continue;
			best.found = 1;
			best.d = (uint8_t)d;
			best.v = (uint8_t)v;
			best.dm = dm;
			best.vm = vm;
			best.score = s;
		}
	}
	return best;
}

/* Level 3: a group margin is the minimum over its lanes (one byte each). */
static inline uint8_t eye_group_margin(const uint8_t *m, unsigned int n)
{
	uint8_t g = 0xff;
	unsigned int i;

	for (i = 0; i < n; i++)
		if (m[i] < g)
			g = m[i];
	return n ? g : 0;
}

#endif
