/*
 * Copyright (c) 2004-2007, 2009-2013, 2015-2021 Apple Inc. All rights reserved.
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
 * May 27, 2004		Allan Nathanson <ajn@apple.com>
 * - initial revision
 */


#include "SCNetworkConfigurationInternal.h"
#include "SCPreferencesInternal.h"

#include <sys/ioctl.h>
#include <net/if.h>


#pragma mark -
#pragma mark SCNetworkConfiguration logging


__private_extern__ os_log_t
__log_SCNetworkConfiguration(void)
{
	static os_log_t	log	= NULL;

	if (log == NULL) {
		log = os_log_create("com.apple.SystemConfiguration", "SCNetworkConfiguration");
	}

	return log;
}


static void
logConfiguration_NetworkInterfaces(int level, const char *description, SCPreferencesRef prefs)
{
	CFArrayRef	interfaces;

	interfaces = SCPreferencesGetValue(prefs, INTERFACES);
	if (isA_CFArray(interfaces)) {
		CFStringRef	model	= SCPreferencesGetValue(prefs, MODEL);
		CFIndex		n	= CFArrayGetCount(interfaces);

		SC_log(level, "%s%sinterfaces (%@)",
		       description != NULL ? description : "",
		       description != NULL ? " " : "",
		       model != NULL ? model : CFSTR("No model"));

		for (CFIndex i = 0; i < n; i++) {
			CFStringRef	bsdName;
			CFBooleanRef	bVal;
			CFDictionaryRef	dict;
			CFDictionaryRef	info;
			Boolean		isBuiltin;
			Boolean		isHidden;
			CFStringRef	name;

			dict = CFArrayGetValueAtIndex(interfaces, i);
			if (!isA_CFDictionary(dict)) {
				continue;
			}

			bsdName = CFDictionaryGetValue(dict, CFSTR(kSCNetworkInterfaceBSDName));
			if (!isA_CFString(bsdName)) {
				continue;
			}

			bVal = CFDictionaryGetValue(dict, CFSTR(kSCNetworkInterfaceIOBuiltin));
			isBuiltin = (bVal != NULL) && CFBooleanGetValue(bVal);

			isHidden = CFDictionaryContainsKey(dict, kSCNetworkInterfaceHiddenConfigurationKey) ||
				   CFDictionaryContainsKey(dict, kSCNetworkInterfaceHiddenInterfaceKey);

			info = CFDictionaryGetValue(dict, CFSTR(kSCNetworkInterfaceInfo));
			name = CFDictionaryGetValue(info, kSCPropUserDefinedName);
			SC_log(level, "  %@ (%@%s%s)",
			       bsdName,
			       name != NULL ? name : CFSTR("???"),
			       isBuiltin ? ", built-in" : "",
			       isHidden ? ", hidden" : "");
		}

	}
}

