/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright (C) 2026 Thalles Campagnani */
/*
 * Filtered streaming log. Our stream IDs are our own (documented in
 * docs/firmware/streaming-ids.rst); they are not the reference IDs.
 */
#ifndef COMMON_LOG_H
#define COMMON_LOG_H

#include <stdint.h>

/* Stream IDs 0x0800-0x0fff: outside the 0x0000-0x02ff reference range. */
#define LOG_ID_BOOT		0x0800	/* fw id, version, build */
#define LOG_ID_EXCEPTION	0x0801	/* ecr, eret, efa, sp */
#define LOG_ID_ASSERT		0x0802	/* line, value */
#define LOG_ID_STAGE		0x0803	/* stage bit, status */
#define LOG_ID_PROBE_BCR	0x0810	/* aux addr, value pairs */
#define LOG_ID_PROBE_RTC	0x0811	/* rtc ctrl, lo, hi, ... */
#define LOG_ID_PROBE_CSR	0x0812	/* index, value pairs */
#define LOG_ID_PROBE_DMEM	0x0813	/* arc addr, halfword pairs */
#define LOG_ID_PROBE_MEM	0x0814	/* memory test results */
#define LOG_ID_PROBE_ISA	0x0815	/* instruction self-test results */

void log_stream(uint8_t level, uint16_t id, const uint32_t *p, uint16_t len);
void log_words(uint8_t level, uint16_t id, int n, ...);
int log_enabled(uint8_t level);

#endif
