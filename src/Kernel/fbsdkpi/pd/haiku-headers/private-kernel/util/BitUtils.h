/*
 * Copyright 2013 Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Paweł Dziepak, pdziepak@quarnos.org
 */
#ifndef KERNEL_UTIL_BITUTIL_H
#define KERNEL_UTIL_BITUTIL_H


#include <string.h>

#include <SupportDefs.h>


// http://graphics.stanford.edu/~seander/bithacks.html
static inline uint32
next_power_of_2(uint32 v)
{
	v--;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	v++;

	return v;
}


// http://graphics.stanford.edu/~seander/bithacks.html
static inline uint32
count_set_bits(uint32 v)
{
	v = v - ((v >> 1) & 0x55555555);
	v = (v & 0x33333333) + ((v >> 2) & 0x33333333);
	return (((v + (v >> 4)) & 0xF0F0F0F) * 0x1010101) >> 24;
}


static inline uint32
fls(uint32 value)
{
	if (value == 0)
		return 0;

#if __has_builtin(__builtin_clz)
	return ((sizeof(value) * 8) - __builtin_clz(value));
#else
	// https://graphics.stanford.edu/~seander/bithacks.html#IntegerLog
	static const uint32 masks[] = {
		0xaaaaaaaa,
		0xcccccccc,
		0xf0f0f0f0,
		0xff00ff00,
		0xffff0000
	};
	uint32 result = (value & masks[0]) != 0;
	for (int i = 4; i > 0; i--)
		result |= ((value & masks[i]) != 0) << i;
	return result + 1;
#endif
}


static inline uint32
log2(uint32 v)
{
	static const int MultiplyDeBruijnBitPosition[32] = {
		0, 9, 1, 10, 13, 21, 2, 29, 11, 14, 16, 18, 22, 25, 3, 30,
		8, 12, 20, 28, 15, 17, 24, 7, 19, 27, 23, 6, 26, 5, 4, 31
	};

	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;

	return MultiplyDeBruijnBitPosition[(uint32)(v * 0x07C4ACDDU) >> 27];
}


template<typename T>
void
bitmap_shift(T* bits, size_t bitCount, ssize_t shift)
{
	if (shift == 0)
		return;

	const size_t bitsPerElement = sizeof(T) * 8;
	const size_t elementsCount = (bitCount + bitsPerElement - 1) / bitsPerElement;
	const size_t absoluteShift = (shift > 0) ? shift : -shift;
	const size_t nElements = absoluteShift / bitsPerElement;
	const size_t nBits = absoluteShift % bitsPerElement;
	if (nElements != 0) {
		if (shift > 0) {
			// "Left" shift.
			memmove(&bits[nElements], bits, sizeof(T) * (elementsCount - nElements));
			memset(bits, 0, sizeof(T) * nElements);
		} else if (shift < 0) {
			// "Right" shift.
			memmove(bits, &bits[nElements], sizeof(T) * (elementsCount - nElements));
			memset(&bits[elementsCount - nElements], 0, sizeof(T) * nElements);
		}
	}

	// If the shift was by a multiple of the element size, nothing more to do.
	if (nBits == 0)
		return;

	// One set of bits comes from the "current" element and are shifted in the
	// direction of the shift; the other set comes from the next-processed
	// element and are shifted in the opposite direction.
	if (shift > 0) {
		// "Left" shift.
		for (ssize_t i = elementsCount - 1; i >= 0; i--) {
			T low = 0;
			if (i != 0)
				low = bits[i - 1] >> (bitsPerElement - nBits);
			const T high = bits[i] << nBits;
			bits[i] = low | high;
		}
	} else if (shift < 0) {
		// "Right" shift.
		for (size_t i = 0; i < elementsCount; i++) {
			const T low = bits[i] >> nBits;
			T high = 0;
			if (i != (elementsCount - 1))
				high = bits[i + 1] << (bitsPerElement - nBits);
			bits[i] = low | high;
		}
	}
}


#endif	// KERNEL_UTIL_BITUTIL_H

