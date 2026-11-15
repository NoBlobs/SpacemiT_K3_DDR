/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright (C) 2026 Thalles Campagnani */
#ifndef HAL_TIMER_H
#define HAL_TIMER_H

#include <stdint.h>

/* RTC rate measured against the SPL timer: ~399.9 ticks/us (logsRefs/001). */
#define RTC_TICKS_PER_US	400u

void timer_init(void);
uint64_t timer_now(void);
/* Busy-wait for 'ticks' RTC ticks; < 16 uses PHY bus pacing (docsClean BR-06). */
void timer_delay_ticks(uint32_t ticks);

static inline void timer_delay_us(uint32_t us)
{
	timer_delay_ticks(us * RTC_TICKS_PER_US);
}

#endif
