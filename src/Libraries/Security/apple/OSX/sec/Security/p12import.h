/*
 * Copyright (c) 2007-2008,2010,2012-2013 Apple Inc. All Rights Reserved.
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

#include <CoreFoundation/CoreFoundation.h>
#include <security_asn1/SecAsn1Coder.h>

#ifndef _SECURITY_P12IMPORT_H_
#define _SECURITY_P12IMPORT_H_

__BEGIN_DECLS

typedef enum {
    p12_noErr = 0,
    p12_decodeErr,
    p12_passwordErr,
} p12_error;

typedef struct {
	SecAsn1CoderRef coder;
    CFStringRef passphrase;
    CFMutableDictionaryRef items;
} pkcs12_context;

p12_error p12decode(pkcs12_context * context, CFDataRef cdpfx);

__END_DECLS

#endif /* !_SECURITY_P12IMPORT_H_ */
