/*
 * Copyright (c) 1998-2013 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */


#include "BigNum128.h"


U128 UInt64mult(const uint64_t A, const uint64_t B)
{
	U128	result;
	
#ifndef __OPEN_SOURCE__							// <rdar://13613944>
	// Karatsuba multiplication
	// Suppose we want to multiply two 2 numbers A * B, where A = a1 << 32 + a0, B = b1 << 32 + b0:
	// 1. compute a1 * b1, call the result X
	// 2. compute a0 * b0, call the result Y
	// 3. compute Z, this number is equal to a1 * b0 + a0 * b1.
	// 4. compute X << 64 + Z << 32 + Y
#endif

	uint64_t a1, a0, b1, b0;
	a1 = A >> 32;
	a0 = A - (a1 << 32);
	b1 = B >> 32;
	b0 = B - (b1 << 32);
	
	uint64_t X, Y, Z;
	X = a1 * b1;
	Y = a0 * b0;
	Z = a1 * b0 + a0 * b1;
	
	uint64_t z1, z0;
	z1 = Z >> 32;
	z0 = Z - (z1 << 32);
	
	return U128(X, 0) + U128(z1, uint64_t(z0) << 32) + U128(0, Y);
}
