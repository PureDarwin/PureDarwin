/*
 * Copyright (c) 2013-2018 Apple Inc. All rights reserved.
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

#ifndef _S_IPCONFIGURATIONCONTROLPREFS_H
#define _S_IPCONFIGURATIONCONTROLPREFS_H

/*
 * IPConfigurationControlPrefs.h
 * - definitions for accessing IPConfiguration controlpreferences and being
 *   notified when they change
 */

/* 
 * Modification History
 *
 * March 26, 2013	Dieter Siegmund (dieter@apple)
 * - created (from EAPOLControlPrefs.h)
 */
#include <CoreFoundation/CFRunLoop.h>
#include <SystemConfiguration/SCPreferences.h>

typedef void (*IPConfigurationControlPrefsCallBack)(SCPreferencesRef prefs);

SCPreferencesRef
IPConfigurationControlPrefsInit(CFRunLoopRef runloop,
				IPConfigurationControlPrefsCallBack callback);

void
IPConfigurationControlPrefsSynchronize(void);

Boolean
IPConfigurationControlPrefsGetVerbose(void);

Boolean
IPConfigurationControlPrefsSetVerbose(Boolean verbose);

typedef CF_ENUM(uint32_t, IPConfigurationInterfaceTypes) {
    kIPConfigurationInterfaceTypesUnspecified = 0,
    kIPConfigurationInterfaceTypesNone = 1,
    kIPConfigurationInterfaceTypesCellular = 2,
    kIPConfigurationInterfaceTypesAll = 3,
};

IPConfigurationInterfaceTypes
IPConfigurationInterfaceTypesFromString(CFStringRef str);

CFStringRef
IPConfigurationInterfaceTypesToString(IPConfigurationInterfaceTypes types);

Boolean
IPConfigurationControlPrefsSetAWDReportInterfaceTypes(IPConfigurationInterfaceTypes
						      types);

IPConfigurationInterfaceTypes
IPConfigurationControlPrefsGetAWDReportInterfaceTypes(void);

Boolean
IPConfigurationControlPrefsGetCellularCLAT46AutoEnable(void);

Boolean
IPConfigurationControlPrefsSetCellularCLAT46AutoEnable(Boolean enable);

#endif /* _S_IPCONFIGURATIONCONTROLPREFS_H */
