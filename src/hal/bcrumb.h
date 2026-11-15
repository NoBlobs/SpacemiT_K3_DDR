/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright (C) 2026 Thalles Campagnani */
/*
 * Bring-up breadcrumbs: 32 halfword slots at ARC 0x80003f00 (host PHY index
 * 0x59f80-0x59f9f), in the range the host clears before release and that
 * nothing else uses. The SPL dumps them when a PMU run times out.
 */
#ifndef HAL_BCRUMB_H
#define HAL_BCRUMB_H

#include "hal/mmio.h"

#define BCRUMB_BASE	0x80003f00u
#define BCRUMB_SLOTS	32

/* slot assignment */
#define BC_START	0	/* 0xc0de written by _start */
#define BC_MAIN		1	/* 0x0001 main entered */
#define BC_MBOX_ST0	2	/* status seen before first send */
#define BC_MBOX_SENT	3	/* number of items fully sent */
#define BC_MBOX_WAIT	4	/* 1 waiting free, 2 waiting ack */
#define BC_MBOX_ST1	5	/* last status read while waiting */
#define BC_EXC		6	/* 0xdead in exception path */
#define BC_EXC_ECR_LO	7
#define BC_EXC_ECR_HI	8
#define BC_EXC_RET_LO	9
#define BC_EXC_RET_HI	10

static inline void bcrumb(unsigned slot, uint16_t v)
{
	wr16(BCRUMB_BASE + 2 * slot, v);
}

#endif
