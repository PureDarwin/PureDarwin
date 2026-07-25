#pragma once

#include <stdint.h>

#define STRINGIFY(a) STRINGIFY_(a)
#define STRINGIFY_(a) #a

static inline uint32_t div_u64(uint64_t a, uint32_t b) {
	if(__builtin_constant_p(b))
		return a / b;

	uint32_t lo = a;
	uint32_t hi = a >> 32;

	// clang always picks memory for "rm" ; this is a workaround that doesn't hurt GCC
	asm ("div %2" : "+a,a" (lo), "+d,d" (hi) : "r,m" (b));

	return lo;
}

static inline uint32_t div_u32_ceil(uint32_t a, uint32_t b) {
	return a / b + (a % b > 0);
}
