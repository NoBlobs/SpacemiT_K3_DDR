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
#define LOG_ID_CSRDUMP		0x0820	/* phypara/ACSM read-out summary */
#define LOG_ID_CSRSCAN		0x0821	/* first index, count, packed halfwords */
#define LOG_ID_PLLPROBE		0x0822	/* tag, time us, packed register values */
/* 0x0830-0x0833: DevInit per docsClean b03a788 (experiment 006, retired) */
#define LOG_ID_DEVINIT_CAL	0x0830	/* clock-period calibration values */
#define LOG_ID_DEVINIT_STATE	0x0831	/* 0xc0086, 0x100a3, 0x20060 */
#define LOG_ID_DEVINIT_WATCH	0x0832	/* ticks, 0x300bf, 0x100e0, 0x100d5 */
#define LOG_ID_DEVINIT_DMEM	0x0833	/* (ARC address, halfword) pairs */
/* 0x0834-0x0837: DevInit per docsClean 0041e77 */
#define LOG_ID_DEVINIT_INPUT	0x0834	/* message-block inputs, 0xc0080, ranks */
#define LOG_ID_DEVINIT_ACSM	0x0835	/* tag, poll iterations, final busy */
#define LOG_ID_DEVINIT_CAL2	0x0836	/* period latch and frequency buckets */
#define LOG_ID_DEVINIT_STATE2	0x0837	/* tag, us, key DevInit registers */
#define LOG_ID_DEVINIT_LATCH	0x0838	/* tag, wait (exp 007) or 0x200a2 (exp 008), e3, e5, counters */
#define LOG_ID_DEVINIT_REGS	0x0839	/* tag, us, step 3.5 registers (experiment 007) */
#define LOG_ID_DEVINIT_PHASE0	0x083a	/* canary bytes/registers, multiplier, 0x8000040c */
#define LOG_ID_DEVINIT_DESC	0x083b	/* index, csr, value, replaced */
#define LOG_ID_DEVINIT_DQCAL	0x083c	/* rank, k, d5 before, lane 0 d0..d4, d5 now */
#define LOG_ID_DEVINIT_ACSMTRY	0x083d	/* tag, busy before/after start/after poll, polls, after stop */
#define LOG_ID_DEVINIT_DQCAL2	0x083e	/* rank, d5, k per lane (3 bits each), fallbacks, 0x01r0ad */
#define LOG_ID_DEVINIT_PHASE0B	0x083f	/* version, canary DMEM, 0xc00ee, branch, read-back, mult, pair */

void log_stream(uint8_t level, uint16_t id, const uint32_t *p, uint16_t len);
void log_words(uint8_t level, uint16_t id, int n, ...);
int log_enabled(uint8_t level);

#endif
