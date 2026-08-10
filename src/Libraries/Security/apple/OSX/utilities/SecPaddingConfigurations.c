/*
 * Copyright (c) 2017 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#define SecPaddingDomain CFSTR("com.apple.security.padding")

typedef enum {
	SecPaddingErrorUnknownType     = -1
} SecPaddingError;

#include "debugging.h"
#include "SecCFError.h"
#include "SecCFWrappers.h"
#include <Security/SecPaddingConfigurationsPriv.h>

#pragma mark Padding Helper Methods

// Compute the next power of two
// Requires: v <= UINT64_MAX/2
static uint64_t nextPowerOfTwo(uint64_t v)
{
	if (v > (UINT64_MAX>>1)) {
		secerror("Overflowing uint64_t by requesting nextPowerOfTwo of: %llx", v);
		assert(0);
	}
	if (v & (v - 1)) {
		// Not already a power of 2
		return ((uint64_t)1 << ((int)sizeof(v)*8 - __builtin_clzll(v)));
	} else {
		// Already a power of 2
		return v;
	}
}

// Round to a multiple of n
// Requires: v+n <= UINT64_MAX
static uint64_t nextMultiple(uint64_t v,uint64_t n)
{
	// Multiples of 0 are 0. Preventing division by 0.
	if (n == 0) {
		return 0;
	}

	if (n <= 0 || v > (UINT64_MAX-n)) {
		secerror("Overflowing uint64_t by requesting nextMutiple with parameters v: %llx and n: %llx", v, n);
		assert(0);
	}
	return n*((v+n-1)/n);
}

#pragma mark Padding Configurations

int64_t SecPaddingCompute(SecPaddingType type, uint32_t size, CFErrorRef *error) {
	if (type != SecPaddingTypeMMCS) {
		if (error) {
			*error = CFErrorCreate(CFAllocatorGetDefault(), SecPaddingDomain, SecPaddingErrorUnknownType, NULL);
		}
		return SecPaddingErrorUnknownType;
	}

	int64_t paddedSize = 0;

	if (size <= 64){
		paddedSize = 64;
	} else if (size <= 1024) {
		paddedSize = nextPowerOfTwo(size);
	} else if (size <= 32000) {
		paddedSize = nextMultiple(size, 1024);
	} else {
		paddedSize = nextMultiple(size, 8192);
	}
	
	assert(paddedSize >= size);
	return paddedSize - size;
}
