/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright (C) 2026 Thalles Campagnani */
#ifndef TRAIN_TRAIN_H
#define TRAIN_TRAIN_H

#include <stdint.h>

/* Explicit training context (docsClean/12: no state inherited implicitly). */
struct train_ctx {
	uint16_t seq;
	uint16_t freq;
	uint8_t cs_a, cs_b;
	uint8_t dq_a, dq_b;
	uint8_t x8;
};

enum train_status { TRAIN_OK = 0, TRAIN_FAIL, TRAIN_NOT_IMPLEMENTED };

enum train_status stage_devinit(struct train_ctx *c);
enum train_status stage_wrlvl(struct train_ctx *c);
enum train_status stage_rxen(struct train_ctx *c);
enum train_status stage_rddqs(struct train_ctx *c);
enum train_status stage_wrdq(struct train_ctx *c);
enum train_status stage_mxrdlat(struct train_ctx *c);
enum train_status stage_dram_dca(struct train_ctx *c);
enum train_status stage_phy_rddca(struct train_ctx *c);
enum train_status stage_phy_wrdca(struct train_ctx *c);
enum train_status stage_dram_dfe(struct train_ctx *c);
enum train_status stage_lpca(struct train_ctx *c);

#endif
