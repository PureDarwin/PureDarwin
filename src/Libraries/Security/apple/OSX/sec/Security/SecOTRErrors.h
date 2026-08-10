/*
 * Copyright (c) 2012,2014 Apple Inc. All Rights Reserved.
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


#ifndef messsageProtection_SecMessageProtectionErrors_h
#define messsageProtection_SecMessageProtectionErrors_h

static const CFIndex kSecOTRErrorFailedToEncrypt = -1;
static const CFIndex kSecOTRErrorFailedToDecrypt = -2;
static const CFIndex kSecOTRErrorFailedToVerify = -3;
static const CFIndex kSecOTRErrorFailedToSign = -4;
static const CFIndex kSecOTRErrorSignatureDidNotMatch = -5;
static const CFIndex kSecOTRErrorFailedSelfTest = -6;
static const CFIndex kSecOTRErrorParameterError = -7;
static const CFIndex kSecOTRErrorUnknownFormat = -8;
static const CFIndex kSecOTRErrorCreatePublicIdentity = -9;
static const CFIndex kSecOTRErrorCreatePublicBytes = -10;

// Errors 100-199 reserved for errors being genrated by workarounds/known issues failing
static const CFIndex kSecOTRErrorSignatureTooLarge = -100;
static const CFIndex kSecOTRErrorSignatureDidNotRecreate = -101;

#endif