static void
logConfiguration_preferences(int level, const char *description, SCPreferencesRef prefs)
{
	CFArrayRef		interfaces;
	CFStringRef		model	= SCPreferencesGetValue(prefs, MODEL);
	CFIndex			n;
	CFMutableArrayRef	orphans;
	CFArrayRef		services;
	CFArrayRef		sets;

	SC_log(level, "%s%sconfiguration (%@)",
	       description != NULL ? description : "",
	       description != NULL ? " " : "",
	       model != NULL ? model : CFSTR("No model"));

	services = SCNetworkServiceCopyAll(prefs);
	if (services != NULL) {
		orphans = CFArrayCreateMutableCopy(NULL, 0, services);
		CFRelease(services);
	} else {
		orphans = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	}

	sets = SCNetworkSetCopyAll(prefs);
	if (sets != NULL) {
		SCNetworkSetRef	currentSet;

		currentSet = SCNetworkSetCopyCurrent(prefs);

		n = CFArrayGetCount(sets);
		for (CFIndex i = 0; i < n; i++) {
			SCNetworkSetRef	set;

			set = CFArrayGetValueAtIndex(sets, i);
			SC_log(level, "  Set %@ (%@%s)",
			       SCNetworkSetGetSetID(set),
			       SCNetworkSetGetName(set),
			       _SC_CFEqual(set, currentSet) ? ", current" : "");

			services = SCNetworkSetCopyServices(set);
			if (services != NULL) {
				CFIndex		n;
				CFIndex		nOrder	= 0;
				CFArrayRef	order;

				order = SCNetworkSetGetServiceOrder(set);
				if (order != NULL) {
					nOrder = CFArrayGetCount(order);
				}

				n = CFArrayGetCount(services);
				if (n > 1) {
					CFMutableArrayRef	sorted;

					sorted = CFArrayCreateMutableCopy(NULL, 0, services);
					CFArraySortValues(sorted,
							  CFRangeMake(0, CFArrayGetCount(sorted)),
							  _SCNetworkServiceCompare,
							  (void *)order);
					CFRelease(services);
					services = sorted;
				}

				for (CFIndex i = 0; i < n; i++) {
					CFStringRef		bsdName;
					SCNetworkInterfaceRef	interface;
					CFIndex			o;
					CFIndex			orderIndex	= kCFNotFound;
					SCNetworkServiceRef	service;
					CFStringRef		serviceName;
					CFStringRef		serviceID;
					CFStringRef		userDefinedName;

					service     = CFArrayGetValueAtIndex(services, i);
					serviceID   = SCNetworkServiceGetServiceID(service);
					serviceName = SCNetworkServiceGetName(service);
					if (serviceName == NULL) serviceName = CFSTR("");

					interface       = SCNetworkServiceGetInterface(service);
					bsdName         = SCNetworkInterfaceGetBSDName(interface);

					userDefinedName = __SCNetworkInterfaceGetUserDefinedName(interface);
					if (_SC_CFEqual(serviceName, userDefinedName)) {
						userDefinedName = NULL;
					}

					if (order != NULL) {
						orderIndex  = CFArrayGetFirstIndexOfValue(order,
											  CFRangeMake(0, nOrder),
											  serviceID);
					}
					if (orderIndex != kCFNotFound) {
						SC_log(level, "    Service %2ld : %@, %2d (%@%s%@%s%@%s%s%s%s)",
						       orderIndex + 1,
						       serviceID,
						       __SCNetworkInterfaceOrder(interface),	// temp?
						       serviceName,
						       bsdName != NULL ? ", " : "",
						       bsdName != NULL ? bsdName : CFSTR(""),
						       userDefinedName != NULL ? " : " : "",
						       userDefinedName != NULL ? userDefinedName : CFSTR(""),
						       SCNetworkInterfaceGetDisablePrivateRelay(interface) ? ", no-private-relay" : "",
						       SCNetworkInterfaceGetDisableUntilNeeded(interface) ? ", disable-until-needed" : "",
						       _SCNetworkInterfaceIsHiddenConfiguration(interface) ? ", hidden" : "",
						       SCNetworkServiceGetEnabled(service) ? "" : ", disabled");
					} else {
						SC_log(level, "    Service    : %@, %2d (%@%s%@%s%@%s%s%s%s)",
						       serviceID,
						       __SCNetworkInterfaceOrder(interface),	// temp?
						       serviceName,
						       bsdName != NULL ? ", " : "",
						       bsdName != NULL ? bsdName : CFSTR(""),
						       userDefinedName != NULL ? " : " : "",
						       userDefinedName != NULL ? userDefinedName : CFSTR(""),
						       SCNetworkInterfaceGetDisablePrivateRelay(interface) ? ", no-private-relay" : "",
						       SCNetworkInterfaceGetDisableUntilNeeded(interface) ? ", disable-until-needed" : "",
						       _SCNetworkInterfaceIsHiddenConfiguration(interface) ? ", hidden" : "",
						       SCNetworkServiceGetEnabled(service) ? "" : ", disabled");
					}

					o = CFArrayGetFirstIndexOfValue(orphans, CFRangeMake(0, CFArrayGetCount(orphans)), service);
					if (o != kCFNotFound) {
						CFArrayRemoveValueAtIndex(orphans, o);
					}
				}

				CFRelease(services);
			}
		}

		CFRelease(sets);
		if (currentSet != NULL) {
			CFRelease(currentSet);
		}
	}

	n = CFArrayGetCount(orphans);
	if (n > 0) {
		SC_log(level, "  Orphans");

		for (CFIndex i = 0; i < n; i++) {
			CFStringRef		bsdName;
			SCNetworkInterfaceRef	interface;
			SCNetworkServiceRef	service;
			CFStringRef		serviceName;
			CFStringRef		serviceID;

			service     = CFArrayGetValueAtIndex(orphans, i);
			serviceID   = SCNetworkServiceGetServiceID(service);
			serviceName = SCNetworkServiceGetName(service);
			if (serviceName == NULL) serviceName = CFSTR("");

			interface   = SCNetworkServiceGetInterface(service);
			bsdName     = SCNetworkInterfaceGetBSDName(interface);

			SC_log(level, "    Service    : %@, %2d (%@%s%@)",
			       serviceID,
			       __SCNetworkInterfaceOrder(SCNetworkServiceGetInterface(service)),	// temp?
			       serviceName,
			       bsdName != NULL ? ", " : "",
			       bsdName != NULL ? bsdName : CFSTR(""));
		}
	}
	CFRelease(orphans);

	interfaces = SCBridgeInterfaceCopyAll(prefs);
	if (interfaces != NULL) {
		CFIndex		n;

		n = CFArrayGetCount(interfaces);
		if (n > 0) {
			SC_log(level, "  Bridge interfaces");
		}

		for (CFIndex i = 0; i < n; i++) {
			SCBridgeInterfaceRef	bridge;
			CFMutableStringRef	bridgeDescription;
			CFStringRef		bsdName;
			CFArrayRef		members;

			bridge = CFArrayGetValueAtIndex(interfaces, i);
			bsdName = SCNetworkInterfaceGetBSDName(bridge);
			if (bsdName == NULL) {
				continue;
			}

			bridgeDescription = CFStringCreateMutable(NULL, 0);
			CFStringAppendFormat(bridgeDescription, NULL, CFSTR("%@: "), bsdName);

			members = SCBridgeInterfaceGetMemberInterfaces(bridge);
			if (members != NULL) {
				CFIndex		n_members;

				n_members = CFArrayGetCount(members);
				for (CFIndex j = 0; j < n_members; j++) {
					SCNetworkInterfaceRef	member;

					member = CFArrayGetValueAtIndex(members, j);
					bsdName = SCNetworkInterfaceGetBSDName(member);
					if (bsdName == NULL) {
						bsdName = CFSTR("?");
					}
					CFStringAppendFormat(bridgeDescription, NULL, CFSTR("%s%@"),
								j == 0 ? "" : ", ",
								bsdName);
				}
			}

			SC_log(level, "    %@", bridgeDescription);
			CFRelease(bridgeDescription);
		}
		CFRelease(interfaces);
	}

#if	!TARGET_OS_IPHONE
	interfaces = SCBondInterfaceCopyAll(prefs);
	if (interfaces != NULL) {
		CFIndex		n;

		n = CFArrayGetCount(interfaces);
		if (n > 0) {
			SC_log(level, "  Ethernet Bond interfaces");
		}

		for (CFIndex i = 0; i < n; i++) {
			SCBondInterfaceRef	bond;
			CFMutableStringRef	bondDescription;
			CFStringRef		bsdName;
			CFArrayRef		members;

			bond = CFArrayGetValueAtIndex(interfaces, i);
			bsdName = SCNetworkInterfaceGetBSDName(bond);
			if (bsdName == NULL) {
				continue;
			}

			bondDescription = CFStringCreateMutable(NULL, 0);
			CFStringAppendFormat(bondDescription, NULL, CFSTR("%@: "), bsdName);

			members = SCBondInterfaceGetMemberInterfaces(bond);
			if (members != NULL) {
				CFIndex		n_members;

				n_members = CFArrayGetCount(members);
				for (CFIndex j = 0; j < n_members; j++) {
					SCNetworkInterfaceRef	member;

					member = CFArrayGetValueAtIndex(members, j);
					bsdName = SCNetworkInterfaceGetBSDName(member);
					if (bsdName == NULL) {
						bsdName = CFSTR("?");
					}
					CFStringAppendFormat(bondDescription, NULL, CFSTR("%s%@"),
								j == 0 ? "" : ", ",
								bsdName);
				}
			}

			SC_log(level, "    %@", bondDescription);
			CFRelease(bondDescription);
		}
		CFRelease(interfaces);
	}
#endif	// !TARGET_OS_IPHONE

	interfaces = SCVLANInterfaceCopyAll(prefs);
	if (interfaces != NULL) {
		CFIndex		n;

		n = CFArrayGetCount(interfaces);
		if (n > 0) {
			SC_log(level, "  VLAN interfaces");
		}

		for (CFIndex i = 0; i < n; i++) {
			CFStringRef		bsdName;
			SCVLANInterfaceRef	vlan;
			SCNetworkInterfaceRef	physical;
			CFStringRef		physicalName;
			CFNumberRef		tag;

			vlan = CFArrayGetValueAtIndex(interfaces, i);
			bsdName = SCNetworkInterfaceGetBSDName(vlan);
			if (bsdName == NULL) {
				continue;
			}

			physical = SCVLANInterfaceGetPhysicalInterface(vlan);
			physicalName = (physical != NULL) ? SCNetworkInterfaceGetBSDName(physical) : NULL;

			tag = SCVLANInterfaceGetTag(vlan);

			SC_log(level, "    %@: physical=%@, tag=%@", bsdName,
			       physicalName != NULL ? physicalName : CFSTR("?"),
			       tag != NULL ? tag : (CFNumberRef)CFSTR("?"));
		}
		CFRelease(interfaces);
	}

	return;
}

