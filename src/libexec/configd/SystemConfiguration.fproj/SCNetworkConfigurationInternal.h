/*
 * Copyright (c) 2004-2021 Apple Inc. All rights reserved.
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

#ifndef _SCNETWORKCONFIGURATIONINTERNAL_H
#define _SCNETWORKCONFIGURATIONINTERNAL_H

#include <TargetConditionals.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFRuntime.h>

#ifndef	SC_LOG_HANDLE
#define	SC_LOG_HANDLE	__log_SCNetworkConfiguration
#endif	// SC_LOG_HANDLE
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPrivate.h>
#include <SystemConfiguration/SCValidation.h>
#include "SCPreferencesPathKey.h"
#include "IPMonitorControl.h"
#include <IOKit/IOKitLib.h>
#include <IOKit/IOBSD.h>
#include <IOKit/network/IONetworkInterface.h>


#define	NETWORK_CONFIGURATION_VERSION	20191120


typedef struct {

	// base CFType information
	CFRuntimeBase		cfBase;

	// set id
	CFStringRef		setID;

	// prefs
	SCPreferencesRef	prefs;

	// name
	CFStringRef		name;

	// misc
	Boolean			established;

} SCNetworkSetPrivate, *SCNetworkSetPrivateRef;


typedef struct {

	// base CFType information
	CFRuntimeBase		cfBase;

	// service id
	CFStringRef		serviceID;

	// interface
	SCNetworkInterfaceRef   interface;

	// prefs
	SCPreferencesRef	prefs;

	// store (for live SCNetworkService)
	SCDynamicStoreRef	store;

	// name
	CFStringRef		name;

	// external identifiers
	CFMutableDictionaryRef	externalIDs;

} SCNetworkServicePrivate, *SCNetworkServicePrivateRef;


typedef struct {

	// base CFType information
	CFRuntimeBase		cfBase;

	// entity id
	CFStringRef		entityID;

	// service
	SCNetworkServiceRef     service;

} SCNetworkProtocolPrivate, *SCNetworkProtocolPrivateRef;


typedef struct {

	// base CFType information
	CFRuntimeBase		cfBase;

	// interface information
	CFStringRef		interface_type;		// interface type

	Boolean			active;

	// [non-localized] name
	CFStringRef		name;			// non-localized [display] name

	// localized name
	CFStringRef		localized_name;		// localized [display] name
	CFStringRef		localized_key;
	CFStringRef		localized_arg1;
	CFStringRef		localized_arg2;

	// [layered] interface
	SCNetworkInterfaceRef   interface;

	// prefs (for associated service, BOND interfaces, and VLAN interfaces)
	SCPreferencesRef	prefs;

	// SCDynamicStore
	SCDynamicStoreRef	store;

	// serviceID (NULL if not associated with a service)
	CFStringRef		serviceID;

	// unsaved configuration (when prefs not [yet] available)
	CFMutableDictionaryRef	unsaved;

	// [SCPreferences] interface entity information
	CFStringRef		entity_device;		// interface device
	CFStringRef		entity_device_unique;	// ... UniqueIdentifier
	CFStringRef		entity_type;		// interface type
	CFStringRef		entity_subtype;		// interface subtype

	// configuration information
	CFMutableArrayRef       supported_interface_types;
	CFMutableArrayRef       supported_protocol_types;

	// IORegistry (service plane) information
	CFDataRef		address;
	CFStringRef		addressString;
	Boolean			builtin;
	CFStringRef		configurationAction;
	Boolean			hiddenConfiguration;
	Boolean			hiddenInterface;
	CFStringRef		location;
	CFStringRef		path;
	uint64_t		entryID;
	CFMutableDictionaryRef	overrides;
	CFStringRef		prefix;
	Boolean			trustRequired;
	Boolean			isEphemeral;
	Boolean			isSelfNamed;
	CFNumberRef		type;
	CFNumberRef		unit;
	CFNumberRef		family;
	CFNumberRef		subfamily;
	struct {
		CFStringRef	name;
		CFNumberRef	vid;
		CFNumberRef	pid;
	} usb;

	// misc
	CFArrayRef		matchingMACs;
	unsigned int		sort_order;		// sort order for this interface

	// for BOND interfaces
	Boolean			supportsBond;
	struct {
		CFArrayRef		interfaces;
		CFNumberRef		mode;
		CFDictionaryRef		options;
	} bond;

	// for Bridge interfaces
	Boolean			supportsBridge;
	struct {
		CFArrayRef		interfaces;
		CFDictionaryRef		options;
	} bridge;

	// for VLAN interfaces
	Boolean			supportsVLAN;
	struct {
		SCNetworkInterfaceRef	interface;
		CFNumberRef		tag;		// e.g. 1 <= tag <= 4094
		CFDictionaryRef		options;
	} vlan;

	// for interface rank assertions
	IPMonitorControlRef	IPMonitorControl;

} SCNetworkInterfacePrivate, *SCNetworkInterfacePrivateRef;


__BEGIN_DECLS

#pragma mark -
#pragma mark SCNetworkConfiguration (internal)



Boolean
__SCNetworkConfigurationBackup			(SCPreferencesRef	prefs,
						 CFStringRef		suffix,
						 SCPreferencesRef	relativeTo);



#pragma mark -
#pragma mark SCNetworkInterface configuration (internal)


extern const SCNetworkServiceRef __kSCNetworkInterfaceSearchExternal;

extern const SCNetworkServiceRef __kSCNetworkInterfaceSearchSystem;

Boolean
__SCNetworkInterfaceMatchesName			(CFStringRef		name,
						 CFStringRef		key);

CFArrayRef
__SCNetworkInterfaceCopyAll_IONetworkInterface	(Boolean		keep_pre_configured);

/*!
 @function __SCNetworkInterfaceCopyStoredWithPreferences
 @discussion Create an array of network interfaces, which is present in the preferences
 in NetworkInteraces.plist
 @param ni_prefs Preference for network interfaces
 @result Array which contains SCNetworkInterfaceRef.
 You must release the returned value.
 */

