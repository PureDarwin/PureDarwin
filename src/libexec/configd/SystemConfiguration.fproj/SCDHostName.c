/*
 * Copyright (c) 2000-2008, 2011, 2013-2021 Apple Inc. All rights reserved.
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
 * June 1, 2001			Allan Nathanson <ajn@apple.com>
 * - public API conversion
 *
 * January 8, 2001		Allan Nathanson <ajn@apple.com>
 * - initial revision
 */


#include <TargetConditionals.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFStringDefaultEncoding.h>	// for __CFStringGetUserDefaultEncoding
#include <Security/Security.h>
#include <Security/SecTask.h>
#include "SCPreferencesInternal.h"
#include "SCNetworkConfigurationInternal.h"




#ifndef	__OPEN_SOURCE
#pragma mark Support


static Boolean
keepPrivate(void)
{
	static Boolean		keepPrivate	= FALSE;
	static dispatch_once_t	once;

	dispatch_once(&once, ^{
		SecTaskRef	current_task;

		current_task = SecTaskCreateFromSelf(NULL);
		if (current_task != NULL) {
			CFBooleanRef	entitlement;

			entitlement = SecTaskCopyValueForEntitlement(current_task,
								     CFSTR("com.apple.security.on-demand-install-capable"),
								     NULL);
			if (entitlement != NULL) {
				if (isA_CFBoolean(entitlement)) {
					keepPrivate = CFBooleanGetValue(entitlement);
				}
				CFRelease(entitlement);
			}
			CFRelease(current_task);
		}
	});

	return keepPrivate;
}
#endif	// __OPEN_SOURCE


#pragma mark ComputerName


static CFStringEncoding
getNameEncoding(CFDictionaryRef dict)
{
	CFStringEncoding	encoding;
	CFNumberRef		num;

	if (!CFDictionaryGetValueIfPresent(dict,
					   kSCPropSystemComputerNameEncoding,
					   (const void **)&num) ||
	    !isA_CFNumber(num) ||
	    !CFNumberGetValue(num, kCFNumberSInt32Type, &encoding)) {
		encoding = CFStringGetSystemEncoding();
	}

	return encoding;
}


CFStringRef
_SCPreferencesCopyComputerName(SCPreferencesRef	prefs,
			       CFStringEncoding	*nameEncoding)
{
	CFDictionaryRef	dict;
	CFStringRef	name		= NULL;
	CFStringRef	path;
	Boolean		tempPrefs	= FALSE;

	if (prefs == NULL) {
		prefs = SCPreferencesCreate(NULL, CFSTR("_SCPreferencesCopyComputerName"), NULL);
		if (prefs == NULL) {
			return NULL;
		}
		tempPrefs = TRUE;
	}

	path = CFStringCreateWithFormat(NULL,
					NULL,
					CFSTR("/%@/%@"),
					kSCPrefSystem,
					kSCCompSystem);
	dict = SCPreferencesPathGetValue(prefs, path);
	CFRelease(path);

	if (dict != NULL) {
		if (isA_CFDictionary(dict)) {
			name = CFDictionaryGetValue(dict, kSCPropSystemComputerName);
			name = isA_CFString(name);
			if (name != NULL) {
				CFRetain(name);
			}
		}

		if (nameEncoding != NULL) {
			*nameEncoding = getNameEncoding(dict);
		}
	}

	if (tempPrefs)	CFRelease(prefs);
	_SCErrorSet(name != NULL ? kSCStatusOK : kSCStatusNoKey);
	return name;
}


CFStringRef
SCDynamicStoreKeyCreateComputerName(CFAllocatorRef allocator)
{
	return SCDynamicStoreKeyCreate(allocator,
				       CFSTR("%@/%@"),
				       kSCDynamicStoreDomainSetup,
				       kSCCompSystem);
}




