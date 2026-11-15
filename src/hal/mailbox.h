/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright (C) 2026 Thalles Campagnani */
#ifndef HAL_MAILBOX_H
#define HAL_MAILBOX_H

#include <stdint.h>

/* Major codes with a host-side meaning (docsClean/08) */
#define MBOX_MAJOR_DONE		0x07
#define MBOX_MAJOR_STREAM	0x08
#define MBOX_MAJOR_FATAL	0xff

void mbox_major(uint16_t code);
void mbox_word(uint32_t w);

/* Streaming: header (id << 16) | len, then len payload words. */
void mbox_stream(uint16_t id, const uint32_t *payload, uint16_t len);

/* Terminal paths: never return. */
__attribute__((noreturn)) void fw_done(void);
__attribute__((noreturn)) void fw_fatal(void);

#endif
