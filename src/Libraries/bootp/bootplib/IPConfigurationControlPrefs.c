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

/*
 * IPConfigurationControlPrefs.c
 * - definitions for accessing IPConfiguration preferences
 */

/* 
 * Modification History
 *
 * March 26, 2013	Dieter Siegmund (dieter@apple)
 * - created (from EAPOLControlPrefs.c)
 */

#include <SystemConfiguration/SCPreferences.h>
#include <SystemConfiguration/SCPrivate.h>
#include <SystemConfiguration/scprefs_observer.h>
#include <TargetConditionals.h>
#include "IPConfigurationControlPrefs.h"
#include "symbol_scope.h"
#include "cfutil.h"

/*
 * kIPConfigurationControlPrefsID, kIPConfigurationControlPrefsIDStr
 * - identifies the IPConfiguration preferences file that contains
 *   Verbose and other control variables
 * - also identifies the managed preferences ID used to apply control
 *   settings on iOS via configuration profile
 */

#define kIPConfigurationControlPrefsIDStr \
    "com.apple.IPConfiguration.control.plist"
#define kIPConfigurationControlPrefsID	\
    CFSTR(kIPConfigurationControlPrefsIDStr)

/*
 * kVerbose
 * - indicates whether verbose logging is enabled or not
 */
#define kVerbose			CFSTR("Verbose")	/* boolean */

/*
 * kAWDReportInterfaceTypes
 * - indicates whether to submit AWD report for particular interface
 *   types
 * - a string with one of "All", "Cellular", or "None"
 */
#define kAWDReportInterfaceTypes	CFSTR("AWDReportInterfaceTypes") /* string */

/*
 * kCellularCLAT46AutoEnable
 * - indicates whether CLAT46 should be auto-enabled for cellular interfaces
 */
#define kCellularCLAT46AutoEnable	CFSTR("CellularCLAT46AutoEnable")	/* boolean */

STATIC SCPreferencesRef				S_prefs;
STATIC IPConfigurationControlPrefsCallBack	S_callback;

STATIC SCPreferencesRef
IPConfigurationControlPrefsGet(void)
{
    if (S_prefs == NULL) {
	IPConfigurationControlPrefsInit(NULL, NULL);
    }
    return (S_prefs);
}

STATIC void
prefs_changed(__unused void * arg)
{
    if (S_callback != NULL) {
	(*S_callback)(S_prefs);
    }
    return;
}

#if TARGET_OS_IPHONE
/*
 * kIPConfigurationControlManangedPrefsID
 * - identifies the location of the managed preferences file
 */
#define kManagedPrefsDirStr		"/Library/Managed Preferences/mobile/"
#define kIPConfigurationControlManagedPrefsID \
    CFSTR(kManagedPrefsDirStr kIPConfigurationControlPrefsIDStr)
STATIC SCPreferencesRef		S_managed_prefs;

STATIC SCPreferencesRef
IPConfigurationControlManagedPrefsGet(void)
{
    if (S_managed_prefs == NULL) {
	S_managed_prefs
	    = SCPreferencesCreate(NULL, CFSTR("IPConfigurationControlPrefs"),
				  kIPConfigurationControlManagedPrefsID);
    }
    return (S_managed_prefs);
}

STATIC void
enable_prefs_observer(CFRunLoopRef runloop)
{
    CFRunLoopSourceContext 	context;
    dispatch_block_t		handler;
    CFRunLoopSourceRef		source;
    dispatch_queue_t		queue;

    bzero(&context, sizeof(context));
    context.perform = prefs_changed;
    source = CFRunLoopSourceCreate(kCFAllocatorDefault, 0, &context);
    CFRunLoopAddSource(runloop, source, kCFRunLoopCommonModes);
    queue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
    handler = ^{
	if (source != NULL) {
	    CFRunLoopSourceSignal(source);
	    if (runloop != NULL) {
		CFRunLoopWakeUp(runloop);
	    }
	};
    };
    (void)_scprefs_observer_watch(scprefs_observer_type_global,
				  kIPConfigurationControlPrefsIDStr,
				  queue, handler);
    return;
}

