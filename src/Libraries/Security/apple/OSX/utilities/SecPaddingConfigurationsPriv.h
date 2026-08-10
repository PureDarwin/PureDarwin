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


#ifndef SecPaddingConfigurations_h
#define SecPaddingConfigurations_h

#include <CoreFoundation/CoreFoundation.h>

typedef CF_ENUM(uint8_t, SecPaddingType) {
	SecPaddingTypeMMCS CF_ENUM_AVAILABLE(10_13, 11_0) = 0,
} CF_ENUM_AVAILABLE(10_13, 11_0);

/*!
 @function
 @abstract   Compute the padding size given the size of the content
 @param      type Type of content to be protected
 @param      size size before padding
 @param      error Output parameter to a CFErrorRef
 @result     number of bytes to add to the message. Only returns a negative value on SecPaddingType mismatch with a CFError assigned
 */
int64_t SecPaddingCompute(SecPaddingType type, uint32_t size, CFErrorRef *error)
__OSX_AVAILABLE(10.13) __IOS_AVAILABLE(11.0) __TVOS_AVAILABLE(11.0) __WATCHOS_AVAILABLE(4.0);

#endif
