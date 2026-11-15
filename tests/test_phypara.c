// SPDX-License-Identifier: GPL-2.0-or-later
/* Copyright (C) 2026 Thalles Campagnani */
/* phypara order vs. docsClean/02 and /09 statements. */
#include <stdint.h>
#include "check.h"

#define PHYPARA_NSEG 69
#define PHYPARA_COUNT 5170
extern const uint16_t phypara_seg_start[PHYPARA_NSEG + 1];
extern const uint32_t phypara_order[PHYPARA_COUNT];

int main(void)
{
	unsigned i, j, dup = 0;

	CHECK(phypara_seg_start[PHYPARA_NSEG] == PHYPARA_COUNT);
	/* segment 48: 1536 sequential halfwords 0x9002e-0x9062d */
	CHECK(phypara_seg_start[49] - phypara_seg_start[48] == 1536);
	CHECK(phypara_order[phypara_seg_start[48]] == 0x9002e);
	CHECK(phypara_order[phypara_seg_start[49] - 1] == 0x9062d);
	CHECK(phypara_order[phypara_seg_start[54]] == 0x20124);
	CHECK(phypara_order[phypara_seg_start[55]] == 0x2018b);
	/* docsClean/02: with start offsets, all 5170 indices are unique */
	for (i = 0; i < PHYPARA_COUNT; i++)
		for (j = i + 1; j < PHYPARA_COUNT; j++)
			dup += phypara_order[i] == phypara_order[j];
	CHECK(dup == 0);
	if (dup)
		fprintf(stderr, "%u duplicated indices\n", dup);
	CHECK_DONE("test_phypara");
}
