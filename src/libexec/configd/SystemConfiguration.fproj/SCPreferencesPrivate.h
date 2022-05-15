/*
 * Copyright (c) 2000-2005, 2007-2009, 2011, 2012, 2017-2021 Apple Inc. All rights reserved.
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

#ifndef _SCPREFERENCESPRIVATE_H
#define _SCPREFERENCESPRIVATE_H


#include <os/availability.h>
#include <TargetConditionals.h>
#include <sys/cdefs.h>
#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SCPreferences.h>


/*!
	@header SCPreferencesPrivate
 */

/*!
	@defined kSCPreferencesOptionAllowModelConflict
	@abstract The SCPreferences "option" used to indicate that "Model"
		property conflicts (in the default preferences configuration) should
		not be ignored.  Without this option a configuration having a
		model conflict wll be viewed as an empty.
 */
#define kSCPreferencesOptionAllowModelConflict		CFSTR("allow-model-conflict")	// CFBooleanRef

/*!
	@defined kSCPreferencesOptionAvoidDeadlock
	@abstract The SCPreferences "option" used to enable/disable
		deadlock avoidance for the default preferences configuration.
		Deadlocks will be avoided If TRUE or if not provided.
 */
#define kSCPreferencesOptionAvoidDeadlock		CFSTR("avoid-deadlock")		// CFBooleanRef

/*!
	@defined kSCPreferencesOptionChangeNetworkSet
	@abstract The SCPreferences "option" used to indicate that only the
		current network set (location) is being changed.
 */
#define kSCPreferencesOptionChangeNetworkSet		CFSTR("change-network-set")	// CFBooleanRef

/*!
	@defined kSCPreferencesOptionProtectionClass
	@abstract The SCPreferences "option" used to indicate the file
		 protection class of the .plist.
 */
#define kSCPreferencesOptionProtectionClass		CFSTR("ProtectionClass")	// CFStringRef["A"-"F"]

/*!
	@defined kSCPreferencesOptionRemoveWhenEmpty
	@abstract The SCPreferences "option" used to indicate that the .plist
		file should be removed when/if all keys have been removed.
 */
#define kSCPreferencesOptionRemoveWhenEmpty		CFSTR("remove-when-empty")	// CFBooleanRef

/*!
	@defined kSCPreferencesAuthorizationRight_network_set
	@abstract The authorization right used to control whether the current
		network set (location) can be changed.
 */
#define kSCPreferencesAuthorizationRight_network_set	"system.preferences.location"

/*!
	@defined kSCPreferencesAuthorizationRight_write
	@abstract The authorization right used to control whether the network
		configuration can be changed.
 */
#define kSCPreferencesAuthorizationRight_write		"system.services.systemconfiguration.network"

/*!
	@enum SCPreferencesKeyType
	@discussion Used with the SCDynamicStoreKeyCreatePreferences() function
		to describe the resulting CFStringRef argument.
	@constant kSCPreferencesKeyCommit Key used when new preferences are
		committed to the store
	@constant kSCPreferencesKeyApply Key used when new preferences are
		to be applied to the active system configuration.
 */
enum {
	kSCPreferencesKeyLock	= 1,
	kSCPreferencesKeyCommit	= 2,
	kSCPreferencesKeyApply	= 3
};
typedef	int32_t	SCPreferencesKeyType;


__BEGIN_DECLS

/*!
	@const kSCPreferencesUseEntitlementAuthorization
	@discussion An authorization value that can be passed to
		the SCPreferencesCreateWithAuthorization API (or
		the SCPreferencesCreateWithOptions SPI) to indicate
		that the entitlements of the current process should
		be used for authorization purposes.

		This value can ONLY be used with the SCPreferences
		APIs.
 */
extern const AuthorizationRef	kSCPreferencesUseEntitlementAuthorization;

/*!
	@function SCDynamicStoreKeyCreatePreferences
	@discussion Creates a key that can be used by the SCDynamicStoreSetNotificationKeys()
		function to receive notifications of changes to the saved
		preferences.
	@param allocator ...
	@param prefsID A string that identifies the name of the
		group of preferences to be accessed/updated.
	@param keyType A kSCPreferencesKeyType indicating the type a notification
		key to be returned.
	@result A notification string for the specified preference identifier.
 */
CFStringRef
SCDynamicStoreKeyCreatePreferences	(
					CFAllocatorRef		allocator,
					CFStringRef		prefsID,
					SCPreferencesKeyType	keyType
					)	API_DEPRECATED("No longer supported", macos(10.1,10.4), ios(2.0,2.0));

/*!
       @function SCPreferencesCreateCompanion
       @discussion Initiates access to a [companion] of the configuration
	       preferences.
       @param prefs The base preferences session.
       @param companionPrefsID  A string that identifies the companion name
		of the group of preferences to be accessed or updated.
       @result Returns a reference to the new SCPreferences.
	       You must release the returned value.
*/
SCPreferencesRef
SCPreferencesCreateCompanion		(
					 SCPreferencesRef	prefs,
					 CFStringRef		companionPrefsID
					)	SPI_AVAILABLE(macos(10.15.4), ios(13.4), tvos(13.4), watchos(6.2), bridgeos(4.0));

/*!
	@function SCPreferencesCreateWithOptions
	@discussion Initiates access to the per-system set of configuration
		preferences.
	@param allocator The CFAllocator that should be used to allocate
		memory for this preferences session.
		This parameter may be NULL in which case the current
		default CFAllocator is used.
		If this reference is not a valid CFAllocator, the behavior
		is undefined.
	@param name A string that describes the name of the calling
		process.
	@param prefsID A string that identifies the name of the
		group of preferences to be accessed or updated.
	@param authorization An authorization reference that is used to
		authorize any access to the enhanced privileges needed
		to manage the preferences session.
	@param options A CFDictionary with options that affect the
		configuration preferences and how the APIs interact
		with the plist.
	@result Returns a reference to the new SCPreferences.
		You must release the returned value.
 */
SCPreferencesRef
SCPreferencesCreateWithOptions		(
					 CFAllocatorRef		allocator,
					 CFStringRef		name,
					 CFStringRef		prefsID,
					 AuthorizationRef	authorization,
					 CFDictionaryRef	options
					 )			API_AVAILABLE(macos(10.7)) SPI_AVAILABLE(ios(5.0), tvos(9.0), watchos(1.0), bridgeos(1.0));

/*!
	@function SCPreferencesRemoveAllValues
	@discussion Removes all data associated with the preferences.

	This function removes all data associated with the preferences.
	To commit these changes to permanent storage a call must be made
	to the SCPreferencesCommitChanges function.
	@param prefs The preferences session.
	@result Returns TRUE if the value was removed;
		FALSE if the key did not exist or if an error occurred.
 */
Boolean
SCPreferencesRemoveAllValues		(
					 SCPreferencesRef	prefs
					 )			API_AVAILABLE(macos(10.7)) SPI_AVAILABLE(ios(5.0), tvos(9.0), watchos(1.0), bridgeos(1.0));

__END_DECLS

#endif	/* _SCPREFERENCESPRIVATE_H */