CFArrayRef  // SCNetworkInterfaceRef
__SCNetworkInterfaceCopyStoredWithPreferences	(SCPreferencesRef	ni_prefs);

SCNetworkInterfacePrivateRef
__SCNetworkInterfaceCreateCopy			(CFAllocatorRef		allocator,
						 SCNetworkInterfaceRef  interface,
						 SCPreferencesRef	prefs,
						 CFStringRef		serviceID);

/*!
 @function __SCNetworkInterfaceCreateMappingUsingBSDName
 @discussion This function creates mapping of BSD name and network interface using
 preferences which point to the NetworkInterfaces.plist file.
 @param interfaces An array of network interfaces to use for the mapping.
 @result BSD Mapping in a dictionary.
 You must release the returned value.
 */
CFDictionaryRef
__SCNetworkInterfaceCreateMappingUsingBSDName	(CFArrayRef		interfaces);

SCNetworkInterfaceRef
__SCNetworkInterfaceCreateWithNIPreferencesUsingBSDName(CFAllocatorRef		allocator,
							SCPreferencesRef	ni_prefs,
							CFStringRef		bsdName);

SCNetworkInterfacePrivateRef
__SCNetworkInterfaceCreatePrivate		(CFAllocatorRef		allocator,
						 SCNetworkInterfaceRef	interface,
						 SCPreferencesRef	prefs,
						 CFStringRef		serviceID);

SCNetworkInterfacePrivateRef
_SCBondInterfaceCreatePrivate			(CFAllocatorRef		allocator,
						 CFStringRef		bond_if);

SCNetworkInterfacePrivateRef
_SCBridgeInterfaceCreatePrivate			(CFAllocatorRef		allocator,
						 CFStringRef		bridge_if);

SCNetworkInterfacePrivateRef
_SCVLANInterfaceCreatePrivate			(CFAllocatorRef		allocator,
						 CFStringRef		vlan_if);

CFDictionaryRef
__SCNetworkInterfaceCopyInterfaceEntity		(SCNetworkInterfaceRef	interface);

CFArrayRef
__SCNetworkInterfaceCopyDeepConfiguration       (SCNetworkSetRef	set,
						 SCNetworkInterfaceRef	interface);

CFStringRef
__SCNetworkInterfaceCopyOldLocalizedDisplayName	(SCNetworkInterfaceRef	interface,
						 CFStringRef		key_prefix);

CFStringRef
__SCNetworkInterfaceCopyOldNonLocalizedDisplayName
						(SCNetworkInterfaceRef	interface,
						 CFStringRef		key_prefix);

CFStringRef
__SCNetworkInterfaceGetDefaultConfigurationType	(SCNetworkInterfaceRef	interface);

CFStringRef
__SCNetworkInterfaceGetEntitySubType		(SCNetworkInterfaceRef interface);

CFStringRef
__SCNetworkInterfaceGetEntityType		(SCNetworkInterfaceRef interface);

CFStringRef
__SCNetworkInterfaceGetNonLocalizedDisplayName	(SCNetworkInterfaceRef	interface);

void
__SCNetworkInterfaceSetUserDefinedName(SCNetworkInterfaceRef interface, CFStringRef name);

/*!
 @function __SCNetworkInterfaceGetUserDefinedName
 @discussion This function returns the user defined name of the interface if available
 @param interface The network interface.
 @result String containing the user defined name.
 */
