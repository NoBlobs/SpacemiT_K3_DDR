// SPDX-License-Identifier: GPL-2.0-or-later
/* Copyright (C) 2026 Thalles Campagnani */
/*
 * Fatal exception path: keep a copy of the cause in our own .bss (readable by
 * the host at PHY index 0x5a000 + offset), stream it unconditionally and end
 * with major 0xff.
 */
#include <stdint.h>
#include "common/log.h"
#include "hal/mailbox.h"
#include "hal/bcrumb.h"

struct fw_diag {
	uint32_t magic;
	uint32_t ecr;
	uint32_t eret;
	uint32_t efa;
	uint32_t sp;
};

#define FW_DIAG_MAGIC	0x44494147u	/* "DIAG" */

volatile struct fw_diag fw_diag;

__attribute__((noreturn)) void fw_exception(uint32_t ecr, uint32_t eret,
					    uint32_t efa, uint32_t sp)
{
	uint32_t w[4] = { ecr, eret, efa, sp };

	bcrumb(BC_EXC_ECR_LO, (uint16_t)ecr);
	bcrumb(BC_EXC_ECR_HI, (uint16_t)(ecr >> 16));
	bcrumb(BC_EXC_RET_LO, (uint16_t)eret);
	bcrumb(BC_EXC_RET_HI, (uint16_t)(eret >> 16));
	fw_diag.ecr = ecr;
	fw_diag.eret = eret;
	fw_diag.efa = efa;
	fw_diag.sp = sp;
	fw_diag.magic = FW_DIAG_MAGIC;
	mbox_stream(LOG_ID_EXCEPTION, w, 4);
	fw_fatal();
}
