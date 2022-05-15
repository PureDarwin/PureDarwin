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

/*
 * Modification History
 *
 * May 13, 2004		Allan Nathanson <ajn@apple.com>
 * - initial revision
 */


#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFRuntime.h>
#include "SCNetworkConfigurationInternal.h"
#include "SCPreferencesInternal.h"

#include <net/if.h>
#include <pthread.h>

#include <CommonCrypto/CommonDigest.h>

#define EXTERNAL_ID_DOMAIN_PREFIX	"_"

static CFStringRef	__SCNetworkServiceCopyDescription	(CFTypeRef cf);
static void		__SCNetworkServiceDeallocate		(CFTypeRef cf);
static Boolean		__SCNetworkServiceEqual			(CFTypeRef cf1, CFTypeRef cf2);
static CFHashCode	__SCNetworkServiceHash			(CFTypeRef cf);


static CFTypeID __kSCNetworkServiceTypeID		= _kCFRuntimeNotATypeID;


static const CFRuntimeClass __SCNetworkServiceClass = {
	0,					// version
	"SCNetworkService",			// className
	NULL,					// init
	NULL,					// copy
	__SCNetworkServiceDeallocate,		// dealloc
	__SCNetworkServiceEqual,		// equal
	__SCNetworkServiceHash,			// hash
	NULL,					// copyFormattingDesc
	__SCNetworkServiceCopyDescription	// copyDebugDesc
};


static pthread_once_t		initialized		= PTHREAD_ONCE_INIT;


static CFStringRef
__SCNetworkServiceCopyDescription(CFTypeRef cf)
{
	CFAllocatorRef			allocator	= CFGetAllocator(cf);
	CFMutableStringRef		result;
	SCNetworkServiceRef		service		= (SCNetworkServiceRef)cf;
	SCNetworkServicePrivateRef	servicePrivate	= (SCNetworkServicePrivateRef)service;

	result = CFStringCreateMutable(allocator, 0);
	CFStringAppendFormat(result, NULL, CFSTR("<SCNetworkService %p [%p]> {"), service, allocator);
	CFStringAppendFormat(result, NULL, CFSTR("id = %@"), servicePrivate->serviceID);
	if (servicePrivate->prefs != NULL) {
		CFStringAppendFormat(result, NULL, CFSTR(", prefs = %p"), servicePrivate->prefs);
	} else if (servicePrivate->store != NULL) {
		CFStringAppendFormat(result, NULL, CFSTR(", store = %p"), servicePrivate->store);
	}
	if (servicePrivate->name != NULL) {
		CFStringAppendFormat(result, NULL, CFSTR(", name = %@"), servicePrivate->name);
	}
	if (!__SCNetworkServiceExists(service)) {
		CFStringAppendFormat(result, NULL, CFSTR(", REMOVED"));
	}
	CFStringAppendFormat(result, NULL, CFSTR("}"));

	return result;
}


static void
__SCNetworkServiceDeallocate(CFTypeRef cf)
{
	SCNetworkServicePrivateRef	servicePrivate	= (SCNetworkServicePrivateRef)cf;

	/* release resources */

	CFRelease(servicePrivate->serviceID);
	if (servicePrivate->interface != NULL) CFRelease(servicePrivate->interface);
	if (servicePrivate->prefs != NULL) CFRelease(servicePrivate->prefs);
	if (servicePrivate->store != NULL) CFRelease(servicePrivate->store);
	if (servicePrivate->name != NULL) CFRelease(servicePrivate->name);
	if (servicePrivate->externalIDs != NULL) CFRelease(servicePrivate->externalIDs);

	return;
}


static Boolean
__SCNetworkServiceEqual(CFTypeRef cf1, CFTypeRef cf2)
{
	SCNetworkServicePrivateRef	s1	= (SCNetworkServicePrivateRef)cf1;
	SCNetworkServicePrivateRef	s2	= (SCNetworkServicePrivateRef)cf2;

	if (s1 == s2)
		return TRUE;

	if (s1->prefs != s2->prefs)
		return FALSE;   // if not the same prefs

	if (!CFEqual(s1->serviceID, s2->serviceID))
		return FALSE;	// if not the same service identifier

	return TRUE;
}


static CFHashCode
__SCNetworkServiceHash(CFTypeRef cf)
{
	SCNetworkServicePrivateRef	servicePrivate	= (SCNetworkServicePrivateRef)cf;

	return CFHash(servicePrivate->serviceID);
}


static void
__SCNetworkServiceInitialize(void)
{
	__kSCNetworkServiceTypeID = _CFRuntimeRegisterClass(&__SCNetworkServiceClass);
	return;
}


__private_extern__ SCNetworkServicePrivateRef
__SCNetworkServiceCreatePrivate(CFAllocatorRef		allocator,
				SCPreferencesRef	prefs,
				CFStringRef		serviceID,
				SCNetworkInterfaceRef   interface)
{
	SCNetworkServicePrivateRef		servicePrivate;
	uint32_t				size;

	/* initialize runtime */
	pthread_once(&initialized, __SCNetworkServiceInitialize);

	/* allocate target */
	size           = sizeof(SCNetworkServicePrivate) - sizeof(CFRuntimeBase);
	servicePrivate = (SCNetworkServicePrivateRef)_CFRuntimeCreateInstance(allocator,
									      __kSCNetworkServiceTypeID,
									      size,
									      NULL);
	if (servicePrivate == NULL) {
		return NULL;
	}

	/* initialize non-zero/NULL members */
	servicePrivate->prefs		= (prefs != NULL) ? CFRetain(prefs): NULL;
	servicePrivate->serviceID	= CFStringCreateCopy(NULL, serviceID);
	servicePrivate->interface       = (interface != NULL) ? CFRetain(interface) : NULL;

	return servicePrivate;
}


#pragma mark -
#pragma mark Service ordering


CFComparisonResult
_SCNetworkServiceCompare(const void *val1, const void *val2, void *context)
{
	CFStringRef		id1;
	CFStringRef		id2;
	CFArrayRef		order	= (CFArrayRef)context;
	SCNetworkServiceRef	s1	= (SCNetworkServiceRef)val1;
	SCNetworkServiceRef	s2	= (SCNetworkServiceRef)val2;

	id1 = SCNetworkServiceGetServiceID(s1);
	id2 = SCNetworkServiceGetServiceID(s2);

	if (order != NULL) {
		CFIndex	o1;
		CFIndex	o2;
		CFRange	range;

		range = CFRangeMake(0, CFArrayGetCount(order));
		o1 = CFArrayGetFirstIndexOfValue(order, range, id1);
		o2 = CFArrayGetFirstIndexOfValue(order, range, id2);

		if (o1 > o2) {
			return (o2 != kCFNotFound) ? kCFCompareGreaterThan : kCFCompareLessThan;
		} else if (o1 < o2) {
			return (o1 != kCFNotFound) ? kCFCompareLessThan    : kCFCompareGreaterThan;
		}
	}

	return CFStringCompare(id1, id2, 0);
}


#pragma mark -
#pragma mark SCNetworkService APIs


#define	N_QUICK	64


__private_extern__ CFArrayRef /* of SCNetworkServiceRef's */
__SCNetworkServiceCopyAllEnabled(SCPreferencesRef prefs)
{
	CFMutableArrayRef	array	= NULL;
	CFIndex			i_sets;
	CFIndex			n_sets;
	CFArrayRef		sets;

	sets = SCNetworkSetCopyAll(prefs);
	if (sets == NULL) {
		return NULL;
	}

	n_sets = CFArrayGetCount(sets);
	for (i_sets = 0; i_sets < n_sets; i_sets++) {
		CFIndex		i_services;
		CFIndex		n_services;
		CFArrayRef	services;
		SCNetworkSetRef	set;

		set = CFArrayGetValueAtIndex(sets, i_sets);
		services = SCNetworkSetCopyServices(set);
		if (services == NULL) {
			continue;
		}

		n_services = CFArrayGetCount(services);
		for (i_services = 0; i_services < n_services; i_services++) {
			SCNetworkServiceRef service;

			service = CFArrayGetValueAtIndex(services, i_services);
			if (!SCNetworkServiceGetEnabled(service)) {
				// if not enabled
				continue;
			}

			if ((array == NULL) ||
			    !CFArrayContainsValue(array,
						  CFRangeMake(0, CFArrayGetCount(array)),
						  service)) {
				if (array == NULL) {
					array = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
				}
				CFArrayAppendValue(array, service);
			}
		}
		CFRelease(services);
	}
	CFRelease(sets);

	return array;
}


__private_extern__ Boolean
__SCNetworkServiceExistsForInterface(CFArrayRef services, SCNetworkInterfaceRef interface)
{
	CFIndex	i;
	CFIndex	n;

	n = isA_CFArray(services) ? CFArrayGetCount(services) : 0;
	for (i = 0; i < n; i++) {
		SCNetworkServiceRef	service;
		SCNetworkInterfaceRef	service_interface;

		service = CFArrayGetValueAtIndex(services, i);

		service_interface = SCNetworkServiceGetInterface(service);
		while (service_interface != NULL) {
			if (CFEqual(interface, service_interface)) {
				return TRUE;
			}

			service_interface = SCNetworkInterfaceGetInterface(service_interface);
		}
	}

	return FALSE;
}


static void
mergeDict(const void *key, const void *value, void *context)
{
	CFMutableDictionaryRef	newDict	= (CFMutableDictionaryRef)context;

	CFDictionarySetValue(newDict, key, value);
	return;
}


static CF_RETURNS_RETAINED CFDictionaryRef
_protocolTemplate(SCNetworkServiceRef service, CFStringRef protocolType)
{
	SCNetworkInterfaceRef		interface;
	SCNetworkServicePrivateRef      servicePrivate  = (SCNetworkServicePrivateRef)service;
	CFDictionaryRef			template	= NULL;

	interface = servicePrivate->interface;
	if (interface != NULL) {
		SCNetworkInterfaceRef   childInterface;
		CFStringRef		childInterfaceType      = NULL;
		CFStringRef		interfaceType;

		// get the template
		interfaceType = SCNetworkInterfaceGetInterfaceType(servicePrivate->interface);
		childInterface = SCNetworkInterfaceGetInterface(servicePrivate->interface);
		if (childInterface != NULL) {
			childInterfaceType = SCNetworkInterfaceGetInterfaceType(childInterface);
		}

		template = __copyProtocolTemplate(interfaceType, childInterfaceType, protocolType);
		if (template != NULL) {
			CFDictionaryRef		overrides;

			// move to the interface at the lowest layer
			while (childInterface != NULL) {
				interface = childInterface;
				childInterface = SCNetworkInterfaceGetInterface(interface);
			}

			overrides = __SCNetworkInterfaceGetTemplateOverrides(interface, protocolType);
			if (isA_CFDictionary(overrides)) {
				CFMutableDictionaryRef	newTemplate;

				newTemplate = CFDictionaryCreateMutableCopy(NULL, 0, template);
				CFDictionaryApplyFunction(overrides, mergeDict, newTemplate);
				CFRelease(template);
				template = newTemplate;
			}
		}
	}

	if (template == NULL) {
		template = CFDictionaryCreate(NULL,
					      NULL,
					      NULL,
					      0,
					      &kCFTypeDictionaryKeyCallBacks,
					      &kCFTypeDictionaryValueCallBacks);
	}

	return template;
}


