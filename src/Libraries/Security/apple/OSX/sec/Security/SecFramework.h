/*
 * Copyright (c) 2006-2007,2009-2010,2012-2013,2019 Apple Inc. All Rights Reserved.
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

/*!
	@header SecFramework
	The functions provided in SecFramework.h implement generic non API class
    specific functionality.
*/

#ifndef _SECURITY_SECFRAMEWORK_H_
#define _SECURITY_SECFRAMEWORK_H_

#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFURL.h>
#include <Security/SecAsn1Types.h>

__BEGIN_DECLS

#define SecString(key, comment)  CFSTR(key)
#define SecStringFromTable(key, tbl, comment)  CFSTR(key)
#define SecStringWithDefaultValue(key, tbl, bundle, value, comment)  CFSTR(key)

CFStringRef SecFrameworkCopyLocalizedString(CFStringRef key,
    CFStringRef tableName);

/* Return the SHA1 digest of a chunk of data as newly allocated CFDataRef. */
CFDataRef SecSHA1DigestCreate(CFAllocatorRef allocator,
	const UInt8 *data, CFIndex length);

/* Return the SHA256 digest of a chunk of data as newly allocated CFDataRef. */
CFDataRef SecSHA256DigestCreate(CFAllocatorRef allocator,
    const UInt8 *data, CFIndex length);

CFDataRef SecSHA256DigestCreateFromData(CFAllocatorRef allocator, CFDataRef data);

/* Return the digest of a chunk of data as newly allocated CFDataRef, the
   algorithm is selected based on the algorithm and params passed in. */
CFDataRef SecDigestCreate(CFAllocatorRef allocator,
    const SecAsn1Oid *algorithm, const SecAsn1Item *params,
	const UInt8 *data, CFIndex length);

// Wrapper to provide a CFErrorRef for legacy API.
OSStatus SecOSStatusWith(bool (^perform)(CFErrorRef *error));

extern CFStringRef kSecFrameworkBundleID;

/* Returns true if 'string' is a DNS host name as defined in RFC 1035, etc. */
bool SecFrameworkIsDNSName(CFStringRef string);

/* Returns true if 'string' is an IPv4/IPv6 address per RFC 2373, 4632, etc. */
bool SecFrameworkIsIPAddress(CFStringRef string);

/* Returns the canonical data representation of the IPv4 or IPv6 address
   provided as input. NULL is returned if string is not a valid IP address. */
CFDataRef SecFrameworkCopyIPAddressData(CFStringRef string);

__END_DECLS

#endif /* !_SECURITY_SECFRAMEWORK_H_ */
