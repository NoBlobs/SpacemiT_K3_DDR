// SPDX-License-Identifier: GPL-2.0-or-later
/* Copyright (C) 2026 Thalles Campagnani */
/*
 * 64-bit free-running RTC in AUX 0x103-0x105 (docsClean/12, BR-06).
 * The tick rate is not documented; it is measured by the M1 probe.
 */
#include "arch/aux.h"
#include "hal/mmio.h"
#include "hal/timer.h"

#define RTC_ENABLE	0x1u
#define RTC_CLEAR	0x2u
#define PHY_PACE_ARC	0x90040048u	/* CSR 0x20024 dummy read */

void timer_init(void)
{
	if (!(aux_read(AUX_RTC_CTRL) & RTC_ENABLE)) {
		aux_write(AUX_RTC_CTRL, RTC_CLEAR);
		aux_write(AUX_RTC_CTRL, RTC_ENABLE);
	}
}

uint64_t timer_now(void)
{
	uint32_t hi, lo, hi2;

	/* Re-read until the high word is stable across the low read. */
	do {
		hi = aux_read(AUX_RTC_HIGH);
		lo = aux_read(AUX_RTC_LOW);
		hi2 = aux_read(AUX_RTC_HIGH);
	} while (hi != hi2);
	return ((uint64_t)hi << 32) | lo;
}

void timer_delay_ticks(uint32_t ticks)
{
	uint64_t end;

	if (ticks < 16) {
		while (ticks--)
			(void)rd16(PHY_PACE_ARC);
		return;
	}
	timer_init();
	end = timer_now() + ticks;
	while (timer_now() < end)
		;
}
