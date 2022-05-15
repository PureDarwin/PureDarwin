/*
 * Copyright (c) 2016, 2018, 2020 Apple Computer, Inc. All rights reserved.
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

/*
 * Modification History
 *
 * September 8, 2016	Allan Nathanson <ajn@apple.com>
 * - initial revision
 */


#ifndef	_PLUGIN_SHARED_H
#define	_PLUGIN_SHARED_H


#include <CoreFoundation/CoreFoundation.h>


#pragma mark -
#pragma mark InterfaceNamer.bundle --> others

/*
 * Plugin:InterfaceNamer [SCDynamicStore] dictionary content
 */

// IORegistry "quiet", "complete" (last boot interfaces found), and "timeout"
#define	kInterfaceNamerKey_Complete			CFSTR("*COMPLETE*")
#define	kInterfaceNamerKey_Quiet			CFSTR("*QUIET*")
#define	kInterfaceNamerKey_Timeout			CFSTR("*TIMEOUT*")

// Configuration excluded network interfaces
#define	kInterfaceNamerKey_ExcludedInterfaces		CFSTR("_Excluded_")

// Network interfaces that have not yet been made available because the console is "locked"
#define	kInterfaceNamerKey_LockedInterfaces		CFSTR("_Locked_")

// [Apple] pre-configured network interfaces
#define	kInterfaceNamerKey_PreConfiguredInterfaces	CFSTR("_PreConfigured_")

// BT-PAN network interfaces
#define	BT_PAN_NAME					"Bluetooth PAN"
#define	kInterfaceNamerKey_BT_PAN_Name			CFSTR("_" BT_PAN_NAME "_")
#define	kInterfaceNamerKey_BT_PAN_Mac			CFSTR("_" BT_PAN_NAME " (MAC)" "_")


#endif	/* _PLUGIN_SHARED_H */
