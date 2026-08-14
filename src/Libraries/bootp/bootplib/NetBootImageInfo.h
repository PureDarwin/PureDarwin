/*
 * Copyright (c) 2003 Apple Inc. All rights reserved.
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

#include <CoreFoundation/CFString.h>

#define kNetBootImageInfoArchitectures	CFSTR("Architectures")	/* Array[String] */
#define kNetBootImageInfoIndex		CFSTR("Index")		/* Number */
#define kNetBootImageInfoIsEnabled	CFSTR("IsEnabled") 	/* Boolean */
#define kNetBootImageInfoIsInstall	CFSTR("IsInstall")	/* Boolean */
#define kNetBootImageInfoName		CFSTR("Name")		/* String */
#define kNetBootImageInfoType		CFSTR("Type")		/* String */
#define kNetBootImageInfoBootFile	CFSTR("BootFile")	/* String */
#define kNetBootImageInfoIsDefault	CFSTR("IsDefault")	/* Boolean */
#define kNetBootImageInfoKind		CFSTR("Kind")		/* Number */
#define kNetBootImageInfoSupportsDiskless CFSTR("SupportsDiskless") /* Boolean */
#define kNetBootImageInfoEnabledSystemIdentifiers CFSTR("EnabledSystemIdentifiers") /* Array[String] */
#define kNetBootImageInfoFilterOnly 	CFSTR("FilterOnly")	/* Boolean */
#define kNetBootImageInfoEnabledMACAddresses CFSTR("EnabledMACAddresses") /* Array[String] */
#define kNetBootImageInfoDisabledMACAddresses CFSTR("DisabledMACAddresses") /* Array[String] */
#define kNetBootImageLoadBalanceServer 	CFSTR("LoadBalanceServer") /* String */


/* Type values */
#define kNetBootImageInfoTypeClassic	CFSTR("Classic")
#define kNetBootImageInfoTypeNFS	CFSTR("NFS")
#define kNetBootImageInfoTypeHTTP	CFSTR("HTTP")
#define kNetBootImageInfoTypeBootFileOnly CFSTR("BootFileOnly")

/* Classic specific keys */
#define kNetBootImageInfoPrivateImage	CFSTR("PrivateImage")	/* String */
#define kNetBootImageInfoSharedImage	CFSTR("SharedImage")	/* String */

/* NFS, HTTP specific keys */
#define kNetBootImageInfoRootPath	CFSTR("RootPath")	/* String */


