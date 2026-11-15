// SPDX-License-Identifier: GPL-2.0-or-later
/* Copyright (C) 2026 Thalles Campagnani */
/*
 * Global replacements for libgcc helpers that hang on the K3 PMU core
 * (docsClean/12, /13 BR-07). Built WITHOUT -flto. Uses only hardware
 * mpyu/mpymu/divu/divs/remu/rems.
 */
#include <stdint.h>

#define USED __attribute__((used, noinline))

USED uint32_t __udivsi3(uint32_t a, uint32_t b)
{
	uint32_t q;
	__asm__ ("divu %0, %1, %2" : "=r"(q) : "r"(a), "r"(b));
	return q;
}

USED int32_t __divsi3(int32_t a, int32_t b)
{
	int32_t q;
	__asm__ ("div %0, %1, %2" : "=r"(q) : "r"(a), "r"(b));
	return q;
}

USED uint32_t __umodsi3(uint32_t a, uint32_t b)
{
	uint32_t r;
	__asm__ ("remu %0, %1, %2" : "=r"(r) : "r"(a), "r"(b));
	return r;
}

USED int32_t __modsi3(int32_t a, int32_t b)
{
	int32_t r;
	__asm__ ("rem %0, %1, %2" : "=r"(r) : "r"(a), "r"(b));
	return r;
}

USED uint32_t __umulsi3_highpart(uint32_t a, uint32_t b)
{
	uint32_t h;
	__asm__ ("mpymu %0, %1, %2" : "=r"(h) : "r"(a), "r"(b));
	return h;
}

USED int64_t __muldi3(int64_t a, int64_t b)
{
	uint32_t al = (uint32_t)a, ah = (uint32_t)((uint64_t)a >> 32);
	uint32_t bl = (uint32_t)b, bh = (uint32_t)((uint64_t)b >> 32);
	uint32_t lo, hi, t1, t2;

	__asm__ ("mpyu  %0, %1, %2" : "=r"(lo) : "r"(al), "r"(bl));
	__asm__ ("mpymu %0, %1, %2" : "=r"(hi) : "r"(al), "r"(bl));
	__asm__ ("mpyu  %0, %1, %2" : "=r"(t1) : "r"(al), "r"(bh));
	__asm__ ("mpyu  %0, %1, %2" : "=r"(t2) : "r"(ah), "r"(bl));
	hi += t1 + t2;
	return (int64_t)(((uint64_t)hi << 32) | lo);
}

USED void *memset(void *d, int c, uint32_t n)
{
	volatile uint8_t *p = d;

	while (n--)
		*p++ = (uint8_t)c;
	return d;
}

USED void *memcpy(void *d, const void *s, uint32_t n)
{
	volatile uint8_t *dp = d;
	const volatile uint8_t *sp = s;

	while (n--)
		*dp++ = *sp++;
	return d;
}
