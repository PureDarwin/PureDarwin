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


#ifndef sec_SOSUserKeygen_h
#define sec_SOSUserKeygen_h
#include <Security/SecureObjectSync/SOSCloudCircle.h>
#include <Security/SecKey.h>

CFDataRef SOSUserKeyCreateGenerateParameters(CFErrorRef *error);
CF_RETURNS_RETAINED SecKeyRef SOSUserKeygen(CFDataRef password, CFDataRef parameters, CFErrorRef *error);

void debugDumpUserParameters(CFStringRef message, CFDataRef parameters);
CF_RETURNS_RETAINED CFStringRef UserParametersDescription(CFDataRef parameters);

#endif