void
__SCNetworkConfigurationReport(int level, const char *description, SCPreferencesRef prefs, SCPreferencesRef ni_prefs)
{
	logConfiguration_NetworkInterfaces(level, description, ni_prefs);
	logConfiguration_preferences      (level, description, prefs);
	return;
}


#pragma mark -
#pragma mark Misc


static Boolean
isEffectivelyEmptyConfiguration(CFDictionaryRef config)
{
	Boolean		empty	= FALSE;
	CFIndex		n;

	n = isA_CFDictionary(config) ? CFDictionaryGetCount(config) : 0;
	switch (n) {
		case 0 :
			// if no keys
			empty = TRUE;
			break;
		case 1 :
			if (CFDictionaryContainsKey(config, kSCResvInactive)) {
				// ignore [effectively] empty configuration entities
				empty = TRUE;
			}
			break;
		default :
			break;
	}

	return empty;
}


__private_extern__ CFDictionaryRef
__SCNetworkConfigurationGetValue(SCPreferencesRef prefs, CFStringRef path)
{
	CFDictionaryRef config;

	config = SCPreferencesPathGetValue(prefs, path);
	if (isEffectivelyEmptyConfiguration(config)) {
		// ignore [effectively] empty configuration entities
		config = NULL;
	}

	return config;
}


