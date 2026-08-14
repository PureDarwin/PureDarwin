/*
 * Copyright (c) 2007-2009,2012-2020 Apple Inc. All Rights Reserved.
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
    @header SecTrustStoreServer
    CertificateSource API to a system root certificate store
*/

#ifndef _SECURITY_SECTRUSTSTORESERVER_H_
#define _SECURITY_SECTRUSTSTORESERVER_H_

#include "Security/SecTrustStore.h"
#include <CoreFoundation/CFArray.h>
#include <CoreFoundation/CFError.h>

__BEGIN_DECLS

SecTrustStoreRef SecTrustStoreForDomainName(CFStringRef domainName, CFErrorRef *error);

bool _SecTrustStoreSetTrustSettings(SecTrustStoreRef ts,
	SecCertificateRef certificate,
    CFTypeRef trustSettingsDictOrArray, CFErrorRef *error);

bool _SecTrustStoreRemoveCertificate(SecTrustStoreRef ts, SecCertificateRef cert, CFErrorRef *error);

bool _SecTrustStoreRemoveAll(SecTrustStoreRef ts, CFErrorRef *error);

CFArrayRef SecTrustStoreCopyParents(SecTrustStoreRef ts, SecCertificateRef certificate, CFErrorRef *error);

bool _SecTrustStoreContainsCertificate(SecTrustStoreRef source, SecCertificateRef cert, bool *contains, CFErrorRef *error);

bool _SecTrustStoreCopyUsageConstraints(SecTrustStoreRef ts, SecCertificateRef cert, CFArrayRef *usageConstraints, CFErrorRef *error);

bool _SecTrustStoreCopyAll(SecTrustStoreRef ts, CFArrayRef *trustStoreContents, CFErrorRef *error);

bool _SecTrustStoreMigrateUserStore(CFErrorRef *error);

void _SecTrustStoreMigrateConfigurations(void);

bool _SecTrustStoreSetCTExceptions(CFStringRef appID, CFDictionaryRef exceptions, CFErrorRef *error);
CF_RETURNS_RETAINED CFDictionaryRef _SecTrustStoreCopyCTExceptions(CFStringRef appID, CFErrorRef *error);

bool _SecTrustStoreSetCARevocationAdditions(CFStringRef appID, CFDictionaryRef additions, CFErrorRef *error);
CF_RETURNS_RETAINED CFDictionaryRef _SecTrustStoreCopyCARevocationAdditions(CFStringRef appID, CFErrorRef *error);

bool _SecTrustStoreSetTransparentConnectionPins(CFStringRef appID, CFArrayRef pins, CFErrorRef *error);
CF_RETURNS_RETAINED CFArrayRef _SecTrustStoreCopyTransparentConnectionPins(CFStringRef appID, CFErrorRef *error);

__END_DECLS

#endif /* !_SECURITY_SECTRUSTSTORESERVER_H_ */
