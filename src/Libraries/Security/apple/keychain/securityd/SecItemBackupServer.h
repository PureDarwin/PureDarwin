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

/*!
 @header SecItemBackupServer.h
 This file declares backup restore functionality for SecItems.
 */

#ifndef _SECURITYD_SECITEMBACKUPSERVER_H_
#define _SECURITYD_SECITEMBACKUPSERVER_H_

#include <CoreFoundation/CFError.h>
#include <CoreFoundation/CFString.h>

__BEGIN_DECLS

int SecServerItemBackupHandoffFD(CFStringRef backupName, CFErrorRef *error);
bool SecServerItemBackupSetConfirmedManifest(CFStringRef backupName, CFDataRef keybagDigest, CFDataRef manifest, CFErrorRef *error);
CFArrayRef SecServerItemBackupCopyNames(CFErrorRef *error);
CFStringRef SecServerItemBackupEnsureCopyView(CFStringRef peerID, CFErrorRef *error);
bool SecServerItemBackupRestore(CFStringRef backupName, CFStringRef peerID, CFDataRef keybag, CFDataRef secret, CFDataRef backup, CFErrorRef *error);

__END_DECLS

#endif /* _SECURITYD_SECITEMBACKUPSERVER_H_ */
