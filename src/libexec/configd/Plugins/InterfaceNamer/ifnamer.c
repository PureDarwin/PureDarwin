/*
 * Copyright (c) 2001-2021 Apple Inc. All rights reserved.
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
 * May 20, 2006			Joe Liu <joe.liu@apple.com>
 *				Allan Nathanson <ajn@apple.com>
 * - register interface by entryID (and not path)
 *
 * November 6, 2006		Allan Nathanson <ajn@apple.com>
 *				Dan Markarian <markarian@apple.com>
 *				Dieter Siegmund <dieter@apple.com>
 * - updated code to name interfaces quicker (without need for
 *   calling IOKitWaitQuiet).
 *
 * October 3, 2003		Allan Nathanson <ajn@apple.com>
 * - sort new interfaces by IOKit path (rather than MAC address) to
 *   help facilitate a more predictable interface-->name mapping for
 *   like hardware configurations.
 *
 * June 23, 2001		Allan Nathanson <ajn@apple.com>
 * - update to public SystemConfiguration.framework APIs
 *
 * January 23, 2001		Dieter Siegmund <dieter@apple.com>
 * - initial revision
 */

/*
 * ifnamer.c
 * - module that receives IOKit Network Interface messages
 *   and names any interface that currently does not have a name
 * - uses InterfaceNamePrefix and MACAddress as the unique identifying
 *   keys; any interface that doesn't contain both of these properties
 *   is ignored and not processed
 * - stores the InterfaceNamePrefix, MACAddress, and Unit in permanent storage
 *   to give persistent interface names
 */

#include <TargetConditionals.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#if	TARGET_OS_IPHONE
#include <lockdown.h>
#include <notify.h>
#endif	// TARGET_OS_IPHONE
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/param.h>
#include <mach/mach.h>
#include <net/ethernet.h>
#include <net/if_types.h>
#include <pthread.h>

#include <CommonCrypto/CommonDigest.h>

#include <CoreFoundation/CoreFoundation.h>

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCDPlugin.h>
#include <SystemConfiguration/SCPrivate.h>
#include <SystemConfiguration/SCValidation.h>
#include "SCNetworkConfigurationInternal.h"
#include "SCPreferencesInternal.h"
#include "plugin_shared.h"
#if TARGET_OS_OSX
#include "InterfaceNamerControlPrefs.h"
#endif	// TARGET_OS_OSX

#ifdef	TEST_INTERFACE_ASSIGNMENT
#undef	INTERFACES_DEFAULT_CONFIG
#define	INTERFACES_DEFAULT_CONFIG	CFSTR("/tmp/ifnamer-test-NetworkInterfaces.plist")
#endif	// TEST_INTERFACE_ASSIGNMENT

#include <IOKit/IOKitLib.h>
#include <IOKit/IOKitLibPrivate.h>
#include <IOKit/IOKitKeysPrivate.h>
#include <IOKit/IOBSD.h>
#include <IOKit/IOMessage.h>
#include <IOKit/network/IONetworkController.h>
#include <IOKit/network/IONetworkInterface.h>
#include <IOKit/network/IONetworkStack.h>
#include <IOKit/usb/USB.h>
#if TARGET_OS_OSX
#define IFNAMER_SUBSYSTEM	"com.apple.SystemConfiguration.InterfaceNamer"
#include <os/variant_private.h>
#endif /* TARGET_OS_OSX */

#define MY_PLUGIN_NAME			"InterfaceNamer"
#define	MY_PLUGIN_ID			CFSTR("com.apple.SystemConfiguration." MY_PLUGIN_NAME)

#define WAIT_STACK_TIMEOUT_KEY		"WaitStackTimeout"
#define WAIT_STACK_TIMEOUT_DEFAULT	300.0

#define WAIT_QUIET_TIMEOUT_KEY		"WaitQuietTimeout"
#define WAIT_QUIET_TIMEOUT_DEFAULT	240.0

/*
 * S_connect
 *   "IONetworkStack" connect object used to "name" an interface.
 */
static io_connect_t		S_connect		= MACH_PORT_NULL;

/*
 * S_dblist
 *   An array of CFDictionary's representing the interfaces
 *   that have been identified and [need to be] named.
 */
static CFMutableArrayRef	S_dblist		= NULL;

/*
 * S_iflist
 *   An array of SCNetworkInterface's representing the
 *   interfaces that have been identified and likely require
 *   naming.
 */
static CFMutableArrayRef	S_iflist		= NULL;

/*
 * S_iter
 *   IOServiceAddMatchingNotification object used to watch for
 *   new network interfaces.
 */
static io_iterator_t		S_iter			= MACH_PORT_NULL;

#if TARGET_OS_OSX
/*
 * S_locked
 *   An array of CFData(WatchedInfo) objects representing those
 *   interfaces that have been connected to the system while
 *   locked.
 */
static CFMutableArrayRef	S_locked		= NULL;
#endif	// TARGET_OS_OSX

/*
 * S_notify
 *   notification object for receiving IOKit notifications of
 *   new devices or state changes.
 */
static IONotificationPortRef	S_notify		= NULL;

/*
 * S_preconfigured
 *   An array of CFData(WatchedInfo) objects representing those
 *   pre-configured interfaces that have been connected to the
 *   system.
 */
static CFMutableArrayRef	S_preconfigured		= NULL;

/* S_prev_active_list
 *   An array of CFDictionary's representing the previously
 *   named interfaces.
 */
static CFMutableArrayRef	S_prev_active_list	= NULL;

/*
 * S_quiet
 *   IOServiceAddInterestNotification object used to watch for
 *   IOKit matching to quiesce.
 */
static io_object_t		S_quiet			= MACH_PORT_NULL;

/*
 * S_stack
 *   IOServiceAddMatchingNotification object used to watch for
 *   the availability of the "IONetworkStack" object.
 */
static io_iterator_t		S_stack			= MACH_PORT_NULL;

/*
 * S_state
 *   A dictionary containing Information about each network
 *   interface.  For now, the key is the BSD name and the
 *   value is a CFNumber noting how long (in milliseconds)
 *   it took for the interface to be recognized/named.
 */
static CFMutableDictionaryRef	S_state			= NULL;

#if	TARGET_OS_IPHONE
/*
 * S_trustedHostAttached
 *
 * Note: this global must only be updated on trustRequired_queue()
 */
static Boolean			S_trustedHostAttached	= FALSE;

/*
 *
 * Note: this global must only be updated on trustRequired_queue()
 */
static CFIndex			S_trustedHostCount	= 0;

/*
 * S_trustRequired
 *   An array of CFData(WatchedInfo) objects representing those
 *   interfaces that require [lockdownd] trust.
 */
static CFMutableArrayRef	S_trustRequired		= NULL;
#endif	// TARGET_OS_IPHONE

/*
 * S_timer
 *   CFRunLoopTimer tracking how long we are willing to wait
 *   for IOKit matching to quiesce (IOKitWaitQuiet).
 *
 * S_stack_timeout
 *   time to wait for the IONetworkStack object to appear before timeout
 *
 * S_quiet_timeout
 *   time to wait for the IOKit to quiesce (after the IONetworkStack is
 *   has appeared.
 */
static CFRunLoopTimerRef	S_timer			= NULL;
static double			S_stack_timeout		= WAIT_STACK_TIMEOUT_DEFAULT;
static double			S_quiet_timeout		= WAIT_QUIET_TIMEOUT_DEFAULT;

/*
 * Virtual network interface configuration
 *   S_prefs   : SCPreferences to configuration
 *   S_bonds   : most recently actived Bond configuration
 *   S_bridges : most recently actived Bridge configuration
 *   S_vlans   : most recently actived VLAN configuration
 */
static SCPreferencesRef		S_prefs			= NULL;
static CFArrayRef		S_bonds			= NULL;
static CFArrayRef		S_bridges		= NULL;
static CFArrayRef		S_vlans			= NULL;

static inline void
my_IOObjectRelease(io_object_t *obj_p)
{
    io_object_t	obj;

    if (obj_p == NULL) {
	return;
    }
    obj = *obj_p;
    if (obj != IO_OBJECT_NULL) {
	IOObjectRelease(obj);
	*obj_p = IO_OBJECT_NULL;
    }
}

static inline void
my_CFRelease(void * t)
{
    void * * obj = (void * *)t;

    if (obj && *obj) {
	CFRelease(*obj);
	*obj = NULL;
    }
    return;
}

/*
 * Logging
 */
__private_extern__
os_log_t
__log_InterfaceNamer(void)
{
    static os_log_t	log = NULL;

    if (log == NULL) {
	log = os_log_create("com.apple.SystemConfiguration", "InterfaceNamer");
    }

    return log;
}


static void
addTimestamp(CFMutableDictionaryRef dict, CFStringRef key)
{
    CFAbsoluteTime	now;
    CFNumberRef		val;

    now = CFAbsoluteTimeGetCurrent();
    val = CFNumberCreate(NULL, kCFNumberDoubleType, &now);
    CFDictionaryAddValue(dict, key, val);
    CFRelease(val);
    return;
}

static CFComparisonResult
if_unit_compare(const void *val1, const void *val2, void *context)
{
#pragma unused(context)
    CFStringRef		prefix1;
    CFStringRef		prefix2;
    CFComparisonResult	res;
    CFNumberRef		unit1;
    CFNumberRef		unit2;

    prefix1 = CFDictionaryGetValue((CFDictionaryRef)val1,
				   CFSTR(kIOInterfaceNamePrefix));
    prefix2 = CFDictionaryGetValue((CFDictionaryRef)val2,
				   CFSTR(kIOInterfaceNamePrefix));
    res = CFStringCompare(prefix1, prefix2, 0);
    if (res != kCFCompareEqualTo) {
	return (res);
    }
    unit1 = CFDictionaryGetValue((CFDictionaryRef)val1,
				 CFSTR(kIOInterfaceUnit));
    unit2 = CFDictionaryGetValue((CFDictionaryRef)val2,
				 CFSTR(kIOInterfaceUnit));
    return (CFNumberCompare(unit1, unit2, NULL));
}

static void
writeInterfaceListForModel(SCPreferencesRef prefs, CFStringRef old_model)
{
    Boolean			ok;
    CFPropertyListRef		plist;
    SCPreferencesRef		savedPrefs;
    CFStringRef			savedPrefsID;

    savedPrefsID = CFStringCreateWithFormat(NULL, NULL, CFSTR("%@-%@"),
					    INTERFACES_DEFAULT_CONFIG,
					    old_model);
    savedPrefs = SCPreferencesCreate(NULL, CFSTR(MY_PLUGIN_NAME ":writeInterfaceListForModel"), savedPrefsID);
    CFRelease(savedPrefsID);
    if (savedPrefs == NULL) {
	SC_log(LOG_NOTICE, "SCPreferencesCreate(\"NetworkInterfaces-<model>.plist\") failed: %s", SCErrorString(SCError()));
	return;
    }

    plist = SCPreferencesPathGetValue(prefs, CFSTR("/"));
    ok = SCPreferencesPathSetValue(savedPrefs, CFSTR("/"), plist);
    if (!ok) {
	SC_log(LOG_NOTICE, "SCPreferencesPathSetValue() failed: %s", SCErrorString(SCError()));
    }

    ok = SCPreferencesCommitChanges(savedPrefs);
    CFRelease(savedPrefs);
    if (!ok) {
	SC_log(LOG_NOTICE, "SCPreferencesCommitChanges(\"NetworkInterfaces-<model>.plist\") failed: %s", SCErrorString(SCError()));
    }

    return;
}

static void
writeInterfaceList(CFArrayRef if_list)
{
    CFArrayRef		cur_list;
    CFStringRef		new_model;
    SCPreferencesRef	ni_prefs;
    CFStringRef		old_model;

    if (!isA_CFArray(if_list)) {
	return;
    }

    ni_prefs = SCPreferencesCreate(NULL, CFSTR(MY_PLUGIN_NAME ":writeInterfaceList"), INTERFACES_DEFAULT_CONFIG);
    if (ni_prefs == NULL) {
	SC_log(LOG_NOTICE, "SCPreferencesCreate(\"NetworkInterfaces.plist\") failed: %s", SCErrorString(SCError()));
	return;
    }

    cur_list = SCPreferencesGetValue(ni_prefs, INTERFACES);
    if (_SC_CFEqual(cur_list, if_list)) {
	goto done;
    }

    old_model = SCPreferencesGetValue(ni_prefs, MODEL);
    new_model = _SC_hw_model(FALSE);
    if ((old_model != NULL) && !_SC_CFEqual(old_model, new_model)) {
	// if new hardware
	if ((old_model != NULL) && (cur_list != NULL)) {
	    SC_log(LOG_NOTICE, "Hardware model changed\n"
			       "  created on \"%@\"\n"
			       "  now on     \"%@\"",
		   old_model,
		   new_model);

	    // save the interface list that was created on "other" hardware
	    writeInterfaceListForModel(ni_prefs, old_model);
	}
    }

    SCPreferencesSetValue(ni_prefs, MODEL, new_model);
    SCPreferencesSetValue(ni_prefs, INTERFACES, if_list);

    if (cur_list == NULL) {
	const int	new_version	= NETWORK_CONFIGURATION_VERSION;
	CFNumberRef	version;

	version = CFNumberCreate(NULL, kCFNumberIntType, &new_version);
	SCPreferencesSetValue(ni_prefs, kSCPrefVersion, version);
	CFRelease(version);
    }

    if (!SCPreferencesCommitChanges(ni_prefs)) {
	if (SCError() != EROFS) {
	    SC_log(LOG_NOTICE, "SCPreferencesCommitChanges() failed: %s", SCErrorString(SCError()));
	}
	goto done;
    }

done:

    CFRelease(ni_prefs);
    return;
}

static CF_RETURNS_RETAINED CFMutableArrayRef
readInterfaceList()
{
    CFMutableArrayRef 		db_list	= NULL;
    CFArrayRef			if_list;
    SCPreferencesRef		ni_prefs;
    CFStringRef			old_model;

    ni_prefs = SCPreferencesCreate(NULL, CFSTR(MY_PLUGIN_NAME ":readInterfaceList"), INTERFACES_DEFAULT_CONFIG);
    if (ni_prefs == NULL) {
	SC_log(LOG_NOTICE, "SCPreferencesCreate() failed: %s", SCErrorString(SCError()));
	return (NULL);
    }

    if_list = SCPreferencesGetValue(ni_prefs, INTERFACES);
    if_list = isA_CFArray(if_list);

    old_model = SCPreferencesGetValue(ni_prefs, MODEL);
    if (old_model != NULL) {
	CFStringRef new_model;

	new_model = _SC_hw_model(FALSE);
	if (!_SC_CFEqual(old_model, new_model)) {
	    /*
	     * If the interface list was created on other hardware then
	     * we start fresh.
	     */
	    if_list = NULL;
	}
    }

    if (if_list != NULL) {
	CFIndex	n	= CFArrayGetCount(if_list);

	db_list = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	for (CFIndex i = 0; i < n; i++) {
	    CFDictionaryRef	dict;

	    dict = CFArrayGetValueAtIndex(if_list, i);
	    if (isA_CFDictionary(dict) &&
		CFDictionaryContainsKey(dict, CFSTR(kIOInterfaceNamePrefix)) &&
		CFDictionaryContainsKey(dict, CFSTR(kIOInterfaceUnit)) &&
		CFDictionaryContainsKey(dict, CFSTR(kIOMACAddress))) {
		    CFArrayAppendValue(db_list, dict);
	    }
	}
    }

    if (db_list != NULL) {
	CFIndex	n	= CFArrayGetCount(db_list);

	if (n > 1) {
	    CFArraySortValues(db_list, CFRangeMake(0, n), if_unit_compare, NULL);
	}
    }

    CFRelease(ni_prefs);
    return (db_list);
}

static CF_RETURNS_RETAINED CFMutableArrayRef
previouslyActiveInterfaces()
{
    CFMutableArrayRef	active;
    CFIndex		n;

    if (S_dblist == NULL) {
	return NULL;
    }

    active = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

    n = CFArrayGetCount(S_dblist);
    for (CFIndex i = 0; i < n; i++) {
	CFDictionaryRef	if_dict;

	if_dict = CFArrayGetValueAtIndex(S_dblist, i);
	if (CFDictionaryContainsKey(if_dict, CFSTR(kSCNetworkInterfaceActive))) {
	    CFMutableDictionaryRef	new_dict;

	    new_dict = CFDictionaryCreateMutableCopy(NULL, 0, if_dict);
	    CFDictionaryRemoveValue(new_dict, CFSTR(kSCNetworkInterfaceActive));
	    CFArraySetValueAtIndex(S_dblist, i, new_dict);
	    CFArrayAppendValue(active, new_dict);
	    CFRelease(new_dict);
	}
    }

    return active;
}

static void
updateInterfaces(void);

static void
updateStore(void)
{
    CFStringRef		key;

    key = SCDynamicStoreKeyCreate(NULL, CFSTR("%@" MY_PLUGIN_NAME), kSCDynamicStoreDomainPlugin);
    (void)SCDynamicStoreSetValue(NULL, key, S_state);
    CFRelease(key);

    return;
}

#if	TARGET_OS_OSX
static void
updateBondInterfaceConfiguration(SCPreferencesRef prefs)
{
    CFArrayRef	interfaces;

    interfaces = SCBondInterfaceCopyAll(prefs);
    if ((interfaces != NULL) && (CFArrayGetCount(interfaces) == 0)) {
	CFRelease(interfaces);
	interfaces = NULL;
    }

    if (_SC_CFEqual(S_bonds, interfaces)) {
	// if no change
	if (interfaces != NULL) CFRelease(interfaces);
	return;
    }

    if (S_bonds != NULL) CFRelease(S_bonds);
    S_bonds = interfaces;

    if (!_SCBondInterfaceUpdateConfiguration(prefs)) {
	SC_log(LOG_NOTICE, "_SCBondInterfaceUpdateConfiguration() failed: %s",
	       SCErrorString(SCError()));
    }

    return;
}
#endif	// TARGET_OS_OSX

