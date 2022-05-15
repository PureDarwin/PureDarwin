/*
 * Copyright (c) 2006, 2008, 2018 Apple Inc. All rights reserved.
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

#ifndef	_SCPREFERENCESKEYCHAINPRIVATE_H
#define	_SCPREFERENCESKEYCHAINPRIVATE_H

/*
 * SCPreferencesKeychainPrivate.h
 * - routines to deal with keychain passwords
 */

#include <os/availability.h>
#include <TargetConditionals.h>
#include <sys/cdefs.h>
#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SCPreferences.h>
#include <Security/Security.h>

#pragma mark -
#pragma mark Keychain helper APIs

#define	kSCKeychainOptionsAllowRoot		CFSTR("AllowRoot")		// CFBoolean, allow uid==0 applications
#define kSCKeychainOptionsAllowedExecutables	CFSTR("AllowedExecutables")	// CFArray[CFURL]

__BEGIN_DECLS

SecKeychainRef
_SCSecKeychainCopySystemKeychain		(void)					API_AVAILABLE(macos(10.5), ios(2.0));

CFDataRef
_SCSecKeychainPasswordItemCopy			(SecKeychainRef		keychain,
						 CFStringRef		unique_id)	API_AVAILABLE(macos(10.5), ios(2.0));

Boolean
_SCSecKeychainPasswordItemExists		(SecKeychainRef		keychain,
						 CFStringRef		unique_id)	API_AVAILABLE(macos(10.5), ios(2.0));

Boolean
_SCSecKeychainPasswordItemRemove		(SecKeychainRef		keychain,
						 CFStringRef		unique_id)	API_AVAILABLE(macos(10.5), ios(2.0));

Boolean
_SCSecKeychainPasswordItemSet			(SecKeychainRef		keychain,
						 CFStringRef		unique_id,
						 CFStringRef		label,
						 CFStringRef		description,
						 CFStringRef		account,
						 CFDataRef		password,
						 CFDictionaryRef	options)	API_AVAILABLE(macos(10.5), ios(2.0));


#pragma mark -
#pragma mark "System" Keychain APIs (w/SCPreferences)


CFDataRef
_SCPreferencesSystemKeychainPasswordItemCopy	(SCPreferencesRef	prefs,
						 CFStringRef		unique_id)	API_AVAILABLE(macos(10.5), ios(2.0));

Boolean
_SCPreferencesSystemKeychainPasswordItemExists	(SCPreferencesRef	prefs,
						 CFStringRef		unique_id)	API_AVAILABLE(macos(10.5), ios(2.0));

Boolean
_SCPreferencesSystemKeychainPasswordItemRemove	(SCPreferencesRef	prefs,
						 CFStringRef		unique_id)	API_AVAILABLE(macos(10.5), ios(2.0));

Boolean
_SCPreferencesSystemKeychainPasswordItemSet	(SCPreferencesRef	prefs,
						 CFStringRef		unique_id,
						 CFStringRef		label,
						 CFStringRef		description,
						 CFStringRef		account,
						 CFDataRef		password,
						 CFDictionaryRef	options)	API_AVAILABLE(macos(10.5), ios(2.0));

__END_DECLS

#endif	// _SCPREFERENCESKEYCHAINPRIVATE_H
