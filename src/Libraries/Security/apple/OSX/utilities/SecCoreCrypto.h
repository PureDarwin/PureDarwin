/*
 * Copyright (c) 2015 Apple Inc. All Rights Reserved.
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



#ifndef _SECCORECRYPTO_H_
#define _SECCORECRYPTO_H_

#include <ctype.h>
#include <stddef.h>

#include <CoreFoundation/CoreFoundation.h>

__BEGIN_DECLS

#ifndef CCDER_BOOL_SUPPORT
const uint8_t* ccder_decode_bool(bool* boolean, const uint8_t* der, const uint8_t *der_end);
size_t ccder_sizeof_bool(bool value __unused, CFErrorRef *error);
uint8_t* ccder_encode_bool(bool value, const uint8_t *der, uint8_t *der_end);
#endif

__END_DECLS

#endif /* _SECCORECRYPTO_H_ */
