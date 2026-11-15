// SPDX-License-Identifier: GPL-2.0-or-later
/* Copyright (C) 2026 Thalles Campagnani */
/* The firmware's compact phypara table expands to the flat appendix order. */
#include <stdint.h>
#include "check.h"
#include "common/phypara.h"

extern const uint32_t phypara_order[PHYPARA_COUNT];

int main(void)
{
	uint32_t k = 0, s, j;

	for (s = 0; s < PHYPARA_NSEG; s++)
		for (j = 0; j < phypara_segs[s].count; j++, k++)
			if (k < PHYPARA_COUNT)
				CHECK(phypara_index(&phypara_segs[s], (uint16_t)j) == phypara_order[k]);
	CHECK(k == PHYPARA_COUNT);
	CHECK_DONE("test_phypara_fw");
}
