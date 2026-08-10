/*
 * Copyright (c) 2012-2014 Apple Inc. All Rights Reserved.
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


#include <TargetConditionals.h>

#if TARGET_OS_OSX
#include <CoreFoundation/CoreFoundation.h>

#include <sys/types.h>
#include <pwd.h>
#include <uuid/uuid.h>
#include "iOSforOSX.h"
#include <pwd.h>
#include <unistd.h>

// Was in SOSAccount.c
#define SEC_CONST_DECL(k,v) const CFStringRef k = CFSTR(v);
// We may not have all of these we need
SEC_CONST_DECL (kSecAttrAccessible, "pdmn");
SEC_CONST_DECL (kSecAttrAccessibleAlwaysThisDeviceOnly, "dku");
SEC_CONST_DECL (kSecAttrAccessibleAlwaysThisDeviceOnlyPrivate, "dku");
SEC_CONST_DECL (kSecAttrAccessControl, "accc");
SEC_CONST_DECL (kSecAttrTokenID, "tkid");
SEC_CONST_DECL (kSecAttrAccessGroupToken, "com.apple.token");
SEC_CONST_DECL (kSecUseCredentialReference, "u_CredRef");
SEC_CONST_DECL (kSecUseOperationPrompt, "u_OpPrompt");
SEC_CONST_DECL (kSecUseNoAuthenticationUI, "u_NoAuthUI");
SEC_CONST_DECL (kSecUseAuthenticationUI, "u_AuthUI");
SEC_CONST_DECL (kSecUseAuthenticationUIAllow, "u_AuthUIA");
SEC_CONST_DECL (kSecUseAuthenticationUIFail, "u_AuthUIF");
SEC_CONST_DECL (kSecUseAuthenticationUISkip, "u_AuthUIS");
SEC_CONST_DECL (kSecUseAuthenticationContext, "u_AuthCtx");
SEC_CONST_DECL (kSecUseToken, "u_Token");
SEC_CONST_DECL (kSecUseCallerName, "u_CallerName");

#endif
