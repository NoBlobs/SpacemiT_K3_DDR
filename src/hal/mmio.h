/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright (C) 2026 Thalles Campagnani */
/*
 * PMU-side memory access. DMEM shared with the host (0x8000xxxx) and PHY CSRs
 * (0x90xxxxxx) are 16-bit interfaces: only volatile 16-bit (or 8-bit for byte
 * fields) accesses are allowed (docsClean/02, /12, BR-03).
 */
#ifndef HAL_MMIO_H
#define HAL_MMIO_H

#include <stdint.h>

static inline uint16_t rd16(uint32_t addr)
{
	return *(volatile uint16_t *)addr;
}

static inline void wr16(uint32_t addr, uint16_t v)
{
	*(volatile uint16_t *)addr = v;
}

static inline uint8_t rd8(uint32_t addr)
{
	return *(volatile uint8_t *)addr;
}

static inline void wr8(uint32_t addr, uint8_t v)
{
	*(volatile uint8_t *)addr = v;
}

/* PHY CSR index -> ARC address: 0x90000000 | (index << 1) (docsClean/02) */
#define PHY_CSR_ARC(idx)	(0x90000000u | ((uint32_t)(idx) << 1))

/* Compose a CSR index from family base, rank, lane and offset with OR only. */
#define PHY_IDX(base, rank, lane, off) \
	((uint32_t)(base) | ((uint32_t)(rank) << 12) | ((uint32_t)(lane) << 8) | (uint32_t)(off))

static inline uint16_t csr_rd(uint32_t idx)
{
	return rd16(PHY_CSR_ARC(idx));
}

static inline void csr_wr(uint32_t idx, uint16_t v)
{
	wr16(PHY_CSR_ARC(idx), v);
}

/* Host DPHY index in the DMEM window -> ARC DMEM address */
#define DMEM_ARC(idx)		(0x80000000u + (((uint32_t)(idx) - 0x58000u) << 1))

#endif
