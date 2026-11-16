// SPDX-License-Identifier: GPL-2.0-or-later
/* Copyright (C) 2026 Thalles Campagnani */
/* Training engine (src/train/eye.h) on synthetic eyes. */
#include "check.h"
#include "train/eye.h"

/* Synthetic eye: an ellipse in (delay, vref) plus optional noise points. */
struct synth {
	int cd, cv, rd, rv;	/* centre and radii */
	int noise_d, noise_v;	/* one isolated PASS point outside (-1: none) */
	int flaky_d;		/* delay that fails on the 3rd transfer (-1: none) */
	int abort_d;		/* delay that reports a hardware error (-1: none) */
	unsigned int calls;
};

static int synth_eval(void *ctx, unsigned int d, unsigned int v)
{
	struct synth *s = ctx;
	long dx = (long)d - s->cd, dy = (long)v - s->cv;

	s->calls++;
	if ((int)d == s->abort_d)
		return -1;
	if ((int)d == s->flaky_d && s->calls % 3 == 0)
		return 1;
	if ((int)d == s->noise_d && (int)v == s->noise_v)
		return 0;
	/* inside the ellipse: zero errors */
	if (dx * dx * s->rv * s->rv + dy * dy * s->rd * s->rd <= (long)s->rd * s->rd * s->rv * s->rv)
		return 0;
	return 5;
}

int main(void)
{
	struct synth s = { 62, 30, 20, 12, -1, -1, -1, -1, 0 };
	struct eye_axis da = { 0, 1, 128 }, va = { 0, 1, 64 };
	struct eye_axis coarse = { 10, 4, 30 };
	static struct eye_2d e;
	struct eye_row row;
	struct eye_win w;
	struct eye_pick p;
	uint8_t m[4] = { 9, 4, 7, 12 };

	CHECK(eye_samples(64, 0) == 64 && eye_samples(64, 3) == 512);
	CHECK(eye_samples(64, 0xf8) == 64);	/* only bits [2:0] */
	CHECK(eye_coord(&coarse, 3) == 22);

	/* level 1 */
	CHECK(eye_point(synth_eval, &s, 62, 30, 8) == 1);
	CHECK(eye_point(synth_eval, &s, 10, 30, 8) == 0);
	s.abort_d = 62;
	CHECK(eye_point(synth_eval, &s, 62, 30, 8) == -1);
	s.abort_d = -1;

	/* level 2, 1D at the centre voltage: window 42..82, centre 62 */
	CHECK(eye_scan_1d(synth_eval, &s, &da, 30, 4, &row) == 41);
	w = eye_longest_run(&row, da.n);
	CHECK(w.left == 42 && w.right == 82 && eye_win_width(w) == 41);
	CHECK(eye_win_center(w) == 62);
	CHECK(eye_margin(62, w) == 21);		/* L = 41, R = 83 */
	CHECK(eye_margin(42, w) == 1 && eye_margin(82, w) == 1);
	CHECK(eye_margin(41, w) == 0 && eye_margin(100, w) == 0);
	CHECK(eye_win_ok(w, 8) && !eye_win_ok(w, 42));

	/* an isolated PASS island outside the eye is ignored */
	s.noise_d = 100;
	s.noise_v = 30;
	CHECK(eye_scan_1d(synth_eval, &s, &da, 30, 4, &row) == 42);
	w = eye_longest_run(&row, da.n);
	CHECK(w.left == 42 && w.right == 82);
	s.noise_d = -1;

	/* a point that fails only sometimes splits the window */
	s.flaky_d = 50;
	s.calls = 0;
	CHECK(eye_scan_1d(synth_eval, &s, &da, 30, 8, &row) == 40);
	w = eye_longest_run(&row, da.n);
	CHECK(w.left == 51 && w.right == 82);
	s.flaky_d = -1;

	/* hardware error aborts the scan */
	s.abort_d = 70;
	CHECK(eye_scan_1d(synth_eval, &s, &da, 30, 4, &row) == -1);
	s.abort_d = -1;

	/* no eye, eye at the scan edge, full row */
	CHECK(eye_scan_1d(synth_eval, &s, &da, 60, 4, &row) == 0);
	w = eye_longest_run(&row, da.n);
	CHECK(eye_win_width(w) == 0 && !eye_win_ok(w, 0));
	s.cd = 5;
	CHECK(eye_scan_1d(synth_eval, &s, &da, 30, 4, &row) == 26);
	w = eye_longest_run(&row, da.n);
	CHECK(w.left == 0 && w.right == 25);
	CHECK(eye_margin(0, w) == 1);		/* boundary counts as failing */
	s.cd = 62;
	eye_row_clear(&row);
	for (unsigned int i = 0; i < 128; i++)
		eye_row_set(&row, i);
	w = eye_longest_run(&row, 128);
	CHECK(w.left == 0 && w.right == 127 && eye_margin(64, w) == 64);

	/* coarse axis: samples 10, 14, ..., 126; window in sample indices */
	CHECK(eye_scan_1d(synth_eval, &s, &coarse, 30, 4, &row) == 11);
	w = eye_longest_run(&row, coarse.n);
	CHECK(eye_coord(&coarse, (unsigned int)w.left) == 42);
	CHECK(eye_coord(&coarse, (unsigned int)w.right) == 82);
	CHECK(eye_coord(&coarse, (unsigned int)eye_win_center(w)) == 62);

	/* level 2, 2D: the ellipse centre wins with equal weights */
	CHECK(eye_scan_2d(synth_eval, &s, &da, &va, 2, &e) > 700);
	p = eye_pick_2d(&e, 0, 0);
	CHECK(p.found && p.d == 62 && p.v == 30);
	CHECK(p.dm == 21 && p.vm == 13 && p.score == 34);
	/* delay-only weight: the widest row; voltage-only: the tallest column */
	p = eye_pick_2d(&e, 1, 0);
	CHECK(p.found && p.d == 62 && p.dm == 21 && p.score == 21);
	p = eye_pick_2d(&e, 0, 3);
	CHECK(p.found && p.v == 30 && p.vm == 13 && p.score == 39);
	/* 2D margins at an off-centre point */
	{
		uint8_t dm, vm;

		eye_margins_2d(&e, 80, 30, &dm, &vm);
		CHECK(dm == 3 && vm >= 1 && vm < 13);
	}
	/* an eye cut by the voltage boundary: margin limited by the boundary */
	s.cv = 2;
	CHECK(eye_scan_2d(synth_eval, &s, &da, &va, 2, &e) > 0);
	p = eye_pick_2d(&e, 0, 1);
	CHECK(p.found && p.vm <= 8);
	s.cv = 30;
	/* empty scan */
	s.rd = 0;
	s.rv = 1;
	s.cd = 200;
	CHECK(eye_scan_2d(synth_eval, &s, &da, &va, 2, &e) == 0);
	p = eye_pick_2d(&e, 1, 1);
	CHECK(!p.found);

	/* level 3 */
	CHECK(eye_group_margin(m, 4) == 4 && eye_group_margin(m, 1) == 9);
	CHECK(eye_group_margin(m, 0) == 0);

	CHECK_DONE("test_eye");
}
