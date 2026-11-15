/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright (C) 2026 Thalles Campagnani */
/*
 * LPDDR5 1D message block at ARC 0x80000000 (docsClean/05). Access only by
 * byte offset with 8/16-bit volatile accesses -- never as a C struct.
 */
#ifndef COMMON_MSGBLK_H
#define COMMON_MSGBLK_H

#include <stdint.h>
#include "hal/mmio.h"
#include "common/msgblk_fields.h"

#define MB_BASE		0x80000000u

static inline uint8_t mb_rd8(uint32_t off)
{
	return rd8(MB_BASE + off);
}

static inline void mb_wr8(uint32_t off, uint8_t v)
{
	wr8(MB_BASE + off, v);
}

static inline uint16_t mb_rd16(uint32_t off)
{
	return rd16(MB_BASE + off);
}

static inline void mb_wr16(uint32_t off, uint16_t v)
{
	wr16(MB_BASE + off, v);
}

#define MB_GET(name) \
	((MB_##name##_SZ == 2) ? mb_rd16(MB_##name) : mb_rd8(MB_##name))
#define MB_SET(name, v) do { \
	if (MB_##name##_SZ == 2) \
		mb_wr16(MB_##name, (uint16_t)(v)); \
	else \
		mb_wr8(MB_##name, (uint8_t)(v)); \
} while (0)

/* SequenceCtrl stage bits (docsClean/05) */
#define SEQ_DEVINIT	(1u << 0)
#define SEQ_WRLVL	(1u << 1)
#define SEQ_RXEN	(1u << 2)
#define SEQ_RDDQS	(1u << 3)
#define SEQ_WRDQ	(1u << 4)
#define SEQ_DRAM_DCA	(1u << 6)
#define SEQ_PHY_RDDCA	(1u << 7)
#define SEQ_PHY_WRDCA	(1u << 8)
#define SEQ_MXRDLAT	(1u << 9)
#define SEQ_DRAM_DFE	(1u << 10)
#define SEQ_LPCA	(1u << 12)
#define SEQ_RESERVED	((1u << 5) | (1u << 11) | (0x7u << 13))

/* MsgMisc bits */
#define MSGMISC_NO_STREAM	(1u << 4)

/* HdtCtrl levels: a stream message is sent when level < HdtCtrl */
#define HDT_DETAIL	0x05
#define HDT_COARSE	0x0a
#define HDT_STAGE	0xc8
#define HDT_ASSERT	0xc9

#endif