static void
updateBridgeInterfaceConfiguration(SCPreferencesRef prefs)
{
    CFArrayRef	interfaces;

    interfaces = SCBridgeInterfaceCopyAll(prefs);
    if ((interfaces != NULL) && (CFArrayGetCount(interfaces) == 0)) {
	CFRelease(interfaces);
	interfaces = NULL;
    }

    if (_SC_CFEqual(S_bridges, interfaces)) {
	// if no change
	if (interfaces != NULL) CFRelease(interfaces);
	return;
    }

    if (S_bridges != NULL) CFRelease(S_bridges);
    S_bridges = interfaces;

    if (!_SCBridgeInterfaceUpdateConfiguration(prefs)) {
	SC_log(LOG_NOTICE, "_SCBridgeInterfaceUpdateConfiguration() failed: %s",
	       SCErrorString(SCError()));
    }

    return;
}

static void
updateVLANInterfaceConfiguration(SCPreferencesRef prefs)
{
    CFArrayRef	interfaces;

    interfaces = SCVLANInterfaceCopyAll(prefs);
    if ((interfaces != NULL) && (CFArrayGetCount(interfaces) == 0)) {
	CFRelease(interfaces);
	interfaces = NULL;
    }

    if (_SC_CFEqual(S_vlans, interfaces)) {
	// if no change
	if (interfaces != NULL) CFRelease(interfaces);
	return;
    }

    if (S_vlans != NULL) CFRelease(S_vlans);
    S_vlans = interfaces;

    if (!_SCVLANInterfaceUpdateConfiguration(prefs)) {
	SC_log(LOG_NOTICE, "_SCVLANInterfaceUpdateConfiguration() failed: %s",
	       SCErrorString(SCError()));
    }

    return;
}

static void
updateVirtualNetworkInterfaceConfiguration(SCPreferencesRef		prefs,
					   SCPreferencesNotification	notificationType,
					   void				*info)
{
#pragma unused(info)
    if ((notificationType & kSCPreferencesNotificationApply) != kSCPreferencesNotificationApply) {
	return;
    }

    if (prefs == NULL) {
	// if a new interface has been "named"
	prefs = S_prefs;
	if (S_bonds != NULL) {
	    CFRelease(S_bonds);
	    S_bonds = NULL;
	}
	if (S_bridges != NULL) {
	    CFRelease(S_bridges);
	    S_bridges = NULL;
	}
	if (S_vlans != NULL) {
	    CFRelease(S_vlans);
	    S_vlans = NULL;
	}
    }

#if	TARGET_OS_OSX
    updateBondInterfaceConfiguration  (prefs);
#endif	// TARGET_OS_OSX
    updateBridgeInterfaceConfiguration(prefs);
    updateVLANInterfaceConfiguration  (prefs);

    // we are finished with current prefs, wait for changes
    SCPreferencesSynchronize(prefs);

    return;
}

#if	TARGET_OS_OSX

static void
updateBTPANInformation(const void *value, void *context)
{
#pragma unused(context)
    CFDataRef		addr;
    CFDictionaryRef	dict    = (CFDictionaryRef)value;
    CFStringRef		if_name;
    CFDictionaryRef	info;
    CFStringRef		name;

    if_name = CFDictionaryGetValue(dict, CFSTR(kIOBSDNameKey));
    if (!isA_CFString(if_name)) {
	// if no BSD name
	return;
    }

    info = CFDictionaryGetValue(dict, CFSTR(kSCNetworkInterfaceInfo));
    if (!isA_CFDictionary(info)) {
	// if no SCNetworkInterface info
	return;
    }

    name = CFDictionaryGetValue(info, kSCPropUserDefinedName);
    if (!isA_CFString(name) || !CFEqual(name, CFSTR(BT_PAN_NAME))) {
	// if not BT-PAN interface
	return;
    }

    CFDictionaryAddValue(S_state, kInterfaceNamerKey_BT_PAN_Name, if_name);

    addr = CFDictionaryGetValue(dict, CFSTR(kIOMACAddress));
    if (isA_CFData(addr)) {
	CFDictionaryAddValue(S_state, kInterfaceNamerKey_BT_PAN_Mac, addr);
    }

    return;
}
#endif	// TARGET_OS_OSX

static CFDictionaryRef
createInterfaceDict(SCNetworkInterfaceRef interface, CFArrayRef matchingMACs)
{
    CFMutableDictionaryRef	new_if;
    CFTypeRef			val;

    new_if = CFDictionaryCreateMutable(NULL,
				       0,
				       &kCFTypeDictionaryKeyCallBacks,
				       &kCFTypeDictionaryValueCallBacks);

    val = _SCNetworkInterfaceCopyInterfaceInfo(interface);
    if (val != NULL) {
	CFDictionarySetValue(new_if, CFSTR(kSCNetworkInterfaceInfo), val);
	CFRelease(val);
    }

    val = _SCNetworkInterfaceGetIOPath(interface);
    if (val != NULL) {
	CFDictionarySetValue(new_if, CFSTR(kIOPathMatchKey), val);
    }

    val = _SCNetworkInterfaceGetIOInterfaceNamePrefix(interface);
    if (val != NULL) {
	CFDictionarySetValue(new_if, CFSTR(kIOInterfaceNamePrefix), val);
    }

    val = _SCNetworkInterfaceGetIOInterfaceType(interface);
    if (val != NULL) {
	CFDictionarySetValue(new_if, CFSTR(kIOInterfaceType), val);
    }

    val = _SCNetworkInterfaceGetIOInterfaceUnit(interface);
    if (val != NULL) {
	CFDictionarySetValue(new_if, CFSTR(kIOInterfaceUnit), val);
    }

    val = _SCNetworkInterfaceGetHardwareAddress(interface);
    if (val != NULL) {
	CFDictionarySetValue(new_if, CFSTR(kIOMACAddress), val);
    }

    val = SCNetworkInterfaceGetBSDName(interface);
    if (val != NULL) {
	CFDictionarySetValue(new_if, CFSTR(kIOBSDNameKey), val);
    }

    val = SCNetworkInterfaceGetInterfaceType(interface);
    if (val != NULL) {
	CFDictionarySetValue(new_if, CFSTR(kSCNetworkInterfaceType), val);
    }

    CFDictionarySetValue(new_if,
			 CFSTR(kIOBuiltin),
			 _SCNetworkInterfaceIsBuiltin(interface) ? kCFBooleanTrue : kCFBooleanFalse);

    if (_SCNetworkInterfaceIsHiddenConfiguration(interface)) {
	CFDictionarySetValue(new_if, kSCNetworkInterfaceHiddenConfigurationKey, kCFBooleanTrue);
    }

    if (_SCNetworkInterfaceIsHiddenInterface(interface)) {
	CFDictionarySetValue(new_if, kSCNetworkInterfaceHiddenInterfaceKey, kCFBooleanTrue);
    }

    CFDictionarySetValue(new_if, CFSTR(kSCNetworkInterfaceActive), kCFBooleanTrue);

    if (matchingMACs != NULL) {
	CFDictionarySetValue(new_if, CFSTR(kSCNetworkInterfaceMatchingMACs), matchingMACs);
    }

    return new_if;
}

static CFDictionaryRef
lookupInterfaceByAddress(CFArrayRef db_list, SCNetworkInterfaceRef interface, CFIndex * where)
{
    CFDataRef	addr;
    CFIndex	n;
    CFStringRef	prefix;

    if (db_list == NULL) {
	return (NULL);
    }
    prefix = _SCNetworkInterfaceGetIOInterfaceNamePrefix(interface);
    addr = _SCNetworkInterfaceGetHardwareAddress(interface);
    if (prefix == NULL || addr == NULL) {
	return (NULL);
    }

    n = CFArrayGetCount(db_list);
    for (CFIndex i = 0; i < n; i++) {
	CFDictionaryRef	dict = CFArrayGetValueAtIndex(db_list, i);
	CFDataRef	this_addr;
	CFStringRef	this_prefix;

	this_prefix = CFDictionaryGetValue(dict, CFSTR(kIOInterfaceNamePrefix));
	this_addr = CFDictionaryGetValue(dict, CFSTR(kIOMACAddress));
	if (this_prefix == NULL || this_addr == NULL)
	    continue;

	if (CFEqual(prefix, this_prefix) && CFEqual(addr, this_addr)) {
	    if (where != NULL) {
		*where = i;
	    }
	    return (dict);
	}
    }
    return (NULL);
}

static CFDictionaryRef
lookupInterfaceByName(CFArrayRef db_list, CFStringRef bsdName, CFIndex * where)
{
    CFIndex	n;

    if (db_list == NULL) {
	return (NULL);
    }

    n = CFArrayGetCount(db_list);
    for (CFIndex i = 0; i < n; i++) {
	CFDictionaryRef	dict = CFArrayGetValueAtIndex(db_list, i);
	CFStringRef	name;

	name = CFDictionaryGetValue(dict, CFSTR(kIOBSDNameKey));
	if (_SC_CFEqual(name, bsdName)) {
	    if (where != NULL) {
		*where = i;
	    }
	    return (dict);
	}
    }
    return (NULL);
}

static CFDictionaryRef
lookupInterfaceByUnit(CFArrayRef db_list, SCNetworkInterfaceRef interface, CFIndex * where)
{
    CFIndex	n;
    CFStringRef	prefix;
    CFNumberRef	unit;

    if (db_list == NULL) {
	return (NULL);
    }
    prefix = _SCNetworkInterfaceGetIOInterfaceNamePrefix(interface);
    unit = _SCNetworkInterfaceGetIOInterfaceUnit(interface);
    if (prefix == NULL || unit == NULL) {
	return (NULL);
    }

    n = CFArrayGetCount(db_list);
    for (CFIndex i = 0; i < n; i++) {
	CFDictionaryRef	dict = CFArrayGetValueAtIndex(db_list, i);
	CFStringRef	this_prefix;
	CFNumberRef	this_unit;

	this_prefix = CFDictionaryGetValue(dict, CFSTR(kIOInterfaceNamePrefix));
	this_unit = CFDictionaryGetValue(dict, CFSTR(kIOInterfaceUnit));
	if (this_prefix == NULL || this_unit == NULL) {
	    continue;
	}

	if (CFEqual(prefix, this_prefix) && CFEqual(unit, this_unit)) {
	    if (where != NULL) {
		*where = i;
	    }
	    return (dict);
	}
    }
    return (NULL);
}

typedef struct {
    CFDictionaryRef	    match_info;
    CFStringRef		    match_type;
    CFStringRef		    match_prefix;
    CFBooleanRef	    match_builtin;
    CFMutableArrayRef	    matches;
} matchContext, *matchContextRef;

static CF_RETURNS_RETAINED CFDictionaryRef
thinInterfaceInfo(CFDictionaryRef info)
{
    CFNumberRef	num;
    int		vid;

    if (CFDictionaryGetValueIfPresent(info, CFSTR(kUSBVendorID), (const void **)&num)
	&& isA_CFNumber(num)
	&& CFNumberGetValue(num, kCFNumberIntType, &vid)
	&& (vid == kIOUSBAppleVendorID)) {
	CFMutableDictionaryRef  thin;

	// if this is an Apple USB device than we trust that
	// the non-localized name will be correct.
	thin = CFDictionaryCreateMutableCopy(NULL, 0, info);
	CFDictionaryRemoveValue(thin, CFSTR(kUSBProductString));
	CFDictionaryRemoveValue(thin, CFSTR(kUSBVendorID));
	CFDictionaryRemoveValue(thin, CFSTR(kUSBProductID));
	return thin;
    }

    return CFRetain(info);
}

static Boolean
matchInterfaceInfo(CFDictionaryRef known_info, CFDictionaryRef match_info)
{
    Boolean match;

    match = _SC_CFEqual(known_info, match_info);
    if (!match &&
	isA_CFDictionary(known_info) &&
	isA_CFDictionary(match_info)) {

	// if not an exact match, try thinning
	known_info = thinInterfaceInfo(known_info);
	match_info = thinInterfaceInfo(match_info);
	match = _SC_CFEqual(known_info, match_info);
	if (known_info != NULL) CFRelease(known_info);
	if (match_info != NULL) CFRelease(match_info);
    }

    return match;
}

static void
matchKnown(const void *value, void *context)
{
    CFDictionaryRef	known_dict	= (CFDictionaryRef)value;
    matchContextRef	match_context	= (matchContextRef)context;

    // match prefix
    {
	CFStringRef	known_prefix;

	known_prefix
	    = CFDictionaryGetValue(known_dict,
				   CFSTR(kIOInterfaceNamePrefix));
	if (!_SC_CFEqual(known_prefix, match_context->match_prefix)) {
	    return;
	}
    }

    // match interface type
    {
	CFStringRef	known_type;

	known_type = CFDictionaryGetValue(known_dict, CFSTR(kSCNetworkInterfaceType));
	if (!_SC_CFEqual(known_type, match_context->match_type)) {
	    return;
	}
    }

    // match SCNetworkInterfaceInfo
    {
	CFDictionaryRef	known_info;

	known_info = CFDictionaryGetValue(known_dict, CFSTR(kSCNetworkInterfaceInfo));
	if (!matchInterfaceInfo(known_info, match_context->match_info)) {
	    return;
	}
    }

    // if requested, match [non-]builtin
    if (match_context->match_builtin != NULL) {
	CFBooleanRef	known_builtin;

	known_builtin = CFDictionaryGetValue(known_dict, CFSTR(kIOBuiltin));
	if (!isA_CFBoolean(known_builtin)) {
	    known_builtin = kCFBooleanFalse;
	}
	if (!_SC_CFEqual(known_builtin, match_context->match_builtin)) {
	    return;
	}
    }

    // if we have a match
    if (match_context->matches == NULL) {
	match_context->matches = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    }
    CFArrayAppendValue(match_context->matches, known_dict);

    return;
}

static void
matchUnnamed(const void *value, void *context)
{
    SCNetworkInterfaceRef   known_if	    = (SCNetworkInterfaceRef)value;
    matchContextRef	    match_context   = (matchContextRef)context;

    if (match_context->matches == NULL) {
	return;
    }

    // match interface type
    {
	CFStringRef	known_type;

	known_type = SCNetworkInterfaceGetInterfaceType(known_if);
	if (!_SC_CFEqual(known_type, match_context->match_type)) {
	    return;
	}
    }

    // match SCNetworkInterfaceInfo
    {
	CFDictionaryRef	known_info;
	Boolean		match;

	known_info = _SCNetworkInterfaceCopyInterfaceInfo(known_if);
	match = matchInterfaceInfo(known_info, match_context->match_info);
	if (known_info != NULL) CFRelease(known_info);
	if (!match) {
	    return;
	}
    }

    // if requested, match [non-]builtin
    if (match_context->match_builtin != NULL) {
	CFBooleanRef	known_builtin;

	known_builtin = _SCNetworkInterfaceIsBuiltin(known_if) ? kCFBooleanTrue
							       : kCFBooleanFalse;
	if (!_SC_CFEqual(known_builtin, match_context->match_builtin)) {
	    return;
	}
    }

    // if we have a match
    CFRelease(match_context->matches);
    match_context->matches = NULL;

    return;
}

static Boolean
interfaceExists(CFStringRef prefix, CFNumberRef unit)
{
    Boolean		found	    = FALSE;
    CFDictionaryRef	match_dict;
    CFStringRef		match_keys[2];
    CFTypeRef		match_vals[2];
    CFDictionaryRef	matching;



    io_registry_entry_t	entry		= MACH_PORT_NULL;
    io_iterator_t	iterator	= MACH_PORT_NULL;
    kern_return_t	kr;
    mach_port_t		masterPort	= MACH_PORT_NULL;

    kr = IOMainPort(bootstrap_port, &masterPort);
    if (kr != KERN_SUCCESS) {
	SC_log(LOG_ERR, "IOMainPort returned 0x%x", kr);
	goto error;
    }

    // look for kIONetworkInterface with matching prefix and unit
    match_keys[0] = CFSTR(kIOInterfaceNamePrefix);
    match_vals[0] = prefix;
    match_keys[1] = CFSTR(kIOInterfaceUnit);
    match_vals[1] = unit;
    match_dict = CFDictionaryCreate(NULL,
				    (const void **)match_keys,
				    (const void **)match_vals,
				    2,
				    &kCFTypeDictionaryKeyCallBacks,
				    &kCFTypeDictionaryValueCallBacks);

    match_keys[0] = CFSTR(kIOProviderClassKey);
    match_vals[0] = CFSTR(kIONetworkInterfaceClass);
    match_keys[1] = CFSTR(kIOPropertyMatchKey);
    match_vals[1] = match_dict;
    matching = CFDictionaryCreate(NULL,
				  (const void **)match_keys,
				  (const void **)match_vals,
				  sizeof(match_keys)/sizeof(match_keys[0]),
				  &kCFTypeDictionaryKeyCallBacks,
				  &kCFTypeDictionaryValueCallBacks);
    CFRelease(match_dict);

    // note: the "matching" dictionary will be consumed by the following
    kr = IOServiceGetMatchingServices(masterPort, matching, &iterator);
    if ((kr != kIOReturnSuccess) || (iterator == MACH_PORT_NULL)) {
	// if no interface
	goto error;
    }

    entry = IOIteratorNext(iterator);
    if (entry == MACH_PORT_NULL) {
	// if no interface
	goto error;
    }

    found = TRUE;

error:
    if (masterPort != MACH_PORT_NULL) {
	mach_port_deallocate(mach_task_self(), masterPort);
    }
    if (entry != MACH_PORT_NULL) {
	IOObjectRelease(entry);
    }
    if (iterator != MACH_PORT_NULL) {
	IOObjectRelease(iterator);
    }

    return (found);
}