CFStringRef
SCDynamicStoreCopyComputerName(SCDynamicStoreRef	store,
			       CFStringEncoding		*nameEncoding)
{
	CFDictionaryRef		dict		= NULL;
	CFStringRef		key;
	CFStringRef		name		= NULL;

#ifndef	__OPEN_SOURCE
	if (keepPrivate()) {
		return NULL;
	}
#endif	// __OPEN_SOURCE

	if (nameEncoding != NULL) {
		// set a default encoding
		*nameEncoding = kCFStringEncodingUTF8;
	}

	key  = SCDynamicStoreKeyCreateComputerName(NULL);
	dict = SCDynamicStoreCopyValue(store, key);
	CFRelease(key);
	if (dict == NULL) {
		/*
		 * Let's try looking in the preferences.plist file until
		 * (a) we add an API to retrieve the name regardless of
		 *     where it is stored and
		 * (b) this API is deprecated
		 */
		name = _SCPreferencesCopyComputerName(NULL, nameEncoding);
		goto done;
	}
	if (!isA_CFDictionary(dict)) {
		_SCErrorSet(kSCStatusNoKey);
		goto done;
	}

	name = isA_CFString(CFDictionaryGetValue(dict, kSCPropSystemComputerName));
	if (name == NULL) {
		_SCErrorSet(kSCStatusNoKey);
		goto done;
	}
	CFRetain(name);

	if (nameEncoding != NULL) {
		// return the "ComputerNameEncoding" value
		*nameEncoding = getNameEncoding(dict);
	}

	_SCErrorSet(kSCStatusOK);

    done :


	if (dict != NULL)	CFRelease(dict);
	return name;
}


Boolean
SCPreferencesSetComputerName(SCPreferencesRef	prefs,
			     CFStringRef	name,
			     CFStringEncoding	encoding)
{
	CFDictionaryRef		dict;
	CFMutableDictionaryRef	newDict;
	CFNumberRef		num;
	Boolean			ok;
	CFStringRef		path;

	if (name != NULL) {
		CFIndex	len;

		if (!isA_CFString(name)) {
			_SCErrorSet(kSCStatusInvalidArgument);
			return FALSE;
		}

		len = CFStringGetLength(name);
		if (len == 0) {
			name = NULL;
		}
	}

	path = CFStringCreateWithFormat(NULL,
					NULL,
					CFSTR("/%@/%@"),
					kSCPrefSystem,
					kSCCompSystem);

	dict = SCPreferencesPathGetValue(prefs, path);
	if (dict != NULL) {
		newDict = CFDictionaryCreateMutableCopy(NULL, 0, dict);
	} else {
		newDict = CFDictionaryCreateMutable(NULL,
						    0,
						    &kCFTypeDictionaryKeyCallBacks,
						    &kCFTypeDictionaryValueCallBacks);
	}

	if ((name != NULL) && (CFStringGetLength(name) > 0)) {
		CFDictionarySetValue(newDict, kSCPropSystemComputerName, name);

		num = CFNumberCreate(NULL, kCFNumberSInt32Type, &encoding);
		CFDictionarySetValue(newDict, kSCPropSystemComputerNameEncoding, num);
		CFRelease(num);

		CFDictionaryRemoveValue(newDict, kSCPropSystemComputerNameRegion);
		if (encoding == kCFStringEncodingMacRoman) {
			UInt32	userEncoding	= 0;
			UInt32	userRegion	= 0;

			__CFStringGetUserDefaultEncoding(&userEncoding, &userRegion);
			if ((userEncoding == kCFStringEncodingMacRoman) && (userRegion != 0)) {
				num = CFNumberCreate(NULL, kCFNumberSInt32Type, &userRegion);
				CFDictionarySetValue(newDict, kSCPropSystemComputerNameRegion, num);
				CFRelease(num);
			}
		}
	} else {
		CFDictionaryRemoveValue(newDict, kSCPropSystemComputerName);
		CFDictionaryRemoveValue(newDict, kSCPropSystemComputerNameEncoding);
		CFDictionaryRemoveValue(newDict, kSCPropSystemComputerNameRegion);
	}

	ok = __SCNetworkConfigurationSetValue(prefs, path, newDict, FALSE);
	if (ok && __SCPreferencesUsingDefaultPrefs(prefs)) {
		if (name != NULL) {
			SC_log(LOG_NOTICE, "attempting to set the computer name to \"%@\"", name);
		} else {
			SC_log(LOG_NOTICE, "attempting to reset the computer name");
		}
	}

	CFRelease(path);
	CFRelease(newDict);

	return ok;
}


#pragma mark -
#pragma mark HostName


CFStringRef
SCPreferencesGetHostName(SCPreferencesRef	prefs)
{
	CFDictionaryRef	dict;
	CFStringRef	name;
	CFStringRef	path;

#ifndef	__OPEN_SOURCE
	if (keepPrivate()) {
		return NULL;
	}
#endif	// __OPEN_SOURCE

	path = CFStringCreateWithFormat(NULL,
					NULL,
					CFSTR("/%@/%@"),
					kSCPrefSystem,
					kSCCompSystem);
	dict = SCPreferencesPathGetValue(prefs, path);
	CFRelease(path);

	if (!isA_CFDictionary(dict)) {
		_SCErrorSet(kSCStatusNoKey);
		return NULL;
	}

	name = isA_CFString(CFDictionaryGetValue(dict, kSCPropSystemHostName));
	if (name == NULL) {
		_SCErrorSet(kSCStatusNoKey);
		return NULL;
	}

	return name;
}