CFStringRef
__SCNetworkInterfaceGetUserDefinedName(SCNetworkInterfaceRef interface);

/*!
 @function __SCNetworkInterfaceIsActive
 @discussion Identifies if the configuration of network interface is active or not
 @param interface The network interface
 @result	TRUE if the interface configuration is active.
 */
Boolean
__SCNetworkInterfaceIsActive			(SCNetworkInterfaceRef	interface);

Boolean
__SCNetworkInterfaceIsMember			(SCPreferencesRef	prefs,
						 SCNetworkInterfaceRef	interface);

Boolean
__SCNetworkInterfaceEntityIsPPTP		(CFDictionaryRef	entity);

Boolean
__SCNetworkInterfaceIsValidExtendedConfigurationType
						(SCNetworkInterfaceRef	interface,
						 CFStringRef		extendedType,
						 Boolean		requirePerInterface);

CFPropertyListRef
__SCNetworkInterfaceGetTemplateOverrides	(SCNetworkInterfaceRef	interface,
						 CFStringRef		overrideType);

int
__SCNetworkInterfaceOrder			(SCNetworkInterfaceRef	interface);

/*!
 @function __SCNetworkInterfaceSaveStoredWithPreferences
 @discussion Saves the array of interfaces in the preferences passed in the function. The interfaces
 which are already present in the prefs file are replaced.
 @param prefs Preferences which contain the interfaces to be replaced. If NULL, then preferences on
 the system are used.
 @param interfacesToSave The new interfaces array which is to be stored in preferences.
 @result TRUE if saving of the new interfaces was successful.
 */

Boolean
__SCNetworkInterfaceSaveStoredWithPreferences	(SCPreferencesRef prefs,
						 CFArrayRef interfacesToSave);

Boolean
__SCNetworkInterfaceSetConfiguration		(SCNetworkInterfaceRef  interface,
						 CFStringRef		extendedType,
						 CFDictionaryRef	config,
						 Boolean		okToHold);

void
__SCNetworkInterfaceSetDeepConfiguration	(SCNetworkSetRef	set,
						 SCNetworkInterfaceRef	interface,
						 CFArrayRef		configs);

/*!
 @function __SCNetworkInterfaceSetIOInterfaceUnity
 @discussion Will allow the caller to set IO Interface Unit
 @param interface The network interface
 @param unit The new interface unit to set
 */
void
__SCNetworkInterfaceSetIOInterfaceUnit		(SCNetworkInterfaceRef interface,
						 CFNumberRef		unit);

Boolean
__SCNetworkInterfaceSupportsVLAN		(CFStringRef		bsd_if);

void
__SCBondInterfaceListCollectMembers		(CFArrayRef 		interfaces,
						 CFMutableSetRef 	set);

Boolean
__SCBondInterfaceSetMemberInterfaces		(SCBondInterfaceRef bond,
						 CFArrayRef members);

void
__SCBridgeInterfaceListCollectMembers		(CFArrayRef 		interfaces,
						 CFMutableSetRef 	set);

Boolean
__SCBridgeInterfaceSetMemberInterfaces		(SCBridgeInterfaceRef	bridge,
						 CFArrayRef		members);

void
_SCNetworkInterfaceCacheOpen(void);

void
_SCNetworkInterfaceCacheClose(void);


#pragma mark -
#pragma mark SCNetworkInterface (NetworkInterfaces.plist) configuration (internal)


#define kSCNetworkInterfaceActive			"Active"
#define kSCNetworkInterfaceInfo				"SCNetworkInterfaceInfo"
#define kSCNetworkInterfaceType				"SCNetworkInterfaceType"
#define kSCNetworkInterfaceBSDName			kIOBSDNameKey
#define kSCNetworkInterfaceIOBuiltin			kIOBuiltin
#define kSCNetworkInterfaceIOInterfaceNamePrefix	kIOInterfaceNamePrefix
#define kSCNetworkInterfaceIOInterfaceType		kIOInterfaceType
#define kSCNetworkInterfaceIOInterfaceUnit		kIOInterfaceUnit
#define kSCNetworkInterfaceIOMACAddress			kIOMACAddress
#define kSCNetworkInterfaceIOPathMatch			kIOPathMatchKey
#define kSCNetworkInterfaceMatchingMACs			"MatchingMACs"


/*!
	@function __SCNetworkInterfaceCopyStorageEntity
	@discussion Create interface entity of network interface as seen in
		NetworkInterfaces.plist
	@param interface The network interface from which interface entity is create
	@result Dictionary which contains information about interface entity.
		You must release the returned value.
 */
CFDictionaryRef
__SCNetworkInterfaceCopyStorageEntity		(SCNetworkInterfaceRef	interface);


