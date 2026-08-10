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

#ifndef server_security_helpers_h
#define server_security_helpers_h

#include <Security/SecTask.h>
#include "ipc/securityd_client.h"

CFTypeRef SecCreateLocalCFSecuritydXPCServer(void);
void SecAddLocalSecuritydXPCFakeEntitlement(CFStringRef entitlement, CFTypeRef value);
void SecResetLocalSecuritydXPCFakeEntitlements(void);
void SecCreateSecuritydXPCServer(void);

bool fill_security_client(SecurityClient * client, const uid_t uid, audit_token_t auditToken);
CFArrayRef SecTaskCopyAccessGroups(SecTaskRef task);

/*!
 @function SecTaskIsEligiblePlatformBinary
 @abstract Determine whether task belongs to valid platform binary and optionally has one of the allowed identifiers.
 @param task The client task to be evaluated.
 @param identifiers Optional array of codesigning identifiers of allowed callers. Pass NULL to permit any platform binary.
 @result Client satisfies the criteria or not.
 */
bool SecTaskIsEligiblePlatformBinary(SecTaskRef task, CFArrayRef identifiers);

// Testing support
void SecAccessGroupsSetCurrent(CFArrayRef accessGroups);
void SecSecurityClientRegularToAppClip(void);
void SecSecurityClientAppClipToRegular(void);
void SecSecurityClientSetApplicationIdentifier(CFStringRef identifier);

#endif /* server_security_helpers_h */