/*
 * lookupMatchingInterface
 *
 * Looks at the interfaces that have already been [or need to be] named with
 * the goal of allowing a system using a single network interface/adaptor of
 * a given type (vendor, model, ...) to not care about the specific adaptor
 * that is used (i.e. swapping dongle's is OK).  Once a system has had more
 * than one interface/adaptor connected at the same time than we assume that
 * the network configuration is being setup for multi-homing that should be
 * maintained.
 *
 * If no matches are found or if more than one match is found, return NULL.
 * If a single match is found, return the match.
 */
static CFDictionaryRef
lookupMatchingInterface(SCNetworkInterfaceRef	interface,
			CFArrayRef		db_list,	// already named
			CFArrayRef		if_list,	// to be named
			CFIndex			if_list_index,
			CFBooleanRef		builtin)
{
    CFStringRef	    if_type;
    CFStringRef	    if_prefix;
    CFDictionaryRef match	    = NULL;
    matchContext    match_context;

    if_type = SCNetworkInterfaceGetInterfaceType(interface);
    if (if_type == NULL) {
	return NULL;
    }
    if_prefix = _SCNetworkInterfaceGetIOInterfaceNamePrefix(interface);
    if (if_prefix == NULL) {
	return NULL;
    }

    match_context.match_type	= if_type;
    match_context.match_prefix	= if_prefix;
    match_context.match_info	= _SCNetworkInterfaceCopyInterfaceInfo(interface);
    match_context.match_builtin	= builtin;
    match_context.matches	= NULL;

    // check for matches to interfaces that have already been named
    // ... and append each match that we find to match_context.matches
    if (db_list != NULL) {
	CFArrayApplyFunction(db_list,
			     CFRangeMake(0, CFArrayGetCount(db_list)),
			     matchKnown,
			     &match_context);
    }

    // check for matches to interfaces that will be named
    // ... and CFRelease match_context.matches if we find another network
    //     interface of the same type that also needs to be named
    if (if_list != NULL) {
	CFIndex	   if_list_count;

	if_list_count = CFArrayGetCount(if_list);
	if (if_list_index < if_list_count) {
	    CFArrayApplyFunction(if_list,
				 CFRangeMake(if_list_index, if_list_count - if_list_index),
				 matchUnnamed,
				 &match_context);
	}
    }

    // check if we have a single match
    if (match_context.matches != NULL) {
	if (CFArrayGetCount(match_context.matches) == 1) {
	    match = CFArrayGetValueAtIndex(match_context.matches, 0);
	}
	CFRelease(match_context.matches);
    }

    if (match != NULL) {
	Boolean		active	= TRUE;
	CFStringRef	name;

	name = CFDictionaryGetValue(match, CFSTR(kIOBSDNameKey));
	if (isA_CFString(name)) {
	    CFStringRef	    prefix;
	    CFNumberRef	    unit;

	    prefix = CFDictionaryGetValue(match, CFSTR(kIOInterfaceNamePrefix));
	    unit   = CFDictionaryGetValue(match, CFSTR(kIOInterfaceUnit));
	    if (isA_CFString(prefix) && isA_CFNumber(unit)) {
		if (!interfaceExists(prefix, unit)) {
		    active = FALSE;
		}
	    }
	}

	if (active) {
	    match = NULL;
	}
    }

    if (match_context.match_info != NULL) CFRelease(match_context.match_info);
    return match;
}

static void
insertInterface(CFMutableArrayRef db_list, SCNetworkInterfaceRef interface, CFDictionaryRef db_dict_match)
{
    CFIndex		i;
    CFDictionaryRef	if_dict;
    CFStringRef		if_name;
    CFStringRef		if_prefix;
    CFNumberRef		if_unit;
    CFArrayRef		matchingMACs	= NULL;
    CFIndex		n		= CFArrayGetCount(db_list);
    CFComparisonResult	res;

    if_name = SCNetworkInterfaceGetBSDName(interface);
    if (if_name != NULL) {
	addTimestamp(S_state, if_name);
    }

    if (!_SCNetworkInterfaceIsBuiltin(interface) && (db_dict_match != NULL)) {
	CFDataRef	addr_cur;
	CFDataRef	addr_old;

	matchingMACs = CFDictionaryGetValue(db_dict_match, CFSTR(kSCNetworkInterfaceMatchingMACs));
	if (matchingMACs != NULL) {
	    CFRetain(matchingMACs);
	}

	addr_old = CFDictionaryGetValue(db_dict_match, CFSTR(kIOMACAddress));
	addr_cur = _SCNetworkInterfaceGetHardwareAddress(interface);
	if ((addr_old != NULL) && (addr_cur != NULL) && !CFEqual(addr_old, addr_cur)) {
	    CFMutableArrayRef	matching_new;

	    // if MAC address changed, add previous MAC to history
	    if (matchingMACs != NULL) {
		matching_new = CFArrayCreateMutableCopy(NULL, 0, matchingMACs);
		CFRelease(matchingMACs);

		// remove duplicates of the now current MAC from history
		i = CFArrayGetFirstIndexOfValue(matching_new, CFRangeMake(0, CFArrayGetCount(matching_new)), addr_cur);
		if (i != kCFNotFound) {
		    CFArrayRemoveValueAtIndex(matching_new, i);
		}

		// remove duplicates of the previous MAC from history before re-inserting
		i = CFArrayGetFirstIndexOfValue(matching_new, CFRangeMake(0, CFArrayGetCount(matching_new)), addr_old);
		if (i != kCFNotFound) {
		    CFArrayRemoveValueAtIndex(matching_new, i);
		}
	    } else {
		matching_new = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	    }
	    CFArrayInsertValueAtIndex(matching_new, 0, addr_old);

	    // limit history size
#define	MATCHING_HISTORY_MAXLEN	32
	    for (i = CFArrayGetCount(matching_new); i > MATCHING_HISTORY_MAXLEN; i--) {
		CFArrayRemoveValueAtIndex(matching_new, i - 1);
	    }

	    matchingMACs = matching_new;
	}
    }

    if_dict = createInterfaceDict(interface, matchingMACs);
    if (matchingMACs != NULL) {
	CFRelease(matchingMACs);
    }

    if_prefix = _SCNetworkInterfaceGetIOInterfaceNamePrefix(interface);
    if_unit = _SCNetworkInterfaceGetIOInterfaceUnit(interface);
    if ((if_prefix == NULL) || (if_unit == NULL)) {
	CFRelease(if_dict);
	return;
    }

    for (i = 0; i < n; i++) {
	CFStringRef	db_prefix;
	CFNumberRef	db_unit;
	CFDictionaryRef	dict	= CFArrayGetValueAtIndex(db_list, i);

	db_prefix = CFDictionaryGetValue(dict, CFSTR(kIOInterfaceNamePrefix));
	db_unit = CFDictionaryGetValue(dict, CFSTR(kIOInterfaceUnit));
	res = CFStringCompare(if_prefix, db_prefix, 0);
	if (res == kCFCompareLessThan
	    || (res == kCFCompareEqualTo
		&& (CFNumberCompare(if_unit, db_unit, NULL)
		    == kCFCompareLessThan))) {
	    CFArrayInsertValueAtIndex(db_list, i, if_dict);
	    CFRelease(if_dict);
	    return;
	}
    }

    CFArrayAppendValue(S_dblist, if_dict);

#if	TARGET_OS_OSX
    updateBTPANInformation(if_dict, NULL);
#endif	// TARGET_OS_OSX

    CFRelease(if_dict);
    return;
}

static void
removeInterface(CFMutableArrayRef db_list, SCNetworkInterfaceRef interface, CFDictionaryRef *matched)
{
    CFDictionaryRef	db_dict;
    int			n		= 0;
    CFIndex		where;

    // remove any dict that has our prefix+addr
    while (TRUE) {
	db_dict = lookupInterfaceByAddress(db_list, interface, &where);
	if (db_dict == NULL) {
	    break;
	}
	if ((matched != NULL) && (*matched == NULL)) {
	    *matched = CFRetain(db_dict);
	}
	CFArrayRemoveValueAtIndex(db_list, where);
	n++;
    }

    // remove any dict that has the same prefix+unit
    while (TRUE) {
	db_dict = lookupInterfaceByUnit(db_list, interface, &where);
	if (db_dict == NULL) {
	    break;
	}
	if ((matched != NULL) && (*matched == NULL)) {
	    *matched = CFRetain(db_dict);
	}
	CFArrayRemoveValueAtIndex(db_list, where);
	n++;
    }

    if (n > 1) {
	SC_log(LOG_ERR, "Multiple interfaces removed from database (n = %d, %@)", n, interface);
    }

    return;
}

static Boolean
persistInterfaceName(SCNetworkInterfaceRef interface)
{
    return !_SCNetworkInterfaceIsEphemeral(interface);
}

static void
replaceInterface(SCNetworkInterfaceRef interface)
{
    CFDictionaryRef	matched	= NULL;

    if (S_dblist == NULL) {
	S_dblist = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    } else {
	// remove any matching interfaces
	removeInterface(S_dblist, interface, &matched);
    }

    // [re-]insert the new interface
    insertInterface(S_dblist, interface, matched);

    if (matched != NULL) {
	CFRelease(matched);
    }

    return;
}

static int
getNextUnitForPrefix(CFStringRef if_prefix, int requested)
{
    CFIndex	n;

    if (S_dblist == NULL) {
	return requested;
    }

    n = CFArrayGetCount(S_dblist);
    for (CFIndex i = 0; i < n; i++) {
	CFDictionaryRef	dict = CFArrayGetValueAtIndex(S_dblist, i);
	CFStringRef	prefix;

	prefix = CFDictionaryGetValue(dict, CFSTR(kIOInterfaceNamePrefix));
	if (CFEqual(prefix, if_prefix)) {
	    int		u;
	    CFNumberRef	unit;

	    unit = CFDictionaryGetValue(dict, CFSTR(kIOInterfaceUnit));
	    if (!isA_CFNumber(unit) ||
		!CFNumberGetValue(unit, kCFNumberIntType, &u)) {
		u = 0;
	    }

	    if (u < requested) {
		// if we have not yet found our starting unit #
		continue;
	    }

	    if (u == requested) {
		// our starting (or now proposed) unit # is "in use" so
		// let's keep searching
		requested++;
		continue;
	    }

	    // we've found a unit # gap ... so let's re-assign it!
	}
    }

    return requested;
}

/*
 * Function: ensureInterfaceHasUnit
 * Purpose:
 *   Ensure that the SCNetworkInterfaceRef has a unit number.  If it doesn't,
 *   release the interface and return NULL.
 */
static SCNetworkInterfaceRef
ensureInterfaceHasUnit(SCNetworkInterfaceRef net_if)
{
    if (net_if != NULL
	&& _SCNetworkInterfaceGetIOInterfaceUnit(net_if) == NULL) {
	CFRelease(net_if);
	net_if = NULL;
    }
    return (net_if);
}

static kern_return_t
registerInterfaceWithIORegistryEntryID(io_connect_t connect,
				       uint64_t	    entryID,
				       CFNumberRef  unit,
				       const int    command)
{
    CFDataRef			data;
    CFMutableDictionaryRef	dict;
    kern_return_t		kr;
    CFNumberRef			num;

    dict = CFDictionaryCreateMutable(NULL, 0,
				     &kCFTypeDictionaryKeyCallBacks,
				     &kCFTypeDictionaryValueCallBacks);
    num = CFNumberCreate(NULL, kCFNumberIntType, &command);
    CFDictionarySetValue(dict, CFSTR(kIONetworkStackUserCommandKey), num);
    CFRelease(num);
    data = CFDataCreate(NULL, (void *) &entryID, sizeof(entryID));
    CFDictionarySetValue(dict, CFSTR(kIORegistryEntryIDKey), data);
    CFRelease(data);
    CFDictionarySetValue(dict, CFSTR(kIOInterfaceUnit), unit);
    kr = IOConnectSetCFProperties(connect, dict);
    CFRelease(dict);
    return kr;
}

static SCNetworkInterfaceRef
copyInterfaceForIORegistryEntryID(uint64_t entryID)
{
    io_registry_entry_t		entry		= MACH_PORT_NULL;
    SCNetworkInterfaceRef	interface	= NULL;
    io_iterator_t		iterator	= MACH_PORT_NULL;
    kern_return_t		kr;
    mach_port_t			masterPort	= MACH_PORT_NULL;

    kr = IOMainPort(bootstrap_port, &masterPort);
    if (kr != KERN_SUCCESS) {
	SC_log(LOG_ERR, "IOMainPort returned 0x%x", kr);
	goto error;
    }

    kr = IOServiceGetMatchingServices(masterPort,
				      IORegistryEntryIDMatching(entryID),
				      &iterator);
    if ((kr != KERN_SUCCESS) || (iterator == MACH_PORT_NULL)) {
	SC_log(LOG_NOTICE, "IOServiceGetMatchingServices(0x%llx) returned 0x%x/%d",
	       entryID,
	       kr,
	       iterator);
	goto error;
    }

    entry = IOIteratorNext(iterator);
    if (entry == MACH_PORT_NULL) {
	SC_log(LOG_NOTICE, "IORegistryEntryIDMatching(0x%llx) failed", entryID);
	goto error;
    }

    interface = _SCNetworkInterfaceCreateWithIONetworkInterfaceObject(entry);

 error:
    if (masterPort != MACH_PORT_NULL) {
	mach_port_deallocate(mach_task_self(), masterPort);
    }
    if (entry != MACH_PORT_NULL) {
	IOObjectRelease(entry);
    }
    if (iterator != MACH_PORT_NULL) {
	IOObjectRelease(iterator);
    }
    return (interface);
}

static SCNetworkInterfaceRef
copyNamedInterfaceForIORegistryEntryID(uint64_t entryID)
{
    SCNetworkInterfaceRef	net_if;

    net_if = copyInterfaceForIORegistryEntryID(entryID);
    return (ensureInterfaceHasUnit(net_if));
}


static void
displayInterface(SCNetworkInterfaceRef interface)
{
    CFStringRef		addr;
    CFStringRef		name;
    CFStringRef		prefix;
    CFNumberRef		unit;

    name = SCNetworkInterfaceGetBSDName(interface);
    unit = _SCNetworkInterfaceGetIOInterfaceUnit(interface);
    prefix = _SCNetworkInterfaceGetIOInterfaceNamePrefix(interface);
    addr = SCNetworkInterfaceGetHardwareAddressString(interface);

    SC_log(LOG_INFO, "  %s%@%sPrefix: %@, %s%@%sMAC address: %@",
	  (name != NULL) ? "BSD Name: " : "",
	  (name != NULL) ? name : CFSTR(""),
	  (name != NULL) ? ", " : "",
	  prefix,
	  (unit != NULL) ? "Unit: " : "",
	  (unit != NULL) ? (CFTypeRef)unit : (CFTypeRef)CFSTR(""),
	  (unit != NULL) ? ", " : "",
	  (addr != NULL) ? addr : CFSTR("?"));
}

static Boolean
builtinAvailable(SCNetworkInterfaceRef	interface,	// new interface
		 CFNumberRef		if_unit)	// desired unit
{
    CFIndex	i;
    CFStringRef if_prefix = _SCNetworkInterfaceGetIOInterfaceNamePrefix(interface);
    CFIndex	n;

    n = (S_dblist != NULL) ? CFArrayGetCount(S_dblist) : 0;
    for (i = 0; i < n; i++) {
	CFStringRef	    if_path;
	CFDictionaryRef	    known_dict	    = CFArrayGetValueAtIndex(S_dblist, i);
	CFStringRef	    known_path;
	CFStringRef	    known_prefix;
	CFNumberRef	    known_unit;

	known_prefix = CFDictionaryGetValue(known_dict, CFSTR(kIOInterfaceNamePrefix));
	if (!_SC_CFEqual(if_prefix, known_prefix)) {
	    continue;	// if not the same interface prefix
	}

	known_unit = CFDictionaryGetValue(known_dict, CFSTR(kIOInterfaceUnit));
	if (!_SC_CFEqual(if_unit, known_unit)) {
	    continue;	// if not the same interface unit
	}

	if_path    = _SCNetworkInterfaceGetIOPath(interface);
	known_path = CFDictionaryGetValue(known_dict, CFSTR(kIOPathMatchKey));
	if (!_SC_CFEqual(if_path, known_path)) {
	    // if different IORegistry path
	    return FALSE;
	}

	// if same prefix+unit+path
	return TRUE;
    }

    // if interface prefix+unit not found
    return TRUE;
}

static int
builtinCount(CFArrayRef if_list, CFIndex last, CFStringRef if_prefix)
{
    CFIndex	i;
    int		n	= 0;

    for (i = 0; i < last; i++) {
	SCNetworkInterfaceRef	builtin_if;
	CFStringRef		builtin_prefix;

	builtin_if   = CFArrayGetValueAtIndex(if_list, i);
	builtin_prefix = _SCNetworkInterfaceGetIOInterfaceNamePrefix(builtin_if);
	if (CFEqual(if_prefix, builtin_prefix)) {
	    if (_SCNetworkInterfaceIsBuiltin(builtin_if)) {
		n++;	// if built-in interface
	    }
	}
    }

    return n;
}


#pragma mark -
#pragma mark Internet Sharing configuration support


#if	TARGET_OS_OSX

static SCPreferencesRef	nat_configuration	= NULL;		// com.apple.nat.plist
static SCPreferencesRef	nat_preferences		= NULL;		// preferences.plist


static void
sharingConfigurationClose(void)
{
    if (nat_configuration != NULL) {
	CFRelease(nat_configuration);
	nat_configuration = NULL;
    }

    if (nat_preferences != NULL) {
	CFRelease(nat_preferences);
	nat_preferences = NULL;
    }

    return;
}


