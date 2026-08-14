/*
 * Copyright (c) 2011-2018 Apple Inc. All rights reserved.
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
 * IPConfigurationService.h
 * - API to communicate with IPConfiguration to instantiate services
 */

/* 
 * Modification History
 *
 * April 14, 2011 	Dieter Siegmund (dieter@apple.com)
 * - initial revision
 */

#ifndef _IPCONFIGURATIONSERVICE_H
#define _IPCONFIGURATIONSERVICE_H

#include <CoreFoundation/CFDictionary.h>
#include <CoreFoundation/CFString.h>

typedef struct __IPConfigurationService * IPConfigurationServiceRef;

CFTypeID
IPConfigurationServiceGetTypeID(void);

/**
 ** IPConfigurationService Options
 **/
/*
 * kIPConfigurationServiceOptionMTU (CFNumberRef)
 * - specify the MTU to set on the interface when the service is created
 */
extern const CFStringRef	kIPConfigurationServiceOptionMTU; /* number */

/*
 * kIPConfigurationServiceOptionPerformNUD (CFBooleanRef, default TRUE)
 * - specify whether to perform Neighbor Unreachability Detection
 */
extern const CFStringRef	kIPConfigurationServiceOptionPerformNUD; /* boolean */

/*
 * kIPConfigurationServiceOptionIPv6Entity (CFDictionaryRef)
 */
extern const CFStringRef	kIPConfigurationServiceOptionIPv6Entity; /* dictionary */

/*
 * kIPConfigurationServiceOptionIPv6LinkLocalAddress (CFStringRef)
 * - use the specified IPv6 link local address when configuring IPv6
 */
extern const CFStringRef	kIPConfigurationServiceOptionIPv6LinkLocalAddress; /* string */

/*
 * kIPConfigurationServiceOptionEnableDAD (CFBooleanRef, default TRUE)
 * - specify whether to do DAD (Duplicate Address Detection)
 */
extern const CFStringRef	kIPConfigurationServiceOptionEnableDAD; /* boolean */

/*
 * kIPConfigurationServiceOptionAPNName (CFStringRef)
 * - specify the APN name
 */
extern const CFStringRef	kIPConfigurationServiceOptionAPNName; /* string */

/*
 * kIPConfigurationServiceOptionEnableCLAT46 (CFBooleanRef, default FALSE)
 * - specify whether to enable CLAT46 translation or not
 */
extern const CFStringRef	kIPConfigurationServiceOptionEnableCLAT46; /* boolean */

/*
 * Function: IPConfigurationServiceCreate
 *
 * Purpose:
 *   Instantiate a new service over the specified interface that is managed
 *   and maintained by the IPConfiguration server.
 *
 *   Supports creating an IPv6 Manual or Automatic service, depending on 
 *   whether or not kIPConfigurationServiceOptionIPv6Entity is specified in the
 *   'options' dictionary. If the property is missing, an Automatic service
 *   is created.  If kIPConfigurationServiceOptionIPv6Entity is specified, 
 *   it must represent a valid Manual or Automatic service, using the IPv6
 *   schema defined in <SystemConfiguration/SCSchemaDefinitions.h>.
 *
 *   The resulting service that gets instantiated is ineligible to become
 *   primary. The caller is responsible for publishing
 *   the service information to the SCDynamic store when it receives
 *   notifications that the service information is available.
 *
 *   Use IPConfigurationServiceCopyInformation() to copy the current
 *   service information.
 *
 *   Use IPConfigurationServiceGetNotificationKey() to get the SCDynamicStore
 *   key that is notified when the service information changes.
 *
 * Parameters:
 *   interface_name		: the BSD name of the interface e.g. "pdp_ip0"
 *   options			: either NULL, or a dictionary that only
 *				  contains properties with keys specified
 * 				  under "IPConfigurationService Options" above
 * Returns:
 *   Non-NULL IPConfigurationServiceRef if the service was successfully
 *   instantiated, NULL otherwise
 *
 * Note:
 * - When the last reference to the IPConfigurationServiceRef is removed by
 *   calling CFRelease(), the service is terminated by IPConfiguration.
 * - Invoking this function multiple times with the same parameters will
 *   cause an existing service to first be deallocated.  The net result
 *   is that there will only ever be one active service of the specified
 *   type for the specified interface.
 * - If the process that invokes this function terminates, the 
 *   service will be terminated by IPConfiguration.
 */
IPConfigurationServiceRef
IPConfigurationServiceCreate(CFStringRef interface_name, 
			     CFDictionaryRef options);

/*
 * Function: IPConfigurationServiceGetNotificationKey
 *
 * Purpose:
 *   Return the SCDynamicStoreKeyRef used to monitor the specified service using
 *   SCDynamicStoreSetNotificationKeys().
 *
 * Parameters:
 *   service			: the service to monitor
 */
CFStringRef
IPConfigurationServiceGetNotificationKey(IPConfigurationServiceRef service);

/*
 * Function: IPConfigurationServiceCopyInformation
 *
 * Purpose:
 *   Retrieves the service information for the specified 'service'.  The
 *   format of the returned information is a dictionary of dictionaries.
 *   The key of each sub-dictionary is a kSCEntNet* key as defined in
 *   <SystemConfiguration/SCSchemaDefinitions.h>.  The value of each dictionary
 *   is a dictionary of keys matching the schema for the particular kSCEntNet*
 *   key.
 *
 * Parameters:
 *   service			: the service to monitor
 *
 * Returns:
 *   NULL if no information is ready for consumption, non-NULL dictionary of
 *   service information otherwise.
 * 
 * Example of returned information:
 * <dict>
 *     <key>IPv6</key>
 *     <dict>
 *         <key>Addresses</key>
 *         <array>
 *	       <string>2001:470:1f05:3cb:cabc:c8ff:fed9:125a</string>
 *             <string>2001:470:1f05:3cb:415c:9de:9cc4:7d12</string>
 *         <array>
 *         <key>InterfaceName</key>
 *         <string>pdp_ip0</string>
 *         <key>PrefixLength</key>
 *         <array>
 *             <integer>64</integer>
 *             <integer>64</integer>
 *         </array>
 *         <key>Router</key>
 *         <string>fe80::21f:f3ff:fe43:1abf</string>
 *     </dict>
 *     <key>DNS</key>
 *     <dict>
 *         <key>ServerAddresses</key>
 *         <array>
 *	       <string>2001:470:1f05:3cb::1</string>
 *         </array>
 *     </dict>
 * </dict>
 */
CFDictionaryRef
IPConfigurationServiceCopyInformation(IPConfigurationServiceRef service);

/*
 * Function: IPConfigurationServiceRefreshConfiguration
 * Purpose:
 *   Force a configuration refresh for the specified 'service'. Analogous
 *   to SCNetworkInterfaceForceConfigurationRefresh().
 *
 * Parameters:
 *   service			: the service to refresh configuration
 */
void
IPConfigurationServiceRefreshConfiguration(IPConfigurationServiceRef service);

#endif /* _IPCONFIGURATIONSERVICE_H */