Boolean
SCNetworkServiceAddProtocolType(SCNetworkServiceRef service, CFStringRef protocolType)
{
	CFDictionaryRef			entity;
	Boolean				newEnabled;
	CFDictionaryRef			newEntity       = NULL;
	Boolean				ok		= FALSE;
	CFStringRef			path;
	SCNetworkProtocolRef		protocol	= NULL;
	SCNetworkServicePrivateRef      servicePrivate  = (SCNetworkServicePrivateRef)service;

	if (!isA_SCNetworkService(service) || (servicePrivate->prefs == NULL)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	if (!__SCNetworkProtocolIsValidType(protocolType)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	if (!__SCNetworkServiceExists(service)) {
		SC_log(LOG_ERR, "SCNetworkServiceAddProtocolType() w/removed service\n  service = %@\n  protocol = %@",
		       service,
		       protocolType);
		_SC_crash_once("SCNetworkServiceAddProtocolType() w/removed service", NULL, NULL);
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	path = SCPreferencesPathKeyCreateNetworkServiceEntity(NULL,				// allocator
							      servicePrivate->serviceID,	// service
							      protocolType);			// entity

	entity = SCPreferencesPathGetValue(servicePrivate->prefs, path);
	if (entity != NULL) {
		// if "protocol" already exists
		_SCErrorSet(kSCStatusKeyExists);
		goto done;
	}

	newEntity = CFDictionaryCreate(NULL,
				       NULL,
				       NULL,
				       0,
				       &kCFTypeDictionaryKeyCallBacks,
				       &kCFTypeDictionaryValueCallBacks);
	ok = SCPreferencesPathSetValue(servicePrivate->prefs, path, newEntity);
	CFRelease(newEntity);
	newEntity = NULL;
	if (!ok) {
		goto done;
	}

	protocol  = SCNetworkServiceCopyProtocol(service, protocolType);
	assert(protocol != NULL);

	newEntity = _protocolTemplate(service, protocolType);
	assert(newEntity != NULL);

	ok = SCNetworkProtocolSetConfiguration(protocol, newEntity);
	if (!ok) {
		// could not set default configuration
		goto done;
	}

	newEnabled = !CFDictionaryContainsKey(newEntity, kSCResvInactive);
	ok = SCNetworkProtocolSetEnabled(protocol, newEnabled);
	if (!ok) {
		// could not enable/disable protocol
		goto done;
	}

    done :

	if (newEntity != NULL) CFRelease(newEntity);
	if (protocol != NULL) CFRelease(protocol);

	if (ok) {
		SC_log(LOG_DEBUG, "SCNetworkServiceAddProtocolType(): %@, %@", service, protocolType);
	}

	CFRelease(path);
	return ok;
}


CFArrayRef /* of SCNetworkServiceRef's */
SCNetworkServiceCopyAll(SCPreferencesRef prefs)
{
	CFMutableArrayRef	array;
	CFIndex			n;
	CFStringRef		path;
	CFDictionaryRef		services;


	path = SCPreferencesPathKeyCreateNetworkServices(NULL);
	services = SCPreferencesPathGetValue(prefs, path);
	CFRelease(path);

	if ((services != NULL) && !isA_CFDictionary(services)) {
		return NULL;
	}

	array = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

	n = (services != NULL) ? CFDictionaryGetCount(services) : 0;
	if (n > 0) {
		CFIndex		i;
		const void *    keys_q[N_QUICK];
		const void **   keys	= keys_q;
		const void *    vals_q[N_QUICK];
		const void **   vals	= vals_q;

		if (n > (CFIndex)(sizeof(keys_q) / sizeof(CFTypeRef))) {
			keys = CFAllocatorAllocate(NULL, n * sizeof(CFTypeRef), 0);
			vals = CFAllocatorAllocate(NULL, n * sizeof(CFPropertyListRef), 0);
		}
		CFDictionaryGetKeysAndValues(services, keys, vals);
		for (i = 0; i < n; i++) {
			CFDictionaryRef			entity;
			SCNetworkServicePrivateRef	servicePrivate;

			if (!isA_CFDictionary(vals[i])) {
				SC_log(LOG_INFO, "error w/service \"%@\"", keys[i]);
				continue;
			}

			entity = CFDictionaryGetValue(vals[i], kSCEntNetInterface);
			if (!isA_CFDictionary(entity)) {
				// if no "interface"
				SC_log(LOG_INFO, "no \"%@\" entity for service \"%@\"",
				       kSCEntNetInterface,
				       keys[i]);
				continue;
			}

			if (__SCNetworkInterfaceEntityIsPPTP(entity)) {
				SC_log(LOG_INFO, "PPTP services are no longer supported");
				continue;
			}

			servicePrivate = __SCNetworkServiceCreatePrivate(NULL, prefs, keys[i], NULL);
			assert(servicePrivate != NULL);
			CFArrayAppendValue(array, (SCNetworkServiceRef)servicePrivate);
			CFRelease(servicePrivate);
		}
		if (keys != keys_q) {
			CFAllocatorDeallocate(NULL, keys);
			CFAllocatorDeallocate(NULL, vals);
		}
	}

	return array;
}


__private_extern__
CFArrayRef /* of SCNetworkInterfaceRef's */
__SCNetworkServiceCopyAllInterfaces(SCPreferencesRef prefs)
{
	CFMutableArrayRef interfaces = NULL;
	CFArrayRef services = NULL;
	CFIndex servicesCount = 0;
	SCNetworkServiceRef service = NULL;
	SCNetworkInterfaceRef interface = NULL;

	services = SCNetworkServiceCopyAll(prefs);
	if (services == NULL) {
		goto done;
	}

	servicesCount = CFArrayGetCount(services);
	if (servicesCount == 0) {
		goto done;
	}

	interfaces = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

	for (CFIndex idx = 0; idx < servicesCount; idx++) {
		service = CFArrayGetValueAtIndex(services, idx);
		interface = SCNetworkServiceGetInterface(service);

		if (!isA_SCNetworkInterface(interface)) {
			continue;
		}
		CFArrayAppendValue(interfaces, interface);
	}

	if (CFArrayGetCount(interfaces) == 0) {
		// Do not return an empty array
		CFRelease(interfaces);
		interfaces = NULL;
	}

    done:

	if (services != NULL) {
		CFRelease(services);
	}
	return  interfaces;
}


/*
 * build a list of all of a services entity types that are associated
 * with the services interface.  The list will include :
 *
 * - entity types associated with the interface type (Ethernet, FireWire, PPP, ...)
 * - entity types associated with the interface sub-type (PPPSerial, PPPoE, L2TP, PPTP, ...)
 * - entity types associated with the hardware device (Ethernet, AirPort, FireWire, Modem, ...)
 */
static CFSetRef
_copyInterfaceEntityTypes(CFDictionaryRef protocols)
{
	CFDictionaryRef interface;
	CFMutableSetRef interface_entity_types;

	interface_entity_types = CFSetCreateMutable(NULL, 0, &kCFTypeSetCallBacks);

	interface = CFDictionaryGetValue(protocols, kSCEntNetInterface);
	if (isA_CFDictionary(interface)) {
		CFStringRef	entities[]	= { kSCPropNetInterfaceType,
						    kSCPropNetInterfaceSubType,
						    kSCPropNetInterfaceHardware };

		// include the "Interface" entity itself
		CFSetAddValue(interface_entity_types, kSCEntNetInterface);

		// include the entities associated with the interface
		for (size_t i = 0; i < sizeof(entities)/sizeof(entities[0]); i++) {
			CFStringRef     entity;

			entity = CFDictionaryGetValue(interface, entities[i]);
			if (isA_CFString(entity)) {
				CFSetAddValue(interface_entity_types, entity);
			}
		}

		/*
		 * and, because we've found some misguided network preference code
		 * developers leaving [PPP] entity dictionaries around even though
		 * they are unused and/or unneeded...
		 */
		CFSetAddValue(interface_entity_types, kSCEntNetPPP);
	}

	return interface_entity_types;
}


SCNetworkServiceRef
SCNetworkServiceCopy(SCPreferencesRef prefs, CFStringRef serviceID)
{
	CFDictionaryRef			entity;
	CFStringRef			path;
	SCNetworkServicePrivateRef      servicePrivate;

	if (!isA_CFString(serviceID)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}

	path = SCPreferencesPathKeyCreateNetworkServiceEntity(NULL,			// allocator
							      serviceID,		// service
							      kSCEntNetInterface);      // entity
	entity = SCPreferencesPathGetValue(prefs, path);
	CFRelease(path);

	if (!isA_CFDictionary(entity)) {
		// a "service" must have an "interface"
		_SCErrorSet(kSCStatusNoKey);
		return NULL;
	}

	if (__SCNetworkInterfaceEntityIsPPTP(entity)) {
		SC_log(LOG_INFO, "PPTP services are no longer supported");
		_SCErrorSet(kSCStatusNoKey);
		return NULL;
	}

	servicePrivate = __SCNetworkServiceCreatePrivate(NULL, prefs, serviceID, NULL);
	return (SCNetworkServiceRef)servicePrivate;
}


SCNetworkServiceRef
_SCNetworkServiceCopyActive(SCDynamicStoreRef store, CFStringRef serviceID)
{
	SCNetworkServicePrivateRef      servicePrivate;

	if (!isA_CFString(serviceID)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}

	servicePrivate = __SCNetworkServiceCreatePrivate(NULL, NULL, serviceID, NULL);
	assert(servicePrivate != NULL);
	if (store != NULL) {
		servicePrivate->store = store;
		CFRetain(servicePrivate->store);
	}
	return (SCNetworkServiceRef)servicePrivate;
}


SCNetworkProtocolRef
SCNetworkServiceCopyProtocol(SCNetworkServiceRef service, CFStringRef protocolType)
{
	CFSetRef			non_protocol_entities;
	CFStringRef			path;
	CFDictionaryRef			protocols;
	SCNetworkProtocolPrivateRef	protocolPrivate = NULL;
	SCNetworkServicePrivateRef	servicePrivate  = (SCNetworkServicePrivateRef)service;

	if (!isA_SCNetworkService(service) || (servicePrivate->prefs == NULL)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}

	if (!isA_CFString(protocolType)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}

	path = SCPreferencesPathKeyCreateNetworkServiceEntity(NULL,				// allocator
							      servicePrivate->serviceID,	// service
							      NULL);				// entity
	protocols = SCPreferencesPathGetValue(servicePrivate->prefs, path);
	CFRelease(path);

	if (!isA_CFDictionary(protocols)) {
		// if corrupt prefs
		_SCErrorSet(kSCStatusFailed);
		return NULL;
	}

	non_protocol_entities = _copyInterfaceEntityTypes(protocols);
	if (CFSetContainsValue(non_protocol_entities, protocolType)) {
		// if the "protocolType" matches an interface entity type
		_SCErrorSet(kSCStatusInvalidArgument);
		goto done;
	}

	if (!CFDictionaryContainsKey(protocols, protocolType)) {
		// if the "protocolType" entity does not exist
		_SCErrorSet(kSCStatusNoKey);
		goto done;
	}

	protocolPrivate = __SCNetworkProtocolCreatePrivate(NULL, protocolType, service);

    done :

	CFRelease(non_protocol_entities);

	return (SCNetworkProtocolRef)protocolPrivate;
}


CFArrayRef /* of SCNetworkProtocolRef's */
SCNetworkServiceCopyProtocols(SCNetworkServiceRef service)
{
	CFMutableArrayRef		array;
	CFIndex				n;
	CFSetRef			non_protocol_entities;
	CFStringRef			path;
	CFDictionaryRef			protocols;
	SCNetworkServicePrivateRef	servicePrivate  = (SCNetworkServicePrivateRef)service;

	if (!isA_SCNetworkService(service) || (servicePrivate->prefs == NULL)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}

	path = SCPreferencesPathKeyCreateNetworkServiceEntity(NULL,				// allocator
							      servicePrivate->serviceID,	// service
							      NULL);				// entity
	protocols = SCPreferencesPathGetValue(servicePrivate->prefs, path);
	CFRelease(path);

	if (!isA_CFDictionary(protocols)) {
		// if corrupt prefs
		_SCErrorSet(kSCStatusFailed);
		return NULL;
	}

	non_protocol_entities = _copyInterfaceEntityTypes(protocols);

	array = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

	n = CFDictionaryGetCount(protocols);
	if (n > 0) {
		CFIndex				i;
		const void *			keys_q[N_QUICK];
		const void **			keys		= keys_q;
		const void *			vals_q[N_QUICK];
		const void **			vals		= vals_q;

		if (n > (CFIndex)(sizeof(keys_q) / sizeof(CFTypeRef))) {
			keys = CFAllocatorAllocate(NULL, n * sizeof(CFTypeRef), 0);
			vals = CFAllocatorAllocate(NULL, n * sizeof(CFPropertyListRef), 0);
		}
		CFDictionaryGetKeysAndValues(protocols, keys, vals);
		for (i = 0; i < n; i++) {
			SCNetworkProtocolPrivateRef	protocolPrivate;

			if (!isA_CFDictionary(vals[i])) {
				// if it's not a dictionary then it can't be a protocol entity
				continue;
			}

			if (CFSetContainsValue(non_protocol_entities, keys[i])) {
				// skip any non-protocol (interface) entities
				continue;
			}

			protocolPrivate = __SCNetworkProtocolCreatePrivate(NULL, keys[i], service);
			CFArrayAppendValue(array, (SCNetworkProtocolRef)protocolPrivate);

			CFRelease(protocolPrivate);
		}
		if (keys != keys_q) {
			CFAllocatorDeallocate(NULL, keys);
			CFAllocatorDeallocate(NULL, vals);
		}
	}

	CFRelease(non_protocol_entities);

	return array;
}


static Boolean
__SCNetworkServiceSetInterfaceEntity(SCNetworkServiceRef     service,
				     SCNetworkInterfaceRef   interface)
{
	CFDictionaryRef			entity;
	Boolean				ok;
	CFStringRef			path;
	SCNetworkServicePrivateRef      servicePrivate		= (SCNetworkServicePrivateRef)service;

	path = SCPreferencesPathKeyCreateNetworkServiceEntity(NULL,				// allocator
							      servicePrivate->serviceID,	// service
							      kSCEntNetInterface);		// entity
	entity = __SCNetworkInterfaceCopyInterfaceEntity(interface);
	ok = SCPreferencesPathSetValue(servicePrivate->prefs, path, entity);
	CFRelease(entity);
	CFRelease(path);

	return ok;
}


SCNetworkServiceRef
SCNetworkServiceCreate(SCPreferencesRef prefs, SCNetworkInterfaceRef interface)
{
	CFArrayRef			components;
	CFArrayRef			interface_config;
	CFStringRef			interface_name;
	SCNetworkInterfaceRef		newInterface;
	CFStringRef			path;
	CFStringRef			prefix;
	CFStringRef			serviceID;
	SCNetworkServicePrivateRef	servicePrivate;
	CFArrayRef			supported_protocols;

	if (!isA_SCNetworkInterface(interface)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}

	// only allow network interfaces which support one or more protocols
	// to be added to a service.  The one exception is that we allow
	// third-party interface types to be configured.
	supported_protocols = SCNetworkInterfaceGetSupportedProtocolTypes(interface);
	if (supported_protocols == NULL) {
		CFStringRef	interface_type;

		interface_type = SCNetworkInterfaceGetInterfaceType(interface);
		if (CFStringFind(interface_type, CFSTR("."), 0).location == kCFNotFound) {
			_SCErrorSet(kSCStatusInvalidArgument);
			return NULL;
		}
	}

	// do not allow creation of a network service if the interface is a
	// member of a bond or bridge
	if (__SCNetworkInterfaceIsMember(prefs, interface)) {
		_SCErrorSet(kSCStatusKeyExists);
		return NULL;
	}

	// establish the service
	prefix = SCPreferencesPathKeyCreateNetworkServices(NULL);
	path = SCPreferencesPathCreateUniqueChild(prefs, prefix);
	CFRelease(prefix);
	if (path == NULL) {
		return NULL;
	}

	components = CFStringCreateArrayBySeparatingStrings(NULL, path, CFSTR("/"));
	CFRelease(path);

	serviceID = CFArrayGetValueAtIndex(components, 2);
	servicePrivate = __SCNetworkServiceCreatePrivate(NULL, prefs, serviceID, NULL);
	CFRelease(components);

	// duplicate the interface and associate the copy with the new service
	newInterface = (SCNetworkInterfaceRef)__SCNetworkInterfaceCreateCopy(NULL,
									     interface,
									     prefs,
									     serviceID);
	servicePrivate->interface = newInterface;

	// establish "default" configuration(s) for the interface
	for (interface = newInterface;
	     interface != NULL;
	     interface = SCNetworkInterfaceGetInterface(interface)) {
		SCNetworkInterfaceRef   childInterface;
		CFStringRef		childInterfaceType      = NULL;
		CFDictionaryRef		config;
		CFStringRef		interfaceType;
		CFMutableDictionaryRef	newConfig;
		CFDictionaryRef		overrides		= NULL;

		interfaceType = SCNetworkInterfaceGetInterfaceType(interface);
		childInterface = SCNetworkInterfaceGetInterface(interface);
		if (childInterface != NULL) {
			childInterfaceType = SCNetworkInterfaceGetInterfaceType(childInterface);
		}

		config = __copyInterfaceTemplate(interfaceType, childInterfaceType);
		if (config != NULL) {
			newConfig = CFDictionaryCreateMutableCopy(NULL, 0, config);
			CFRelease(config);
		} else {
			newConfig = CFDictionaryCreateMutable(NULL,
							      0,
							      &kCFTypeDictionaryKeyCallBacks,
							      &kCFTypeDictionaryValueCallBacks);
		}

		if (CFEqual(interfaceType, kSCNetworkInterfaceTypeBluetooth) ||
			   CFEqual(interfaceType, kSCNetworkInterfaceTypeModem    ) ||
			   CFEqual(interfaceType, kSCNetworkInterfaceTypeSerial   ) ||
			   CFEqual(interfaceType, kSCNetworkInterfaceTypeWWAN     )) {
			// modem (and variants)
			overrides = __SCNetworkInterfaceGetTemplateOverrides(interface, kSCNetworkInterfaceTypeModem);
			if (isA_CFDictionary(overrides) &&
			    CFDictionaryContainsKey(overrides, kSCPropNetModemConnectionScript)) {
				// the [Modem] "ConnectionScript" (and related keys) from the interface
				// should trump the settings from the configuration template.
				CFDictionaryRemoveValue(newConfig, kSCPropNetModemConnectionPersonality);
				CFDictionaryRemoveValue(newConfig, kSCPropNetModemConnectionScript);
				CFDictionaryRemoveValue(newConfig, kSCPropNetModemDeviceVendor);
				CFDictionaryRemoveValue(newConfig, kSCPropNetModemDeviceModel);
			}
		} else if (CFEqual(interfaceType, kSCNetworkInterfaceTypePPP) ||
			   CFEqual(interfaceType, kSCNetworkInterfaceTypeVPN)) {
			// PPP (and variants)
			overrides = __SCNetworkInterfaceGetTemplateOverrides(interface, kSCNetworkInterfaceTypePPP);
		} else {
			overrides = __SCNetworkInterfaceGetTemplateOverrides(interface, interfaceType);
		}

		if (isA_CFDictionary(overrides)) {
			CFDictionaryApplyFunction(overrides, mergeDict, newConfig);
		}

		if (CFDictionaryGetCount(newConfig) > 0) {
			if (!__SCNetworkInterfaceSetConfiguration(interface, NULL, newConfig, TRUE)) {
				SC_log(LOG_INFO, "__SCNetworkInterfaceSetConfiguration failed(), interface=%@, type=NULL",
				       interface);
			}
		}

		CFRelease(newConfig);
	}

	// add the interface [entity] to the service
	(void) __SCNetworkServiceSetInterfaceEntity((SCNetworkServiceRef)servicePrivate,
						    servicePrivate->interface);

	// push the [deep] interface configuration into the service.
	interface_config = __SCNetworkInterfaceCopyDeepConfiguration(NULL, servicePrivate->interface);
	__SCNetworkInterfaceSetDeepConfiguration(NULL, servicePrivate->interface, interface_config);
	if (interface_config != NULL) CFRelease(interface_config);

	// set the service name to match that of the associated interface
	//
	// Note: It might seem a bit odd to call SCNetworkServiceGetName
	// followed by an immediate call to SCNetworkServiceSetName.  The
	// trick here is that if no name has previously been set, the
	// "get" function will return the name of the associated interface.
	//
	// ... and we "set" a name to ensure that applications that do
	// not use the APIs will still find a UserDefinedName property
	// in the SCDynamicStore.
	//
	interface_name = SCNetworkServiceGetName((SCNetworkServiceRef)servicePrivate);
	if (interface_name != NULL) {
		(void) SCNetworkServiceSetName((SCNetworkServiceRef)servicePrivate,
					       interface_name);
	}

	SC_log(LOG_DEBUG, "SCNetworkServiceCreate(): %@", servicePrivate);

	return (SCNetworkServiceRef)servicePrivate;
}


static CF_RETURNS_RETAINED CFStringRef
copyInterfaceUUID(CFStringRef bsdName)
{
	union {
		unsigned char	sha256_bytes[CC_SHA256_DIGEST_LENGTH];
		CFUUIDBytes	uuid_bytes;
	} bytes;
	CC_SHA256_CTX	ctx;
	char		if_name[IF_NAMESIZE];
	CFUUIDRef	uuid;
	CFStringRef	uuid_str;

	// start with interface name
	memset(&if_name, 0, sizeof(if_name));
	(void) _SC_cfstring_to_cstring(bsdName,
				       if_name,
				       sizeof(if_name),
				       kCFStringEncodingASCII);

	// create SHA256 hash
	memset(&bytes, 0, sizeof(bytes));
	CC_SHA256_Init(&ctx);
	CC_SHA256_Update(&ctx,
			 if_name,
			 sizeof(if_name));
	CC_SHA256_Final(bytes.sha256_bytes, &ctx);

	// create UUID string
	uuid = CFUUIDCreateFromUUIDBytes(NULL, bytes.uuid_bytes);
	uuid_str = CFUUIDCreateString(NULL, uuid);
	CFRelease(uuid);

	return uuid_str;
}


SCNetworkServiceRef
_SCNetworkServiceCreatePreconfigured(SCPreferencesRef prefs, SCNetworkInterfaceRef interface)
{
	CFStringRef		bsdName;
	Boolean			ok;
	SCNetworkServiceRef	service;
	CFStringRef		serviceID;

	bsdName = SCNetworkInterfaceGetBSDName(interface);

	// create network service
	service = SCNetworkServiceCreate(prefs, interface);
	if (service == NULL) {
		SC_log(LOG_ERR, "could not create network service for \"%@\": %s",
		       bsdName,
		       SCErrorString(SCError()));
		return NULL;
	}

	// update network service to use a consistent serviceID
	serviceID = copyInterfaceUUID(bsdName);
	if (serviceID != NULL) {
		ok = _SCNetworkServiceSetServiceID(service, serviceID);
		CFRelease(serviceID);
		if (!ok) {
			SC_log(LOG_ERR, "_SCNetworkServiceSetServiceID() failed: %s",
			       SCErrorString(SCError()));
			// ... and keep whatever random UUID was created for the service
		}
	} else {
		SC_log(LOG_ERR, "could not create serviceID for \"%@\"", bsdName);
		// ... and we'll use whatever random UUID was created for the service
	}

	// establish [template] configuration
	ok = SCNetworkServiceEstablishDefaultConfiguration(service);
	if (!ok) {
		SC_log(LOG_ERR, "could not establish network service for \"%@\": %s",
		       bsdName,
		       SCErrorString(SCError()));
		SCNetworkServiceRemove(service);
		CFRelease(service);
		service = NULL;
	}

	return service;
}


Boolean
SCNetworkServiceEstablishDefaultConfiguration(SCNetworkServiceRef service)
{
	CFIndex				i;
	SCNetworkInterfaceRef		interface;
	CFIndex				n;
	Boolean				ok;
	CFArrayRef			protocolTypes;
	CFStringRef			rankStr;
	SCNetworkServicePrivateRef	servicePrivate  = (SCNetworkServicePrivateRef)service;

	if (!isA_SCNetworkService(service) || (servicePrivate->prefs == NULL)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	interface = SCNetworkServiceGetInterface(service);
	if (interface == NULL) {
		return FALSE;
	}

	protocolTypes = SCNetworkInterfaceGetSupportedProtocolTypes(interface);
	n = (protocolTypes != NULL) ? CFArrayGetCount(protocolTypes) : 0;
	for (i = 0; i < n; i++) {
		CFStringRef	protocolType;

		protocolType = CFArrayGetValueAtIndex(protocolTypes, i);
		ok = SCNetworkServiceAddProtocolType(service, protocolType);
		if (!ok) {
			SC_log(LOG_INFO,
			       "SCNetworkServiceEstablishDefaultConfiguration(): could not add protocol \"%@\"",
			       protocolType);
		}
	}

	rankStr = __SCNetworkInterfaceGetTemplateOverrides(interface, kSCPropNetServicePrimaryRank);
	if (isA_CFString(rankStr)) {
		SCNetworkServicePrimaryRank	rank;

		ok = __str_to_rank(rankStr, &rank);
		if (!ok) {
			SC_log(LOG_INFO,
			       "SCNetworkServiceEstablishDefaultConfiguration(): unknown rank \"%@\"",
			       rankStr);
			goto done;
		}

		ok = SCNetworkServiceSetPrimaryRank(service, rank);
		if (!ok) {
			SC_log(LOG_INFO,
			       "SCNetworkServiceEstablishDefaultConfiguration(): could not set rank \"%@\"",
			       rankStr);
			goto done;
		}
	}

    done :

	return TRUE;
}


Boolean
SCNetworkServiceGetEnabled(SCNetworkServiceRef service)
{
	Boolean				enabled;
	CFStringRef			path;
	SCNetworkServicePrivateRef      servicePrivate  = (SCNetworkServicePrivateRef)service;

	if (!isA_SCNetworkService(service) || (servicePrivate->prefs == NULL)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	path = SCPreferencesPathKeyCreateNetworkServiceEntity(NULL,				// allocator
							      servicePrivate->serviceID,	// service
							      NULL);				// entity
	enabled = __getPrefsEnabled(servicePrivate->prefs, path);
	CFRelease(path);

	return enabled;
}


SCNetworkInterfaceRef
SCNetworkServiceGetInterface(SCNetworkServiceRef service)
{
	SCNetworkServicePrivateRef      servicePrivate  = (SCNetworkServicePrivateRef)service;

	if (!isA_SCNetworkService(service) || (servicePrivate->prefs == NULL)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}

	if (servicePrivate->interface == NULL) {
		CFDictionaryRef entity;
		CFStringRef     path;

		path = SCPreferencesPathKeyCreateNetworkServiceEntity(NULL,				// allocator
								      servicePrivate->serviceID,	// service
								      kSCEntNetInterface);		// entity
		entity = SCPreferencesPathGetValue(servicePrivate->prefs, path);
		CFRelease(path);

		if (isA_CFDictionary(entity)) {
			servicePrivate->interface = _SCNetworkInterfaceCreateWithEntity(NULL, entity, service);
		}
	}

	return servicePrivate->interface;
}


static CFStringRef
__SCNetworkServiceGetName(SCNetworkServiceRef service, Boolean includeDefaultName)
{
	SCNetworkInterfaceRef		interface;
	CFStringRef			name		= NULL;
	SCNetworkServicePrivateRef	servicePrivate	= (SCNetworkServicePrivateRef)service;

	if (!isA_SCNetworkService(service) || (servicePrivate->prefs == NULL)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}

	name = servicePrivate->name;
	if (name != NULL) {
		if (includeDefaultName) {
			return name;
		}
	} else {
		CFDictionaryRef	entity;
		CFStringRef	path;

		path = SCPreferencesPathKeyCreateNetworkServiceEntity(NULL,				// allocator
								      servicePrivate->serviceID,	// service
								      NULL);				// entity
		entity = SCPreferencesPathGetValue(servicePrivate->prefs, path);
		CFRelease(path);

		if (isA_CFDictionary(entity)) {
			name = CFDictionaryGetValue(entity, kSCPropUserDefinedName);
			if (isA_CFString(name)) {
				Boolean	useSystemInterfaces;

				servicePrivate->name = CFRetain(name);
				useSystemInterfaces = !_SCNetworkConfigurationBypassSystemInterfaces(servicePrivate->prefs);
				if (!useSystemInterfaces) {
					return servicePrivate->name;
				}
			} else if (!includeDefaultName) {
				return NULL;
			}
		}
	}

	interface = SCNetworkServiceGetInterface(service);
	while (interface != NULL) {
		SCNetworkInterfaceRef   childInterface;
		CFStringRef		interfaceType;

		interfaceType = SCNetworkInterfaceGetInterfaceType(interface);
		if (CFEqual(interfaceType, kSCNetworkInterfaceTypeVPN)) {
			break;
		}

		childInterface = SCNetworkInterfaceGetInterface(interface);
		if ((childInterface == NULL) ||
		    CFEqual(childInterface, kSCNetworkInterfaceIPv4)) {
			break;
		}

		interface = childInterface;
	}

	if (interface != NULL) {
		int		i;
		CFStringRef	interface_name	= NULL;
		CFStringRef	suffix		= NULL;

		//
		// Check if the [stored] service name matches the non-localized interface
		// name.  If so, return the localized name.
		//
		// Note: the .../XX.lproj/NetworkInterfaces.strings file contains all
		//	 of the localization key to localized name mappings.
		//

		//
		// We also handle older "Built-in XXX" interface names that were too
		// long for the current UI.  If we find that the [stored] service name
		// matches the older name, return the newer (and shorter) localized
		// name.
		//
		// The mappings for these older names use the same localization key
		// prefixed with "X-".
		//

		//
		// We also have code interface names that used an older "Adaptor"
		// spelling vs. the preferred "Adapter".
		//
		// The mappings for these spelling corrections use the same
		// localization key prefixed with "Y-".
		//

		//
		// Note: the user/admin will no longer be able to set the service
		//	 name to "Built-in Ethernet" (or the interface names with
		//	 the old spellings).
		//

		for (i = 0; i < 5; i++) {
			if (servicePrivate->name == NULL) {
				// if no [stored] service name to compare
				break;
			}

			switch (i) {
				case 0 :
					// compare the non-localized interface name
					interface_name = __SCNetworkInterfaceGetNonLocalizedDisplayName(interface);
					if (interface_name != NULL) {
						CFRetain(interface_name);
					}
					break;
				case 1 :
					// compare the older [misspelled] localized name
					interface_name = __SCNetworkInterfaceCopyOldLocalizedDisplayName(interface, CFSTR("Y"));
					break;
				case 2 :
					// compare the older [misspelled] non-localized name
					interface_name = __SCNetworkInterfaceCopyOldNonLocalizedDisplayName(interface, CFSTR("Y"));
					break;
#if	!TARGET_OS_IPHONE
				case 3 :
					// compare the older "Built-in XXX" localized name
					interface_name = __SCNetworkInterfaceCopyOldLocalizedDisplayName(interface, CFSTR("X"));
					break;
				case 4 :
					// compare the older "Built-in XXX" non-localized name
					interface_name = __SCNetworkInterfaceCopyOldNonLocalizedDisplayName(interface, CFSTR("X"));
					break;
#else	// !TARGET_OS_IPHONE
				default :
					continue;
#endif	// !TARGET_OS_IPHONE
			}

			if (interface_name != NULL) {
				Boolean	match	= FALSE;

				if (CFEqual(name, interface_name)) {
					// if service name matches the OLD localized
					// interface name
					match = TRUE;
				} else if (CFStringHasPrefix(name, interface_name)) {
					CFIndex	prefixLen	= CFStringGetLength(interface_name);
					CFIndex	suffixLen	= CFStringGetLength(name);

					suffix = CFStringCreateWithSubstring(NULL,
									     name,
									     CFRangeMake(prefixLen, suffixLen - prefixLen));
					match = TRUE;
				}
				CFRelease(interface_name);

				if (match) {
					CFRelease(servicePrivate->name);
					servicePrivate->name = NULL;
					break;
				}
			}
		}

		//
		// if the service name has not been set, use the localized interface name
		//
		if ((servicePrivate->name == NULL) && includeDefaultName) {
			interface_name = SCNetworkInterfaceGetLocalizedDisplayName(interface);
			if (interface_name != NULL) {
				if (suffix != NULL) {
					servicePrivate->name = CFStringCreateWithFormat(NULL,
											NULL,
											CFSTR("%@%@"),
											interface_name,
											suffix);
				} else {
					servicePrivate->name = CFRetain(interface_name);
				}
			}
		}
		if (suffix != NULL) CFRelease(suffix);
	}

	return servicePrivate->name;
}


CFStringRef
SCNetworkServiceGetName(SCNetworkServiceRef service)
{
	CFStringRef	serviceName;

	serviceName = __SCNetworkServiceGetName(service, TRUE);
	return serviceName;
}


CFStringRef
SCNetworkServiceGetServiceID(SCNetworkServiceRef service)
{
	SCNetworkServicePrivateRef	servicePrivate	= (SCNetworkServicePrivateRef)service;

	if (!isA_SCNetworkService(service)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}

	return servicePrivate->serviceID;
}


CFTypeID
SCNetworkServiceGetTypeID(void)
{
	pthread_once(&initialized, __SCNetworkServiceInitialize);	/* initialize runtime */
	return __kSCNetworkServiceTypeID;
}


Boolean
SCNetworkServiceRemove(SCNetworkServiceRef service)
{
	Boolean				ok		= FALSE;
	CFStringRef			path;
	SCNetworkServicePrivateRef	servicePrivate	= (SCNetworkServicePrivateRef)service;
	CFArrayRef			sets;

	if (!isA_SCNetworkService(service) || (servicePrivate->prefs == NULL)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	if (!__SCNetworkServiceExists(service)) {
		SC_log(LOG_ERR, "SCNetworkServiceRemove() w/removed service\n  service = %@", service);
		_SC_crash_once("SCNetworkServiceRemove() w/removed service", NULL, NULL);
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	// remove service from all sets

	_SCNetworkInterfaceCacheOpen();

	sets = SCNetworkSetCopyAll(servicePrivate->prefs);
	if (sets != NULL) {
		CFIndex n;

		n = CFArrayGetCount(sets);
		for (CFIndex i = 0; i < n; i++) {
			SCNetworkSetRef	set;

			set = CFArrayGetValueAtIndex(sets, i);
			ok = SCNetworkSetRemoveService(set, service);
			if (!ok && (SCError() != kSCStatusNoKey)) {
				CFRelease(sets);
				return ok;
			}
		}
		CFRelease(sets);
	}

	_SCNetworkInterfaceCacheClose();

	// remove service

	path = SCPreferencesPathKeyCreateNetworkServiceEntity(NULL,				// allocator
							      servicePrivate->serviceID,	// service
							      NULL);				// entity
	ok = SCPreferencesPathRemoveValue(servicePrivate->prefs, path);
	CFRelease(path);

	if (ok) {
		SC_log(LOG_DEBUG, "SCNetworkServiceRemove(): %@", service);
	}

	return ok;
}


Boolean
SCNetworkServiceRemoveProtocolType(SCNetworkServiceRef service, CFStringRef protocolType)
{
	CFDictionaryRef			entity;
	Boolean				ok		= FALSE;
	CFStringRef			path;
	SCNetworkServicePrivateRef      servicePrivate  = (SCNetworkServicePrivateRef)service;

	if (!isA_SCNetworkService(service) || (servicePrivate->prefs == NULL)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	if (!__SCNetworkServiceExists(service)) {
		SC_log(LOG_ERR, "SCNetworkServiceRemoveProtocolType() w/removed service\n  service = %@\n  protocol = %@",
		       service,
		       protocolType);
		_SC_crash_once("SCNetworkServiceRemoveProtocolType() w/removed service", NULL, NULL);
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	if (!__SCNetworkProtocolIsValidType(protocolType)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	path = SCPreferencesPathKeyCreateNetworkServiceEntity(NULL,				// allocator
							      servicePrivate->serviceID,	// service
							      protocolType);			// entity

	entity = SCPreferencesPathGetValue(servicePrivate->prefs, path);
	if (entity == NULL) {
		// if "protocol" does not exist
		_SCErrorSet(kSCStatusNoKey);
		goto done;
	}

	ok = SCPreferencesPathRemoveValue(servicePrivate->prefs, path);

    done :

	if (ok) {
		SC_log(LOG_DEBUG, "SCNetworkServiceRemoveProtocolType(): %@, %@", service, protocolType);
	}

	CFRelease(path);
	return ok;
}


Boolean
SCNetworkServiceSetEnabled(SCNetworkServiceRef service, Boolean enabled)
{
	Boolean				ok;
	CFStringRef			path;
	SCNetworkServicePrivateRef      servicePrivate  = (SCNetworkServicePrivateRef)service;

	if (!isA_SCNetworkService(service) || (servicePrivate->prefs == NULL)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	if (!__SCNetworkServiceExists(service)) {
		SC_log(LOG_ERR, "SCNetworkServiceSetEnabled() w/removed service\n  service = %@", service);
		_SC_crash_once("SCNetworkProtocolSetEnabled() w/removed service", NULL, NULL);
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	// make sure that we do not enable a network service if the
	// associated interface is a member of a bond or bridge.
	if (enabled) {
		SCNetworkInterfaceRef	interface;

		interface = SCNetworkServiceGetInterface(service);
		if ((interface != NULL) &&
		    __SCNetworkInterfaceIsMember(servicePrivate->prefs, interface)) {
			_SCErrorSet(kSCStatusKeyExists);
			return FALSE;
		}
	}

	path = SCPreferencesPathKeyCreateNetworkServiceEntity(NULL,				// allocator
							      servicePrivate->serviceID,	// service
							      NULL);				// entity
	ok = __setPrefsEnabled(servicePrivate->prefs, path, enabled);
	CFRelease(path);

	if (ok) {
		SC_log(LOG_DEBUG, "SCNetworkServiceSetEnabled(): %@ -> %s",
		       service,
		       enabled ? "Enabled" : "Disabled");
	}

	return ok;
}


Boolean
SCNetworkServiceSetName(SCNetworkServiceRef service, CFStringRef name)
{
	CFDictionaryRef			entity;
	Boolean				ok		= FALSE;
	CFStringRef			path;
	CFStringRef			saveName	= NULL;
	SCNetworkServicePrivateRef	servicePrivate	= (SCNetworkServicePrivateRef)service;

	if (!isA_SCNetworkService(service) || (servicePrivate->prefs == NULL)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	if (!__SCNetworkServiceExists(service)) {
		SC_log(LOG_ERR, "SCNetworkServiceSetName() w/removed service\n  service = %@\n  name = %@",
		       service,
		       name != NULL ? name : CFSTR("<NULL>"));
		_SC_crash_once("SCNetworkServiceSetName() w/removed service", NULL, NULL);
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	if (name != NULL) {
		if (!isA_CFString(name)) {
			_SCErrorSet(kSCStatusInvalidArgument);
			return FALSE;
		}
		saveName = CFRetain(name);
	}

	if (name != NULL) {
		SCNetworkInterfaceRef	interface;

		interface = SCNetworkServiceGetInterface(service);
		while (interface != NULL) {
			SCNetworkInterfaceRef	childInterface;

			childInterface = SCNetworkInterfaceGetInterface(interface);
			if (childInterface == NULL) {
				break;
			}

			interface = childInterface;
		}

		if (interface != NULL) {
			CFStringRef	interface_name;

			interface_name = SCNetworkInterfaceGetLocalizedDisplayName(interface);
			if (interface_name != NULL) {
				if (CFEqual(name, interface_name)) {
					// if service name matches the localized interface name
					// then store the non-localized name.
					interface_name = __SCNetworkInterfaceGetNonLocalizedDisplayName(interface);
					if (interface_name != NULL) {
						CFRelease(saveName);
						saveName = CFRetain(interface_name);
					}
				} else if (CFStringHasPrefix(name, interface_name)) {
					CFIndex		prefixLen	= CFStringGetLength(interface_name);
					CFStringRef	suffix;
					CFIndex		suffixLen	= CFStringGetLength(name);

					// if service name matches the localized interface name plus
					// a few extra characters) then store the non-localized name with
					// the same suffix.
					suffix = CFStringCreateWithSubstring(NULL,
									     name,
									     CFRangeMake(prefixLen, suffixLen - prefixLen));
					interface_name = __SCNetworkInterfaceGetNonLocalizedDisplayName(interface);
					if (interface_name != NULL) {
						CFRelease(saveName);
						saveName = CFStringCreateWithFormat(NULL,
										    NULL,
										    CFSTR("%@%@"),
										    interface_name,
										    suffix);
					}
					CFRelease(suffix);
				}
			}
		}
	}

#define PREVENT_DUPLICATE_SERVICE_NAMES
#ifdef  PREVENT_DUPLICATE_SERVICE_NAMES
	if (name != NULL) {
		CFArrayRef      sets;

		// ensure that each service is uniquely named within its sets

		sets = SCNetworkSetCopyAll(servicePrivate->prefs);
		if (sets != NULL) {
			CFIndex		set_index;
			CFIndex		set_count;

			set_count = CFArrayGetCount(sets);
			for (set_index = 0; set_index < set_count; set_index++) {
				CFIndex		service_index;
				Boolean		isDup		= FALSE;
				Boolean		isMember	= FALSE;
				CFIndex		service_count;
				CFArrayRef      services;
				SCNetworkSetRef set		= CFArrayGetValueAtIndex(sets, set_index);

				services = SCNetworkSetCopyServices(set);

				service_count = CFArrayGetCount(services);
				for (service_index = 0; service_index < service_count; service_index++) {
					CFStringRef		otherID;
					CFStringRef		otherName;
					SCNetworkServiceRef     otherService;

					otherService = CFArrayGetValueAtIndex(services, service_index);

					otherID = SCNetworkServiceGetServiceID(otherService);
					if (CFEqual(servicePrivate->serviceID, otherID)) {
						// if the service is a member of this set
						isMember = TRUE;
						continue;
					}

					otherName = SCNetworkServiceGetName(otherService);
					if ((otherName != NULL) && CFEqual(name, otherName)) {
						isDup = TRUE;
						continue;
					}
				}

				CFRelease(services);

				if (isMember && isDup) {
					/*
					 * if this service is a member of the set and
					 * the "name" is not unique.
					 */
					CFRelease(sets);
					if (saveName != NULL) CFRelease(saveName);
					_SCErrorSet(kSCStatusKeyExists);
					return FALSE;
				}
			}

			CFRelease(sets);
		}
	}
#endif  /* PREVENT_DUPLICATE_SERVICE_NAMES */

	path = SCPreferencesPathKeyCreateNetworkServiceEntity(NULL,				// allocator
							      servicePrivate->serviceID,	// service
							      NULL);				// entity
	entity = SCPreferencesPathGetValue(servicePrivate->prefs, path);
	if (isA_CFDictionary(entity) ||
	    ((entity == NULL) && (name != NULL))) {
		CFMutableDictionaryRef	newEntity;

		if (entity != NULL) {
			newEntity = CFDictionaryCreateMutableCopy(NULL, 0, entity);
		} else {
			newEntity = CFDictionaryCreateMutable(NULL,
							      0,
							      &kCFTypeDictionaryKeyCallBacks,
							      &kCFTypeDictionaryValueCallBacks);
		}
		if (saveName != NULL) {
			CFDictionarySetValue(newEntity, kSCPropUserDefinedName, saveName);
		} else {
			CFDictionaryRemoveValue(newEntity, kSCPropUserDefinedName);
		}
		ok = SCPreferencesPathSetValue(servicePrivate->prefs, path, newEntity);
		CFRelease(newEntity);
	}
	CFRelease(path);
	if (saveName != NULL) CFRelease(saveName);

	if (servicePrivate->name != NULL) CFRelease(servicePrivate->name);
	if (name != NULL) CFRetain(name);
	servicePrivate->name = name;

	if (ok) {
		SC_log(LOG_DEBUG, "SCNetworkServiceSetName(): %@", service);
	}

	return ok;
}


#pragma mark -
#pragma mark SCNetworkService SPIs


__private_extern__
Boolean
__SCNetworkServiceExists(SCNetworkServiceRef service)
{
	CFDictionaryRef			entity;
	CFStringRef			path;
	SCNetworkServicePrivateRef      servicePrivate	= (SCNetworkServicePrivateRef)service;

	if (servicePrivate->prefs == NULL) {
		// if no prefs
		return FALSE;
	}

	path = SCPreferencesPathKeyCreateNetworkServiceEntity(NULL,				// allocator
							      servicePrivate->serviceID,	// service
							      kSCEntNetInterface);     		 // entity
	entity = SCPreferencesPathGetValue(servicePrivate->prefs, path);
	CFRelease(path);

	if (!isA_CFDictionary(entity)) {
		// a "service" must have an "interface"
		return FALSE;
	}

	return TRUE;
}


SCNetworkServicePrimaryRank
SCNetworkServiceGetPrimaryRank(SCNetworkServiceRef service)
{
	CFDictionaryRef			entity;
	Boolean				ok		= TRUE;
	CFStringRef			path;
	SCNetworkServicePrimaryRank	rank		= kSCNetworkServicePrimaryRankDefault;
	CFStringRef			rankStr		= NULL;
	SCNetworkServicePrivateRef      servicePrivate	= (SCNetworkServicePrivateRef)service;

	if (!isA_SCNetworkService(service)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return rank;
	}

	if (servicePrivate->prefs != NULL) {
		path = SCPreferencesPathKeyCreateNetworkServiceEntity(NULL,
								      servicePrivate->serviceID,
								      NULL);
		entity = SCPreferencesPathGetValue(servicePrivate->prefs, path);
		CFRelease(path);
		if (isA_CFDictionary(entity)) {
			rankStr = CFDictionaryGetValue(entity, kSCPropNetServicePrimaryRank);
			ok = __str_to_rank(rankStr, &rank);
		}
	} else if (servicePrivate->store != NULL) {
		path = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
								   kSCDynamicStoreDomainState,
								   servicePrivate->serviceID,
								   NULL);
		entity = SCDynamicStoreCopyValue(servicePrivate->store, path);
		CFRelease(path);
		if (entity != NULL) {
			if (isA_CFDictionary(entity)) {
				rankStr = CFDictionaryGetValue(entity, kSCPropNetServicePrimaryRank);
				ok = __str_to_rank(rankStr, &rank);
			}
			CFRelease(entity);
		}
	} else {
		_SCErrorSet(kSCStatusInvalidArgument);
		return rank;
	}

	if (!ok) {
		rank = kSCNetworkServicePrimaryRankDefault;
		_SCErrorSet(kSCStatusInvalidArgument);
	} else if (rank == kSCNetworkServicePrimaryRankDefault) {
		_SCErrorSet(kSCStatusOK);
	}

	return rank;
}


Boolean
SCNetworkServiceSetPrimaryRank(SCNetworkServiceRef		service,
			       SCNetworkServicePrimaryRank	newRank)
{
	Boolean				ok;
	CFDictionaryRef			entity;
	CFMutableDictionaryRef		newEntity;
	CFStringRef			path		= NULL;
	CFStringRef			rankStr		= NULL;
	SCNetworkServicePrivateRef      servicePrivate	= (SCNetworkServicePrivateRef)service;

	if (!isA_SCNetworkService(service)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	if ((servicePrivate->prefs != NULL) && !__SCNetworkServiceExists(service)) {
		SC_log(LOG_ERR, "SCNetworkServiceSetPrimaryRank() w/removed\n  service = %@", service);
		_SC_crash_once("SCNetworkServiceSetPrimaryRank() w/removed service", NULL, NULL);
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	ok = __rank_to_str(newRank, &rankStr);
	if (!ok) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	if (servicePrivate->prefs != NULL) {
		switch (newRank) {
		case kSCNetworkServicePrimaryRankDefault:
		case kSCNetworkServicePrimaryRankNever:
		case kSCNetworkServicePrimaryRankScoped:
			path = SCPreferencesPathKeyCreateNetworkServiceEntity(NULL,
									      servicePrivate->serviceID,
									      NULL);
			entity = SCPreferencesPathGetValue(servicePrivate->prefs, path);
			if (entity != NULL) {
				if (!isA_CFDictionary(entity)) {
					// if corrupt prefs
					_SCErrorSet(kSCStatusFailed);
					goto done;
				}
				newEntity = CFDictionaryCreateMutableCopy(NULL, 0, entity);
			} else {
				newEntity = CFDictionaryCreateMutable(NULL,
								      0,
								      &kCFTypeDictionaryKeyCallBacks,
								      &kCFTypeDictionaryValueCallBacks);
			}
			if (rankStr != NULL) {
				CFDictionarySetValue(newEntity, kSCPropNetServicePrimaryRank, rankStr);
			} else {
				CFDictionaryRemoveValue(newEntity, kSCPropNetServicePrimaryRank);
			}
			if (CFDictionaryGetCount(newEntity) > 0) {
				ok = SCPreferencesPathSetValue(servicePrivate->prefs, path, newEntity);
			} else {
				ok = SCPreferencesPathRemoveValue(servicePrivate->prefs, path);
			}
			CFRelease(newEntity);
			if (!ok) {
				goto done;
			}
			break;
		default:
			_SCErrorSet(kSCStatusInvalidArgument);
			return FALSE;
		}
	} else if (servicePrivate->store != NULL) {
		path = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
								   kSCDynamicStoreDomainState,
								   servicePrivate->serviceID,
								   NULL);
		entity = SCDynamicStoreCopyValue(servicePrivate->store, path);
		if (entity != NULL) {
			if (!isA_CFDictionary(entity)) {
				// if corrupt prefs
				CFRelease(entity);
				_SCErrorSet(kSCStatusFailed);
				goto done;
			}
			newEntity = CFDictionaryCreateMutableCopy(NULL, 0, entity);
			CFRelease(entity);
		} else {
			newEntity = CFDictionaryCreateMutable(NULL,
							      0,
							      &kCFTypeDictionaryKeyCallBacks,
							      &kCFTypeDictionaryValueCallBacks);
		}
		if (rankStr != NULL) {
			CFDictionarySetValue(newEntity, kSCPropNetServicePrimaryRank, rankStr);
		} else {
			CFDictionaryRemoveValue(newEntity, kSCPropNetServicePrimaryRank);
		}
		if (CFDictionaryGetCount(newEntity) > 0) {
			ok = SCDynamicStoreSetValue(servicePrivate->store, path, newEntity);
		} else {
			ok = SCDynamicStoreRemoveValue(servicePrivate->store, path);
		}
		CFRelease(newEntity);
		if (!ok) {
			goto done;
		}
	} else {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

    done :

	if (path != NULL)	CFRelease(path);
	return ok;
}


Boolean
_SCNetworkServiceIsVPN(SCNetworkServiceRef service)
{
	SCNetworkInterfaceRef	interface;
	CFStringRef		interfaceType;

	interface = SCNetworkServiceGetInterface(service);
	if (interface == NULL) {
		return FALSE;
	}

	interfaceType = SCNetworkInterfaceGetInterfaceType(interface);
	if (CFEqual(interfaceType, kSCNetworkInterfaceTypePPP)) {
		interface = SCNetworkInterfaceGetInterface(interface);
		if (interface == NULL) {
			return FALSE;
		}

		interfaceType = SCNetworkInterfaceGetInterfaceType(interface);
		if (CFEqual(interfaceType, kSCNetworkInterfaceTypeL2TP)) {
			return TRUE;
		}
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated"
		if (CFEqual(interfaceType, kSCNetworkInterfaceTypePPTP)) {
			return TRUE;
		}
#pragma GCC diagnostic pop
	} else if (CFEqual(interfaceType, kSCNetworkInterfaceTypeVPN)) {
		return TRUE;
	} else if (CFEqual(interfaceType, kSCNetworkInterfaceTypeIPSec)) {
		return TRUE;
	}

	return FALSE;
}


Boolean
SCNetworkServiceSetExternalID(SCNetworkServiceRef service, CFStringRef identifierDomain, CFStringRef identifier)
{
	CFStringRef			prefs_path;
	CFDictionaryRef			service_dictionary;
	SCNetworkServicePrivateRef	servicePrivate		= (SCNetworkServicePrivateRef)service;
	Boolean				success			= FALSE;
	CFStringRef			prefixed_domain;

	if (!isA_SCNetworkService(service) || (servicePrivate->prefs == NULL) || !isA_CFString(identifierDomain)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	if (!__SCNetworkServiceExists(service)) {
		SC_log(LOG_ERR, "SCNetworkServiceSetExternalID() w/removed\n  service = %@\n  id = %@",
		       service,
		       identifier);
		_SC_crash_once("SCNetworkServiceSetExternalID() w/removed service", NULL, NULL);
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	if (identifier != NULL && !isA_CFString(identifier)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	prefixed_domain = CFStringCreateWithFormat(NULL, 0, CFSTR("%s%@"), EXTERNAL_ID_DOMAIN_PREFIX, identifierDomain);

	prefs_path = SCPreferencesPathKeyCreateNetworkServiceEntity(NULL,
								    servicePrivate->serviceID,
								    NULL);

	service_dictionary = SCPreferencesPathGetValue(servicePrivate->prefs, prefs_path);
	if (isA_CFDictionary(service_dictionary) || ((service_dictionary == NULL) && (identifier != NULL))) {
		CFMutableDictionaryRef	new_service_dictionary;

		if (service_dictionary != NULL) {
			new_service_dictionary = CFDictionaryCreateMutableCopy(NULL, 0, service_dictionary);
		} else {
			new_service_dictionary = CFDictionaryCreateMutable(NULL,
									   0,
									   &kCFTypeDictionaryKeyCallBacks,
									   &kCFTypeDictionaryValueCallBacks);
		}

		if (identifier != NULL) {
			CFDictionarySetValue(new_service_dictionary, prefixed_domain, identifier);
		} else {
			CFDictionaryRemoveValue(new_service_dictionary, prefixed_domain);
		}
		success = SCPreferencesPathSetValue(servicePrivate->prefs, prefs_path, new_service_dictionary);
		CFRelease(new_service_dictionary);
	}
	CFRelease(prefs_path);

	if (identifier != NULL) {
	    if (servicePrivate->externalIDs == NULL) {
			servicePrivate->externalIDs = CFDictionaryCreateMutable(NULL,
										 0,
										 &kCFTypeDictionaryKeyCallBacks,
										 &kCFTypeDictionaryValueCallBacks);
	    }
	    CFDictionarySetValue(servicePrivate->externalIDs, prefixed_domain, identifier);
	} else {
	    if (servicePrivate->externalIDs != NULL) {
			CFDictionaryRemoveValue(servicePrivate->externalIDs, prefixed_domain);
	    }
	}

	CFRelease(prefixed_domain);

	if (!success) {
		_SCErrorSet(kSCStatusFailed);
	}

	return success;
}


CFStringRef
SCNetworkServiceCopyExternalID(SCNetworkServiceRef service, CFStringRef identifierDomain)
{
	CFStringRef			identifier		= NULL;
	CFStringRef			prefixed_domain;
	SCNetworkServicePrivateRef	service_private		= (SCNetworkServicePrivateRef)service;

	if (!isA_SCNetworkService(service) || (service_private->prefs == NULL) || !isA_CFString(identifierDomain)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}

	prefixed_domain = CFStringCreateWithFormat(NULL, 0, CFSTR("%s%@"), EXTERNAL_ID_DOMAIN_PREFIX, identifierDomain);

	if (service_private->externalIDs != NULL) {
		identifier = CFDictionaryGetValue(service_private->externalIDs, prefixed_domain);
		if (identifier != NULL) {
			CFRetain(identifier);
		}
	}

	if (identifier == NULL) {
		CFStringRef		prefs_path;
		CFDictionaryRef		service_dictionary;

		prefs_path = SCPreferencesPathKeyCreateNetworkServiceEntity(NULL,
									    service_private->serviceID,
									    NULL);

		service_dictionary = SCPreferencesPathGetValue(service_private->prefs, prefs_path);
		if (isA_CFDictionary(service_dictionary)) {
			identifier = CFDictionaryGetValue(service_dictionary, prefixed_domain);
			if (identifier != NULL) {
				CFRetain(identifier);
				if (service_private->externalIDs == NULL) {
					service_private->externalIDs = CFDictionaryCreateMutable(NULL,
												 0,
												 &kCFTypeDictionaryKeyCallBacks,
												 &kCFTypeDictionaryValueCallBacks);
				}
				CFDictionarySetValue(service_private->externalIDs, prefixed_domain, identifier);
			}
		}
		CFRelease(prefs_path);
	}

	CFRelease(prefixed_domain);

	if (identifier == NULL) {
		_SCErrorSet(kSCStatusNoKey);
	}

	return identifier;
}


typedef struct {
	CFStringRef	oldServiceID;
	CFStringRef	newServiceID;
} serviceContext, *serviceContextRef;


static void
replaceServiceID(const void *value, void *context)
{
	CFStringRef		link		= NULL;
	CFStringRef		oldLink;
	CFMutableArrayRef	newServiceOrder;
	CFStringRef		path;
	serviceContextRef	service_context	= (serviceContextRef)context;
	CFArrayRef		serviceOrder	= NULL;
	SCNetworkSetRef		set		= (SCNetworkSetRef)value;
	SCNetworkSetPrivateRef	setPrivate	= (SCNetworkSetPrivateRef)set;

	// update service order
	serviceOrder = SCNetworkSetGetServiceOrder(set);
	if ((isA_CFArray(serviceOrder) != NULL) &&
	    CFArrayContainsValue(serviceOrder,
				  CFRangeMake(0, CFArrayGetCount(serviceOrder)),
				  service_context->oldServiceID)) {
		CFIndex	count;
		CFIndex	serviceOrderIndex;

		// replacing all instances of old service ID with new one
		newServiceOrder = CFArrayCreateMutableCopy(NULL, 0, serviceOrder);
		count = CFArrayGetCount(newServiceOrder);
		for (serviceOrderIndex = 0; serviceOrderIndex < count; serviceOrderIndex++) {
			CFStringRef	serviceID;

			serviceID = CFArrayGetValueAtIndex(newServiceOrder, serviceOrderIndex);
			if (CFEqual(serviceID, service_context->oldServiceID)) {
				CFArraySetValueAtIndex(newServiceOrder, serviceOrderIndex, service_context->newServiceID);
			}
		}
		SCNetworkSetSetServiceOrder(set, newServiceOrder);
		CFRelease(newServiceOrder);
	}

	// check if service with old serviceID is part of the set
	path = SCPreferencesPathKeyCreateSetNetworkServiceEntity(NULL,				// allocator
								 setPrivate->setID,		// set
								 service_context->oldServiceID,	// service
								 NULL);				// entity
	oldLink = SCPreferencesPathGetLink(setPrivate->prefs, path);
	if (oldLink == NULL) {
		// don't make any changes if service with old serviceID is not found
		goto done;
	}

	// remove link between "set" and old "service"
	(void) SCPreferencesPathRemoveValue(setPrivate->prefs, path);
	CFRelease(path);

	// create the link between "set" and the "service"
	path = SCPreferencesPathKeyCreateSetNetworkServiceEntity(NULL,				// allocator
								 setPrivate->setID,		// set
								 service_context->newServiceID,	// service
								 NULL);				// entity
	link = SCPreferencesPathKeyCreateNetworkServiceEntity(NULL,				// allocator
							      service_context->newServiceID,	// service
							      NULL);				// entity
	(void) SCPreferencesPathSetLink(setPrivate->prefs, path, link);

    done:

	if (path != NULL) {
		CFRelease(path);
	}
	if (link != NULL) {
		CFRelease(link);
	}

	return;
}


Boolean
_SCNetworkServiceSetServiceID(SCNetworkServiceRef service, CFStringRef newServiceID)
{
	CFArrayRef			allSets		= NULL;
	CFDictionaryRef			entity;
	CFStringRef			newPath;
	Boolean				ok		= FALSE;
	CFStringRef			oldPath		= NULL;
	serviceContext			service_context;
	SCNetworkServicePrivateRef	servicePrivate	= (SCNetworkServicePrivateRef)service;

	if (!isA_SCNetworkService(service) || (servicePrivate->prefs == NULL)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	if (!isA_CFString(newServiceID)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	if (CFEqual(newServiceID, servicePrivate->serviceID)) {
		// no work needs to be done if new service ID is equal to current service ID
		return TRUE;
	}

	if (!__SCNetworkServiceExists(service)) {
		SC_log(LOG_ERR, "_SCNetworkServiceSetServiceID() w/removed service\n  service = %@\n  serviceID = %@",
		       service,
		       newServiceID);
		_SC_crash_once("_SCNetworkServiceSetServiceID() w/removed service", NULL, NULL);
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	newPath = SCPreferencesPathKeyCreateNetworkServiceEntity(NULL,				// allocator
								 newServiceID,			// service
								 NULL);				// entity
	entity = SCPreferencesPathGetValue(servicePrivate->prefs, newPath);
	if (isA_CFDictionary(entity)) {
		// if the new service already exists
		_SCErrorSet(kSCStatusKeyExists);
		goto done;
	}

	oldPath = SCPreferencesPathKeyCreateNetworkServiceEntity(NULL,				// allocator
								 servicePrivate->serviceID,	// service
								 NULL);				// entity
	entity = SCPreferencesPathGetValue(servicePrivate->prefs, oldPath);
	if (!isA_CFDictionary(entity)) {
		// if the service has already been removed
		_SCErrorSet(kSCStatusNoKey);
		goto done;
	}

	ok = SCPreferencesPathSetValue(servicePrivate->prefs, newPath, entity);
	if (!ok) goto done;

	ok = SCPreferencesPathRemoveValue(servicePrivate->prefs, oldPath);
	if (!ok) goto done;

	allSets = SCNetworkSetCopyAll(servicePrivate->prefs);

	service_context.newServiceID = newServiceID;
	service_context.oldServiceID = servicePrivate->serviceID;

	// find all sets w/oldServiceID and update
	// ... and update the serviceOrder
	CFArrayApplyFunction(allSets,
			     CFRangeMake(0, CFArrayGetCount(allSets)),
			     replaceServiceID,
			     &service_context);

	if (servicePrivate->interface != NULL) {
		SCNetworkInterfaceRef		newInterface;

		// duplicate the interface and associate the copy with the new service ID
		newInterface = (SCNetworkInterfaceRef)__SCNetworkInterfaceCreateCopy(NULL,
										     servicePrivate->interface,
										     servicePrivate->prefs,
										     newServiceID);
		CFRelease(servicePrivate->interface);
		servicePrivate->interface = newInterface;
	}

	SC_log(LOG_DEBUG, "_SCNetworkServiceSetServiceID(): %@ --> %@", service, newServiceID);

	// replace serviceID with new one
	CFRetain(newServiceID);
	CFRelease(servicePrivate->serviceID);
	servicePrivate->serviceID = newServiceID;

    done:

	if (oldPath != NULL) {
		CFRelease(oldPath);
	}
	if (newPath != NULL) {
		CFRelease(newPath);
	}
	if (allSets != NULL) {
		CFRelease(allSets);
	}

	return ok;
}

#define kVPNProtocolPayloadInfo			CFSTR("com.apple.payload")
#define kSCEntNetLoginWindowEAPOL		CFSTR("EAPOL.LoginWindow")

static void
copyInterfaceConfiguration(SCNetworkServiceRef oldService, SCNetworkServiceRef newService)
{
	SCNetworkInterfaceRef	oldInterface;
	SCNetworkInterfaceRef	newInterface;

	oldInterface = SCNetworkServiceGetInterface(oldService);
	newInterface = SCNetworkServiceGetInterface(newService);

	while (oldInterface != NULL) {
		CFDictionaryRef	configuration;
		CFStringRef		interfaceType;

		if (newInterface == NULL) {
			// oops ... interface layering does not match
			return;
		}

		// copy interface configuration
		configuration = SCNetworkInterfaceGetConfiguration(oldInterface);

		if ((configuration != NULL) ||
		    (SCError() == kSCStatusOK)) {
			if (!SCNetworkInterfaceSetConfiguration(newInterface, configuration)) {
				SC_log(LOG_INFO, "problem setting interface configuration");
			}

		}

		// special case: PPP/L2TP + IPSec
		interfaceType = SCNetworkInterfaceGetInterfaceType(oldInterface);
		if (CFEqual(interfaceType, kSCNetworkInterfaceTypePPP)) {
			SCNetworkInterfaceRef	childInterface;

			childInterface = SCNetworkInterfaceGetInterface(oldInterface);
			if (childInterface != NULL) {
				CFStringRef		childInterfaceType;

				childInterfaceType = SCNetworkInterfaceGetInterfaceType(childInterface);

				if (CFEqual(childInterfaceType, kSCNetworkInterfaceTypeL2TP)) {
					configuration = SCNetworkInterfaceGetExtendedConfiguration(oldInterface, kSCEntNetIPSec);
					if ((configuration != NULL) ||
					    (SCError() == kSCStatusOK)) {
						if (!SCNetworkInterfaceSetExtendedConfiguration(newInterface, kSCEntNetIPSec, configuration)) {
							SC_log(LOG_INFO, "problem setting child interface configuration");
						}
					}
				}
			}
		}

		// special case: 802.1x
		configuration = SCNetworkInterfaceGetExtendedConfiguration(oldInterface, kSCEntNetEAPOL);
		if ((configuration != NULL) ||
		    (SCError() == kSCStatusOK)) {
			(void) SCNetworkInterfaceSetExtendedConfiguration(newInterface, kSCEntNetEAPOL, configuration);
		}

		// special case: Managed Client
		configuration = SCNetworkInterfaceGetExtendedConfiguration(oldInterface, kVPNProtocolPayloadInfo);
		if ((configuration != NULL) ||
		    (SCError() == kSCStatusOK)) {
			(void) SCNetworkInterfaceSetExtendedConfiguration(newInterface, kVPNProtocolPayloadInfo, configuration);
		}

		// special case: Network Pref
		configuration = SCNetworkInterfaceGetExtendedConfiguration(oldInterface, kSCValNetPPPAuthProtocolEAP);
		if ((configuration != NULL) ||
		    (SCError() == kSCStatusOK)) {
			(void) SCNetworkInterfaceSetExtendedConfiguration(newInterface, kSCValNetPPPAuthProtocolEAP, configuration);
		}

		// special case: Remote Pref
		configuration = SCNetworkInterfaceGetExtendedConfiguration(oldInterface, kSCEntNetLoginWindowEAPOL);
		if ((configuration != NULL) ||
		    (SCError() == kSCStatusOK)) {
			(void) SCNetworkInterfaceSetExtendedConfiguration(newInterface, kSCEntNetLoginWindowEAPOL, configuration);
		}

		// special case: Network Extension
		configuration = SCNetworkInterfaceGetExtendedConfiguration(oldInterface, kSCNetworkInterfaceTypeIPSec);
		if ((configuration != NULL) ||
		    (SCError() == kSCStatusOK)) {
			(void) SCNetworkInterfaceSetExtendedConfiguration(newInterface, kSCNetworkInterfaceTypeIPSec, configuration);
		}

		oldInterface = SCNetworkInterfaceGetInterface(oldInterface);
		newInterface = SCNetworkInterfaceGetInterface(newInterface);
	}

	return;
}

__private_extern__
void
__SCNetworkServiceAddProtocolToService(SCNetworkServiceRef service, CFStringRef protocolType, CFDictionaryRef configuration, Boolean enabled)
{
	Boolean ok;
	SCNetworkProtocolRef protocol;

	protocol = SCNetworkServiceCopyProtocol(service, protocolType);

	if ((protocol == NULL) &&
	    (SCError() == kSCStatusNoKey)) {
		ok = SCNetworkServiceAddProtocolType(service, protocolType);
		if (ok) {
			protocol = SCNetworkServiceCopyProtocol(service, protocolType);
		}
	}
	if (protocol != NULL) {
		SCNetworkProtocolSetConfiguration(protocol, configuration);
		SCNetworkProtocolSetEnabled(protocol, enabled);
		CFRelease(protocol);
	}
	return;
}



__private_extern__
Boolean
__SCNetworkServiceMigrateNew(SCPreferencesRef		prefs,
			     SCNetworkServiceRef	service,
			     CFDictionaryRef		bsdNameMapping,
			     CFDictionaryRef		setMapping,
			     CFDictionaryRef		serviceSetMapping)
{
	CFStringRef			deviceName			= NULL;
	Boolean				enabled;
	SCNetworkInterfaceRef		interface			= NULL;
	CFDictionaryRef			interfaceEntity			= NULL;
	SCNetworkSetRef			newSet				= NULL;
	SCNetworkInterfaceRef		newInterface			= NULL;
	SCNetworkServiceRef		newService			= NULL;
	SCNetworkSetRef			oldSet				= NULL;
	Boolean				serviceAdded			= FALSE;
	CFStringRef			serviceID			= NULL;
	CFStringRef			serviceName;
	SCNetworkServicePrivateRef	servicePrivate			= (SCNetworkServicePrivateRef)service;
	CFArrayRef			setList				= NULL;
	Boolean				success				= FALSE;
	CFStringRef			targetDeviceName		= NULL;
	CFArrayRef			protocols			= NULL;

	if (!isA_SCNetworkService(service) ||
	    !isA_SCNetworkInterface(servicePrivate->interface) ||
	    (servicePrivate->prefs == NULL)) {
		goto done;
	}
	serviceID = servicePrivate->serviceID;

	newService = SCNetworkServiceCopy(prefs, serviceID);
	if (newService != NULL) {
		// Cannot add service if it already exists
		SC_log(LOG_INFO, "Service already exists");
		goto done;
	}

	interface = SCNetworkServiceGetInterface(service);
	if (interface == NULL) {
		SC_log(LOG_INFO, "No interface");
		goto done;
	}

	interfaceEntity = __SCNetworkInterfaceCopyInterfaceEntity(interface);
	if (interfaceEntity == NULL) {
		SC_log(LOG_INFO, "No interface entity");
		goto done;
	}

	if (bsdNameMapping != NULL) {
		deviceName = CFDictionaryGetValue(interfaceEntity, kSCPropNetInterfaceDeviceName);
		if (deviceName != NULL) {
			targetDeviceName = CFDictionaryGetValue(bsdNameMapping, deviceName);
			if (targetDeviceName != NULL) {
				CFStringRef		name;
				CFMutableDictionaryRef	newInterfaceEntity;

				SC_log(LOG_INFO, "  mapping \"%@\" --> \"%@\"", deviceName, targetDeviceName);

				newInterfaceEntity = CFDictionaryCreateMutableCopy(NULL, 0, interfaceEntity);

				// update mapping
				CFDictionarySetValue(newInterfaceEntity, kSCPropNetInterfaceDeviceName, targetDeviceName);

				// update interface name
				name = CFDictionaryGetValue(newInterfaceEntity, kSCPropUserDefinedName);
				if (name != NULL) {
					CFMutableStringRef	newName;

					/*
					 * the interface "entity" can include the a UserDefinedName
					 * like "Ethernet Adapter (en4)".  If this interface name is
					 * mapped to another then we want to ensure that corresponding
					 * UserDefinedName is also updated to match.
					 */
					newName = CFStringCreateMutableCopy(NULL, 0, name);
					CFStringFindAndReplace(newName,
							       deviceName,
							       targetDeviceName,
							       CFRangeMake(0, CFStringGetLength(newName)),
							       0);
					CFDictionarySetValue(newInterfaceEntity, kSCPropUserDefinedName, newName);
					CFRelease(newName);
				}

				CFRelease(interfaceEntity);
				interfaceEntity = newInterfaceEntity;
			}
		}
	}

	newInterface = _SCNetworkInterfaceCreateWithEntity(NULL,
							   interfaceEntity,
							   __kSCNetworkInterfaceSearchExternal);

	if ((setMapping == NULL) ||
	    (serviceSetMapping == NULL) ||
	    !CFDictionaryGetValueIfPresent(serviceSetMapping, service, (const void **)&setList)) {
		// if no mapping
		SC_log(LOG_INFO, "No mapping");
		goto done;
	}

	newService = SCNetworkServiceCreate(prefs, newInterface);
	if (newService == NULL) {
		SC_log(LOG_INFO, "SCNetworkServiceCreate() failed");
		goto done;
	}

	enabled = SCNetworkServiceGetEnabled(service);
	if (!SCNetworkServiceSetEnabled(newService, enabled)) {
		SCNetworkServiceRemove(newService);
		SC_log(LOG_INFO, "SCNetworkServiceSetEnabled() failed");
		goto done;
	}

	if (!SCNetworkServiceEstablishDefaultConfiguration(newService)) {
		SCNetworkServiceRemove(newService);
		SC_log(LOG_INFO, "SCNetworkServiceEstablishDefaultConfiguration() failed");
		goto done;
	}

	// Set service ID
	_SCNetworkServiceSetServiceID(newService, serviceID);

	// Determine which sets to add service
	for (CFIndex idx = 0; idx < CFArrayGetCount(setList); idx++) {
		oldSet = CFArrayGetValueAtIndex(setList, idx);
		newSet = CFDictionaryGetValue(setMapping, oldSet);
		if (newSet == NULL) {
			continue;
		}

		SC_log(LOG_INFO, "  adding service to set: %@", SCNetworkSetGetSetID(newSet));
		if (SCNetworkSetAddService(newSet, newService)) {
			serviceAdded = TRUE;
		} else {
			SC_log(LOG_INFO, "SCNetworkSetAddService() failed");
		}
	}

	if (!serviceAdded) {
		SCNetworkServiceRemove(newService);
		SC_log(LOG_INFO, "  service not added to any sets");
		goto done;
	}

	// Set (non-default) service name
	serviceName = __SCNetworkServiceGetName(service, FALSE);
	if ((serviceName != NULL) &&
	    !SCNetworkServiceSetName(newService, serviceName)) {
		SC_log(LOG_INFO, "SCNetworkServiceSetName() failed");
	}

	protocols = SCNetworkServiceCopyProtocols(service);
	if (protocols != NULL) {

		for (CFIndex idx = 0; idx < CFArrayGetCount(protocols); idx++) {
			SCNetworkProtocolRef protocol = CFArrayGetValueAtIndex(protocols, idx);
			CFDictionaryRef configuration = SCNetworkProtocolGetConfiguration(protocol);
			CFStringRef protocolType = SCNetworkProtocolGetProtocolType(protocol);
			enabled = SCNetworkProtocolGetEnabled(protocol);
			__SCNetworkServiceAddProtocolToService(newService, protocolType, configuration, enabled);
		}
		CFRelease(protocols);
	}

	copyInterfaceConfiguration(service, newService);

	success = TRUE;

    done:

	if (interfaceEntity != NULL) {
		CFRelease(interfaceEntity);
	}
	if (newInterface != NULL) {
		CFRelease(newInterface);
	}
	if (newService != NULL) {
		CFRelease(newService);
	}
	return success;
}


__private_extern__
Boolean
__SCNetworkServiceCreate(SCPreferencesRef	prefs,
			 SCNetworkInterfaceRef	interface,
			 CFStringRef		userDefinedName)
{
	SCNetworkSetRef currentSet = NULL;
	Boolean ok = FALSE;
	SCNetworkServiceRef service = NULL;

	if (interface == NULL) {
		goto done;
	}

	if (userDefinedName == NULL) {
		userDefinedName = __SCNetworkInterfaceGetUserDefinedName(interface);
		if (userDefinedName == NULL) {
			SC_log(LOG_INFO, "No userDefinedName");
			goto done;
		}
	}
	service = SCNetworkServiceCreate(prefs, interface);
	if (service == NULL) {
		SC_log(LOG_INFO, "SCNetworkServiceCreate() failed: %s", SCErrorString(SCError()));
	} else {
		ok = SCNetworkServiceSetName(service, userDefinedName);
		if (!ok) {
			SC_log(LOG_INFO, "SCNetworkServiceSetName() failed: %s", SCErrorString(SCError()));
			SCNetworkServiceRemove(service);
			goto done;
		}

		ok = SCNetworkServiceEstablishDefaultConfiguration(service);
		if (!ok) {
			SC_log(LOG_INFO, "SCNetworkServiceEstablishDefaultConfiguration() failed: %s", SCErrorString(SCError()));
			SCNetworkServiceRemove(service);
			goto done;
		}
	}
	currentSet = SCNetworkSetCopyCurrent(prefs);
	if (currentSet == NULL) {
		SC_log(LOG_INFO, "No current set");
		if (service != NULL) {
			SCNetworkServiceRemove(service);
		}
		goto done;
	}
	if (service != NULL) {
		ok = SCNetworkSetAddService(currentSet, service);
		if (!ok) {
			SC_log(LOG_INFO, "Could not add service to the current set");
			SCNetworkServiceRemove(service);
			goto done;
		}
	}

    done:
	if (service != NULL) {
		CFRelease(service);
	}
	if (currentSet != NULL) {
		CFRelease(currentSet);
	}
	return ok;
}

__private_extern__ Boolean
__SCNetworkServiceIsPPTP(SCNetworkServiceRef	service)
{
	CFStringRef intfSubtype;
	SCNetworkServicePrivateRef servicePrivate = (SCNetworkServicePrivateRef)service;

	if (servicePrivate == NULL || servicePrivate->interface == NULL) {
		return FALSE;
	}

	intfSubtype = __SCNetworkInterfaceGetEntitySubType(servicePrivate->interface);
	if (intfSubtype == NULL) {
		return FALSE;
	}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated"
	if (CFEqual(intfSubtype, kSCValNetInterfaceSubTypePPTP)) {
		return TRUE;
	}
#pragma GCC diagnostic pop

	return FALSE;
}