static Boolean
sharingConfigurationUsesInterface(CFStringRef bsdName, Boolean keepOpen)
{
    CFDictionaryRef	config;
    Boolean		isShared	= FALSE;

    if (nat_configuration == NULL) {
	nat_configuration = SCPreferencesCreate(NULL, CFSTR(MY_PLUGIN_NAME ":sharingConfigurationUsesInterface"), CFSTR("com.apple.nat.plist"));
	if (nat_configuration == NULL) {
	    return FALSE;
	}
    }

    config = SCPreferencesGetValue(nat_configuration, CFSTR("NAT"));
    if (isA_CFDictionary(config)) {
	CFBooleanRef	bVal			= NULL;
	Boolean		enabled			= FALSE;
	CFStringRef	sharedFromServiceID	= NULL;
	CFArrayRef	sharedToInterfaces	= NULL;

	if (CFDictionaryGetValueIfPresent(config,
					  CFSTR("Enabled"),
					  (const void **)&bVal) &&
	    isA_CFBoolean(bVal)) {
	    enabled = CFBooleanGetValue(bVal);
	}

	if (enabled &&
	    CFDictionaryGetValueIfPresent(config,
					  CFSTR("SharingDevices"),
					  (const void **)&sharedToInterfaces) &&
	    isA_CFArray(sharedToInterfaces)) {
	    CFIndex	n;

	    // if "To computers using" interfaces configured
	    n = CFArrayGetCount(sharedToInterfaces);
	    for (CFIndex i = 0; i < n; i++) {
		CFStringRef	sharedToInterface_bsdName;

		sharedToInterface_bsdName = CFArrayGetValueAtIndex(sharedToInterfaces, i);
		if (_SC_CFEqual(bsdName, sharedToInterface_bsdName)) {
		    isShared = TRUE;
		    break;
		}
	    }
	}

	if (enabled &&
	    !isShared &&
	    CFDictionaryGetValueIfPresent(config,
					  CFSTR("PrimaryService"),
					  (const void **)&sharedFromServiceID) &&
	    isA_CFString(sharedFromServiceID)) {
	    if (nat_preferences == NULL) {
		nat_preferences = SCPreferencesCreateCompanion(nat_configuration, NULL);
	    }
	    if (nat_preferences != NULL) {
		SCNetworkServiceRef	sharedFromService;

		// if "Share your connection from" service configured
		sharedFromService = SCNetworkServiceCopy(nat_preferences, sharedFromServiceID);
		if (sharedFromService != NULL) {
		    CFStringRef			sharedFromService_bsdName;
		    SCNetworkInterfaceRef	sharedFromService_interface;

		    sharedFromService_interface = SCNetworkServiceGetInterface(sharedFromService);
		    sharedFromService_bsdName = SCNetworkInterfaceGetBSDName(sharedFromService_interface);
		    isShared = _SC_CFEqual(bsdName, sharedFromService_bsdName);
		    CFRelease(sharedFromService);
		}
	    }
	}
    }

    if (!keepOpen) {
	sharingConfigurationClose();
    }

    return isShared;
}

#endif	// TARGET_OS_OSX


#pragma mark -
#pragma mark Interface monitoring (e.g. watch for "detach")


typedef struct WatchedInfo	*WatchedInfoRef;

typedef void (*InterfaceUpdateCallBack)	(
    CFDataRef			watched,
    natural_t			messageType,
    void			*messageArgument
);

typedef struct {
    SCNetworkInterfaceRef	interface;
    io_service_t		interface_node;
    io_object_t			notification;
    InterfaceUpdateCallBack	callback;
} WatchedInfo;

static void
watcherRelease(CFDataRef watched);

static void
updateWatchedInterface(void *refCon, io_service_t service, natural_t messageType, void *messageArgument)
{
#pragma unused(service)
    switch (messageType) {
	case kIOMessageServiceIsTerminated : {		// if [watched] interface yanked
	    SCNetworkInterfaceRef	remove;
	    CFDataRef			watched		= (CFDataRef)refCon;
	    WatchedInfo			*watchedInfo	= (WatchedInfo *)(void *)CFDataGetBytePtr(watched);

	    remove = watchedInfo->interface;
	    if (_SCNetworkInterfaceIsBuiltin(remove)) {
		// if built-in, keep
		remove = NULL;
	    } else if (!_SCNetworkInterfaceIsApplePreconfigured(remove)) {
		// if not pre-configured, keep
		remove = NULL;
	    } else {
		// if not built-in *and* pre-configured
		CFRetain(remove);
	    }

#if	TARGET_OS_OSX
	    if (remove != NULL) {
		CFStringRef	bsdName;

		bsdName = SCNetworkInterfaceGetBSDName(remove);
		if ((bsdName != NULL) && sharingConfigurationUsesInterface(bsdName, FALSE)) {
		    // if referenced in the Internet Sharing configuration, keep
		    CFRelease(remove);
		    remove = NULL;
		}
	    }
#endif	// TARGET_OS_OSX

	    CFRetain(watched);
	    watchedInfo->callback(watched, messageType, messageArgument);
	    watcherRelease(watched);
	    CFRelease(watched);

	    if (remove != NULL) {
		SC_log(LOG_INFO, "Interface released unit %@ (from database)",
		       _SCNetworkInterfaceGetIOInterfaceUnit(remove));
		removeInterface(S_dblist, remove, NULL);
		CFRelease(remove);

		// update the DB with the [remaining] interfaces that have been named
		writeInterfaceList(S_dblist);
	    }

	    break;
	}

	default :
	    return;
    }

    return;
}

static io_service_t
getIOServiceObjectForEntryID(uint64_t entryID)
{
    CFDictionaryRef	matching;
    io_service_t	interface_node;

    matching = IORegistryEntryIDMatching(entryID);
    interface_node = IOServiceGetMatchingService(kIOMainPortDefault, matching);
    return (interface_node);
}

static CFDataRef
watcherCreate(SCNetworkInterfaceRef interface, InterfaceUpdateCallBack callback)
{
    uint64_t		entryID;
    io_service_t	interface_node;
    kern_return_t	kr;
    CFMutableDataRef	watched;
    WatchedInfo		*watchedInfo;

    // get the IORegistry node
    entryID = _SCNetworkInterfaceGetIORegistryEntryID(interface);
    interface_node = getIOServiceObjectForEntryID(entryID);
    if (interface_node == MACH_PORT_NULL) {
	// interface no longer present
	return NULL;
    }

    // create [locked/trusted] interface watcher
    watched = CFDataCreateMutable(NULL, sizeof(WatchedInfo));
    CFDataSetLength(watched, sizeof(WatchedInfo));
    watchedInfo = (WatchedInfo *)(void *)CFDataGetBytePtr(watched);
    memset(watchedInfo, 0, sizeof(*watchedInfo));

    // retain interface
    watchedInfo->interface = CFRetain(interface);

    // ... and the interface node
    watchedInfo->interface_node = interface_node;

    // ... and set the callback
    watchedInfo->callback = callback;

    kr = IOServiceAddInterestNotification(S_notify,			// IONotificationPortRef
					  watchedInfo->interface_node,	// io_service_t
					  kIOGeneralInterest,		// interestType
					  updateWatchedInterface,	// IOServiceInterestCallback
					  (void *)watched,		// refCon
					  &watchedInfo->notification);	// notification
    if (kr != KERN_SUCCESS) {
	SC_log(LOG_ERR,
	       "IOServiceAddInterestNotification() failed, kr =  0x%x",
	       kr);
	watcherRelease(watched);
	CFRelease(watched);
	return NULL;
    }

    return watched;
}

static void
watcherRelease(CFDataRef watched)
{
    WatchedInfo	*watchedInfo	= (WatchedInfo *)(void *)CFDataGetBytePtr(watched);
    my_IOObjectRelease(&watchedInfo->notification);
    my_IOObjectRelease(&watchedInfo->interface_node);
    my_CFRelease(&watchedInfo->interface);

    return;
}

static Boolean
isWatchedInterface(CFArrayRef watchedInterfaces, SCNetworkInterfaceRef interface)
{
    CFIndex	n;

    n = (watchedInterfaces != NULL) ? CFArrayGetCount(watchedInterfaces) : 0;
    for (CFIndex i = 0; i < n; i++) {
	CFDataRef	watched		= CFArrayGetValueAtIndex(watchedInterfaces, i);
	WatchedInfo	*watchedInfo	= (WatchedInfo *)(void *)CFDataGetBytePtr(watched);

	if (CFEqual((watchedInfo->interface), interface)) {
	    return TRUE;
	}
    }

    return FALSE;
}


#pragma mark -
#pragma mark Locked device support [macOS]


#if	TARGET_OS_OSX
static void
shareLocked(void)
{
    CFIndex	n;

    n = (S_locked != NULL) ? CFArrayGetCount(S_locked) : 0;
    if (n > 0) {
	CFMutableArrayRef	locked;

	locked = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

	for (CFIndex i = 0; i < n; i++) {
	    CFStringRef		addr;
	    CFStringRef		name;
	    CFStringRef		path;
	    CFStringRef		str;
	    CFDataRef		watched		= CFArrayGetValueAtIndex(S_locked, i);
	    WatchedInfo		*watchedInfo	= (WatchedInfo *)(void *)CFDataGetBytePtr(watched);

	    name = SCNetworkInterfaceGetLocalizedDisplayName(watchedInfo->interface);
	    addr = SCNetworkInterfaceGetHardwareAddressString(watchedInfo->interface);
	    path = _SCNetworkInterfaceGetIOPath(watchedInfo->interface);
	    str = CFStringCreateWithFormat(NULL, NULL, CFSTR("%@: %@: %@"),
					   (name != NULL) ? name : CFSTR("?"),
					   (addr != NULL) ? addr : CFSTR("?"),
					   path);
	    CFArrayAppendValue(locked, str);
	    CFRelease(str);
	}

	CFDictionarySetValue(S_state, kInterfaceNamerKey_LockedInterfaces, locked);
	CFRelease(locked);
    } else {
	CFDictionaryRemoveValue(S_state, kInterfaceNamerKey_LockedInterfaces);
    }

    updateStore();

    return;
}

static boolean_t
blockNewInterfaces()
{
    static boolean_t	    allow	= TRUE;
    static dispatch_once_t  once;

    dispatch_once(&once, ^{
	if (os_variant_is_darwinos(IFNAMER_SUBSYSTEM)) {
	    return;
	}
	allow = InterfaceNamerControlPrefsAllowNewInterfaces();
    });

    return !allow;
}

static boolean_t
isConsoleLocked()
{
    CFArrayRef		console_sessions;
    boolean_t		locked		    = FALSE;
    io_registry_entry_t	root;

    root = IORegistryGetRootEntry(kIOMainPortDefault);
    console_sessions = IORegistryEntryCreateCFProperty(root,
						       CFSTR(kIOConsoleUsersKey),
						       NULL,
						       0);
    if (isA_CFArray(console_sessions)) {
	CFIndex	n;

	n = CFArrayGetCount(console_sessions);
	for (CFIndex i = 0; i < n; i++) {
	    CFBooleanRef	isLocked;
	    CFBooleanRef	isLoginDone;
	    CFBooleanRef	onConsole;
	    CFDictionaryRef	session;

	    session = CFArrayGetValueAtIndex(console_sessions, i);
	    if (!isA_CFDictionary(session)) {
		// if not dictionary
		continue;
	    }

	    if (!CFDictionaryGetValueIfPresent(session,
					       CFSTR(kIOConsoleSessionOnConsoleKey),
					       (const void **)&onConsole) ||
		!isA_CFBoolean(onConsole) ||
		!CFBooleanGetValue(onConsole)) {
		// if not "on console" session
		continue;
	    }

	    if ((n > 1) &&
		CFDictionaryGetValueIfPresent(session,
					      CFSTR(kIOConsoleSessionLoginDoneKey),
					      (const void **)&isLoginDone) &&
		isA_CFBoolean(isLoginDone) &&
		!CFBooleanGetValue(isLoginDone)) {
		// if @ loginwindow
		SC_log(LOG_INFO, "multiple sessions, console @ loginwindow");
		locked = TRUE;
		goto done;
	    }

	    if (CFDictionaryGetValueIfPresent(session,
					      CFSTR(kIOConsoleSessionScreenIsLockedKey),
					      (const void **)&isLocked) &&
		isA_CFBoolean(isLocked) &&
		CFBooleanGetValue(isLocked)) {
		// if screen locked
		SC_log(LOG_INFO, "console screen locked");
		locked = TRUE;
		goto done;
	    }
	}
    }

    SC_log(LOG_INFO, "console not locked");

  done :

    if (console_sessions != NULL) {
	CFRelease(console_sessions);
    }
    IOObjectRelease(root);

    return locked;
}

//#define	ENABLE_LOCKED_CONSOLE_INTERFACE_NOTIFICATIONS
#ifdef	ENABLE_LOCKED_CONSOLE_INTERFACE_NOTIFICATIONS

static CFUserNotificationRef	userNotification;
static CFRunLoopSourceRef	userRls;

static void
lockedNotification_remove(void)
{
    if (userRls != NULL) {
	CFRunLoopSourceInvalidate(userRls);
	userRls = NULL;
    }

    if (userNotification != NULL) {
	SInt32	status;

	status = CFUserNotificationCancel(userNotification);
	if (status != 0) {
	    SC_log(LOG_ERR,
		   "CFUserNotificationCancel() failed, status=%d",
		   (int)status);
	}
	CFRelease(userNotification);
	userNotification = NULL;
    }

    return;
}

#define	MY_ICON_PATH	"/System/Library/PreferencePanes/Network.prefPane/Contents/Resources/Network.icns"

static void
lockedNotification_reply(CFUserNotificationRef userNotification, CFOptionFlags response_flags)
{
#pragma unused(userNotification)

    CFIndex		n;

    n = (S_locked != NULL) ? CFArrayGetCount(S_locked) : 0;
    for (CFIndex i = 0; i < n; i++) {
	CFDataRef	watched		= CFArrayGetValueAtIndex(S_locked, i);
	WatchedInfo	*watchedInfo	= (WatchedInfo *)(void *)CFDataGetBytePtr(watched);

	// process user response
	switch (response_flags & 0x3) {
	    case kCFUserNotificationDefaultResponse: {
		// if OK'd, [re-]process new interfaces
		if (i == 0) {
		    SC_log(LOG_INFO, "Reprocessing %ld [locked] interface%s", n, n == 1 ? "" : "s");

		    if (S_iflist == NULL) {
			S_iflist = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
		    }
		}

		// add the interface to those newly discovered
		CFArrayAppendValue(S_iflist, watchedInfo->interface);
		break;
	    }
	    default: {
		// if cancelled, ignore [remaining] new interfaces
		SC_log(LOG_INFO, "[locked] interface ignored");
		SC_log(LOG_INFO, "  path = %@", _SCNetworkInterfaceGetIOPath(watchedInfo->interface));
		break;
	    }
	}

	// stop watching
	watcherRelease(watched);
    }

    if (S_locked != NULL) {
	CFRelease(S_locked);
	S_locked = NULL;
    }

    lockedNotification_remove();

    if (S_iflist != NULL) {
	updateInterfaces();
    }

    return;
}

static void
lockedNotification_add(void)
{
    CFBundleRef			bundle;
    CFMutableDictionaryRef	dict;
    SInt32			error	= 0;
    CFMutableArrayRef		message;
    CFIndex			n;
    CFURLRef			url	= NULL;

    n = (S_locked != NULL) ? CFArrayGetCount(S_locked) : 0;
    if (n == 0) {
	// no locked interfaces, no notification needed
	return;
    }

    dict = CFDictionaryCreateMutable(NULL,
				     0,
				     &kCFTypeDictionaryKeyCallBacks,
				     &kCFTypeDictionaryValueCallBacks);

    // set localization URL
    bundle = _SC_CFBundleGet();
    if (bundle != NULL) {
	url = CFBundleCopyBundleURL(bundle);
    }
    if (url != NULL) {
	// set URL
	CFDictionarySetValue(dict, kCFUserNotificationLocalizationURLKey, url);
	CFRelease(url);
    } else {
	SC_log(LOG_ERR, "can't find bundle");
	goto done;
    }

    // set icon URL
    url = CFURLCreateFromFileSystemRepresentation(NULL,
						  (const UInt8 *)MY_ICON_PATH,
						  sizeof(MY_ICON_PATH) - 1,
						  FALSE);
    if (url != NULL) {
	CFDictionarySetValue(dict, kCFUserNotificationIconURLKey, url);
	CFRelease(url);
    }

    // header
    CFDictionarySetValue(dict,
			 kCFUserNotificationAlertHeaderKey,
			 (n == 1) ? CFSTR("LOCKED_SINGLE_INTERFACE_HEADER")
				  : CFSTR("LOCKED_MULTIPLE_INTERFACES_HEADER"));

    // message
    message = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    CFArrayAppendValue(message,
		       (n == 1) ? CFSTR("LOCKED_SINGLE_INTERFACE_MESSAGE")
				: CFSTR("LOCKED_MULTIPLE_INTERFACES_MESSAGE"));
    for (CFIndex i = 0; i < n; i++) {
	CFStringRef		name;
	CFStringRef		str;
	CFDataRef		watched		= CFArrayGetValueAtIndex(S_locked, i);
	WatchedInfo		*watchedInfo	= (WatchedInfo *)(void *)CFDataGetBytePtr(watched);

	name = SCNetworkInterfaceGetLocalizedDisplayName(watchedInfo->interface);
	str = CFStringCreateWithFormat(NULL, NULL, CFSTR("\n\t%@"), name);
	CFArrayAppendValue(message, str);
	CFRelease(str);
    }
    CFDictionarySetValue(dict, kCFUserNotificationAlertMessageKey, message);
    CFRelease(message);

    // button titles
    CFDictionarySetValue(dict,
			 kCFUserNotificationDefaultButtonTitleKey,
			 CFSTR("LOCKED_INTERFACES_IGNORE"));
    CFDictionarySetValue(dict,
			 kCFUserNotificationAlternateButtonTitleKey,
			 (n == 1) ? CFSTR("LOCKED_SINGLE_INTERFACE_ADD")
				  : CFSTR("LOCKED_MULTIPLE_INTERFACES_ADD"));

    // create and post notification
    userNotification = CFUserNotificationCreate(NULL,
						0,
						kCFUserNotificationNoteAlertLevel,
						&error,
						dict);
    if (userNotification == NULL) {
	SC_log(LOG_ERR, "CFUserNotificationCreate() failed: %d", (int)error);
	goto done;
    }

    // establish callback
    userRls = CFUserNotificationCreateRunLoopSource(NULL,
						    userNotification,
						    lockedNotification_reply,
						    0);
    if (userRls == NULL) {
	SC_log(LOG_ERR, "CFUserNotificationCreateRunLoopSource() failed");
	CFRelease(userNotification);
	userNotification = NULL;
	goto done;
    }
    CFRunLoopAddSource(CFRunLoopGetCurrent(), userRls,  kCFRunLoopDefaultMode);

  done :

    if (dict != NULL) CFRelease(dict);
    return;
}