#else /* TARGET_OS_IPHONE */

STATIC void
enable_prefs_observer(CFRunLoopRef runloop)
{
    return;
}

#endif /* TARGET_OS_IPHONE */

PRIVATE_EXTERN void
IPConfigurationControlPrefsSynchronize(void)
{
    if (S_prefs != NULL) {
	SCPreferencesSynchronize(S_prefs);
    }
#if TARGET_OS_IPHONE
    if (S_managed_prefs != NULL) {
	SCPreferencesSynchronize(S_managed_prefs);
    }
#endif /* TARGET_OS_IPHONE */
    return;
}

STATIC void
IPConfigurationControlPrefsChanged(SCPreferencesRef prefs,
				   SCPreferencesNotification type,
				   void * info)
{
    prefs_changed(NULL);
    return;
}

PRIVATE_EXTERN SCPreferencesRef
IPConfigurationControlPrefsInit(CFRunLoopRef runloop, 
				IPConfigurationControlPrefsCallBack callback)
{
    S_prefs = SCPreferencesCreate(NULL, CFSTR("IPConfigurationControlPrefs"),
				  kIPConfigurationControlPrefsID);
    if (runloop != NULL && callback != NULL) {
	S_callback = callback;
	SCPreferencesSetCallback(S_prefs,
				 IPConfigurationControlPrefsChanged, NULL);
	SCPreferencesScheduleWithRunLoop(S_prefs, runloop,
					 kCFRunLoopCommonModes);
	enable_prefs_observer(runloop);
    }
    return (S_prefs);
}

STATIC Boolean
IPConfigurationControlPrefsSave(void)
{
    Boolean		saved = FALSE;

    if (S_prefs != NULL) {
	saved = SCPreferencesCommitChanges(S_prefs);
	SCPreferencesSynchronize(S_prefs);
    }
    return (saved);
}

STATIC CFBooleanRef
prefs_get_boolean(CFStringRef key)
{
    CFBooleanRef	b = NULL;

#if TARGET_OS_IPHONE
    b = SCPreferencesGetValue(IPConfigurationControlManagedPrefsGet(), key);
    b = isA_CFBoolean(b);
#endif /* TARGET_OS_IPHONE */
    if (b == NULL) {
	b = SCPreferencesGetValue(IPConfigurationControlPrefsGet(), key);
	b = isA_CFBoolean(b);
    }
    return (b);
}

STATIC void
prefs_set_boolean(CFStringRef key, CFBooleanRef b)
{
    SCPreferencesRef	prefs = IPConfigurationControlPrefsGet();

    if (prefs != NULL) {
	if (isA_CFBoolean(b) == NULL) {
	    SCPreferencesRemoveValue(prefs, key);
	}
	else {
	    SCPreferencesSetValue(prefs, key, b);
	}
    }
    return;
}

STATIC CFStringRef
prefs_get_string(CFStringRef key)
{
    CFStringRef	str = NULL;

#if TARGET_OS_IPHONE
    str = SCPreferencesGetValue(IPConfigurationControlManagedPrefsGet(), key);
    str = isA_CFString(str);
#endif /* TARGET_OS_IPHONE */
    if (str == NULL) {
	str = SCPreferencesGetValue(IPConfigurationControlPrefsGet(), key);
	str = isA_CFString(str);
    }
    return (str);
}

STATIC void
prefs_set_string(CFStringRef key, CFStringRef str)
{
    SCPreferencesRef	prefs = IPConfigurationControlPrefsGet();

    if (prefs != NULL) {
	if (isA_CFString(str) == NULL) {
	    SCPreferencesRemoveValue(prefs, key);
	}
	else {
	    SCPreferencesSetValue(prefs, key, str);
	}
    }
    return;
}


