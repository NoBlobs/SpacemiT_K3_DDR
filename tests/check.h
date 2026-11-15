/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright (C) 2026 Thalles Campagnani */
#ifndef TESTS_CHECK_H
#define TESTS_CHECK_H
#include <stdio.h>
#include <stdlib.h>

static int check_failures;
#define CHECK(cond) do { \
	if (!(cond)) { \
		fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
		check_failures++; \
	} \
} while (0)
#define CHECK_DONE(name) do { \
	if (check_failures) { fprintf(stderr, "%s: %d failure(s)\n", name, check_failures); return 1; } \
	printf("%s: ok\n", name); return 0; \
} while (0)
#endif
