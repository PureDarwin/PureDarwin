/*
 * Copyright (c) 2007-2008,2012-2013 Apple Inc. All Rights Reserved.
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
    @header SecTrustStore
    CertificateSource API to a system root certificate store
*/

#ifndef _SECURITY_SECTRUSTSTORE_H_
#define _SECURITY_SECTRUSTSTORE_H_

#include <Security/SecCertificate.h>
#include <CoreFoundation/CoreFoundation.h>

__BEGIN_DECLS

typedef struct __SecTrustStore *SecTrustStoreRef;

enum {
	kSecTrustStoreDomainSystem = 1,
	kSecTrustStoreDomainUser = 2,
	kSecTrustStoreDomainAdmin = 3,
};
typedef uint32_t SecTrustStoreDomain;

typedef int32_t SecTrustSettingsVersionNumber;

typedef int32_t SecTrustSettingsAssetVersionNumber;

SecTrustStoreRef SecTrustStoreForDomain(SecTrustStoreDomain domain);

Boolean SecTrustStoreContains(SecTrustStoreRef source,
	SecCertificateRef certificate);

/* Only allowed for writable trust stores. */
OSStatus SecTrustStoreSetTrustSettings(SecTrustStoreRef ts,
	SecCertificateRef certificate,
    CFTypeRef trustSettingsDictOrArray);

OSStatus SecTrustStoreRemoveCertificate(SecTrustStoreRef ts,
	SecCertificateRef certificate);

OSStatus SecTrustStoreGetSettingsVersionNumber(SecTrustSettingsVersionNumber* p_settings_version_number);

OSStatus SecTrustStoreGetSettingsAssetVersionNumber(SecTrustSettingsAssetVersionNumber* p_settings_asset_version_number);

OSStatus SecTrustStoreCopyAll(SecTrustStoreRef ts, CFArrayRef *CF_RETURNS_RETAINED trustStoreContents);

/* Note that usageConstraints may be NULL on success. */
OSStatus SecTrustStoreCopyUsageConstraints(SecTrustStoreRef ts,
	SecCertificateRef certificate,
	CFArrayRef *CF_RETURNS_RETAINED usageConstraints);

__END_DECLS

#endif /* !_SECURITY_SECTRUSTSTORE_H_ */