Boolean
SCPreferencesSetHostName(SCPreferencesRef	prefs,
			 CFStringRef		name)
{
	CFDictionaryRef		dict;
	CFMutableDictionaryRef	newDict;
	Boolean			ok;
	CFStringRef		path;

	if (name != NULL) {
		CFIndex	len;

		if (!isA_CFString(name)) {
			_SCErrorSet(kSCStatusInvalidArgument);
			return FALSE;
		}

		len = CFStringGetLength(name);
		if (len == 0) {
			name = NULL;
		}
	}

	path = CFStringCreateWithFormat(NULL,
					NULL,
					CFSTR("/%@/%@"),
					kSCPrefSystem,
					kSCCompSystem);

	dict = SCPreferencesPathGetValue(prefs, path);
	if (dict != NULL) {
		newDict = CFDictionaryCreateMutableCopy(NULL, 0, dict);
	} else {
		newDict = CFDictionaryCreateMutable(NULL,
						    0,
						    &kCFTypeDictionaryKeyCallBacks,
						    &kCFTypeDictionaryValueCallBacks);
	}

	if (name != NULL) {
		CFDictionarySetValue(newDict, kSCPropSystemHostName, name);
	} else {
		CFDictionaryRemoveValue(newDict, kSCPropSystemHostName);
	}

	ok = __SCNetworkConfigurationSetValue(prefs, path, newDict, FALSE);
	if (ok && __SCPreferencesUsingDefaultPrefs(prefs)) {
		if (name != NULL) {
			SC_log(LOG_NOTICE, "attempting to set the host name to \"%@\"", name);
		} else {
			SC_log(LOG_NOTICE, "attempting to reset the host name");
		}
	}

	CFRelease(path);
	CFRelease(newDict);

	return ok;
}


#pragma mark -
#pragma mark LocalHostName


CFStringRef
_SCPreferencesCopyLocalHostName(SCPreferencesRef	prefs)
{
	CFDictionaryRef	dict;
	CFStringRef	name		= NULL;
	CFStringRef	path;
	Boolean		tempPrefs	= FALSE;

	if (prefs == NULL) {
		prefs = SCPreferencesCreate(NULL, CFSTR("_SCPreferencesCopyLocalHostName"), NULL);
		if (prefs == NULL) {
			return NULL;
		}
		tempPrefs = TRUE;
	}

	path = CFStringCreateWithFormat(NULL,
					NULL,
					CFSTR("/%@/%@/%@"),
					kSCPrefSystem,
					kSCCompNetwork,
					kSCCompHostNames);
	dict = SCPreferencesPathGetValue(prefs, path);
	CFRelease(path);

	if (dict != NULL) {
		if (isA_CFDictionary(dict)) {
			name = CFDictionaryGetValue(dict, kSCPropNetLocalHostName);
			name = isA_CFString(name);
			if (name != NULL) {
				CFRetain(name);
			}
		}
	}

	if (tempPrefs)	CFRelease(prefs);
	_SCErrorSet(name != NULL ? kSCStatusOK : kSCStatusNoKey);
	return name;
}


CFStringRef
SCDynamicStoreKeyCreateHostNames(CFAllocatorRef allocator)
{
	return SCDynamicStoreKeyCreate(allocator,
				       CFSTR("%@/%@/%@"),
				       kSCDynamicStoreDomainSetup,
				       kSCCompNetwork,
				       kSCCompHostNames);
}


CFStringRef
SCDynamicStoreCopyLocalHostName(SCDynamicStoreRef store)
{
	CFDictionaryRef		dict		= NULL;
	CFStringRef		key;
	CFStringRef		name		= NULL;

#ifndef	__OPEN_SOURCE
	if (keepPrivate()) {
		return NULL;
	}
#endif	// __OPEN_SOURCE

	key  = SCDynamicStoreKeyCreateHostNames(NULL);
	dict = SCDynamicStoreCopyValue(store, key);
	CFRelease(key);
	if (dict == NULL) {
		/*
		 * Let's try looking in the preferences.plist file until
		 * (a) we add an API to retrieve the name regardless of
		 *     where it is stored and
		 * (b) this API is deprecated
		 */
		name = _SCPreferencesCopyLocalHostName(NULL);
		goto done;
	}
	if (!isA_CFDictionary(dict)) {
		_SCErrorSet(kSCStatusNoKey);
		goto done;
	}

	name = isA_CFString(CFDictionaryGetValue(dict, kSCPropNetLocalHostName));
	if (name == NULL) {
		_SCErrorSet(kSCStatusNoKey);
		goto done;
	}
	CFRetain(name);

	_SCErrorSet(kSCStatusOK);

    done :


	if (dict != NULL)	CFRelease(dict);
	return name;
}