__private_extern__ Boolean
__SCNetworkConfigurationSetValue(SCPreferencesRef	prefs,
				 CFStringRef		path,
				 CFDictionaryRef	config,
				 Boolean		keepInactive)
{
	Boolean			changed;
	CFDictionaryRef		curConfig;
	CFMutableDictionaryRef	newConfig	= NULL;
	Boolean			ok;

	if ((config != NULL) && !isA_CFDictionary(config)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	curConfig = SCPreferencesPathGetValue(prefs, path);
	curConfig = isA_CFDictionary(curConfig);

	if (config != NULL) {
		newConfig = CFDictionaryCreateMutableCopy(NULL, 0, config);
	}

	if (keepInactive) {
		if (config == NULL) {
			newConfig = CFDictionaryCreateMutable(NULL,
							      0,
							      &kCFTypeDictionaryKeyCallBacks,
							      &kCFTypeDictionaryValueCallBacks);
		}

		if (isA_CFDictionary(curConfig) && CFDictionaryContainsKey(curConfig, kSCResvInactive)) {
			// if currently disabled
			CFDictionarySetValue(newConfig, kSCResvInactive, kCFBooleanTrue);
		} else {
			// if currently enabled
			CFDictionaryRemoveValue(newConfig, kSCResvInactive);
		}
	}

	// check if the configuration changed
	changed = !_SC_CFEqual(curConfig, newConfig);

	// set new configuration
	if (!changed) {
		// if no change
		if (newConfig != NULL) CFRelease(newConfig);
		ok = TRUE;
	} else if (newConfig != NULL) {
		// if new configuration (or we are preserving a disabled state), update the prefs
		ok = SCPreferencesPathSetValue(prefs, path, newConfig);
		CFRelease(newConfig);
	} else {
		// update the prefs
		ok = SCPreferencesPathRemoveValue(prefs, path);
		if (!ok && (SCError() == kSCStatusNoKey)) {
			ok = TRUE;
		}
	}

	return ok;
}


__private_extern__ Boolean
__getPrefsEnabled(SCPreferencesRef prefs, CFStringRef path)
{
	CFDictionaryRef config;

	config = SCPreferencesPathGetValue(prefs, path);
	if (isA_CFDictionary(config) && CFDictionaryContainsKey(config, kSCResvInactive)) {
		return FALSE;
	}

	return TRUE;
}


__private_extern__ Boolean
__setPrefsEnabled(SCPreferencesRef      prefs,
		  CFStringRef		path,
		  Boolean		enabled)
{
	Boolean					changed;
	CFDictionaryRef				curConfig;
	CFMutableDictionaryRef 			newConfig       = NULL;
	Boolean					ok		= FALSE;

	// preserve current configuration
	curConfig = SCPreferencesPathGetValue(prefs, path);
	if (curConfig != NULL) {
		if (!isA_CFDictionary(curConfig)) {
			_SCErrorSet(kSCStatusFailed);
			return FALSE;
		}
		newConfig = CFDictionaryCreateMutableCopy(NULL, 0, curConfig);

		if (enabled) {
			// enable
			CFDictionaryRemoveValue(newConfig, kSCResvInactive);
		} else {
			// disable
			CFDictionarySetValue(newConfig, kSCResvInactive, kCFBooleanTrue);
		}
	} else {
		if (!enabled) {
			// disable
			newConfig = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
			CFDictionarySetValue(newConfig, kSCResvInactive, kCFBooleanTrue);
		}
	}

	// check if the configuration changed
	changed = !_SC_CFEqual(curConfig, newConfig);

	// set new configuration
	if (!changed) {
		// if no change
		if (newConfig != NULL) CFRelease(newConfig);
		ok = TRUE;
	} else if (newConfig != NULL) {
		// if updated configuration (or we are establishing as disabled)

		ok = SCPreferencesPathSetValue(prefs, path, newConfig);
		CFRelease(newConfig);
	} else {
		ok = SCPreferencesPathRemoveValue(prefs, path);
		if (!ok && (SCError() == kSCStatusNoKey)) {
			ok = TRUE;
		}
	}

	return ok;
}

#if	TARGET_OS_OSX
#define SYSTEMCONFIGURATION_RESOURCES_PATH	SYSTEMCONFIGURATION_FRAMEWORK_PATH "/Resources"
#else
#define SYSTEMCONFIGURATION_RESOURCES_PATH	SYSTEMCONFIGURATION_FRAMEWORK_PATH
#endif	// TARGET_OS_OSX

#define NETWORKCONFIGURATION_RESOURCE_FILE	"NetworkConfiguration.plist"

static CFDictionaryRef
__copyTemplates()
{
	CFBundleRef     bundle;
	CFDictionaryRef templates;
	CFURLRef	url;

	bundle = _SC_CFBundleGet();
	if (bundle == NULL) {
		return NULL;
	}

	url = CFBundleCopyResourceURL(bundle, CFSTR("NetworkConfiguration"), CFSTR("plist"), NULL);
	if (url == NULL) {
		SC_log(LOG_ERR, "failed to GET resource URL to \"%s\". Trying harder...", NETWORKCONFIGURATION_RESOURCE_FILE);
		url = CFURLCreateWithFileSystemPath(NULL,
						    CFSTR(SYSTEMCONFIGURATION_RESOURCES_PATH
							  "/"
							  NETWORKCONFIGURATION_RESOURCE_FILE),
						    kCFURLPOSIXPathStyle,
						    TRUE);

		if (url == NULL) {
			SC_log(LOG_ERR, "failed to CREATE resource URL to \"%s\"", SYSTEMCONFIGURATION_RESOURCES_PATH
										   "/"
										   NETWORKCONFIGURATION_RESOURCE_FILE);
			return NULL;
		}
	}

	templates = _SCCreatePropertyListFromResource(url);
	CFRelease(url);

	if ((templates != NULL) && !isA_CFDictionary(templates)) {
		CFRelease(templates);
		return NULL;
	}

	return templates;
}


__private_extern__ CFDictionaryRef
__copyInterfaceTemplate(CFStringRef      interfaceType,
			CFStringRef      childInterfaceType)
{
	CFDictionaryRef interface       = NULL;
	CFDictionaryRef interfaces;
	CFDictionaryRef templates;

	templates = __copyTemplates();
	if (templates == NULL) {
		return NULL;
	}

	interfaces = CFDictionaryGetValue(templates, CFSTR("Interface"));
	if (!isA_CFDictionary(interfaces)) {
		CFRelease(templates);
		return NULL;
	}

	if (childInterfaceType == NULL) {
		interface = CFDictionaryGetValue(interfaces, interfaceType);
	} else {
		CFStringRef     expandedType;

		if (CFStringFind(childInterfaceType, CFSTR("."), 0).location != kCFNotFound) {
			// if "vendor" type
			childInterfaceType = CFSTR("*");
		}

		expandedType = CFStringCreateWithFormat(NULL,
							NULL,
							CFSTR("%@-%@"),
							interfaceType,
							childInterfaceType);
		interface = CFDictionaryGetValue(interfaces, expandedType);
		CFRelease(expandedType);
	}

	if (isA_CFDictionary(interface) && (CFDictionaryGetCount(interface) > 0)) {
		CFRetain(interface);
	} else {
		interface = NULL;
	}

	CFRelease(templates);

	return interface;
}


__private_extern__ CFDictionaryRef
__copyProtocolTemplate(CFStringRef      interfaceType,
		       CFStringRef      childInterfaceType,
		       CFStringRef      protocolType)
{
	CFDictionaryRef interface       = NULL;
	CFDictionaryRef protocol	= NULL;
	CFDictionaryRef protocols;
	CFDictionaryRef templates;

	templates = __copyTemplates();
	if (templates == NULL) {
		return NULL;
	}

	protocols = CFDictionaryGetValue(templates, CFSTR("Protocol"));
	if (!isA_CFDictionary(protocols)) {
		CFRelease(templates);
		return NULL;
	}

	if (childInterfaceType == NULL) {
		interface = CFDictionaryGetValue(protocols, interfaceType);
	} else {
		CFStringRef     expandedType;

		if (CFStringFind(childInterfaceType, CFSTR("."), 0).location != kCFNotFound) {
			// if "vendor" type
			childInterfaceType = CFSTR("*");
		}

		expandedType = CFStringCreateWithFormat(NULL,
							NULL,
							CFSTR("%@-%@"),
							interfaceType,
							childInterfaceType);
		interface = CFDictionaryGetValue(protocols, expandedType);
		CFRelease(expandedType);
	}

	if (isA_CFDictionary(interface)) {
		protocol = CFDictionaryGetValue(interface, protocolType);
		if (isA_CFDictionary(protocol)) {
			CFRetain(protocol);
		} else {
			protocol = NULL;
		}
	}

	CFRelease(templates);

	return protocol;
}


__private_extern__ Boolean
__createInterface(int s, CFStringRef interface)
{
	struct ifreq	ifr;

	memset(&ifr, 0, sizeof(ifr));
	(void) _SC_cfstring_to_cstring(interface,
				       ifr.ifr_name,
				       sizeof(ifr.ifr_name),
				       kCFStringEncodingASCII);

	if (ioctl(s, SIOCIFCREATE, &ifr) == -1) {
		SC_log(LOG_NOTICE, "could not create interface \"%@\": %s",
		       interface,
		       strerror(errno));
		return FALSE;
	}

	return TRUE;
}


__private_extern__ Boolean
__destroyInterface(int s, CFStringRef interface)
{
	struct ifreq	ifr;

	memset(&ifr, 0, sizeof(ifr));
	(void) _SC_cfstring_to_cstring(interface,
				       ifr.ifr_name,
				       sizeof(ifr.ifr_name),
				       kCFStringEncodingASCII);

	if (ioctl(s, SIOCIFDESTROY, &ifr) == -1) {
		SC_log(LOG_NOTICE, "could not destroy interface \"%@\": %s",
		       interface,
		       strerror(errno));
		return FALSE;
	}

	return TRUE;
}


static CFDataRef
__copy_legacy_password(CFTypeRef password)
{
	if (password == NULL) {
		return NULL;
	}

	if (isA_CFData(password)) {
		CFIndex	n;

		n = CFDataGetLength(password);
		if ((n % sizeof(UniChar)) == 0) {
			CFStringEncoding	encoding;
			CFStringRef		str;

#ifdef	__BIG_ENDIAN__
			encoding = (*(CFDataGetBytePtr(password) + 1) == 0x00) ? kCFStringEncodingUTF16LE : kCFStringEncodingUTF16BE;
#else	// __LITTLE_ENDIAN__
			encoding = (*(CFDataGetBytePtr(password)    ) == 0x00) ? kCFStringEncodingUTF16BE : kCFStringEncodingUTF16LE;
#endif
			str = CFStringCreateWithBytes(NULL,
						      (const UInt8 *)CFDataGetBytePtr(password),
						      n,
						      encoding,
						      FALSE);
			password = CFStringCreateExternalRepresentation(NULL,
									str,
									kCFStringEncodingUTF8,
									0);
			CFRelease(str);
		} else {
			password = NULL;
		}
	} else if (isA_CFString(password) && (CFStringGetLength(password) > 0)) {
		// convert password to CFData
		password = CFStringCreateExternalRepresentation(NULL,
								password,
								kCFStringEncodingUTF8,
								0);
	} else {
		password = NULL;
	}

	return password;
}


__private_extern__
Boolean
__extract_password(SCPreferencesRef	prefs,
		   CFDictionaryRef	config,
		   CFStringRef		passwordKey,
		   CFStringRef		encryptionKey,
		   CFStringRef		encryptionKeyChainValue,
		   CFStringRef		unique_id,
		   CFDataRef		*password)
{
	CFStringRef	encryption	= NULL;
	Boolean		exists		= FALSE;

	// check for keychain password
	if (config != NULL) {
		encryption = CFDictionaryGetValue(config, encryptionKey);
	}
	if ((encryption == NULL) ||
	    (isA_CFString(encryption) &&
	     CFEqual(encryption, encryptionKeyChainValue))) {
		// check password
		if (password != NULL) {
			if (prefs != NULL) {
				*password = _SCPreferencesSystemKeychainPasswordItemCopy(prefs, unique_id);
			} else {
				*password = _SCSecKeychainPasswordItemCopy(NULL, unique_id);
			}
			exists = (*password != NULL);
		} else {
			if (prefs != NULL) {
				exists = _SCPreferencesSystemKeychainPasswordItemExists(prefs, unique_id);
			} else {
				exists = _SCSecKeychainPasswordItemExists(NULL, unique_id);
			}
		}
	}

	// as needed, check for in-line password
	if (!exists && (encryption == NULL) && (config != NULL)) {
		CFDataRef	inline_password;

		inline_password = CFDictionaryGetValue(config, passwordKey);
		inline_password = __copy_legacy_password(inline_password);
		if (inline_password != NULL) {
			exists = TRUE;

			if (password != NULL) {
				*password = inline_password;
			} else {
				CFRelease(inline_password);
			}
		}
	}

	return exists;
}


__private_extern__
Boolean
__remove_password(SCPreferencesRef	prefs,
		  CFDictionaryRef	config,
		  CFStringRef		passwordKey,
		  CFStringRef		encryptionKey,
		  CFStringRef		encryptionKeyChainValue,
		  CFStringRef		unique_id,
		  CFDictionaryRef	*newConfig)
{
	CFStringRef		encryption	= NULL;
	Boolean			ok		= FALSE;

	// check for keychain password
	if (config != NULL) {
		encryption = CFDictionaryGetValue(config, encryptionKey);
	}
	if ((encryption == NULL) ||
	    (isA_CFString(encryption) &&
	     CFEqual(encryption, encryptionKeyChainValue))) {
		    // remove keychain password
		    if (prefs != NULL) {
			    ok = _SCPreferencesSystemKeychainPasswordItemRemove(prefs, unique_id);
		    } else {
			    ok = _SCSecKeychainPasswordItemRemove(NULL, unique_id);
		    }
	    }

	// as needed, check if we have an in-line password that we can remove
	if (!ok && (encryption == NULL) && (config != NULL)) {
		CFDataRef	inline_password;

		inline_password = CFDictionaryGetValue(config, passwordKey);
		inline_password = __copy_legacy_password(inline_password);
		if (inline_password != NULL) {
			CFRelease(inline_password);
			ok = TRUE;
		}
	}

	if (newConfig != NULL) {
		if (ok && (config != NULL)) {
			CFMutableDictionaryRef	temp;

			temp = CFDictionaryCreateMutableCopy(NULL, 0, config);
			CFDictionaryRemoveValue(temp, passwordKey);
			CFDictionaryRemoveValue(temp, encryptionKey);
			*newConfig = (CFDictionaryRef)temp;
		} else {
			*newConfig = NULL;
		}
	}

	return ok;
}


__private_extern__ Boolean
__rank_to_str(SCNetworkServicePrimaryRank rank, CFStringRef *rankStr)
{
	switch (rank) {
		case kSCNetworkServicePrimaryRankDefault :
			*rankStr = NULL;
			break;
		case kSCNetworkServicePrimaryRankFirst :
			*rankStr = kSCValNetServicePrimaryRankFirst;
			break;
		case kSCNetworkServicePrimaryRankLast :
			*rankStr = kSCValNetServicePrimaryRankLast;
			break;
		case kSCNetworkServicePrimaryRankNever :
			*rankStr = kSCValNetServicePrimaryRankNever;
			break;
		case kSCNetworkServicePrimaryRankScoped :
			*rankStr = kSCValNetServicePrimaryRankScoped;
			break;
		default :
			return FALSE;
	}

	return TRUE;
}


__private_extern__ Boolean
__str_to_rank(CFStringRef rankStr, SCNetworkServicePrimaryRank *rank)
{
	if (isA_CFString(rankStr)) {
		if (CFEqual(rankStr, kSCValNetServicePrimaryRankFirst)) {
			*rank = kSCNetworkServicePrimaryRankFirst;
		} else if (CFEqual(rankStr, kSCValNetServicePrimaryRankLast)) {
			*rank = kSCNetworkServicePrimaryRankLast;
		} else if (CFEqual(rankStr, kSCValNetServicePrimaryRankNever)) {
			*rank = kSCNetworkServicePrimaryRankNever;
		} else if (CFEqual(rankStr, kSCValNetServicePrimaryRankScoped)) {
			*rank = kSCNetworkServicePrimaryRankScoped;
		} else {
			return FALSE;
		}
	} else if (rankStr == NULL) {
		*rank = kSCNetworkServicePrimaryRankDefault;
	} else {
		return FALSE;
	}

	return TRUE;
}


#define	kSCNetworkConfigurationFlagsBypassSystemInterfaces		(1<<0)
#define	kSCNetworkConfigurationFlagsBypassSystemInterfacesForced	(1<<1)


Boolean
_SCNetworkConfigurationBypassSystemInterfaces(SCPreferencesRef prefs)
{
	Boolean		bypass;
	uint32_t	nc_flags;

	nc_flags = __SCPreferencesGetNetworkConfigurationFlags(prefs);
	bypass = ((nc_flags & kSCNetworkConfigurationFlagsBypassSystemInterfaces) != 0);
	if (bypass ||
	    (nc_flags & kSCNetworkConfigurationFlagsBypassSystemInterfacesForced) != 0) {
		// if bypass flag explicitly requested
		return bypass;
	}

	if (!__SCPreferencesUsingDefaultPrefs(prefs)) {
		// if not using the default prefs (i.e. /L/P/SC/preferences.plist)
		return TRUE;
	}

	return FALSE;
}


void
_SCNetworkConfigurationSetBypassSystemInterfaces(SCPreferencesRef prefs, Boolean shouldBypass)
{
	uint32_t	nc_flags;

	nc_flags = __SCPreferencesGetNetworkConfigurationFlags(prefs);
	if (shouldBypass) {
		nc_flags |= kSCNetworkConfigurationFlagsBypassSystemInterfaces;
	} else {
		nc_flags &= ~kSCNetworkConfigurationFlagsBypassSystemInterfaces;
	}
	nc_flags |= kSCNetworkConfigurationFlagsBypassSystemInterfacesForced;
	__SCPreferencesSetNetworkConfigurationFlags(prefs, nc_flags);

	return;
}