static void
lockedNotification_update(void)
{
    // if present, remove current notification
    lockedNotification_remove();

    // post notification (if needed)
    lockedNotification_add();

    return;
}

#endif	// ENABLE_LOCKED_CONSOLE_INTERFACE_NOTIFICATIONS

static void
lockedInterfaceUpdated(CFDataRef watched, natural_t messageType, void *messageArgument)
{
#pragma unused(messageArgument)
    Boolean	updated		= FALSE;
    WatchedInfo	*watchedInfo	= (WatchedInfo *)(void *)CFDataGetBytePtr(watched);

    switch (messageType) {
	case kIOMessageServiceIsTerminated : {		// if [locked] interface yanked
	    SC_log(LOG_INFO, "[locked] interface removed");
	    SC_log(LOG_INFO, "  path = %@", _SCNetworkInterfaceGetIOPath(watchedInfo->interface));

	    if (S_locked != NULL) {
		CFIndex	i;
		CFIndex	n	= CFArrayGetCount(S_locked);

		i = CFArrayGetFirstIndexOfValue(S_locked, CFRangeMake(0, n), watched);
		if (i != kCFNotFound) {
		    CFArrayRemoveValueAtIndex(S_locked, i);
		    if (CFArrayGetCount(S_locked) == 0) {
			CFRelease(S_locked);
			S_locked = NULL;
		    }
		    updated = TRUE;
		}
	    }

	    break;
	}

	default :
	    return;
    }

    if (updated) {
#ifdef	ENABLE_LOCKED_CONSOLE_INTERFACE_NOTIFICATIONS
	// update user notification after interface removed
	lockedNotification_update();
#endif	// ENABLE_LOCKED_CONSOLE_INTERFACE_NOTIFICATIONS

	// post info about interfaces not added because the console is locked
	shareLocked();
    }

    return;
}

static void
watchLockedInterface(SCNetworkInterfaceRef interface)
{
    Boolean	updated	= FALSE;
    CFDataRef	watched;

    watched = watcherCreate(interface, lockedInterfaceUpdated);
    if (watched != NULL) {
	SC_log(LOG_INFO, "watching [locked] interface");
	SC_log(LOG_INFO, "  path = %@", _SCNetworkInterfaceGetIOPath(interface));

	if (S_locked == NULL) {
	    S_locked = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	}
	CFArrayAppendValue(S_locked, watched);
	CFRelease(watched);
	updated = TRUE;
    }

    if (updated) {
	// post info about interfaces not added because the console is locked
	shareLocked();

#ifdef	ENABLE_LOCKED_CONSOLE_INTERFACE_NOTIFICATIONS
	// post/update user notification with new interface
	lockedNotification_update();
#endif	// ENABLE_LOCKED_CONSOLE_INTERFACE_NOTIFICATIONS
    }

    return;
}

static void
addWatchedLockedInterface(SCNetworkInterfaceRef interface, CFStringRef path,
			  const char *str)
{
    CFStringRef		addr;

    addr = SCNetworkInterfaceGetHardwareAddressString(interface);
    SC_log(LOG_NOTICE,
	   "Console locked, network interface%s ignored: path = %@, addr = %@",
	   str,  path, (addr != NULL) ? addr : CFSTR("?"));
    watchLockedInterface(interface);
}

#endif	// TARGET_OS_OSX


#pragma mark -
#pragma mark Trust required support [iOS]

#if	TARGET_OS_IPHONE

#include <SoftLinking/WeakLinking.h>
WEAK_LINK_FORCE_IMPORT(lockdown_is_host_trusted);
WEAK_LINK_FORCE_IMPORT(kLockdownNotificationHostAttached);
WEAK_LINK_FORCE_IMPORT(kLockdownNotificationHostDetached);
WEAK_LINK_FORCE_IMPORT(kLockdownNotificationTrustedHostAttached);
WEAK_LINK_FORCE_IMPORT(kLockdownNotificationTrustedPTPAttached);

static Boolean
haveLockdown()
{
    Boolean		haveLibrary;

    haveLibrary = ((lockdown_is_host_trusted != NULL) &&
		   (&kLockdownNotificationHostAttached != NULL) &&
		   (&kLockdownNotificationHostDetached != NULL) &&
		   (&kLockdownNotificationTrustedHostAttached != NULL) &&
		   (&kLockdownNotificationTrustedPTPAttached != NULL)
		  );
    return haveLibrary;
}

static void
shareExcluded()
{
    CFIndex	n;

    n = (S_trustRequired != NULL) ? CFArrayGetCount(S_trustRequired) : 0;
    if ((n > 0) && !S_trustedHostAttached) {
	CFMutableArrayRef	excluded;

	// if we have interfaces that require not [yet] granted "trust".

	excluded = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

	for (CFIndex i = 0; i < n; i++) {
	    CFStringRef	bsdName;
	    CFDataRef	watched		= CFArrayGetValueAtIndex(S_trustRequired, i);
	    WatchedInfo	*watchedInfo	= (WatchedInfo *)(void *)CFDataGetBytePtr(watched);

	    bsdName = SCNetworkInterfaceGetBSDName(watchedInfo->interface);
	    if (bsdName == NULL) {
		SC_log(LOG_NOTICE, "[trust required] interface w/no BSD name not excluded");
		SC_log(LOG_NOTICE, "  interface = %@", watchedInfo->interface);
		continue;
	    }
	    CFArrayAppendValue(excluded, bsdName);
	}

	CFDictionarySetValue(S_state, kInterfaceNamerKey_ExcludedInterfaces, excluded);
	CFRelease(excluded);
    } else {
	CFDictionaryRemoveValue(S_state, kInterfaceNamerKey_ExcludedInterfaces);
    }

    updateStore();

    return;
}

static dispatch_queue_t
trustRequired_queue()
{
    static dispatch_once_t	once;
    static dispatch_queue_t	q;

    dispatch_once(&once, ^{
	q = dispatch_queue_create("Trust Required queue", NULL);
    });

    return q;

}

// runs on "Trust Required" dispatch queue
static void
trustRequiredNotification_update(CFRunLoopRef rl, CFStringRef reason)
{
    Boolean			changed		= FALSE;
    CFStringRef			error		= NULL;
    CFIndex			n;
    Boolean			trusted;

    /*
     * determine whether the device has "trusted" the host (or other device)
     */
    trusted = lockdown_is_host_trusted(MY_PLUGIN_ID, NULL, &error);
    n = (S_trustRequired != NULL) ? CFArrayGetCount(S_trustRequired) : 0;
    if ((S_trustedHostCount != n) || (S_trustedHostAttached != trusted)) {
	changed = TRUE;
    }

    SC_log(LOG_INFO, "%@, trusted = %s%s%@, %ld interface%s)%s",
	   reason,
	   trusted ? "Yes" : "No",
	   (error != NULL) ? ", error = " : "",
	   (error != NULL) ? error : CFSTR(""),
	   n,
	   (n == 1) ? "" : "s",
	   changed ? " *" : "");

    if (changed) {
	S_trustedHostAttached = trusted;
	S_trustedHostCount = n;
	CFRunLoopPerformBlock(rl, kCFRunLoopDefaultMode, ^{
	    shareExcluded();
	});
	CFRunLoopWakeUp(rl);
    }

    if (error != NULL) {
	CFRelease(error);
    }

    return;
}

static void
trustRequiredInterfaceUpdated(CFDataRef watched, natural_t messageType, void *messageArgument)
{
#pragma unused(messageArgument)
    Boolean	updated		= FALSE;
    WatchedInfo	*watchedInfo	= (WatchedInfo *)(void *)CFDataGetBytePtr(watched);

    switch (messageType) {
	case kIOMessageServiceIsTerminated : {		// if [locked] interface yanked
	    SC_log(LOG_INFO, "[trust required] interface removed");
	    SC_log(LOG_INFO, "  path = %@", _SCNetworkInterfaceGetIOPath(watchedInfo->interface));

	    if (S_trustRequired != NULL) {
		CFIndex	i;
		CFIndex	n	= CFArrayGetCount(S_trustRequired);

		i = CFArrayGetFirstIndexOfValue(S_trustRequired, CFRangeMake(0, n), watched);
		if (i != kCFNotFound) {
		    CFArrayRemoveValueAtIndex(S_trustRequired, i);
		    if (CFArrayGetCount(S_trustRequired) == 0) {
			CFRelease(S_trustRequired);
			S_trustRequired = NULL;
		    }
		    updated = TRUE;
		}
	    }

	    break;
	}

	default :
	    return;
    }

    if (updated) {
	CFRunLoopRef	rl	= CFRunLoopGetCurrent();

	CFRetain(rl);
	dispatch_async(trustRequired_queue(), ^{
	    trustRequiredNotification_update(rl, CFSTR("TrustRequired interface removed"));
	    CFRelease(rl);
	});
    }

    return;
}

static void
watchTrustedStatus(CFStringRef notification, CFStringRef reason)
{
    const char *	key;
    int			notify_token	= -1;
    uint32_t		ret;
    CFRunLoopRef	rl	= CFRunLoopGetCurrent();

    key = CFStringGetCStringPtr(notification, kCFStringEncodingUTF8);
    assert(key != NULL);

    CFRetain(rl);
    CFRetain(reason);
    ret = notify_register_dispatch(key,
				   &notify_token,
				   trustRequired_queue(),
				   ^(int token){
#pragma unused(token)
	trustRequiredNotification_update(rl, reason);
    });
    if (ret != NOTIFY_STATUS_OK) {
	SC_log(LOG_ERR, "notify_register_dispatch(%@) failed: %u", notification, ret);
	CFRelease(rl);
	CFRelease(reason);
    }

    return;
}

static void
updateTrustRequiredInterfaces(CFArrayRef interfaces)
{
    CFIndex	n;
    Boolean	updated	= FALSE;

    n = (interfaces != NULL) ? CFArrayGetCount(interfaces) : 0;
    for (CFIndex i = 0; i < n; i++) {
	SCNetworkInterfaceRef	interface;

	interface = CFArrayGetValueAtIndex(interfaces, i);
	if (_SCNetworkInterfaceIsTrustRequired(interface) &&
	    !isWatchedInterface(S_trustRequired, interface)) {
	    CFDataRef	watched;

	    watched = watcherCreate(interface, trustRequiredInterfaceUpdated);
	    if (watched != NULL) {
		CFStringRef	bsdName;

		bsdName = SCNetworkInterfaceGetBSDName(interface);
		if (bsdName != NULL) {
		    SC_log(LOG_INFO, "watching [trust required] interface: %@", bsdName);
		} else {
		    SC_log(LOG_INFO, "watching [trust required] interface w/no BSD name");
		    SC_log(LOG_INFO, "  interface = %@", interface);
		}

		if (S_trustRequired == NULL) {
		    S_trustRequired = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
		}
		CFArrayAppendValue(S_trustRequired, watched);
		CFRelease(watched);
		updated = TRUE;
	    }
	}
    }

    if (updated) {
	static dispatch_once_t	once;
	CFRunLoopRef		rl	= CFRunLoopGetCurrent();

	dispatch_once(&once, ^{
	    // watch for "Host attached"
	    watchTrustedStatus(kLockdownNotificationHostAttached,
			       CFSTR("Host attached"));

	    // watch for "Host detached"
	    watchTrustedStatus(kLockdownNotificationHostDetached,
			       CFSTR("Host detached"));

	    // watch for "Trusted host attached"
	    watchTrustedStatus(kLockdownNotificationTrustedHostAttached,
			       CFSTR("Trusted Host attached"));

	    // watch for "Trusted PDP attached"
	    watchTrustedStatus(kLockdownNotificationTrustedPTPAttached,
			       CFSTR("Trusted PTP attached"));
	});

	CFRetain(rl);
	dispatch_async(trustRequired_queue(), ^{
	    trustRequiredNotification_update(rl, CFSTR("TrustRequired interface added"));
	    CFRelease(rl);
	});
    }

    return;
}
#endif	// TARGET_OS_IPHONE


#pragma mark -
#pragma mark Pre-configured interface support


static void
sharePreconfigured()
{
    CFIndex	n;

    n = (S_preconfigured != NULL) ? CFArrayGetCount(S_preconfigured) : 0;
    if (n > 0) {
	CFMutableArrayRef	preconfigured;

	preconfigured = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

	for (CFIndex i = 0; i < n; i++) {
	    CFStringRef	bsdName;
	    CFDataRef	watched		= CFArrayGetValueAtIndex(S_preconfigured, i);
	    WatchedInfo	*watchedInfo	= (WatchedInfo *)(void *)CFDataGetBytePtr(watched);

	    bsdName = SCNetworkInterfaceGetBSDName(watchedInfo->interface);
	    if (bsdName == NULL) {
		SC_log(LOG_NOTICE, "pre-configured interface w/no BSD name");
		SC_log(LOG_NOTICE, "  interface = %@", watchedInfo->interface);
		continue;
	    }
	    CFArrayAppendValue(preconfigured, bsdName);
	}

	CFDictionarySetValue(S_state, kInterfaceNamerKey_PreConfiguredInterfaces, preconfigured);
	CFRelease(preconfigured);
    } else {
	CFDictionaryRemoveValue(S_state, kInterfaceNamerKey_PreConfiguredInterfaces);
    }

    updateStore();

    return;
}

static void
preconfiguredInterfaceUpdated(CFDataRef watched, natural_t messageType, void *messageArgument)
{
#pragma unused(messageArgument)
    Boolean	updated		= FALSE;
    WatchedInfo	*watchedInfo	= (WatchedInfo *)(void *)CFDataGetBytePtr(watched);
    switch (messageType) {
	case kIOMessageServiceIsTerminated : {		// if [locked] interface yanked
	    CFStringRef	bsdName;

	    bsdName = SCNetworkInterfaceGetBSDName(watchedInfo->interface);
	    if (bsdName != NULL) {
		SC_log(LOG_INFO, "[pre-configured] interface removed: %@", bsdName);
	    } else {
		SC_log(LOG_INFO, "[pre-configured] interface w/no BSD name removed");
		SC_log(LOG_INFO, "  interface = %@", watchedInfo->interface);
	    }

	    if (S_preconfigured != NULL) {
		CFIndex	i;
		CFIndex	n	= CFArrayGetCount(S_preconfigured);

		i = CFArrayGetFirstIndexOfValue(S_preconfigured, CFRangeMake(0, n), watched);
		if (i != kCFNotFound) {
		    CFArrayRemoveValueAtIndex(S_preconfigured, i);
		    if (CFArrayGetCount(S_preconfigured) == 0) {
			CFRelease(S_preconfigured);
			S_preconfigured = NULL;
		    }
		    updated = TRUE;
		}
	    }

	    break;
	}

	default :
	    return;
    }

    if (updated) {
	sharePreconfigured();
    }

    return;
}

static void
updatePreConfiguredInterfaces(CFArrayRef interfaces)
{
    CFIndex	n;
    Boolean	updated	= FALSE;

    n = (interfaces != NULL) ? CFArrayGetCount(interfaces) : 0;
    for (CFIndex i = 0; i < n; i++) {
	SCNetworkInterfaceRef	interface;

	interface = CFArrayGetValueAtIndex(interfaces, i);
	if (_SCNetworkInterfaceIsApplePreconfigured(interface) &&
	    !isWatchedInterface(S_preconfigured, interface)) {
	    CFDataRef	watched;

	    watched = watcherCreate(interface, preconfiguredInterfaceUpdated);
	    if (watched != NULL) {
		CFStringRef	bsdName;

		bsdName = SCNetworkInterfaceGetBSDName(interface);
		if (bsdName != NULL) {
		    SC_log(LOG_INFO, "watching [pre-configured] interface: %@", bsdName);
		} else {
		    SC_log(LOG_INFO, "watching [pre-configured] interface w/no BSD name");
		    SC_log(LOG_INFO, "  interface = %@", interface);
		}

		if (S_preconfigured == NULL) {
		    S_preconfigured = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
		}
		CFArrayAppendValue(S_preconfigured, watched);
		CFRelease(watched);
		updated = TRUE;
	    }
	}
    }

    if (updated) {
	sharePreconfigured();
    }

    return;
}


#pragma mark -
#pragma mark Interface naming


static __inline__ boolean_t
isQuiet(void)
{
    return (S_quiet == MACH_PORT_NULL);
}

static Boolean
wasPreviouslyUsedInterface(CFDictionaryRef dbdict, SCNetworkInterfaceRef interface)
{
    CFArrayRef	matchingMACs;

    matchingMACs = CFDictionaryGetValue(dbdict, CFSTR(kSCNetworkInterfaceMatchingMACs));
    if (matchingMACs != NULL) {
	CFDataRef	addr;

	addr = _SCNetworkInterfaceGetHardwareAddress(interface);
	if (addr != NULL) {
	    if (CFArrayContainsValue(matchingMACs,
				     CFRangeMake(0, CFArrayGetCount(matchingMACs)),
				     addr)) {
		return TRUE;
	    }
	}
    }

    return FALSE;
}