Boolean
_SC_stringIsValidDNSName(const char *name)
{
	size_t		i;
	size_t		len	= strlen(name);
	char		prev	= '\0';
	const char	*scan;

	if (len == 0) {
		return FALSE;
	}

	for (scan = name, i = 0; i < len; i++, scan++) {
		char	ch	= *scan;
		char 	next	= *(scan + 1);

		if (prev == '.' || prev == '\0') {
			if (isalnum(ch) == 0) {
				/* a label must begin with a letter or digit */
				return FALSE;
			}
		} else if (next == '\0' || next == '.') {
			if (isalnum(ch) == 0) {
				/* a label must end with a letter or digit */
				return FALSE;
			}
		} else if (isalnum(ch) == 0) {
			switch (ch) {
				case '.':
					/* a label separator */
					break;
				case '-':
					/* hyphens are OK within a label */
					break;
				default:
					/* an invalid character */
					return FALSE;
			}
		}
		prev = ch;
	}

	return TRUE;
}


Boolean
_SC_CFStringIsValidDNSName(CFStringRef name)
{
	Boolean	clean	= FALSE;
	char	*str	= NULL;

	if (!isA_CFString(name)) {
		return FALSE;
	}

	str = _SC_cfstring_to_cstring(name, NULL, 0, kCFStringEncodingASCII);
	if (str == NULL) {
		return FALSE;
	}

	clean = _SC_stringIsValidDNSName(str);

	if (str != NULL)	CFAllocatorDeallocate(NULL, str);
	return clean;
}


Boolean
SCPreferencesSetLocalHostName(SCPreferencesRef	prefs,
			      CFStringRef	name)
{
	CFDictionaryRef		dict;
	CFMutableDictionaryRef	newDict;
	Boolean			ok;
	CFStringRef		path;

	if (name != NULL) {
		CFIndex	len;

		if (!isA_CFString(name)) {
			_SCErrorSet(kSCStatusInvalidArgument);
			return FALSE;
		}

		len = CFStringGetLength(name);
		if (len > 0) {
			if (!_SC_CFStringIsValidDNSName(name)) {
				_SCErrorSet(kSCStatusInvalidArgument);
				return FALSE;
			}

			if (CFStringFindWithOptions(name, CFSTR("."), CFRangeMake(0, len), 0, NULL)) {
				_SCErrorSet(kSCStatusInvalidArgument);
				return FALSE;
			}
		} else {
			name = NULL;
		}
	}

	path = CFStringCreateWithFormat(NULL,
					NULL,
					CFSTR("/%@/%@/%@"),
					kSCPrefSystem,
					kSCCompNetwork,
					kSCCompHostNames);

	dict = SCPreferencesPathGetValue(prefs, path);
	if (dict != NULL) {
		newDict = CFDictionaryCreateMutableCopy(NULL, 0, dict);
	} else {
		newDict = CFDictionaryCreateMutable(NULL,
						    0,
						    &kCFTypeDictionaryKeyCallBacks,
						    &kCFTypeDictionaryValueCallBacks);
	}

	if (name != NULL) {
		CFDictionarySetValue(newDict, kSCPropNetLocalHostName, name);
	} else {
		CFDictionaryRemoveValue(newDict, kSCPropNetLocalHostName);
	}

	ok = __SCNetworkConfigurationSetValue(prefs, path, newDict, FALSE);
	if (ok && __SCPreferencesUsingDefaultPrefs(prefs)) {
		if (name != NULL) {
			SC_log(LOG_NOTICE, "attempting to set the local host name to \"%@\"", name);
		} else {
			SC_log(LOG_NOTICE, "attempting to reset the local host name");
		}
	}

	CFRelease(path);
	CFRelease(newDict);

	return ok;
}


Boolean
_SC_CFStringIsValidNetBIOSName(CFStringRef name)
{
	if (!isA_CFString(name)) {
		return FALSE;
	}

	if (CFStringGetLength(name) > 15) {
		return FALSE;
	}

	return TRUE;
}
