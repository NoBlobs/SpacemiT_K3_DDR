// SPDX-License-Identifier: GPL-2.0-or-later
/* Copyright (C) 2026 Thalles Campagnani */
/* DevInit bucket and delay computations against docsClean/07 (0041e77). */
#include "check.h"
#include "train/devinit_calc.h"

int main(void)
{
	struct freq_buckets b;

	/* worked example: 6400 MT/s, ratio 4 -> 0x7353 / 0x0073 (f_hi = 6f) */
	CHECK(freq_buckets(6400, 4, &b) == 0);
	CHECK(b.f_lo == 1600 && b.f_hi == 9600);
	CHECK(b.r20012 == 0x7353);
	CHECK(b.r20186 == 0x0073);

	/* 4800 MT/s, ratio 4: f = 1200 (0x50, 3), f_hi = 4800 (7, 3) */
	CHECK(freq_buckets(4800, 4, &b) == 0);
	CHECK(b.r20012 == 0x7353 && b.r20186 == 0x0073);

	/* ratio 2: f = 400, f_lo = 800 (0x50, 3), f_hi = 2400 (7, 3) */
	CHECK(freq_buckets(800, 2, &b) == 0);
	CHECK(b.f_lo == 800 && b.f_hi == 2400);
	CHECK(b.r20012 == 0x7353 && b.r20186 == 0x0073);
	/* ratio 4, 1600 MT/s: f = 400 (0x30, 1), f_hi = 2400 (7, 3) */
	CHECK(freq_buckets(1600, 4, &b) == 0);
	CHECK(b.r20012 == 0x7331 && b.r20186 == 0x0073);

	/* table rows: boundaries are strict '>' */
	CHECK(freq_interval(331) == 0 && freq_interval(332) == 1);
	CHECK(freq_interval(531) == 1 && freq_interval(532) == 2);
	CHECK(freq_interval(799) == 2 && freq_interval(800) == 3);
	CHECK(freq_interval(1799) == 3 && freq_interval(1800) == 4);
	CHECK(freq_interval(2199) == 4 && freq_interval(2200) == 5);
	CHECK(freq_buckets(100, 0, &b) == -1);

	/* shift: 1 when 0xc0080 == 3, 0 when 7; delay(10) -> 5 or 11 */
	CHECK(delay_shift(3) == 1 && delay_shift(7) == 0);
	CHECK(delay_scaled(10, 1) == 5 && delay_scaled(10, 0) == 11);
	CHECK(delay_scaled(0x1f, 1) == 16);

	/* two-tier delay table of docsClean/07 at 6400/4, mult 8 */
	CHECK(clk_mult(6400, 4) == 8);
	CHECK(ps_or_clks(130000000u, 0, 6400, 4, 8, 1) == 104001);
	CHECK(ps_or_clks(1000000u, 0, 6400, 4, 8, 1) == 801);
	CHECK(ps_or_clks(500000u, 0, 6400, 4, 8, 1) == 401);
	CHECK(ps_or_clks(10000000u, 0, 6400, 4, 8, 1) == 8001);
	CHECK(ps_or_clks(0, 10000, 6400, 4, 8, 1) == 320000);
	CHECK(ps_or_clks(0, 10000, 6400, 4, 8, 0) == 10001);
	CHECK(ps_or_clks(0, 0x20, 6400, 4, 8, 1) == 1024);
	CHECK(ps_or_clks(2000000u, 0, 6400, 4, 8, 1) == 1601);
	CHECK(ps_or_clks(200000000u, 0, 6400, 4, 8, 1) == 160001);
	CHECK(ps_or_clks(2125, 4, 6400, 4, 8, 1) == 128);
	CHECK(delay_scaled(320000, 1) == 160000);

	/* ACSM MRW encoding: reference capture */
	CHECK(acsm_mrw1(3, 19) == 0xc9d8 && acsm_mrw2(3, 0x10) == 0xc808);
	CHECK(acsm_mrw1(3, 18) == 0xc958 && acsm_mrw2(3, 0x0d) == 0xc688);
	CHECK(acsm_mrw1(3, 1) == 0xc0d8 && acsm_mrw2(3, 0xb0) == 0xd848);
	CHECK(acsm_mrw1(3, 2) == 0xc158 && acsm_mrw2(3, 0xbb) == 0xddc8);
	CHECK(acsm_mrw1(3, 11) == 0xc5d8 && acsm_mrw2(3, 0x63) == 0xf188);
	CHECK(acsm_mrw1(1, 17) == 0x48d8 && acsm_mrw2(1, 0xae) == 0x5748);
	CHECK(acsm_mrw1(2, 17) == 0x88d8 && acsm_mrw2(2, 0xae) == 0x9748);

	CHECK_DONE("test_devinit_calc");
}