typedef struct {
    IPConfigurationInterfaceTypes	type;
    CFStringRef				str;
} if_type_table_entry, *if_type_table_entry_ref;

STATIC if_type_table_entry interface_types[] = {
    { kIPConfigurationInterfaceTypesNone, CFSTR("None") },
    { kIPConfigurationInterfaceTypesCellular, CFSTR("Cellular") },
    { kIPConfigurationInterfaceTypesAll, CFSTR("All") },
};
#define INTERFACE_TYPES_COUNT 	(sizeof(interface_types) / sizeof(interface_types[0]))
#define DEFAULT_INTERFACE_TYPES	kIPConfigurationTypesCellular

PRIVATE_EXTERN IPConfigurationInterfaceTypes
IPConfigurationInterfaceTypesFromString(CFStringRef str)
{
    if (str != NULL) {
	int			i;
	if_type_table_entry_ref	scan;

	for (i = 0, scan = interface_types;
	     i < INTERFACE_TYPES_COUNT;
	     i++, scan++) {
	    if (CFEqual(scan->str, str)) {
		return (scan->type);
	    }
	}
    }
    return (kIPConfigurationInterfaceTypesUnspecified);
}

PRIVATE_EXTERN CFStringRef
IPConfigurationInterfaceTypesToString(IPConfigurationInterfaceTypes types)
{
    int				i;
    if_type_table_entry_ref	scan;

    for (i = 0, scan = interface_types;
	 i < INTERFACE_TYPES_COUNT;
	 i++, scan++) {
	if (types == scan->type) {
	    return (scan->str);
	}
    }
    return (NULL);
}

/**
 ** Get
 **/
PRIVATE_EXTERN Boolean
IPConfigurationControlPrefsGetVerbose(void)
{
    CFBooleanRef	val;
    Boolean		verbose = FALSE;

    val = prefs_get_boolean(kVerbose);
    if (val != NULL) {
	verbose = CFBooleanGetValue(val);
    }
    return (verbose);
}

PRIVATE_EXTERN IPConfigurationInterfaceTypes
IPConfigurationControlPrefsGetAWDReportInterfaceTypes(void)
{
    CFStringRef	types;

    types = prefs_get_string(kAWDReportInterfaceTypes);
    return (IPConfigurationInterfaceTypesFromString(types));
}

Boolean
IPConfigurationControlPrefsGetCellularCLAT46AutoEnable(void)
{
	Boolean		enabled = FALSE;
	CFBooleanRef	val;

	val = prefs_get_boolean(kCellularCLAT46AutoEnable);
	if (val != NULL) {
		enabled = CFBooleanGetValue(val);
	}
	return (enabled);
}

/**
 ** Set
 **/
PRIVATE_EXTERN Boolean
IPConfigurationControlPrefsSetVerbose(Boolean verbose)
{
    if (verbose == FALSE) {
	prefs_set_boolean(kVerbose, NULL);
    }
    else {
	prefs_set_boolean(kVerbose, kCFBooleanTrue); 
    }
    return (IPConfigurationControlPrefsSave());
}

PRIVATE_EXTERN Boolean
IPConfigurationControlPrefsSetAWDReportInterfaceTypes(IPConfigurationInterfaceTypes
						      types)
{
    CFStringRef		str;

    str = IPConfigurationInterfaceTypesToString(types);
    prefs_set_string(kAWDReportInterfaceTypes, str);
    return (IPConfigurationControlPrefsSave());
}

Boolean
IPConfigurationControlPrefsSetCellularCLAT46AutoEnable(Boolean enable)
{
	if (enable == FALSE) {
		prefs_set_boolean(kCellularCLAT46AutoEnable, NULL);
	}
	else {
		prefs_set_boolean(kCellularCLAT46AutoEnable, kCFBooleanTrue);
	}
	return (IPConfigurationControlPrefsSave());
}
