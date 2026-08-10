/*
 * Copyright (c) 2007-2008,2010,2012 Apple Inc. All Rights Reserved.
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
 
/* bit twiddling keygen for p12 password based encryption */

#include <CoreFoundation/CFString.h>

typedef enum {
    PBE_ID_Key      = 1,
    PBE_ID_IV       = 2,
    PBE_ID_MAC      = 3
} P12_PBE_ID;

int p12_pbe_gen(CFStringRef passphrase, uint8_t *salt_ptr, size_t salt_length, 
    unsigned iter_count, P12_PBE_ID pbe_id, uint8_t *data, size_t length);
