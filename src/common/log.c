// SPDX-License-Identifier: GPL-2.0-or-later
/* Copyright (C) 2026 Thalles Campagnani */
#include <stdarg.h>
#include "common/log.h"
#include "common/msgblk.h"
#include "hal/mailbox.h"

#define LOG_MAX_ARGS	31

int log_enabled(uint8_t level)
{
	if (MB_GET(MsgMisc) & MSGMISC_NO_STREAM)
		return 0;
	return level < MB_GET(HdtCtrl);
}

void log_stream(uint8_t level, uint16_t id, const uint32_t *p, uint16_t len)
{
	if (log_enabled(level))
		mbox_stream(id, p, len);
}

void log_words(uint8_t level, uint16_t id, int n, ...)
{
	uint32_t buf[LOG_MAX_ARGS];
	va_list ap;
	int i;

	if (!log_enabled(level))
		return;
	if (n > LOG_MAX_ARGS)
		n = LOG_MAX_ARGS;
	va_start(ap, n);
	for (i = 0; i < n; i++)
		buf[i] = va_arg(ap, uint32_t);
	va_end(ap);
	mbox_stream(id, buf, (uint16_t)n);
}