#pragma mark -
#pragma mark SCNetworkProtocol configuration (internal)


SCNetworkProtocolPrivateRef
__SCNetworkProtocolCreatePrivate		(CFAllocatorRef		allocator,
						 CFStringRef		entityID,
						 SCNetworkServiceRef	service);

Boolean
__SCNetworkProtocolIsValidType			(CFStringRef		protocolType);


#pragma mark -
#pragma mark SCNetworkService configuration (internal)


CFArrayRef /* of SCNetworkServiceRef's */
__SCNetworkServiceCopyAllEnabled		(SCPreferencesRef	prefs);

CFArrayRef /* of SCNetworkInterfaceRef's */
__SCNetworkServiceCopyAllInterfaces		(SCPreferencesRef	prefs);

SCNetworkServicePrivateRef
__SCNetworkServiceCreatePrivate			(CFAllocatorRef		allocator,
						 SCPreferencesRef	prefs,
						 CFStringRef		serviceID,
						 SCNetworkInterfaceRef	interface);

Boolean
__SCNetworkServiceExists			(SCNetworkServiceRef	service);

Boolean
__SCNetworkServiceExistsForInterface		(CFArrayRef		services,
						 SCNetworkInterfaceRef	interface);

Boolean
__SCNetworkServiceCreate			(SCPreferencesRef	prefs,
						 SCNetworkInterfaceRef	interface,
						 CFStringRef		userDefinedName);

Boolean
__SCNetworkServiceIsPPTP			(SCNetworkServiceRef	service);


/*!
 @function __SCNetworkServiceMigrateNew
 @discussion Adds network service to SCPreferencesRef if it doesn't exists
 @param prefs SCPreferencesRef
 @param service The network service
 @param bsdNameMapping Mapping of interface names between configurations
 @result TRUE if add service to prefs is successful
 */
Boolean
__SCNetworkServiceMigrateNew			(SCPreferencesRef	prefs,
						 SCNetworkServiceRef	service,
						 CFDictionaryRef	bsdNameMapping,
						 CFDictionaryRef	setMapping,
						 CFDictionaryRef	serviceSetMapping);

void
__SCNetworkServiceAddProtocolToService		(SCNetworkServiceRef		service,
						 CFStringRef			protocolType,
						 CFDictionaryRef		configuration,
						 Boolean			enabled);


#pragma mark -
#pragma mark SCNetworkSet configuration (internal)


Boolean
__SCNetworkSetExists				(SCNetworkSetRef		set);


#pragma mark -
#pragma mark Logging


os_log_t
__log_SCNetworkConfiguration			(void);


#pragma mark -
#pragma mark Miscellaneous (internal)


CFDictionaryRef
__copyInterfaceTemplate				(CFStringRef		interfaceType,
						 CFStringRef		childInterfaceType);

CFDictionaryRef
__copyProtocolTemplate				(CFStringRef		interfaceType,
						 CFStringRef		childInterfaceType,
						 CFStringRef		protocolType);

CFDictionaryRef
__SCNetworkConfigurationGetValue		(SCPreferencesRef       prefs,
						 CFStringRef		path);

Boolean
__SCNetworkConfigurationSetValue		(SCPreferencesRef       prefs,
						 CFStringRef		path,
						 CFDictionaryRef	config,
						 Boolean		keepInactive);

Boolean
__getPrefsEnabled				(SCPreferencesRef       prefs,
						 CFStringRef		path);

Boolean
__setPrefsEnabled				(SCPreferencesRef       prefs,
						 CFStringRef		path,
						 Boolean		enabled);

Boolean
__createInterface				(int			s,
						 CFStringRef		interface);

Boolean
__destroyInterface				(int			s,
						 CFStringRef		interface);

Boolean
__extract_password				(SCPreferencesRef	prefs,
						 CFDictionaryRef	config,
						 CFStringRef		passwordKey,
						 CFStringRef		encryptionKey,
						 CFStringRef		encryptionKeyChainValue,
						 CFStringRef		unique_id,
						 CFDataRef		*password);

Boolean
__remove_password				(SCPreferencesRef	prefs,
						 CFDictionaryRef	config,
						 CFStringRef		passwordKey,
						 CFStringRef		encryptionKey,
						 CFStringRef		encryptionKeyChainValue,
						 CFStringRef		unique_id,
						 CFDictionaryRef	*newConfig);

Boolean
__rank_to_str					(SCNetworkServicePrimaryRank	rank,
						 CFStringRef			*rankStr);

Boolean
__str_to_rank					(CFStringRef			rankStr,
						 SCNetworkServicePrimaryRank	*rank);

__END_DECLS

#endif	/* _SCNETWORKCONFIGURATIONINTERNAL_H */