static SCNetworkInterfaceRef
nameAndCopyInterfaceOnce(SCNetworkInterfaceRef interface,
			 Boolean is_known, CFNumberRef unit)
{
    uint64_t 			entryID;
    kern_return_t		kr;
    int				naming_command;
    SCNetworkInterfaceRef	new_interface = NULL;
    CFStringRef			path;

    path = _SCNetworkInterfaceGetIOPath(interface);
    entryID = _SCNetworkInterfaceGetIORegistryEntryID(interface);
    naming_command = is_known ? kIONetworkStackRegisterInterfaceWithUnit
	    : kIONetworkStackRegisterInterfaceWithLowestUnit;
    kr = registerInterfaceWithIORegistryEntryID(S_connect,
						entryID,
						unit,
						naming_command);
    new_interface = copyNamedInterfaceForIORegistryEntryID(entryID);
    if (new_interface != NULL) {
	// interface named successfully
	SC_log(LOG_INFO, "%s interface named\n"
	       "  path = %@\n"
	       "  unit = %@",
	       is_known ? "Known" : "New",
	       path,
	       unit);
    } else {
	SC_log(LOG_NOTICE,
	       "failed to name %s interface, kr=0x%x\n"
	       "  path = %@\n"
	       "  id   = 0x%llx\n"
	       "  unit = %@",
	       is_known ? "Known" : "New",
	       kr,
	       path,
	       entryID,
	       unit);
	displayInterface(interface);
    }
    return (new_interface);
}

static SCNetworkInterfaceRef
nameAndCopyInterface(SCNetworkInterfaceRef interface,
		     Boolean is_known, CFNumberRef unit)
{
    SCNetworkInterfaceRef	new_interface = NULL;

    for (int try = 1; try <= 3; try++) {
	if (try != 1) {
	    usleep((useconds_t)50 * 1000);	// sleep between attempts
	}
	new_interface = nameAndCopyInterfaceOnce(interface, is_known, unit);
	if (new_interface != NULL) {
	    break;
	}
	if (!is_known) {
	    // only retry if the interface is known
	    break;
	}
    }
    return (new_interface);
}


#define DEFERRED_NAMING_MAX_TRIES	30

//#define TEST_DEFERRED_NAMING	1

#ifdef TEST_DEFERRED_NAMING
/*
 * Function: shouldDeferNamingInterface
 * Purpose:
 *   Return whether the interface should defer naming. Helps
 *   test deferred naming logic without having to reproduce
 *   the difficult conditions under which actual naming failures occur.
 */
static inline Boolean
shouldDeferNamingInterface(SCNetworkInterfaceRef interface)
{
    Boolean		defer = FALSE;
    CFStringRef		name;

    name = SCNetworkInterfaceGetLocalizedDisplayName(interface);
    if (name != NULL) {
	CFRange	range;

	/* defer naming anything with "USB" in the name */
	range = CFStringFind(name, CFSTR("USB"), 0);
	if (range.location != kCFNotFound) {
	    SC_log(LOG_NOTICE, "%s: %@", __func__, name);
	    defer = TRUE;
	}
    }
    return (defer);
}

#define DEFERRED_NAMING_FAILURE_INITIAL	(DEFERRED_NAMING_MAX_TRIES / 3)

typedef CF_ENUM(uint32_t, DeferredNamingFailure) {
	kDeferredNamingFailureNone	= 0,	/* don't fail */
	kDeferredNamingFailureInitial	= 1,	/* fail initial attempts */
	kDeferredNamingFailureAll	= 2,	/* fail all attempts */
};

static inline const char *
getDeferredNamingFailureString(DeferredNamingFailure failure)
{
    static const char * strings[] = { "none", "initial", "all" };

    if (failure < (sizeof(strings) / sizeof(strings[0]))) {
	return (strings[failure]);
    }
    return ("<unknown>");
}

/*
 * Function: getNextDeferredNamingFailure
 * Purpose:
 *   Returns the next failure condition. Allows getting complete code
 *   coverage of the various failure scenarios.
 */
static inline DeferredNamingFailure
getNextDeferredNamingFailure(void)
{
    DeferredNamingFailure		failure;
    static DeferredNamingFailure	S_failure;

    failure = S_failure;
    S_failure++;
    if (S_failure > kDeferredNamingFailureAll) {
	S_failure = kDeferredNamingFailureNone;
    }
    return (failure);
}
#endif

typedef struct {
    SCNetworkInterfaceRef	interface;
    io_service_t		node;
    io_object_t			notification;
    CFNumberRef			unit;
    uint32_t			try;
#ifdef TEST_DEFERRED_NAMING
    DeferredNamingFailure	failure;
#endif
} NamingRequest, *NamingRequestRef;

static void
invalidateNamingRequest(NamingRequestRef request)
{
    my_CFRelease(&request->interface);
    my_CFRelease(&request->unit);
    my_IOObjectRelease(&request->notification);
    my_IOObjectRelease(&request->node);
}

static CFMutableArrayRef	S_deferred_list;
static CFRunLoopTimerRef	S_deferred_timer;

static void
entryForNamingRequestChanged(void *refCon, io_service_t service,
			     natural_t messageType, void *messageArgument);
static void
handleDeferredNamingRequests(void);

typedef void (^TimerBlock)(CFRunLoopTimerRef timer);

static void
enableNamingRequestCallBacks(void)
{
    TimerBlock	handler;

    if (S_deferred_timer != NULL) {
	return;
    }
    handler = ^(CFRunLoopTimerRef timer) {
#pragma unused(timer)
	handleDeferredNamingRequests();
    };
#define DEFERRED_NAMING_TIME_INTERVAL	(1.0)
    S_deferred_timer
	= CFRunLoopTimerCreateWithHandler(NULL,
					  0,
					  DEFERRED_NAMING_TIME_INTERVAL,
					  0, /* flags */
					  0, /* order */
					  handler);
    CFRunLoopAddTimer(CFRunLoopGetCurrent(), S_deferred_timer,
		      kCFRunLoopDefaultMode);
    SC_log(LOG_NOTICE, "%s: timer scheduled", __func__);
}

static void
disableNamingRequestCallBacks(void)
{
    if (S_deferred_timer == NULL) {
	return;
    }
    CFRunLoopTimerInvalidate(S_deferred_timer);
    my_CFRelease(&S_deferred_timer);
    SC_log(LOG_NOTICE, "%s: timer cancelled", __func__);
}

static void
removeNamingRequestAtIndex(CFIndex where)
{
    assert(S_deferred_list != NULL);
    assert(where < CFArrayGetCount(S_deferred_list));
    CFArrayRemoveValueAtIndex(S_deferred_list, where);
    if (CFArrayGetCount(S_deferred_list) == 0) {
	my_CFRelease(&S_deferred_list);
	disableNamingRequestCallBacks();
    }
}

static void
handleDeferredNamingRequests(void)
{
    Boolean	interface_named = FALSE;

    if (S_deferred_list == NULL) {
	return;
    }
    SC_log(LOG_NOTICE, "%s", __func__);
    for (CFIndex i = 0, count = CFArrayGetCount(S_deferred_list);
	 i < count; i++) {
	CFDataRef		data;
	SCNetworkInterfaceRef	new_interface = NULL;
	NamingRequestRef	request;

	data = CFArrayGetValueAtIndex(S_deferred_list, i);
	request = (NamingRequestRef)(void *)CFDataGetBytePtr(data);
	request->try++;
#ifdef TEST_DEFERRED_NAMING
	if (request->failure == kDeferredNamingFailureInitial
	    && request->try < DEFERRED_NAMING_FAILURE_INITIAL) {
	    SC_log(LOG_NOTICE,
		   "%s: simulating initial failure, try = %u",
		   __func__, request->try);
	    continue;
	}
	if (request->failure == kDeferredNamingFailureAll) {
	    SC_log(LOG_NOTICE,
		   "%s: simulating continued failure, try = %u",
		   __func__, request->try);
	}
	else
#endif
	    new_interface = nameAndCopyInterfaceOnce(request->interface,
						     TRUE, request->unit);
	if (new_interface != NULL) {
	    SC_log(LOG_NOTICE, "%s: %@ unit %@ SUCCESS",
		   __func__, request->interface,
		   request->unit);
	    invalidateNamingRequest(request);
	    removeNamingRequestAtIndex(i);
	    count--;
	    /* XXX update database? */
	    CFRelease(new_interface);
	    interface_named = TRUE;
	}
	else if (request->try >= DEFERRED_NAMING_MAX_TRIES) {
	    SC_log(LOG_NOTICE, "%s: failed to name %@ unit %@ after %u tries",
		   __func__, request->interface, request->unit, request->try);
	    invalidateNamingRequest(request);
	    removeNamingRequestAtIndex(i);
	    count--;
	}
    }
    if (interface_named) {
	updateInterfaces();
    }
    return;
}
		     
static CFIndex
getNamingRequestIndex(CFDataRef data)
{
    CFIndex		count;

    if (S_deferred_list == NULL) {
	return kCFNotFound;
    }
    count = CFArrayGetCount(S_deferred_list);
    return (CFArrayGetFirstIndexOfValue(S_deferred_list,
					CFRangeMake(0, count), data));
}

static void
scheduleNamingRequest(CFDataRef data)
{
    if (S_deferred_list == NULL) {
	S_deferred_list
	    = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	enableNamingRequestCallBacks();
    }
    CFArrayAppendValue(S_deferred_list, data);
}

static void
addNamingRequest(SCNetworkInterfaceRef interface, CFNumberRef unit)    
{
    CFMutableDataRef	data;
    uint64_t 		entryID;
    kern_return_t	kr;
    io_service_t	node;
    io_object_t		notification;
    NamingRequestRef	request;

    entryID = _SCNetworkInterfaceGetIORegistryEntryID(interface);
    node = getIOServiceObjectForEntryID(entryID);
    if (node == MACH_PORT_NULL) {
	/* interface no longer exists */
	SC_log(LOG_NOTICE, "%s: interface %@ unit %@ no longer exists",
	       __func__, interface, unit);
	return;
    }
    /* register to be notified in case the interface is removed */
    data = CFDataCreateMutable(NULL, sizeof(*request));
    kr = IOServiceAddInterestNotification(S_notify,
					  node,
					  kIOGeneralInterest,
					  entryForNamingRequestChanged,
					  (void *)data,
					  &notification);
    if (kr != KERN_SUCCESS) {
	SC_log(LOG_ERR,
	       "%s: IOServiceAddInterestNotification() failed, kr =  0x%x",
	       __func__, kr);
	CFRelease(data);
	my_IOObjectRelease(&node);
	return;
    }
    CFDataSetLength(data, sizeof(*request));
    request = (NamingRequestRef)(void *)CFDataGetBytePtr(data);
    memset(request, 0, sizeof(*request));
    request->interface = CFRetain(interface);
    request->unit = CFRetain(unit);
    request->node = node;
    request->notification = notification;
    scheduleNamingRequest(data);
    CFRelease(data);
    
    SC_log(LOG_NOTICE, "%s: interface %@ unit %@", __func__, interface, unit);
#ifdef TEST_DEFERRED_NAMING
    if (shouldDeferNamingInterface(interface)) {
	request->failure = getNextDeferredNamingFailure();
	SC_log(LOG_NOTICE, "%s: using naming failure %s",
	       __func__,
	       getDeferredNamingFailureString(request->failure));
    }
#endif
    return;
}


static void
entryForNamingRequestChanged(void *refCon, io_service_t service,
			     natural_t messageType, void *messageArgument)
{
#pragma unused(service)
#pragma unused(messageArgument)
    CFDataRef		data;
    NamingRequestRef	request;
    CFIndex		where;

    if (messageType != kIOMessageServiceIsTerminated) {
	/* only care about removals */
	return;
    }
    data = (CFDataRef)refCon;
    request = (NamingRequestRef)(void *)CFDataGetBytePtr(data);
    SC_log(LOG_NOTICE, "%s: interface %@ unit %@ interface removed", __func__,
	   request->interface, request->unit);
    invalidateNamingRequest(request);
    where = getNamingRequestIndex(data);
    removeNamingRequestAtIndex(where);
    return;
}

static SCNetworkInterfaceRef
assignNameAndCopyInterface(SCNetworkInterfaceRef interface,
			   CFMutableArrayRef if_list, CFIndex i)
{
    CFDictionaryRef 		dbdict = NULL;
    boolean_t			is_builtin = FALSE;
    SCNetworkInterfaceRef	new_interface = NULL;
    CFStringRef			path;
    CFStringRef			prefix;
    CFNumberRef			unit = NULL;
    CFIndex			where;

    path = _SCNetworkInterfaceGetIOPath(interface);
    prefix = _SCNetworkInterfaceGetIOInterfaceNamePrefix(interface);
    is_builtin = _SCNetworkInterfaceIsBuiltin(interface);

    if (!persistInterfaceName(interface)) {
	/*
	 * The interface does not require a persistent name.
	 * Let IONetworkStack find the next available unit by
	 * starting at unit zero.
	 */
	int		zero_val = 0;

#if TARGET_OS_OSX
	if (blockNewInterfaces() &&
	    !_SCNetworkInterfaceIsApplePreconfigured(interface) &&
	    !_SCNetworkInterfaceIsVMNET(interface) &&
	    isConsoleLocked()) {
	    // ignore interface until console is unlocked
	    addWatchedLockedInterface(interface, path, "[ephemeral]");
	    goto done;
	}
#endif // TARGET_OS_OSX
	unit = CFNumberCreate(NULL, kCFNumberIntType, &zero_val);
    } else {
	// interface requires persistent name
	dbdict = lookupInterfaceByAddress(S_dblist, interface, NULL);
	if (dbdict != NULL) {
	    unit = CFDictionaryGetValue(dbdict, CFSTR(kIOInterfaceUnit));
	    CFRetain(unit);

	    SC_log(LOG_INFO, "Interface assigned unit %@ (from database)", unit);
	}

	if ((dbdict == NULL) && !isQuiet()) {
	    // if new interface, wait until quiet before naming
	    addTimestamp(S_state, path);
	    goto done;
	}

	if (dbdict == NULL) {
	    dbdict = lookupMatchingInterface(interface,
					     S_dblist,
					     if_list,
					     i + 1,
					     is_builtin ? kCFBooleanTrue : kCFBooleanFalse);

	    if ((dbdict != NULL) && wasPreviouslyUsedInterface(dbdict, interface)) {
		unit = CFDictionaryGetValue(dbdict, CFSTR(kIOInterfaceUnit));
		CFRetain(unit);
		SC_log(LOG_INFO, "Interface assigned unit %@ (updating database w/previously used interface)", unit);
	    }
	}
#if TARGET_OS_OSX
	if ((unit == NULL) &&
	    !is_builtin &&
	    (dbdict != NULL) &&
	    blockNewInterfaces() &&
	    !_SCNetworkInterfaceIsApplePreconfigured(interface) &&
	    isConsoleLocked()) {
	    // if new (but matching) interface and console locked, ignore
	    addWatchedLockedInterface(interface, path, "[matching]");
	    goto done;
	}
#endif	// TARGET_OS_OSX

	if ((unit == NULL) && (dbdict != NULL)) {
	    unit = CFDictionaryGetValue(dbdict, CFSTR(kIOInterfaceUnit));
	    CFRetain(unit);

	    SC_log(LOG_INFO, "Interface assigned unit %@ (updating database w/new interface)", unit);
	}
	if ((dbdict != NULL) && (S_prev_active_list != NULL)) {
	    // update the list of interfaces that were previously named
	    where = CFArrayGetFirstIndexOfValue(S_prev_active_list,
						CFRangeMake(0, CFArrayGetCount(S_prev_active_list)),
						dbdict);
	    if (where != kCFNotFound) {
		CFArrayRemoveValueAtIndex(S_prev_active_list, where);
	    }
	}
	if (dbdict == NULL) {
	    int 		next_unit	= 0;

	    if (is_builtin) {
		// built-in interface, try to use the reserved slots
		next_unit = builtinCount(if_list, i, prefix);

		// But, before claiming a reserved slot we check to see if the
		// slot had previously been used.  If so, and if the slot had been
		// assigned to the same type of interface, then we will perform a
		// replacement (e.g. assume that this was a board swap).  But, if
		// the new interface is a different type then we assume that the
		// built-in configuration has changed and allocate a new unit from
		// the non-reserved slots.
		unit = CFNumberCreate(NULL, kCFNumberIntType, &next_unit);
		if (!builtinAvailable(interface, unit)) {
		    // if [built-in] unit not available
		    SC_log(LOG_INFO, "Interface not assigned [built-in] unit %@", unit);
		    my_CFRelease(&unit);
		}
	    }
#if TARGET_OS_OSX
	    if (!is_builtin &&
		(unit == NULL) &&
		blockNewInterfaces() &&
		!_SCNetworkInterfaceIsApplePreconfigured(interface) &&
		isConsoleLocked()) {
		// if new interface and console locked, ignore
		addWatchedLockedInterface(interface, path, "[new]");
		goto done;
	    }
#endif	// TARGET_OS_OSX

	    if (unit == NULL) {
		// not built-in (or built-in unit not available), allocate from
		// the non-reserved slots
		next_unit = builtinCount(if_list, CFArrayGetCount(if_list),
					 prefix);
		next_unit = getNextUnitForPrefix(prefix, next_unit);
		unit = CFNumberCreate(NULL, kCFNumberIntType, &next_unit);
	    }

	    SC_log(LOG_INFO, "Interface assigned unit %@ (%s)",
		   unit,
		   is_builtin ? "built-in" : "next available");
	}
    }
#ifdef TEST_DEFERRED_NAMING
    if (dbdict != NULL && shouldDeferNamingInterface(interface)) {
	SC_log(LOG_NOTICE, "%s: simulating naming failure", __func__);
    } else
#endif
    {
	new_interface = nameAndCopyInterface(interface, (dbdict != NULL), unit);
    }
    if (new_interface != NULL) {
	CFNumberRef	new_unit;

	new_unit = _SCNetworkInterfaceGetIOInterfaceUnit(new_interface);
	if (persistInterfaceName(new_interface) && !CFEqual(unit, new_unit)) {
	    SC_log(LOG_INFO,
		   "interface prefix %@ assigned unit %@ instead of %@",
		   prefix, new_unit, unit);
	}
    } else if (dbdict != NULL) {
	/* naming failed on a known interface, try again later */
	addNamingRequest(interface, unit);
    }

 done:
    my_CFRelease(&unit);
    return (new_interface);
}

