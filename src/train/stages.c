// SPDX-License-Identifier: GPL-2.0-or-later
/* Copyright (C) 2026 Thalles Campagnani */
/* Training stages not implemented yet. Each moves to its own file when done. */
#include "train/eye.h"
#include "train/train.h"

#define STUB(name) \
	enum train_status name(struct train_ctx *c) \
	{ \
		(void)c; \
		return TRAIN_NOT_IMPLEMENTED; \
	}

STUB(stage_wrlvl)
STUB(stage_rxen)
STUB(stage_wrdq)
STUB(stage_mxrdlat)
STUB(stage_dram_dca)
STUB(stage_phy_rddca)
STUB(stage_phy_wrdca)
STUB(stage_dram_dfe)
STUB(stage_lpca)
