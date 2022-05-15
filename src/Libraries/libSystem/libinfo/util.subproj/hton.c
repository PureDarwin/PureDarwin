/*
 * Copyright (c) 1999-2018 Apple Inc. All rights reserved.
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
/* Copyright (c) 1994 NeXT Computer, Inc.  All rights reserved. */

#include "libinfo_common.h"

#import <libkern/OSByteOrder.h>
#import <stdint.h>

LIBINFO_EXPORT
uint32_t ntohl(uint32_t x)
{
	return OSSwapBigToHostInt32(x);
}

LIBINFO_EXPORT
uint32_t htonl(uint32_t x)
{
	return OSSwapHostToBigInt32(x);
}

LIBINFO_EXPORT
uint16_t ntohs(uint16_t x)
{
	return OSSwapBigToHostInt16(x);
}

LIBINFO_EXPORT
uint16_t htons(uint16_t x)
{
	return OSSwapHostToBigInt16(x);
}
