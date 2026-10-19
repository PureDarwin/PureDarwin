/*
 * Copyright 2013 Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Paweł Dziepak, pdziepak@quarnos.org
 */
#ifndef KERNEL_UTIL_RANDOM_H
#define KERNEL_UTIL_RANDOM_H


#include <smp.h>
#include <SupportDefs.h>


#define MAX_FAST_RANDOM_VALUE		0x7fff
#define MAX_RANDOM_VALUE			0x7fffffffu
#define MAX_SECURE_RANDOM_VALUE		0xffffffffu

static const int	kFastRandomShift		= 15;
static const int	kRandomShift			= 31;
static const int	kSecureRandomShift		= 32;

#ifdef __cplusplus
extern "C" {
#endif

unsigned int	fast_random_value(void);
unsigned int	random_value(void);
unsigned int	secure_random_value(void);

#ifdef __cplusplus
}
#endif


#ifdef __cplusplus

template<typename T>
T
fast_get_random()
{
	size_t shift = 0;
	T random = 0;
	while (shift < sizeof(T) * 8) {
		random |= (T)fast_random_value() << shift;
		shift += kFastRandomShift;
	}

	return random;
}


template<typename T>
T
get_random()
{
	size_t shift = 0;
	T random = 0;
	while (shift < sizeof(T) * 8) {
		random |= (T)random_value() << shift;
		shift += kRandomShift;
	}

	return random;
}


template<typename T>
T
secure_get_random()
{
	size_t shift = 0;
	T random = 0;
	while (shift < sizeof(T) * 8) {
		random |= (T)secure_random_value() << shift;
		shift += kSecureRandomShift;
	}

	return random;
}


#endif	// __cplusplus

#endif	// KERNEL_UTIL_RANDOM_H