static void
nameInterfaces(CFMutableArrayRef if_list)
{
    CFIndex	i;
    CFIndex	n	= CFArrayGetCount(if_list);

    for (i = 0; i < n; i++) {
	SCNetworkInterfaceRef	interface;
	CFNumberRef		unit;

	interface = CFArrayGetValueAtIndex(if_list, i);
	SC_log(LOG_INFO, "%s: %d. %@", __func__, (int)i, interface);
	if (_SCNetworkInterfaceIsSelfNamed(interface)) {
	    /* ignore self-named interfaces */
	    SC_log(LOG_INFO, "Ignoring self-named interface");
	    continue;
	}
	unit = _SCNetworkInterfaceGetIOInterfaceUnit(interface);
	if (unit != NULL) {
	    // interface already has a unit number
	    CFStringRef	if_name;
	    CFIndex	where;

	    if_name = SCNetworkInterfaceGetBSDName(interface);
	    if ((if_name == NULL) || !CFDictionaryContainsKey(S_state, if_name)) {
		SC_log(LOG_INFO, "Interface already has a unit number");
		displayInterface(interface);
	    }

	    // update the list of interfaces that were previously named
	    if ((S_prev_active_list != NULL)
		&& lookupInterfaceByAddress(S_prev_active_list, interface, &where) != NULL) {
		CFArrayRemoveValueAtIndex(S_prev_active_list, where);
	    }
	    if (persistInterfaceName(interface)) {
		replaceInterface(interface);
	    }
	} else {
	    SCNetworkInterfaceRef	new_interface;

	    new_interface = assignNameAndCopyInterface(interface, if_list, i);
	    if (new_interface != NULL) {
		displayInterface(new_interface);

		// update if_list (with the interface name & unit)
		CFArraySetValueAtIndex(if_list, i, new_interface);
		CFRelease(new_interface);

		if (_SCNetworkInterfaceIsBuiltin(new_interface) &&
		    S_prev_active_list != NULL) {
		    CFIndex	where;

		    // update the list of [built-in] interfaces that were previously named
		    if (lookupInterfaceByUnit(S_prev_active_list, new_interface, &where) != NULL) {
			SC_log(LOG_DEBUG, "  and updated database (new address)");
			CFArrayRemoveValueAtIndex(S_prev_active_list, where);
		    }
		}
		if (persistInterfaceName(new_interface)) {
		    replaceInterface(new_interface);
		}
	    }
	}
    }
    return;
}


#if TARGET_OS_OSX

static void
updateNetworkConfiguration(CFArrayRef if_list)
{
    Boolean		do_commit	= FALSE;
    CFIndex		i;
    CFIndex		n;
    SCPreferencesRef	prefs		= NULL;
    SCNetworkSetRef	set		= NULL;

    prefs = SCPreferencesCreate(NULL, CFSTR(MY_PLUGIN_NAME ":updateNetworkConfiguration"), NULL);
    if (prefs == NULL) {
	SC_log(LOG_NOTICE, "SCPreferencesCreate() failed: %s", SCErrorString(SCError()));
	return;
    }

    set = SCNetworkSetCopyCurrent(prefs);
    if (set == NULL) {
	SC_log(LOG_INFO, "No current set, adding default");
	set = _SCNetworkSetCreateDefault(prefs);
	if (set == NULL) {
	    SC_log(LOG_NOTICE, "_SCNetworkSetCreateDefault() failed: %s", SCErrorString(SCError()));
	    goto done;
	}
    }

    n = (if_list != NULL) ? CFArrayGetCount(if_list) : 0;
    for (i = 0; i < n; i++) {
	SCNetworkInterfaceRef	interface;
	Boolean			is_hidden;

	interface = CFArrayGetValueAtIndex(if_list, i);
	is_hidden = _SCNetworkInterfaceIsHiddenInterface(interface);
	if (is_hidden) {
	    SC_log(LOG_NOTICE, "InterfaceNamer %@: ignoring hidden interface",
		   SCNetworkInterfaceGetBSDName(interface));
	}
	else if (SCNetworkSetEstablishDefaultInterfaceConfiguration(set, interface)) {
	    SC_log(LOG_INFO, "added default configuration for %@",
		   SCNetworkInterfaceGetBSDName(interface));
	    do_commit = TRUE;
	}
    }

    if (do_commit) {
	Boolean	ok;

	ok = SCPreferencesCommitChanges(prefs);
	if (!ok) {
	    SC_log(LOG_NOTICE, "SCPreferencesCommitChanges() failed: %s", SCErrorString(SCError()));
	    goto done;
	}

	ok = SCPreferencesApplyChanges(prefs);
	if (!ok) {
	    SC_log(LOG_NOTICE, "SCPreferencesApplyChanges() failed: %s", SCErrorString(SCError()));
	    goto done;
	}
    }

  done :

    if (set != NULL) {
	CFRelease(set);
	set = NULL;
    }

    if (prefs != NULL) {
	CFRelease(prefs);
	prefs = NULL;
    }

    return;
}
#endif	// TARGET_OS_OSX

static void
upgradeNetworkConfigurationOnce(void)
{
    static dispatch_once_t	once;

    /*
     * Once, per start of InterfaceNamer, we check/ensure that the
     * configuration has been upgraded.
     *
     * Note: this check should not be performed until we know that
     *       the __wait_for_IOKit_to_quiesce() conditions have been
     *       satisfied.
     */

    dispatch_once(&once, ^{
	SCPreferencesRef	ni_prefs;
	CFStringRef		option_keys[]	= { kSCPreferencesOptionAvoidDeadlock };
	CFPropertyListRef	option_vals[]	= { kCFBooleanFalse };
	CFDictionaryRef		options;
	Boolean			updated;

	// save the [current] DB with the interfaces that have been named
	writeInterfaceList(S_dblist);

	// upgrade the configuration
	options = CFDictionaryCreate(NULL,
				     (const void **)option_keys,
				     (const void **)option_vals,
				     sizeof(option_keys) / sizeof(option_keys[0]),
				     &kCFTypeDictionaryKeyCallBacks,
				     &kCFTypeDictionaryValueCallBacks);
	ni_prefs = SCPreferencesCreateWithOptions(NULL,
						  CFSTR(MY_PLUGIN_NAME ":upgradeNetworkConfiguration"),
						  INTERFACES_DEFAULT_CONFIG,
						  NULL,	// authorization
						  options);
	CFRelease(options);
	if (ni_prefs == NULL) {
	    SC_log(LOG_NOTICE, "SCPreferencesCreate() failed: %s", SCErrorString(SCError()));
	    return;
	}
	updated = __SCNetworkConfigurationUpgrade(NULL, &ni_prefs, TRUE);
	CFRelease(ni_prefs);

	if (updated) {
	    // re-read list of previously named network interfaces
	    if (S_dblist != NULL) {
		CFRelease(S_dblist);
	    }
	    S_dblist = readInterfaceList();

	    addTimestamp(S_state, CFSTR("*UPGRADED*"));
	    SC_log(LOG_INFO, "network configuration upgraded");
	    updateStore();
	}
    });

    return;
}

static void
upgradeNetworkConfiguration(void)
{
#if TARGET_OS_OSX
    if (os_variant_is_basesystem(IFNAMER_SUBSYSTEM)) {
	/* don't bother trying to upgrade on a base system */
	return;
    }
#endif /* TARGET_OS_OSX */
    upgradeNetworkConfigurationOnce();
}

static void
removeInactiveInterfaces(void)
{
    CFIndex	n;

    /*
     * remove any previous interfaces that were built-in,
     * were active, and were hidden (pre-configured) that
     * are no longer plugged in.
     */

    if ((S_dblist == NULL) || (S_prev_active_list == NULL)) {
	return;
    }

    n = CFArrayGetCount(S_prev_active_list);
    for (CFIndex i = n - 1; i >= 0; i--) {
	CFBooleanRef		builtin;
	CFBooleanRef		hidden;
	CFDictionaryRef		if_dict;
	CFDictionaryRef		info;
	CFStringRef		name;
	CFIndex			where;

	if_dict = CFArrayGetValueAtIndex(S_prev_active_list, i);

	// Note: keep the following logic in sync with _SCNetworkInterfaceIsApplePreconfigured()

	name = CFDictionaryGetValue(if_dict, CFSTR(kIOBSDNameKey));
	if (!isA_CFString(name)) {
	    // if no BSD name
	    continue;
	}

	hidden = CFDictionaryGetValue(if_dict, kSCNetworkInterfaceHiddenConfigurationKey);
	if (!isA_CFBoolean(hidden) || !CFBooleanGetValue(hidden)) {
	    // if not hidden
	    continue;
	}

	builtin = CFDictionaryGetValue(if_dict, CFSTR(kIOBuiltin));
	if (isA_CFBoolean(builtin) && CFBooleanGetValue(builtin)) {
	    // if [hidden] builtin
	    goto remove;
	}

	info = CFDictionaryGetValue(if_dict, CFSTR(kSCNetworkInterfaceInfo));
	if (isA_CFDictionary(info)) {
	    int		vid;
	    CFNumberRef	vidNum;

	    if (CFDictionaryGetValueIfPresent(info, CFSTR(kUSBVendorID), (const void **)&vidNum) &&
		isA_CFNumber(vidNum) &&
		CFNumberGetValue(vidNum, kCFNumberIntType, &vid) &&
		(vid == kIOUSBAppleVendorID)) {
		// if [hidden] Apple interface

#if	TARGET_OS_OSX
		if (sharingConfigurationUsesInterface(name, TRUE)) {
		    // do not remove interfaces referenced in the sharing configuration
		    continue;
		}
#endif	// TARGET_OS_OSX

		goto remove;
	    }
	}

	continue;

    remove :

	SC_log(LOG_INFO, "Removing no-longer-active \"hidden\" interface: %@", name);

	if (lookupInterfaceByName(S_dblist, name, &where) != NULL) {
	    // remove from the list of interfaces we know about
	    CFArrayRemoveValueAtIndex(S_dblist, where);
	    // remove from the previously active list
	    CFArrayRemoveValueAtIndex(S_prev_active_list, i);
	}
    }

#if	TARGET_OS_OSX
    sharingConfigurationClose();
#endif	// TARGET_OS_OSX

    return;
}

static void
reportInactiveInterfaces(void)
{
    CFIndex	n;

    /*
     * report any previous interfaces that are not [yet] active
     */

    if (S_prev_active_list == NULL) {
	return;
    }

    n = CFArrayGetCount(S_prev_active_list);
    if (n > 0) {
	SC_log(LOG_INFO, "Interface%s not [yet] active",
	       (n > 1) ? "s" : "");
    }
    for (CFIndex i = 0; i < n; i++) {
	CFDictionaryRef		if_dict;
	CFStringRef		name;
	CFStringRef		prefix;
	CFNumberRef		unit;

	if_dict = CFArrayGetValueAtIndex(S_prev_active_list, i);
	name = CFDictionaryGetValue(if_dict, CFSTR(kIOBSDNameKey));
	prefix = CFDictionaryGetValue(if_dict, CFSTR(kIOInterfaceNamePrefix));
	unit = CFDictionaryGetValue(if_dict, CFSTR(kIOInterfaceUnit));
	SC_log(LOG_INFO, "  %s%@%sPrefix: %@, Unit: %@",
	       (name != NULL) ? "BSD Name: " : "",
	       (name != NULL) ? name : CFSTR(""),
	       (name != NULL) ? ", " : "",
	       prefix,
	       unit);
    }

    return;
}

static void
updateInterfaces(void)
{
    if (S_connect == MACH_PORT_NULL) {
	// if we don't have the "IONetworkStack" connect object
	return;
    }

    if (S_iflist != NULL) {
	CFIndex	n;

	n = CFArrayGetCount(S_iflist);
	if (n > 1) {
	    CFArraySortValues(S_iflist, CFRangeMake(0, n), _SCNetworkInterfaceCompare, NULL);
	}
	nameInterfaces(S_iflist);
    }

    /*
     * Update the list of [Apple] pre-configured interfaces
     */
    updatePreConfiguredInterfaces(S_iflist);

#if	TARGET_OS_IPHONE
    /*
     * Update the list of "trust required" interfaces
     */
    if (haveLockdown()) {
	updateTrustRequiredInterfaces(S_iflist);
    }
#endif	// TARGET_OS_IPHONE

    if (isQuiet()) {
	/*
	 * The registry [matching] has quiesced
	 */

	// remove any inactive interfaces
	removeInactiveInterfaces();

	// save the DB with the interfaces that have been named
	writeInterfaceList(S_dblist);

	// update the VLAN/BOND configuration
	updateVirtualNetworkInterfaceConfiguration(NULL, kSCPreferencesNotificationApply, NULL);

#if TARGET_OS_OSX
	/*
	 * On a basesystem or darwinos variant of the OS, the per-user session
	 * UserEventAgent daemon that would normally load "SCMonitor" is not
	 * running. Configure new interfaces as soon as they appear here.
	 */
	if (os_variant_is_basesystem(IFNAMER_SUBSYSTEM)
	    || os_variant_is_darwinos(IFNAMER_SUBSYSTEM)) {
	    updateNetworkConfiguration(S_iflist);
	}
#endif /* TARGET_OS_OSX */

	// tell everyone that we've finished (at least for now)
	updateStore();

	// log those interfaces which are no longer present in
	// the HW config (or have yet to show up).
	reportInactiveInterfaces();

	if (S_prev_active_list != NULL) {
	    CFRelease(S_prev_active_list);
	    S_prev_active_list = NULL;
	}

	if (S_iflist != NULL) {
	    CFRelease(S_iflist);
	    S_iflist = NULL;
	}
    } else {
	if ((S_prev_active_list != NULL) && (CFArrayGetCount(S_prev_active_list) == 0)) {
	    /*
	     * if we've named all of the interfaces that
	     * were used during the previous boot.
	     */
	    addTimestamp(S_state, kInterfaceNamerKey_Complete);
	    SC_log(LOG_INFO, "last boot interfaces have been named");
	    updateStore();
	    CFRelease(S_prev_active_list);
	    S_prev_active_list = NULL;
	}
    }

    return;
}

static void
interfaceArrivalCallback(void *refcon, io_iterator_t iter)
{
#pragma unused(refcon)
    io_object_t		obj;

    while ((obj = IOIteratorNext(iter)) != MACH_PORT_NULL) {
	SCNetworkInterfaceRef	interface;

	interface = _SCNetworkInterfaceCreateWithIONetworkInterfaceObject(obj);
	if (interface != NULL) {
	    if (S_iflist == NULL) {
		S_iflist = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	    }
	    CFArrayAppendValue(S_iflist, interface);
	    CFRelease(interface);
	}
	IOObjectRelease(obj);
    }

    updateInterfaces();

    return;
}

/*
 * Function: stackCallback
 * Purpose:
 *   Get a reference to the single IONetworkStack object instance in
 *   the kernel. Naming requests must be sent to this object, which is
 *   attached as a client to all network interface objects in the system.
 * Note:
 *   Call IOObjectRelease on the returned object.
 */
static void
stackCallback(void *refcon, io_iterator_t iter)
{
#pragma unused(refcon)
    kern_return_t	kr;
    io_object_t		stack;

    stack = IOIteratorNext(iter);
    if (stack == MACH_PORT_NULL) {
	goto error;
    }

    kr = IOServiceOpen(stack, mach_task_self(), 0, &S_connect);
    if (kr != KERN_SUCCESS) {
	SC_log(LOG_ERR, "IOServiceOpen returned 0x%x", kr);
	goto error;
    }

    addTimestamp(S_state, CFSTR("*STACK*"));
    SC_log(LOG_INFO, "IONetworkStack found");

    if (S_stack != MACH_PORT_NULL) {
	IOObjectRelease(S_stack);
	S_stack = MACH_PORT_NULL;
    }

    if ((S_timer != NULL) && CFRunLoopTimerIsValid(S_timer)) {
	// With the IONetworkStack object now available we can
	// reset (shorten?) the time we are willing to wait for
	// IOKit to quiesce.
	CFRunLoopTimerSetNextFireDate(S_timer,
				      CFAbsoluteTimeGetCurrent() + S_quiet_timeout);
    }

    updateInterfaces();

 error:

    if (stack != MACH_PORT_NULL) {
	IOObjectRelease(stack);
    }

    return;
}

static void
quietCallback(void		*refcon,
	      io_service_t	service,
	      natural_t		messageType,
	      void		*messageArgument)
{
#pragma unused(refcon)
#pragma unused(service)
    if (messageArgument != NULL) {
	// if not yet quiet
	return;
    }

    if (messageType == kIOMessageServiceBusyStateChange) {
	addTimestamp(S_state, kInterfaceNamerKey_Quiet);
	SC_log(LOG_INFO, "IOKit quiet");
    }

    if (S_connect == MACH_PORT_NULL) {
	SC_log(LOG_ERR, "No network stack object");
	return;
    }

    if (S_quiet != MACH_PORT_NULL) {
	IOObjectRelease(S_quiet);
	S_quiet = MACH_PORT_NULL;
    }

    if (S_timer != NULL) {
	CFRunLoopTimerInvalidate(S_timer);
	CFRelease(S_timer);
	S_timer = NULL;
    }

    // grab (and name) any additional interfaces.
    interfaceArrivalCallback((void *)S_notify, S_iter);

    if (messageType == kIOMessageServiceBusyStateChange) {
	addTimestamp(S_state, CFSTR("*QUIET&NAMED*"));
	updateStore();
	upgradeNetworkConfiguration();
    }

    return;
}

