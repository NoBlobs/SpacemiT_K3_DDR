// SPDX-License-Identifier: GPL-2.0-or-later
/* Copyright (C) 2026 Thalles Campagnani */
/*
 * Full-training firmware entry: validates the message block and runs the
 * stages enabled in SequenceCtrl in dependency order (docsClean/07).
 *
 * No stage is implemented yet: every enabled stage reports "not implemented"
 * and the firmware ends with 0xff, so the host never activates a DDRC on an
 * untrained PHY.
 */
#include <stdint.h>
#include "common/log.h"
#include "common/msgblk.h"
#include "hal/mailbox.h"
#include "hal/timer.h"
#include "train/train.h"

#define FW_ID_TRAIN	0x5452414eu	/* "TRAN" */
#define TRAIN_VERSION	0x0001u		/* written to PmuRevision */

/* Our own progress majors; the host logs unknown majors as progress. */
#define MAJOR_TRAIN_START	0x30
#define MAJOR_STAGE_BASE	0x40	/* + SequenceCtrl bit number */

static const struct {
	uint16_t bit;
	enum train_status (*run)(struct train_ctx *);
} stages[] = {
	{ 0, stage_devinit },
	{ 1, stage_wrlvl },
	{ 2, stage_rxen },
	{ 3, stage_rddqs },
	{ 4, stage_wrdq },
	{ 9, stage_mxrdlat },
	{ 6, stage_dram_dca },
	{ 7, stage_phy_rddca },
	{ 8, stage_phy_wrdca },
	{ 10, stage_dram_dfe },
	{ 12, stage_lpca },
};

static int validate(struct train_ctx *c)
{
	uint16_t freq = MB_GET(DRAMFreq);
	uint8_t x8 = MB_GET(X8Mode);

	c->seq = MB_GET(SequenceCtrl);
	c->cs_a = MB_GET(CsPresentChA);
	c->cs_b = MB_GET(CsPresentChB);
	c->dq_a = MB_GET(EnabledDQsChA);
	c->dq_b = MB_GET(EnabledDQsChB);
	c->x8 = x8;
	c->freq = freq;
	c->pstate = MB_GET(Pstate);

	if (!(c->seq & SEQ_DEVINIT) || (c->seq & SEQ_RESERVED))
		return 1;
	if (freq != 4800 && freq != 6400)
		return 2;
	if (c->cs_a != 1 && c->cs_a != 3)
		return 3;
	if (c->cs_b > 3 || c->cs_b == 2)
		return 4;
	if (x8 != 0x0 && x8 != 0xa && x8 != 0xf)
		return 5;
	return 0;
}

int main(void)
{
	struct train_ctx ctx = { 0 };
	uint32_t boot[3] = { FW_ID_TRAIN, TRAIN_VERSION, 0 };
	unsigned int i;
	uint32_t incomplete = 0;
	int err;

	timer_init();
	mbox_major(MAJOR_TRAIN_START);
	log_stream(HDT_ASSERT - 1, LOG_ID_BOOT, boot, 3);
	MB_SET(PmuRevision, TRAIN_VERSION);

	err = validate(&ctx);
	if (err) {
		log_words(0, LOG_ID_ASSERT, 2, (uint32_t)__LINE__, (uint32_t)err);
		fw_fatal();
	}

	/*
	 * Every enabled stage runs in order. A stage that is not (fully)
	 * implemented is reported and the run continues, so experiments see
	 * the effect of the implemented parts; the run then ends with 0xff so
	 * the host never activates the DDRC on an incompletely trained PHY.
	 */
	for (i = 0; i < sizeof(stages) / sizeof(stages[0]); i++) {
		enum train_status st;

		if (!(ctx.seq & (1u << stages[i].bit)))
			continue;
		mbox_major(MAJOR_STAGE_BASE + stages[i].bit);
		st = stages[i].run(&ctx);
		log_words(0, LOG_ID_STAGE, 2, stages[i].bit, st);
		if (st == TRAIN_FAIL)
			fw_fatal();
		if (st != TRAIN_OK)
			incomplete |= 1u << stages[i].bit;
	}
	if (incomplete) {
		log_words(0, LOG_ID_ASSERT, 2, (uint32_t)__LINE__, incomplete);
		fw_fatal();
	}
	fw_done();
}
