/*
 * Copyright (c) 2013-2014 Apple Inc. All Rights Reserved.
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
 @header SecItemDataSource.h
 The functions provided in SecDbItem provide an implementation of a
 SOSDataSource required by the Secure Object Syncing code.
 */

#ifndef _SECURITYD_SECITEMDATASOURCE_H_
#define _SECURITYD_SECITEMDATASOURCE_H_

#include "keychain/SecureObjectSync/SOSDataSource.h"
#include <utilities/SecDb.h>

__BEGIN_DECLS

SOSDataSourceFactoryRef SecItemDataSourceFactoryGetShared(SecDbRef db);

SOSDataSourceRef SecItemDataSourceCreate(SecDbRef db, CFStringRef name, CFErrorRef *error);

SOSManifestRef SOSCreateManifestWithBackup(CFDictionaryRef backup, CFErrorRef *error);

// Hack to log objects from inside SOS code
void SecItemServerAppendItemDescription(CFMutableStringRef desc, CFDictionaryRef object);

// Are you a test? Call this to drop all data sources.
void SecItemDataSourceFactoryReleaseAll(void);

__END_DECLS

#endif /* _SECURITYD_SECITEMDATASOURCE_H_ */