static void
iterateRegistryBusy(io_iterator_t iterator, CFArrayRef nodes, int *count)
{
    kern_return_t	kr  = kIOReturnSuccess;;
    io_object_t		obj;

    while ((kr == kIOReturnSuccess) &&
	   ((obj = IOIteratorNext(iterator)) != MACH_PORT_NULL)) {
	uint64_t		accumulated_busy_time;
	uint32_t		busy_state;
	io_name_t		location;
	io_name_t		name;
	CFMutableArrayRef	newNodes;
	uint64_t		state;
	CFMutableStringRef	str	= NULL;

	if (nodes == NULL) {
	    newNodes = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	} else {
	    newNodes = CFArrayCreateMutableCopy(NULL, 0, nodes);
	}
	assert(newNodes != NULL);

	kr = IORegistryEntryGetName(obj, name);
	if (kr != kIOReturnSuccess) {
	    SC_log(LOG_NOTICE, "IORegistryEntryGetName() returned 0x%x", kr);
	    goto next;
	}

	str = CFStringCreateMutable(NULL, 0);
	CFStringAppendCString(str, name, kCFStringEncodingUTF8);

	kr = IORegistryEntryGetLocationInPlane(obj, kIOServicePlane, location);
	switch (kr) {
	    case kIOReturnSuccess :
		CFStringAppendCString(str, "@", kCFStringEncodingUTF8);
		CFStringAppendCString(str, location, kCFStringEncodingUTF8);
		break;
	    case kIOReturnNotFound :
		break;
	    default :
		SC_log(LOG_NOTICE, "IORegistryEntryGetLocationInPlane() returned 0x%x", kr);
		CFRelease(str);
		goto next;
	}

	CFArrayAppendValue(newNodes, str);
	CFRelease(str);

	kr = IOServiceGetBusyStateAndTime(obj, &state, &busy_state, &accumulated_busy_time);
	if (kr != kIOReturnSuccess) {
	    SC_log(LOG_NOTICE, "IOServiceGetBusyStateAndTime() returned 0x%x", kr);
	    goto next;
	}

#ifdef	TEST_SNAPSHOT
	// report all nodes
	busy_state = 1;
#endif	// TEST_SNAPSHOT

	if (busy_state != 0) {
	    CFStringRef	path;

	    if ((*count)++ == 0) {
		SC_log(LOG_ERR, "Busy services :");
	    }

	    path = CFStringCreateByCombiningStrings(NULL, newNodes, CFSTR("/"));
	    SC_log(LOG_ERR, "  %@ [%s%s%s%d, %lld ms]",
		   path,
		   (state & kIOServiceRegisteredState) ? "" : "!registered, ",
		   (state & kIOServiceMatchedState)    ? "" : "!matched, ",
		   (state & kIOServiceInactiveState)   ? "inactive, " : "",
		   busy_state,
		   accumulated_busy_time / kMillisecondScale);
	    CFRelease(path);
	}

	kr = IORegistryIteratorEnterEntry(iterator);
	if (kr != kIOReturnSuccess) {
	    SC_log(LOG_NOTICE, "IORegistryIteratorEnterEntry() returned 0x%x", kr);
	    goto next;
	}

	iterateRegistryBusy(iterator, newNodes, count);

	kr = IORegistryIteratorExitEntry(iterator);
	if (kr != kIOReturnSuccess) {
	    SC_log(LOG_NOTICE, "IORegistryIteratorExitEntry() returned 0x%x", kr);
	}

      next :

	CFRelease(newNodes);
	IOObjectRelease(obj);
    }

    return;
}

static void
captureBusy()
{
    int			count		= 0;
    io_iterator_t	iterator	= MACH_PORT_NULL;
    kern_return_t	kr;

    kr = IORegistryCreateIterator(kIOMainPortDefault,
				  kIOServicePlane,
				  0,
				  &iterator);
    if (kr != kIOReturnSuccess) {
	SC_log(LOG_NOTICE, "IORegistryCreateIterator() returned 0x%x", kr);
	return;
    }

    iterateRegistryBusy(iterator, NULL, &count);
    if (count == 0) {
	SC_log(LOG_ERR, "w/no busy services");
    }

    IOObjectRelease(iterator);
}

static void
timerCallback(CFRunLoopTimerRef	timer, void *info)
{
#pragma unused(timer)
#pragma unused(info)
    // We've been waiting for IOKit to quiesce and it just
    // hasn't happenned.  Time to just move on!
    addTimestamp(S_state, kInterfaceNamerKey_Timeout);

    // log busy nodes
    SC_log(LOG_ERR, "timed out waiting for IOKit to quiesce");
    captureBusy();

    quietCallback((void *)S_notify, MACH_PORT_NULL, 0, NULL);

    addTimestamp(S_state, CFSTR("*TIMEOUT&NAMED*"));
    updateStore();
    upgradeNetworkConfiguration();

    return;
}

static Boolean
setup_IOKit(CFBundleRef bundle)
{
#pragma unused(bundle)
    uint32_t		busy;
    kern_return_t	kr;
    mach_port_t		masterPort	= MACH_PORT_NULL;
    Boolean		ok		= FALSE;
    io_object_t		root		= MACH_PORT_NULL;

    // read DB of previously named network interfaces
    S_dblist = readInterfaceList();

    // get interfaces that were named during the last boot
    S_prev_active_list = previouslyActiveInterfaces();

    // track how long we've waited to see each interface.
    S_state = CFDictionaryCreateMutable(NULL,
					0,
					&kCFTypeDictionaryKeyCallBacks,
					&kCFTypeDictionaryValueCallBacks);
    addTimestamp(S_state, CFSTR("*START*"));

    // Creates and returns a notification object for receiving IOKit
    // notifications of new devices or state changes.
    kr = IOMainPort(bootstrap_port, &masterPort);
    if (kr != KERN_SUCCESS) {
	SC_log(LOG_ERR, "IOMainPort returned 0x%x", kr);
	goto done;
    }

    S_notify = IONotificationPortCreate(masterPort);
    if (S_notify == NULL) {
	SC_log(LOG_ERR, "IONotificationPortCreate failed");
	goto done;
    }

    // watch IOKit matching activity
    root = IORegistryEntryFromPath(masterPort, kIOServicePlane ":/");
    if (root == MACH_PORT_NULL) {
	SC_log(LOG_ERR, "IORegistryEntryFromPath failed");
	goto done;
    }

    kr = IOServiceAddInterestNotification(S_notify,
					  root,
					  kIOBusyInterest,
					  &quietCallback,
					  (void *)S_notify,	// refCon
					  &S_quiet);		// notification
    if (kr != KERN_SUCCESS) {
	SC_log(LOG_ERR, "IOServiceAddInterestNotification returned 0x%x", kr);
	goto done;
    }

    kr = IOServiceGetBusyState(root, &busy);
    if (kr != KERN_SUCCESS) {
	SC_log(LOG_ERR, "IOServiceGetBusyState returned 0x%x", kr);
	goto done;
    }

    // add a timer so we don't wait forever for IOKit to quiesce
    S_timer = CFRunLoopTimerCreate(NULL,
				   CFAbsoluteTimeGetCurrent() + S_stack_timeout,
				   0,
				   0,
				   0,
				   timerCallback,
				   NULL);
    if (S_timer == NULL) {
	SC_log(LOG_ERR, "CFRunLoopTimerCreate failed");
	goto done;
    }

    CFRunLoopAddTimer(CFRunLoopGetCurrent(), S_timer, kCFRunLoopDefaultMode);

    // watch for the introduction of the IONetworkStack
    kr = IOServiceAddMatchingNotification(S_notify,
					  kIOFirstMatchNotification,
					  IOServiceMatching("IONetworkStack"),
					  &stackCallback,
					  (void *)S_notify,	// refCon
					  &S_stack);		// notification
    if (kr != KERN_SUCCESS) {
	SC_log(LOG_ERR, "IOServiceAddMatchingNotification returned 0x%x", kr);
	goto done;
    }

    // check and see if the stack is already available and arm the
    // notification for its introduction.
    stackCallback((void *)S_notify, S_stack);

    // watch for the introduction of new network interfaces
    kr = IOServiceAddMatchingNotification(S_notify,
					  kIOFirstMatchNotification,
					  IOServiceMatching("IONetworkInterface"),
					  &interfaceArrivalCallback,
					  (void *)S_notify,	// refCon
					  &S_iter);		// notification
    if (kr != KERN_SUCCESS) {
	SC_log(LOG_ERR, "IOServiceAddMatchingNotification returned 0x%x", kr);
	goto done;
    }

    // Get the current list of matches and arm the notification for
    // future interface arrivals.
    interfaceArrivalCallback((void *)S_notify, S_iter);

    // Check if IOKit has already quiesced.
    quietCallback((void *)S_notify,
		  MACH_PORT_NULL,
		  kIOMessageServiceBusyStateChange,
		  (void *)(uintptr_t)busy);

    CFRunLoopAddSource(CFRunLoopGetCurrent(),
		       IONotificationPortGetRunLoopSource(S_notify),
		       kCFRunLoopDefaultMode);

#ifdef	WAIT_PREVIOUS_BOOT_INTERFACES_OR_QUIET
    /*
     * Start the wheels turning until we've named all of
     * the interfaces that were used during the previous
     * boot, until IOKit [matching] has quiesced, or
     * until we've waited long enough.
     */
    CFRunLoopAddTimer(CFRunLoopGetCurrent(), S_timer, MY_PLUGIN_ID);
    CFRunLoopAddSource(CFRunLoopGetCurrent(),
		       IONotificationPortGetRunLoopSource(S_notify),
		       MY_PLUGIN_ID);
    while (S_prev_active_list != NULL) {
	int	rlStatus;

	rlStatus = CFRunLoopRunInMode(MY_PLUGIN_ID, 1.0e10, TRUE);
    }
#endif	/* WAIT_PREVIOUS_BOOT_INTERFACES_OR_QUIET */

#if	TARGET_OS_OSX
    if (S_dblist != NULL) {
	// apply special handling for the BT-PAN interface (if present)
	CFArrayApplyFunction(S_dblist,
			     CFRangeMake(0, CFArrayGetCount(S_dblist)),
			     updateBTPANInformation,
			     NULL);
    }
#endif	// TARGET_OS_OSX

    ok = TRUE;

 done:
    if (root != MACH_PORT_NULL) {
	IOObjectRelease(root);
    }
    if (masterPort != MACH_PORT_NULL) {
	mach_port_deallocate(mach_task_self(), masterPort);
    }

    return ok;
}

static Boolean
setup_Virtual(CFBundleRef bundle)
{
#pragma unused(bundle)
    // open a SCPreferences session
    S_prefs = SCPreferencesCreate(NULL, CFSTR(MY_PLUGIN_NAME ":setup_Virtual"), NULL);
    if (S_prefs == NULL) {
	SC_log(LOG_ERR, "SCPreferencesCreate() failed: %s",
	       SCErrorString(SCError()));
	return FALSE;
    }

    // register for change notifications.
    if (!SCPreferencesSetCallback(S_prefs, updateVirtualNetworkInterfaceConfiguration, NULL)) {
	SC_log(LOG_ERR, "SCPreferencesSetCallBack() failed: %s", SCErrorString(SCError()));
	CFRelease(S_prefs);
	return FALSE;
    }

    // schedule
    if (!SCPreferencesScheduleWithRunLoop(S_prefs, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode)) {
	if (SCError() != kSCStatusNoStoreServer) {
	    SC_log(LOG_ERR, "SCPreferencesScheduleWithRunLoop() failed: %s", SCErrorString(SCError()));
	    CFRelease(S_prefs);
	    return FALSE;
	}
    }

    return TRUE;
}

static void *
exec_InterfaceNamer(void *arg)
{
    CFBundleRef		bundle  = (CFBundleRef)arg;
    CFDictionaryRef	dict;

    pthread_setname_np(MY_PLUGIN_NAME " thread");

    dict = CFBundleGetInfoDictionary(bundle);
    if (isA_CFDictionary(dict)) {
	CFNumberRef	num;

	num = CFDictionaryGetValue(dict, CFSTR(WAIT_STACK_TIMEOUT_KEY));
	if (num != NULL) {
	    if (!isA_CFNumber(num) ||
		!CFNumberGetValue(num, kCFNumberDoubleType, &S_stack_timeout) ||
		(S_stack_timeout <= 0.0)) {
		SC_log(LOG_NOTICE, WAIT_STACK_TIMEOUT_KEY " value error");
		S_stack_timeout = WAIT_STACK_TIMEOUT_DEFAULT;
	    }
	}

	num = CFDictionaryGetValue(dict, CFSTR(WAIT_QUIET_TIMEOUT_KEY));
	if (num != NULL) {
	    if (!isA_CFNumber(num) ||
		!CFNumberGetValue(num, kCFNumberDoubleType, &S_quiet_timeout) ||
		(S_quiet_timeout <= 0.0)) {
		SC_log(LOG_NOTICE, WAIT_QUIET_TIMEOUT_KEY " value error");
		S_quiet_timeout = WAIT_QUIET_TIMEOUT_DEFAULT;
	    }
	}
    }

    // setup virtual network interface monitoring
    if (!setup_Virtual(bundle)) {
	goto error;
    }

    // setup [IOKit] network interface monitoring
    if (!setup_IOKit(bundle)) {
	goto error;
    }

    goto done;

  error :
    if (S_connect != MACH_PORT_NULL) {
	IOServiceClose(S_connect);
	S_connect = MACH_PORT_NULL;
    }
    if (S_dblist != NULL) {
	CFRelease(S_dblist);
	S_dblist = NULL;
    }
    if (S_iter != MACH_PORT_NULL) {
	IOObjectRelease(S_iter);
	S_iter = MACH_PORT_NULL;
    }
    if (S_notify != MACH_PORT_NULL) {
	IONotificationPortDestroy(S_notify);
    }
    if (S_quiet != MACH_PORT_NULL) {
	IOObjectRelease(S_quiet);
	S_quiet = MACH_PORT_NULL;
    }
    if (S_stack != MACH_PORT_NULL) {
	IOObjectRelease(S_stack);
	S_stack = MACH_PORT_NULL;
    }
    if (S_state != NULL) {
	CFRelease(S_state);
	S_state = NULL;
    }
    if (S_timer != NULL) {
	CFRunLoopTimerInvalidate(S_timer);
	CFRelease(S_timer);
	S_timer = NULL;
    }

  done :
    CFRelease(bundle);
    CFRunLoopRun();

    return NULL;
}

__private_extern__
void
load_InterfaceNamer(CFBundleRef bundle, Boolean bundleVerbose)
{
#pragma unused(bundleVerbose)
    pthread_attr_t  tattr;
    pthread_t	    tid;

    CFRetain(bundle);	// released in exec_InterfaceNamer

    pthread_attr_init(&tattr);
    pthread_attr_setscope(&tattr, PTHREAD_SCOPE_SYSTEM);
    pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);
//  pthread_attr_setstacksize(&tattr, 96 * 1024); // each thread gets a 96K stack
    pthread_create(&tid, &tattr, exec_InterfaceNamer, bundle);
    pthread_attr_destroy(&tattr);

    return;
}

//------------------------------------------------------------------------
// Main function.
#ifdef TEST_INTERFACE_ASSIGNMENT
int
main(int argc, char ** argv)
{
#pragma unused(argv)
    CFBundleRef		bundle;
    CFMutableArrayRef	interfaces;
    CFArrayRef		interfaces_all;
    CFIndex		n;

    _sc_log     = kSCLogDestinationFile;
    _sc_verbose = (argc > 1) ? TRUE : FALSE;

    bundle = CFBundleGetMainBundle();
    CFRetain(bundle);	// released in exec_InterfaceNamer

    // setup
    setup_IOKit(bundle);

    // but, when running this test we know that the IORegistry has already quiesced
    IOObjectRelease(S_quiet);
    S_quiet = MACH_PORT_NULL;

    // collect the interfaces
    interfaces = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    interfaces_all = SCNetworkInterfaceCopyAll();
    n = CFArrayGetCount(interfaces_all);
    for (CFIndex i = 0; i < n; i++) {
	SCNetworkInterfaceRef		interface		= CFArrayGetValueAtIndex(interfaces_all, i);
	SCNetworkInterfacePrivateRef	interfacePrivate	= (SCNetworkInterfacePrivateRef)interface;

	if (interfacePrivate->prefix == NULL) {
	    // skip interfaces with no kIOInterfaceNamePrefix property
	    continue;
	}

	if (interfacePrivate->unit != NULL) {
	    // remove any already assigned unit #
	    CFRelease(interfacePrivate->unit);
	    interfacePrivate->unit = NULL;
	}

	CFArrayAppendValue(interfaces, interface);
    }
    CFRelease(interfaces_all);
    SC_log(LOG_INFO, "interfaces = %@", interfaces);

    // exercise the interface naming assignments
    nameInterfaces(interfaces);
    CFRelease(interfaces);

    exit(0);
    return 0;
}
#endif	/* MAIN */

#ifdef	TEST_SNAPSHOT
int
main(int argc, char ** argv)
{
    _sc_log     = kSCLogDestinationFile;
    _sc_verbose = (argc > 1) ? TRUE : FALSE;

    captureBusy();

    exit(0);
    return 0;
}
#endif	/* TEST_SNAPSHOT */

