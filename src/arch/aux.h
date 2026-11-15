/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright (C) 2026 Thalles Campagnani */
/* ARCv2 auxiliary registers -- accessed only with lr/sr (docsClean/12). */
#ifndef ARCH_AUX_H
#define ARCH_AUX_H

#include <stdint.h>

#define AUX_IDENTITY	0x004
#define AUX_STATUS32	0x00a
#define AUX_RTC_CTRL	0x103
#define AUX_RTC_LOW	0x104
#define AUX_RTC_HIGH	0x105
#define AUX_ERET	0x400
#define AUX_ECR		0x403
#define AUX_EFA		0x404

/* Build configuration registers (public ARCv2 ISA) */
#define AUX_BCR_VER	0x060
#define AUX_BCR_FIRST	0x060
#define AUX_BCR_LAST	0x07f
#define AUX_BCR2_FIRST	0x0c0
#define AUX_BCR2_LAST	0x0ff

#define aux_read(reg)		__builtin_arc_lr(reg)
#define aux_write(reg, val)	__builtin_arc_sr((uint32_t)(val), reg)

#endif
