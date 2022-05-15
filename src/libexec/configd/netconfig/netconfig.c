/*
 * Copyright (c) 2021 Apple Inc. All rights reserved.
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
 * November 11, 2020		Dieter Siegmund <dieter@apple.com>
 * - initial revision
 */

/*
 * netconfig
 * - a command-line tool to view and set network configuration
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/errno.h>
#include <sysexits.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPrivate.h>

static void command_specific_help(void) __dead2;

#define countof(array)	(sizeof(array) / sizeof(array[0]))

#define kOptAddress		"address"
#define OPT_ADDRESS		'A'

#define kOptConfigMethod	"config-method"
#define OPT_CONFIG_METHOD	'c'

#define kOptDHCPClientID	"dhcp-client-id"
#define OPT_DHCP_CLIENT_ID	'C'

#define kOptDNSDomainName	"dns-domain-name"
#define OPT_DNS_DOMAIN_NAME	'n'

#define kOptDNSSearchDomains	"dns-search-domains"
#define OPT_DNS_SEARCH_DOMAINS	's'

#define kOptDefaultConfig	"default-config"
#define OPT_DEFAULT_CONFIG	'D'

#define kOptFile		"file"
#define OPT_FILE		'f'

#define kOptHelp		"help"
#define OPT_HELP		'h'

#define kOptInterface		"interface"
#define OPT_INTERFACE		'i'

#define kOptInterfaceType	"interface-type"
#define OPT_INTERFACE_TYPE	't'

#define kOptRouter		"router"
#define OPT_ROUTER		'r'

#define kOptSubnetMask		"subnet-mask"
#define OPT_SUBNET_MASK		'm'

#define kOptProtocol		"protocol"
#define OPT_PROTOCOL		'p'

#define kOptName		"name"
#define OPT_NAME		'N'

#define kOptService		"service"
#define OPT_SERVICE		'S'

#define kOptSet			"set"
#define OPT_SET			'e'

#define kOptVerbose		"verbose"
#define OPT_VERBOSE		'v'

#define kOptVLANID		"vlan-id"
#define OPT_VLAN_ID		'I'

#define kOptVLANDevice		"vlan-device"
#define OPT_VLAN_DEVICE		'P'

static struct option longopts[] = {
	{ kOptAddress,		required_argument, NULL, OPT_ADDRESS },
	{ kOptConfigMethod,	required_argument, NULL, OPT_CONFIG_METHOD },
	{ kOptDHCPClientID, 	required_argument, NULL, OPT_DHCP_CLIENT_ID },
	{ kOptDNSDomainName,	required_argument, NULL, OPT_DNS_DOMAIN_NAME },
	{ kOptDNSSearchDomains,	required_argument, NULL, OPT_DNS_SEARCH_DOMAINS},
	{ kOptDefaultConfig,	no_argument,       NULL, OPT_DEFAULT_CONFIG },
	{ kOptFile,		required_argument, NULL, OPT_FILE },
	{ kOptInterface,	required_argument, NULL, OPT_INTERFACE },
	{ kOptHelp,		no_argument, 	   NULL, OPT_HELP },
	{ kOptInterfaceType,	required_argument, NULL, OPT_INTERFACE_TYPE },
	{ kOptRouter,		required_argument, NULL, OPT_ROUTER },
	{ kOptSubnetMask,	required_argument, NULL, OPT_SUBNET_MASK },
	{ kOptProtocol,		required_argument, NULL, OPT_PROTOCOL },
	{ kOptService,		required_argument, NULL, OPT_SERVICE },
	{ kOptSet,		required_argument, NULL, OPT_SET },
	{ kOptName,		required_argument, NULL, OPT_NAME },
	{ kOptVerbose,		no_argument, 	   NULL, OPT_VERBOSE },
	{ kOptVLANID,		required_argument, NULL, OPT_VLAN_ID },
	{ kOptVLANDevice,	required_argument, NULL, OPT_VLAN_DEVICE },
	{ NULL,			0,		   NULL, 0 }
};

static const char * 	G_argv0;

static CFMutableArrayRef
array_create(void)
{
	return (CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks));
}

static CFMutableDictionaryRef
dict_create(void)
{
	return (CFDictionaryCreateMutable(NULL, 0,
					  &kCFTypeDictionaryKeyCallBacks,
					  &kCFTypeDictionaryValueCallBacks));
}

static void
dict_set_val(CFMutableDictionaryRef dict, CFStringRef key, CFTypeRef val)
{
	if (val == NULL) {
		return;
	}
	CFDictionarySetValue(dict, key, val);
}

static void
my_CFRelease(void * t)
{
	void * * obj = (void * *)t;
	if (obj && *obj) {
		CFRelease(*obj);
		*obj = NULL;
	}
	return;
}

static CFStringRef
my_CFStringCreate(const char * str)
{
	return CFStringCreateWithCString(NULL, str, kCFStringEncodingUTF8);
}

static CFStringRef
my_CFStringCreateWithIPAddress(uint8_t af, const void * addr)
{
	char 		ntopbuf[INET6_ADDRSTRLEN];
	const char *	c_str;

	c_str = inet_ntop(af, addr, ntopbuf, sizeof(ntopbuf));
	return (CFStringCreateWithCString(NULL, c_str, kCFStringEncodingUTF8));
}

static void
my_CFDictionarySetCString(CFMutableDictionaryRef dict,
			  CFStringRef prop, const char * str)
{
	CFStringRef	cfstr;

	cfstr = CFStringCreateWithCString(NULL, str, kCFStringEncodingUTF8);
	CFDictionarySetValue(dict, prop, cfstr);
	CFRelease(cfstr);
	return;
}

static void
my_CFDictionarySetTypeAsArrayValue(CFMutableDictionaryRef dict,
				   CFStringRef prop, CFTypeRef val)
{
	CFArrayRef	array;

	array = CFArrayCreate(NULL, (const void **)&val, 1,
			      &kCFTypeArrayCallBacks);
	if (array != NULL) {
		CFDictionarySetValue(dict, prop, array);
		CFRelease(array);
	}
	return;
}

static void
my_CFDictionarySetIPAddressAsArrayValue(CFMutableDictionaryRef dict,
					CFStringRef prop,
					uint8_t af,
					const void * addr)
{
	CFStringRef		str;

	str = my_CFStringCreateWithIPAddress(af, addr);
	my_CFDictionarySetTypeAsArrayValue(dict, prop, str);
	CFRelease(str);
	return;
}

static void
my_CFDictionarySetNumberAsArrayValue(CFMutableDictionaryRef dict,
				     CFStringRef prop,
				     int val)
{
	CFNumberRef		num;

	num = CFNumberCreate(NULL, kCFNumberIntType, &val);
	my_CFDictionarySetTypeAsArrayValue(dict, prop, num);
	CFRelease(num);
	return;
}

static void
my_CFDictionarySetIPAddress(CFMutableDictionaryRef dict,
			    CFStringRef prop,
			    uint8_t af,
			    const void * addr)
{
	CFStringRef		str;

	str = my_CFStringCreateWithIPAddress(af, addr);
	CFDictionarySetValue(dict, prop, str);
	CFRelease(str);
	return;
}

static Boolean
file_exists(const char * filename)
{
	struct stat	b;

	if (stat(filename, &b) != 0) {
		fprintf(stderr, "stat(%s) failed, %s\n",
			filename, strerror(errno));
		return (FALSE);
	}
	if ((b.st_mode & S_IFREG) == 0) {
		fprintf(stderr, "%s: not a file\n", filename);
		return (FALSE);
	}
	return (TRUE);
}

/*
 * StringMap
 * - map between C string and CFString
 */
typedef struct {
	const char *		string;
	const CFStringRef *	cfstring;
} StringMap, *StringMapRef;

static const char *
StringMapGetString(StringMapRef map, unsigned int count,
		   unsigned int index)
{
	if (index >= count) {
		return (NULL);
	}
	return (map[index].string);
}

static CFStringRef
StringMapGetCFString(StringMapRef map, unsigned int count,
		     unsigned int index)
{
	if (index >= count) {
		return (NULL);
	}
	return (*(map[index].cfstring));
}

static int
StringMapGetIndexOfString(StringMapRef map, unsigned int count,
			  const char * str)
{
	int	where = -1;

	for (unsigned int i = 0; i < count; i++) {
		if (strcasecmp(str, map[i].string) == 0) {
			where = (int)i;
			break;
		}
	}
	return (where);
}

static int
StringMapGetIndexOfCFString(StringMapRef map, unsigned int count,
			    CFStringRef str)
{
	int	where = -1;

	for (unsigned int i = 0; i < count; i++) {
		if (CFEqual(str, *map[i].cfstring)) {
			where = (int)i;
			break;
		}
	}
	return (where);
}

#define STRINGMAP_GET_INDEX_OF_STRING(map, str)			\
	StringMapGetIndexOfString(map, countof(map), str)

#define STRINGMAP_GET_INDEX_OF_CFSTRING(map, str)			\
	StringMapGetIndexOfCFString(map, countof(map), str)

#define STRINGMAP_GET_STRING(map, index) \
	StringMapGetString(map, countof(map), index)

#define STRINGMAP_GET_CFSTRING(map, index) \
	StringMapGetCFString(map, countof(map), index)

static const char *
string_array_get(const char * * strlist, unsigned int count,
		 unsigned int index)
{
	if (index >= count) {
		return (NULL);
	}
	return (strlist[index]);
}

static int
string_array_get_index_of_value(const char * * strlist, unsigned int count,
				const char * str)
{
	int	where = -1;

	for (unsigned int i = 0; i < count; i++) {
		if (strcasecmp(str, strlist[i]) == 0) {
			where = (int)i;
			break;
		}
	}
	return (where);
}

#define STRING_ARRAY_GET_INDEX_OF_VALUE(array, str) \
	string_array_get_index_of_value(array, countof(array), str)

#define STRING_ARRAY_GET(array, index)			\
	string_array_get(array, countof(array), index)

/*
 * Commands
 */
typedef enum {
	kCommandNone		= 0,
	kCommandAdd		= 1,	/* add a service */
	kCommandSet		= 2,	/* set service configuration */
	kCommandRemove		= 3,	/* remove service/protocol */
	kCommandEnable		= 4,	/* enable service/protocol */
	kCommandDisable		= 5,	/* disable service/protocol */
	kCommandShow		= 6,	/* show information */
	kCommandCreate		= 7,	/* create an interface */
	kCommandDestroy 	= 8,	/* destroy an interface */
	kCommandSetVLAN		= 9,	/* set VLAN params */
} Command;

static Command 		G_command;

static const char * command_strings[] = {
	"add",
	"set",
	"remove",
	"enable",
	"disable",
	"show",
	"create",
	"destroy",
	"setvlan",
};

static const char *
CommandGetString(Command command)
{
	return STRING_ARRAY_GET(command_strings, command - 1);
}

static Command
CommandFromString(const char * str)
{
	Command	command;

	command = (Command)
		(STRING_ARRAY_GET_INDEX_OF_VALUE(command_strings, str) + 1);
	return (command);
}

/*
 * Protocols
 */
typedef
enum {
	kProtocolNone			= 0,
	kProtocolIPv4			= 1,
	kProtocolIPv6			= 2,
	kProtocolDNS			= 3,
} Protocol;

static StringMap  protocol_strings[] = {
	{ "ipv4", &kSCNetworkProtocolTypeIPv4 },
	{ "ipv6", &kSCNetworkProtocolTypeIPv6 },
	{ "dns", &kSCNetworkProtocolTypeDNS },
};

static const char *
ProtocolGetString(Protocol protocol)
{
	return STRINGMAP_GET_STRING(protocol_strings, protocol - 1);
}

static Protocol
ProtocolFromString(const char * str)
{
	Protocol	proto;

	proto = (Protocol)(STRINGMAP_GET_INDEX_OF_STRING(protocol_strings,
							 str) + 1);
	return (proto);
}

static CFStringRef
ProtocolGetCFString(Protocol protocol)
{
	return STRINGMAP_GET_CFSTRING(protocol_strings, protocol - 1);
}

/*
 * ConfigMethod map
 */

typedef
enum {
	kIPv4ConfigMethodNone		= 0,
	kIPv4ConfigMethodDHCP		= 1,
	kIPv4ConfigMethodInform		= 2,
	kIPv4ConfigMethodManual		= 3,
	kIPv4ConfigMethodLinkLocal	= 4,
} IPv4ConfigMethod;

static StringMap ipv4_config_method_map[] = {
	{ "dhcp", &kSCValNetIPv4ConfigMethodDHCP },
	{ "inform", &kSCValNetIPv4ConfigMethodINFORM },
	{ "manual", &kSCValNetIPv4ConfigMethodManual },
	{ "linklocal", &kSCValNetIPv4ConfigMethodLinkLocal },
};

typedef
enum {
	kIPv6ConfigMethodNone		= 0,
	kIPv6ConfigMethodAutomatic	= 1,
	kIPv6ConfigMethodManual		= 2,
	kIPv6ConfigMethodLinkLocal	= 3,
} IPv6ConfigMethod;

static StringMap ipv6_config_method_map[] = {
	{ "automatic", 	&kSCValNetIPv6ConfigMethodAutomatic },
	{ "manual", &kSCValNetIPv6ConfigMethodManual },
	{ "linklocal", &kSCValNetIPv6ConfigMethodLinkLocal },
};

static IPv4ConfigMethod
IPv4ConfigMethodFromString(const char * config_method)
{
	IPv4ConfigMethod	m;

	m = (IPv4ConfigMethod)
		(STRINGMAP_GET_INDEX_OF_STRING(ipv4_config_method_map,
					       config_method) + 1);
	return (m);
}

static IPv4ConfigMethod
IPv4ConfigMethodFromCFString(CFStringRef config_method)
{
	IPv4ConfigMethod	m;

	m = (IPv4ConfigMethod)
		(STRINGMAP_GET_INDEX_OF_CFSTRING(ipv4_config_method_map,
						config_method) + 1);
	return (m);
}

static CFStringRef
IPv4ConfigMethodGetCFString(IPv4ConfigMethod method)
{
	return STRINGMAP_GET_CFSTRING(ipv4_config_method_map, method - 1);
}

static const char *
IPv4ConfigMethodGetString(IPv4ConfigMethod method)
{
	return STRINGMAP_GET_STRING(ipv4_config_method_map, method - 1);
}

static CFStringRef
IPv6ConfigMethodGetCFString(IPv6ConfigMethod method)
{
	return STRINGMAP_GET_CFSTRING(ipv6_config_method_map, method - 1);
}

static const char *
IPv6ConfigMethodGetString(IPv6ConfigMethod method)
{
	return STRINGMAP_GET_STRING(ipv6_config_method_map, method - 1);
}

static IPv6ConfigMethod
IPv6ConfigMethodFromString(const char * config_method)
{
	IPv6ConfigMethod	m;

	m = (IPv6ConfigMethod)
		(STRINGMAP_GET_INDEX_OF_STRING(ipv6_config_method_map,
					       config_method) + 1);
	return (m);
}

static IPv6ConfigMethod
IPv6ConfigMethodFromCFString(CFStringRef config_method)
{
	IPv6ConfigMethod	m;

	m = (IPv6ConfigMethod)
		(STRINGMAP_GET_INDEX_OF_CFSTRING(ipv6_config_method_map,
						config_method) + 1);
	return (m);
}

/*
 * SCNetworkConfiguration utility functions
 */
static SCPreferencesRef
prefs_create_with_file(CFStringRef file)
{
	SCPreferencesRef prefs;

	prefs = SCPreferencesCreate(NULL, CFSTR("netconfig"), file);
	if (prefs == NULL) {
		SCPrint(TRUE, stderr,
			CFSTR("SCPreferencesCreate(%@) failed, %s\n"),
			file, SCErrorString(SCError()));
		exit(EX_SOFTWARE);
	}
	return (prefs);
}

static SCPreferencesRef
prefs_create(void)
{
	return (prefs_create_with_file(NULL));
}

static CFStringRef
interface_copy_summary(SCNetworkInterfaceRef netif)
{
	CFStringRef		bsd_name;
	SCNetworkInterfaceRef	child;
	CFStringRef		name;
	CFMutableStringRef	str;
	CFStringRef		type;

	str = CFStringCreateMutable(NULL, 0);
	type = SCNetworkInterfaceGetInterfaceType(netif);
	if (type != NULL) {
		CFStringAppendFormat(str, NULL, CFSTR("%@"), type);
	}
	bsd_name = SCNetworkInterfaceGetBSDName(netif);
	if (bsd_name != NULL) {
		CFStringAppendFormat(str, NULL, CFSTR(" (%@)"), bsd_name);
		name = SCNetworkInterfaceGetLocalizedDisplayName(netif);
		if (name != NULL) {
			CFStringAppendFormat(str, NULL, CFSTR(" %@"), name);
		}
	}
	child = SCNetworkInterfaceGetInterface(netif);
	if (child != NULL) {
		bsd_name = SCNetworkInterfaceGetBSDName(child);
		if (bsd_name != NULL) {
			CFStringAppendFormat(str, NULL, CFSTR(" (%@)"),
					     bsd_name);
		}
		name = SCNetworkInterfaceGetLocalizedDisplayName(child);
		if (name != NULL) {
			CFStringAppendFormat(str, NULL, CFSTR(" %@"), name);
		}
	}
	return (str);
}

static Boolean
append_first_value(CFMutableStringRef str, CFDictionaryRef dict,
		   CFStringRef key)
{
	CFArrayRef	list = CFDictionaryGetValue(dict, key);
	CFTypeRef	val = NULL;

	if (isA_CFArray(list) != NULL && CFArrayGetCount(list) != 0) {
		val = CFArrayGetValueAtIndex(list, 0);
	}
	if (val == NULL) {
		return (FALSE);
	}
	CFStringAppendFormat(str, NULL, CFSTR("%@"), val);
	return (TRUE);
}

static void
append_ipv4_descr(CFMutableStringRef str, CFDictionaryRef dict)
{
	CFStringRef		client_id;
	IPv4ConfigMethod	method;
	CFStringRef		method_str;
	CFStringRef		router;

	method_str = CFDictionaryGetValue(dict, kSCPropNetIPv4ConfigMethod);
	if (method_str == NULL) {
		return;
	}
	CFStringAppendFormat(str, NULL, CFSTR(" %@ "), method_str);
	method = IPv4ConfigMethodFromCFString(method_str);
	switch (method) {
	case kIPv4ConfigMethodDHCP:
		/* DHCP client id */
		client_id = CFDictionaryGetValue(dict,
						 kSCPropNetIPv4DHCPClientID);
		if (client_id != NULL) {
			CFStringAppendFormat(str, NULL, CFSTR("clientID=%@ "),
					     client_id);
		}
		break;
	case kIPv4ConfigMethodInform:
	case kIPv4ConfigMethodManual:
		/* address, subnet mask, router */
		if (append_first_value(str, dict, kSCPropNetIPv4Addresses)) {
			if (CFDictionaryContainsKey(dict,
						    kSCPropNetIPv4SubnetMasks)) {
				CFStringAppend(str, CFSTR("/"));
				append_first_value(str, dict,
						   kSCPropNetIPv4SubnetMasks);
			}
		}
		router = CFDictionaryGetValue(dict, kSCPropNetIPv4Router);
		if (router != NULL) {
			CFStringAppendFormat(str, NULL,
					     CFSTR(" router=%@"), router);
		}
		CFStringAppend(str, CFSTR(" "));
		break;
	case kIPv4ConfigMethodLinkLocal:
	case kIPv4ConfigMethodNone:
		break;
	}
	return;
}

static void
append_ipv6_descr(CFMutableStringRef str, CFDictionaryRef dict)
{
	IPv6ConfigMethod	method;
	CFStringRef		method_str;
	CFStringRef		router;

	method_str = CFDictionaryGetValue(dict, kSCPropNetIPv6ConfigMethod);
	if (method_str == NULL) {
		return;
	}
	CFStringAppendFormat(str, NULL, CFSTR(" %@ "), method_str);
	method = IPv6ConfigMethodFromCFString(method_str);
	switch (method) {
	case kIPv6ConfigMethodManual:
		/* address, subnet mask, router */
		if (append_first_value(str, dict, kSCPropNetIPv6Addresses)) {
			if (CFDictionaryContainsKey(dict,
						    kSCPropNetIPv6PrefixLength)) {
				CFStringAppend(str, CFSTR("/"));
				append_first_value(str, dict,
						   kSCPropNetIPv6PrefixLength);
			}
		}
		router = CFDictionaryGetValue(dict, kSCPropNetIPv6Router);
		if (router != NULL) {
			CFStringAppendFormat(str, NULL,
					     CFSTR(" router=%@"), router);
		}
		CFStringAppend(str, CFSTR(" "));
		break;
	case kIPv6ConfigMethodAutomatic:
	case kIPv6ConfigMethodLinkLocal:
	case kIPv6ConfigMethodNone:
		break;
	}
	return;
}

static Boolean
append_array_values(CFMutableStringRef str, CFDictionaryRef dict,
		    CFStringRef key)
{
	CFArrayRef list;	

	list = CFDictionaryGetValue(dict, key);
	if (isA_CFArray(list) == NULL || CFArrayGetCount(list) == 0) {
		return (FALSE);
	}
	for (CFIndex i = 0, count = CFArrayGetCount(list); i < count; i++) {
		CFTypeRef	val = CFArrayGetValueAtIndex(list, i);

		CFStringAppendFormat(str, NULL, CFSTR("%s%@"),
				    i == 0 ? "" : ",",
				    val);
	}
	return (TRUE);
}

static void
append_dns_descr(CFMutableStringRef str, CFDictionaryRef dict)
{
	CFStringRef	domain;

	if (CFDictionaryContainsKey(dict, kSCPropNetDNSServerAddresses)) {
		CFStringAppend(str, CFSTR(" "));
	}
	if (!append_array_values(str, dict, kSCPropNetDNSServerAddresses)) {
		return;
	}
	domain = CFDictionaryGetValue(dict, kSCPropNetDNSDomainName);
	if (domain != NULL) {
		CFStringAppendFormat(str, NULL, CFSTR(" domain=%@"), domain);
	}
	if (CFDictionaryContainsKey(dict, kSCPropNetDNSSearchDomains)) {
		CFStringAppend(str, CFSTR(" search="));
	}
	append_array_values(str, dict, kSCPropNetDNSSearchDomains);
	CFStringAppend(str, CFSTR(" "));
}

static CFStringRef
service_copy_protocol_summary(SCNetworkServiceRef service)
{
	unsigned int		i;
	CFMutableStringRef	str = NULL;
	StringMapRef		map = protocol_strings;

	for (i = 0, map = protocol_strings;
	     i < countof(protocol_strings); i++, map++) {
		CFDictionaryRef		config;
		SCNetworkProtocolRef	p;
		Protocol		proto = i + 1;
		CFStringRef		type = *map->cfstring;

		p = SCNetworkServiceCopyProtocol(service, type);
		if (p == NULL) {
			continue;
		}
		config = SCNetworkProtocolGetConfiguration(p);
		if (config != NULL) {
			if (str == NULL) {
				str = CFStringCreateMutable(NULL, 0);
				CFStringAppendFormat(str, NULL,
						     CFSTR("%@={"),
						     type);
			}
			else {
				CFStringAppendFormat(str, NULL, CFSTR(" %@={"),
						     type);
			}
			if (!SCNetworkProtocolGetEnabled(p)) {
				CFStringAppendFormat(str, NULL,
						     CFSTR(" [DISABLED]"));
			}
			switch (proto) {
			case kProtocolIPv4:
				append_ipv4_descr(str, config);
				break;
			case kProtocolIPv6:
				append_ipv6_descr(str, config);
				break;
			case kProtocolDNS:
				append_dns_descr(str, config);
				break;
			case kProtocolNone:
				break;
			}
			CFStringAppend(str, CFSTR("}"));
		}
		CFRelease(p);
	}
	return (str);
}

static CFStringRef
service_copy_summary(SCNetworkServiceRef service)
{
	CFStringRef		if_summary;
	CFStringRef		name;
	SCNetworkInterfaceRef	netif;
	CFStringRef		proto_summary;
	CFStringRef		serviceID;
	CFMutableStringRef	str;

	str = CFStringCreateMutable(NULL, 0);
	name = SCNetworkServiceGetName(service);	
	CFStringAppend(str, name);
	serviceID = SCNetworkServiceGetServiceID(service);
	CFStringAppendFormat(str, NULL, CFSTR(" (%@)"), serviceID);
	if (!SCNetworkServiceGetEnabled(service)) {
		CFStringAppend(str, CFSTR(" [DISABLED]"));
	}
	netif = SCNetworkServiceGetInterface(service);
	if_summary = interface_copy_summary(netif);
	if (if_summary != NULL) {
		CFStringAppendFormat(str, NULL, CFSTR(" %@"), if_summary);
		CFRelease(if_summary);
	}
	proto_summary = service_copy_protocol_summary(service);
	if (proto_summary != NULL) {
		CFStringAppendFormat(str, NULL, CFSTR("\n\t%@"), proto_summary);
		CFRelease(proto_summary);
	}
	return (str);
}

static SCNetworkProtocolRef
service_copy_protocol(SCNetworkServiceRef service, CFStringRef type)
{
	SCNetworkProtocolRef	protocol;

	protocol = SCNetworkServiceCopyProtocol(service, type);
	if (protocol == NULL) {
		if (!SCNetworkServiceAddProtocolType(service, type)) {
			SCPrint(TRUE, stderr,
				CFSTR("Failed to add %@\n"),
				type);
			return (NULL);
		}
		protocol = SCNetworkServiceCopyProtocol(service, type);
		if (protocol == NULL) {
			SCPrint(TRUE, stderr,
				CFSTR("can't copy protocol %@\n"),
				type);
			return (NULL);
		}
	}
	return (protocol);
}

static Boolean
service_remove_protocol(SCNetworkServiceRef service, CFStringRef type)
{
	return (SCNetworkServiceRemoveProtocolType(service, type));
}

static Boolean
service_set_protocol_enabled(SCNetworkServiceRef service, CFStringRef type,
			     Boolean enabled)
{
	SCNetworkProtocolRef	proto;
	Boolean			success;

	proto = SCNetworkServiceCopyProtocol(service, type);
	if (proto == NULL) {
		SCPrint(TRUE, stderr, CFSTR("protocol %@ not found\n"),
			type);
		return (FALSE);
	}
	success = SCNetworkProtocolSetEnabled(proto, enabled);
	if (!success) {
		SCPrint(TRUE, stderr, CFSTR("Failed to %s protocol %@\n"),
			enabled ? "enable" : "disable", type);
	}
	CFRelease(proto);
	return (success);
}

static Boolean
service_disable_protocol(SCNetworkServiceRef service, CFStringRef type)
{
	return (service_set_protocol_enabled(service, type, FALSE));
}

static Boolean
service_enable_protocol(SCNetworkServiceRef service, CFStringRef type)
{
	return (service_set_protocol_enabled(service, type, TRUE));
}

static Boolean
set_service_enabled(SCNetworkServiceRef service, Boolean enabled)
{
	if (enabled) {
		StringMapRef	map;

		map = protocol_strings;
		for (unsigned int i = 0; i < countof(protocol_strings);
		     i++, map++) {
			(void)service_set_protocol_enabled(service,
							   *map->cfstring,
							   enabled);
		}
	}
	return (SCNetworkServiceSetEnabled(service, enabled));
}

static Boolean
enable_service(SCNetworkServiceRef service)
{
	return (set_service_enabled(service, TRUE));
}

static Boolean
disable_service(SCNetworkServiceRef service)
{
	return (set_service_enabled(service, FALSE));
}

static Boolean
matchInterface(SCNetworkInterfaceRef netif, CFStringRef name)
{
	Boolean		match = FALSE;
	CFStringRef	this_name;

	do { /* something to break out of */
		this_name = SCNetworkInterfaceGetBSDName(netif);
		if (this_name != NULL) {
			match = CFEqual(this_name, name);
			if (match) {
				break;
			}
		}
		this_name = SCNetworkInterfaceGetLocalizedDisplayName(netif);
		if (this_name != NULL) {
			match = CFEqual(this_name, name);
			if (match) {
				break;
			}
		}
	} while (0);

	return (match);
}

static Boolean
matchService(SCNetworkServiceRef service, SCNetworkInterfaceRef netif,
	     CFStringRef name)
{
	Boolean		match = FALSE;
	CFStringRef	this_name;

	do { /* something to break out of */
		this_name = SCNetworkServiceGetName(service);
		if (this_name != NULL) {
			match = CFEqual(this_name, name);
			if (match) {
				break;
			}
		}
		this_name = SCNetworkServiceGetServiceID(service);
		if (this_name != NULL) {
			match = CFEqual(this_name, name);
			if (match) {
				break;
			}
		}
		match = matchInterface(netif, name);
	} while (0);

	return (match);
}

static SCNetworkServiceRef
copy_configured_service_in_set(SCNetworkSetRef set,
			       CFStringRef name,
			       Boolean is_bsd_name)
{
	SCNetworkServiceRef	ret_service = NULL;
	CFArrayRef		services = NULL;

	services = SCNetworkSetCopyServices(set);
	if (services == NULL) {
		goto done;
	}

	for (CFIndex i = 0, count = CFArrayGetCount(services);
	     i < count; i++) {
		Boolean			found = FALSE;
		SCNetworkInterfaceRef	netif;
		SCNetworkServiceRef	s;

		s = (SCNetworkServiceRef)CFArrayGetValueAtIndex(services, i);
		netif = SCNetworkServiceGetInterface(s);
		if (netif == NULL) {
			continue;
		}
		if (is_bsd_name) {
			found = matchInterface(netif, name);
		}
		else {
			found = matchService(s, netif, name);
		}
		if (found) {
			CFRetain(s);
			ret_service = s;
			break;
		}
	}

 done:
	my_CFRelease(&services);
	return (ret_service);
}

static SCNetworkServiceRef
copy_configured_service(SCPreferencesRef prefs,
			CFStringRef name,
			Boolean is_bsd_name)
{
	SCNetworkSetRef		current_set;
	SCNetworkServiceRef	ret_service = NULL;

	current_set = SCNetworkSetCopyCurrent(prefs);
	if (current_set == NULL) {
		goto done;
	}
	ret_service = copy_configured_service_in_set(current_set,
						     name, is_bsd_name);
 done:
	my_CFRelease(&current_set);
	return (ret_service);
}

static void
show_scerror(const char * message)
{
	fprintf(stderr, "%s failed: %s\n",
		message, SCErrorString(SCError()));
}

static SCNetworkServiceRef
create_service(SCPreferencesRef prefs, SCNetworkInterfaceRef netif)
{
	SCNetworkSetRef		current_set;
	SCNetworkServiceRef	service;
	SCNetworkServiceRef	ret_service = NULL;

	current_set = SCNetworkSetCopyCurrent(prefs);
	if (current_set == NULL) {
		current_set = SCNetworkSetCreate(prefs);
		if (!SCNetworkSetSetCurrent(current_set)) {
			show_scerror("failed to set current set");
			goto done;
		}
		(void)SCNetworkSetSetName(current_set, CFSTR("Automatic"));
	}
	service = SCNetworkServiceCreate(prefs, netif);
	if (!SCNetworkSetAddService(current_set, service)) {
		CFRelease(service);
		show_scerror("failed to add service to set");
		goto done;
	}
	ret_service = service;

 done:
	my_CFRelease(&current_set);
	return (ret_service);
}

static Boolean
remove_service(SCPreferencesRef prefs, SCNetworkServiceRef service)
{
	SCNetworkSetRef		current_set;
	Boolean			success = FALSE;

	current_set = SCNetworkSetCopyCurrent(prefs);
	if (current_set == NULL) {
		fprintf(stderr, "No current set\n");
		goto done;
	}
	success = SCNetworkSetRemoveService(current_set, service);
	CFRelease(current_set);
	if (!success) {
		show_scerror("Failed to remove service from current set");
		goto done;
	}
 done:
	return (success);
					    
}


static SCNetworkInterfaceRef
copy_bsd_interface(CFStringRef name_cf, const char * name)
{
	unsigned int		index;
	SCNetworkInterfaceRef	netif = NULL;

	index = if_nametoindex(name);
	if (index != 0) {
		netif = _SCNetworkInterfaceCreateWithBSDName(NULL, name_cf, 0);
	}
	return (netif);
}

static SCNetworkInterfaceRef
copy_available_interface(CFStringRef name_cf, const char * name)
{
	CFArrayRef		if_list;
	SCNetworkInterfaceRef	ret_netif = NULL;

	if_list = SCNetworkInterfaceCopyAll();
	if (if_list == NULL) {
		goto done;
	}
	for (CFIndex i = 0, count = CFArrayGetCount(if_list);
	     i < count; i++) {
		SCNetworkInterfaceRef	netif;

		netif = (SCNetworkInterfaceRef)
			CFArrayGetValueAtIndex(if_list, i);
		if (matchInterface(netif, name_cf)) {
			CFRetain(netif);
			ret_netif = netif;
			break;
		}
	}
	if (ret_netif == NULL) {
		ret_netif = copy_bsd_interface(name_cf, name);
	}
 done:
	my_CFRelease(&if_list);
	return (ret_netif);
}

static Boolean
service_establish_default(SCNetworkServiceRef service)
{
	CFArrayRef 	protocols;
	Boolean		success = FALSE;

	protocols = SCNetworkServiceCopyProtocols(service);
	if (protocols != NULL) {
		for (CFIndex i = 0, count = CFArrayGetCount(protocols);
		     i < count; i++) {
			SCNetworkProtocolRef	proto;
			CFStringRef		type;

			proto = (SCNetworkProtocolRef)
				CFArrayGetValueAtIndex(protocols, i);
			type = SCNetworkProtocolGetProtocolType(proto);
			if (!SCNetworkServiceRemoveProtocolType(service, type)) {
				SCPrint(TRUE, stderr,
					CFSTR("Failed to remove %@\n"),
					type);
				goto done;
			}
		}	
		CFRelease(protocols);
	}
	success = SCNetworkServiceEstablishDefaultConfiguration(service);
 done:
	return (success);
}

static SCNetworkInterfaceRef
copy_vlan_interface(SCPreferencesRef prefs, CFStringRef vlan_name)
{
	CFArrayRef		list;
	SCNetworkInterfaceRef	ret_netif = NULL;

	list = SCVLANInterfaceCopyAll(prefs);
	if (list != NULL) {
		for (CFIndex i = 0, count = CFArrayGetCount(list);
		     i < count; i++) {
			CFStringRef		name;
			SCNetworkInterfaceRef	netif;

			netif = (SCNetworkInterfaceRef)
				CFArrayGetValueAtIndex(list, i);
			name = SCNetworkInterfaceGetBSDName(netif);
			if (name != NULL && CFEqual(vlan_name, name)) {
				ret_netif = netif;
				break;
			}
			name = SCNetworkInterfaceGetLocalizedDisplayName(netif);
			if (name != NULL && CFEqual(vlan_name, name)) {
				ret_netif = netif;
				break;
			}
		}
		if (ret_netif != NULL) {
			CFRetain(ret_netif);
		}
		CFRelease(list);
	}
	return (ret_netif);
}

static SCNetworkInterfaceRef
copy_vlan_physical_interface(CFStringRef physical)
{
	CFArrayRef		list;
	SCNetworkInterfaceRef	netif = NULL;

	list = SCVLANInterfaceCopyAvailablePhysicalInterfaces();
	if (list == NULL) {
		fprintf(stderr, "No available physical interfaces for VLAN\n");
		goto done;
	}
	for (CFIndex i = 0, count = CFArrayGetCount(list); i < count; i++) {
		SCNetworkInterfaceRef	this;
		CFStringRef		name;

		this = (SCNetworkInterfaceRef)CFArrayGetValueAtIndex(list, i);
		name = SCNetworkInterfaceGetBSDName(this);
		if (name == NULL) {
			continue;
		}
		if (CFEqual(physical, name)) {
			CFRetain(this);
			netif = this;
			break;
		}
	}
	CFRelease(list);
 done:
	return (netif);
}

static SCNetworkInterfaceRef
copy_vlan_device_and_id(const char * vlan_id, const char * vlan_device,
			CFNumberRef * vlan_id_cf_p)
{
	SCNetworkInterfaceRef	netif;
	int			vlan_id_num;
	CFStringRef		vlan_device_cf;
	
	vlan_id_num = atoi(vlan_id);
	if (vlan_id_num < 1 || vlan_id_num > 4094) {
		fprintf(stderr, "Invalid vlan_id %s\n", vlan_id);
		command_specific_help();
	}
	vlan_device_cf = my_CFStringCreate(vlan_device);
	netif = copy_vlan_physical_interface(vlan_device_cf);
	CFRelease(vlan_device_cf);
	if (netif == NULL) {
		unsigned int		index;
		
		index = if_nametoindex(vlan_device);
		if (index == 0) {
			fprintf(stderr, "Can't find physical interface '%s'\n",
				vlan_device);
		} else {
			fprintf(stderr, "Interface '%s' does not support VLAN\n",
				vlan_device);
		}
	}
	else {
		*vlan_id_cf_p
			= CFNumberCreate(NULL, kCFNumberIntType, &vlan_id_num);
	}
	return (netif);
}

/*
 * Typedef: UnConst
 * Work-around for -Wcast-qual not being happy about casting a const pointer
 * e.g. CFArrayRef to (void *).
 */
typedef union {
	void *		non_const_ptr;
	const void *	const_ptr;
} UnConst;

static CFArrayRef
copy_sorted_services(SCNetworkSetRef set)
{
	CFIndex			count;
	CFArrayRef		order = NULL;
	CFArrayRef		services = NULL;
	CFMutableArrayRef	sorted = NULL;
	UnConst			unconst;

	services = SCNetworkSetCopyServices(set);
	if (services == NULL) {
		goto done;
	}
	count = CFArrayGetCount(services);
	if (count == 0) {
		goto done;
	}
	order = SCNetworkSetGetServiceOrder(set);
	sorted = CFArrayCreateMutableCopy(NULL, 0, services);
	unconst.const_ptr = order;
	CFArraySortValues(sorted,
			  CFRangeMake(0, count),
			  _SCNetworkServiceCompare,
			  unconst.non_const_ptr);
 done:
	my_CFRelease(&services);
	return (sorted);
}

static SCNetworkSetRef
set_copy(SCPreferencesRef prefs, CFStringRef set_name)
{
	SCNetworkSetRef	ret_set = NULL;
	CFArrayRef	sets = NULL;

	sets = SCNetworkSetCopyAll(prefs);
	if (sets == NULL) {
		goto done;
	}
	for (CFIndex i = 0, count = CFArrayGetCount(sets);
	     i < count; i++) {
		CFStringRef	name;
		CFStringRef	setID;
		SCNetworkSetRef	s;

		s = (SCNetworkSetRef)CFArrayGetValueAtIndex(sets, i);
		name = SCNetworkSetGetName(s);
		setID = SCNetworkSetGetSetID(s);
		if ((setID != NULL && CFEqual(setID, set_name))
		    || (name != NULL && CFEqual(name, set_name))) {
			ret_set = s;
			CFRetain(s);
			break;
		}
	}

 done:
	my_CFRelease(&sets);
	return (ret_set);
}

static void
commit_apply(SCPreferencesRef prefs)
{
	/* Commit/Apply changes */
	if (!SCPreferencesCommitChanges(prefs)) {
		show_scerror("Commit changes");
		exit(EX_SOFTWARE);
	}
	if (!SCPreferencesApplyChanges(prefs)) {
		show_scerror("Apply changes");
		exit(EX_SOFTWARE);
	}
}

static CFStringRef
createPath(const char * arg)
{
	char	path[MAXPATHLEN];

	if (arg[0] == '/') {
		return (my_CFStringCreate(arg));
	}
	/* relative path, fully qualify it */
	if (getcwd(path, sizeof(path)) == NULL) {
		fprintf(stderr,
			"Can't get current working directory, %s\n",
			strerror(errno));
		return (NULL);
	}
	return (CFStringCreateWithFormat(NULL, NULL, CFSTR("%s/%s"), path, arg));
}

/**
 ** Add/Set routines
 **/
typedef struct {
	union {
		struct in_addr	ipv4;
		struct in6_addr	ipv6;
	};
	uint8_t			af;
	uint8_t			pad[3]; /* -Wpadded */
} IPAddress, *IPAddressRef;

typedef struct {
	const char *		dhcp_client_id;
	IPv4ConfigMethod	config_method;
	struct in_addr		address;
	struct in_addr		subnet_mask;
	struct in_addr		router;
} IPv4Params, *IPv4ParamsRef;

typedef struct {
	struct in6_addr		address;
	struct in6_addr		router;
	IPv6ConfigMethod	config_method;
	uint32_t		_pad; /* -Wpadded */
} IPv6Params, *IPv6ParamsRef;

typedef struct {
	CFMutableArrayRef	addresses;
	const char *		domain_name;
	CFMutableArrayRef	search_domains;
} DNSParams, *DNSParamsRef;

typedef struct {
	Protocol		protocol;
	Boolean			default_configuration;
	uint8_t			_pad[3]; /* -Wpadded */
	union {
		IPv4Params	ipv4;
		IPv6Params	ipv6;
		DNSParams	dns;
	};
} ProtocolParams, *ProtocolParamsRef;

#define ADD_SET_OPTSTRING	"A:c:C:Dhi:m:n:N:p:r:s:S:"

static Boolean
FieldSetIPAddress(struct in_addr * field_p, const char * label,
		  const char * arg)
{
	Boolean		success = FALSE;

	if (field_p->s_addr != 0) {
		fprintf(stderr, "%s specified multiple times\n", label);
	}
	else if (inet_pton(AF_INET, arg, field_p) != 1) {
		fprintf(stderr, "%s invalid IPv4 address '%s'\n", label,
			arg);
	}
	else {
		success = TRUE;
	}
	return (success);
}

static Boolean
FieldSetIPv6Address(struct in6_addr * field_p, const char * label,
		    const char * arg)
{
	Boolean		success = FALSE;

	if (!IN6_IS_ADDR_UNSPECIFIED(field_p)) {
		fprintf(stderr, "%s specified multiple times\n", label);
	}
	else if (inet_pton(AF_INET6, arg, field_p) != 1) {
		fprintf(stderr, "%s invalid IPv6 address '%s'\n", label,
			arg);
	}
	else {
		success = TRUE;
	}
	return (success);
}

static void
ProtocolParamsInit(ProtocolParamsRef params)
{
	bzero(params, sizeof(*params));
}

static void
ProtocolParamsRelease(ProtocolParamsRef params)
{
	switch (params->protocol) {
	case kProtocolIPv4:
		break;
	case kProtocolIPv6:
		break;
	case kProtocolDNS:
		my_CFRelease(&params->dns.addresses);
		my_CFRelease(&params->dns.search_domains);
		break;
	case kProtocolNone:
		break;
	}
}

static Boolean
ProtocolParamsAddDHCPClientID(ProtocolParamsRef params, const char * arg)
{
	Boolean		success = FALSE;

	if (params->protocol != kProtocolIPv4) {
		fprintf(stderr, "dhcp-client-id only applies to ivp4\n");
	}
	else if (params->ipv4.dhcp_client_id != NULL) {
		fprintf(stderr, "dhcp-client-id specified multiple times\n");
	}
	else {
		params->ipv4.dhcp_client_id = arg;
		success = TRUE;
	}
	return (success);
}

static Boolean
ProtocolParamsAddConfigMethod(ProtocolParamsRef params, const char * arg)
{
	Boolean success = FALSE;

	switch (params->protocol) {
	case kProtocolIPv4:
		if (params->ipv4.config_method != kIPv4ConfigMethodNone) {
			fprintf(stderr,
				"config-method specified multiple times\n");
			break;
		}
		params->ipv4.config_method
			= IPv4ConfigMethodFromString(arg);
		if (params->ipv4.config_method == kIPv4ConfigMethodNone) {
			fprintf(stderr,
				"config-method must be one of "
				"dhcp, manual, inform, or linklocal\n");
			break;
		}
		success = TRUE;
		break;
	case kProtocolIPv6:
		if (params->ipv6.config_method != kIPv6ConfigMethodNone) {
			fprintf(stderr,
				"config-method specified multiple times\n");
			break;
		}
		params->ipv6.config_method
			= IPv6ConfigMethodFromString(arg);
		if (params->ipv6.config_method == kIPv6ConfigMethodNone) {
			fprintf(stderr,
				"config-method must be one of "
				"automatic, manual, or linklocal\n");
			break;
		}
		success = TRUE;
		break;
	case kProtocolNone:
	case kProtocolDNS:
		fprintf(stderr,	"config-method not valid with %s\n",
			ProtocolGetString(params->protocol));
		break;
	}
	return (success);
}

static Boolean
ProtocolParamsAddDNSServers(ProtocolParamsRef params, const char * arg)
{
	char *		address;
	char *		addresses;
	char *		dup_arg;
	Boolean		success = FALSE;

	dup_arg = addresses = strdup(arg);
	while ((address = strsep(&addresses, ",")) != NULL) {
		IPAddress	addr;
		CFStringRef	str;

		bzero(&addr, sizeof(addr));
		if (inet_pton(AF_INET, address, &addr.ipv4) == 1) {
			addr.af = AF_INET;
		}
		else if (inet_pton(AF_INET6, address, &addr.ipv6) == 1) {
			addr.af = AF_INET6;
		}
		else {
			fprintf(stderr, "Invalid IP address '%s'\n", address);
			success = FALSE;
			break;
		}
		str = my_CFStringCreateWithIPAddress(addr.af, &addr.ipv4);
		if (params->dns.addresses == NULL) {
			params->dns.addresses = array_create();
		}
		CFArrayAppendValue(params->dns.addresses, str);
		CFRelease(str);
		success = TRUE;
	}
	free(dup_arg);
	return (success);
}

static Boolean
ProtocolParamsAddAddress(ProtocolParamsRef params, const char * arg)
{
	Boolean	success = FALSE;

	switch (params->protocol) {
	case kProtocolDNS:
		success = ProtocolParamsAddDNSServers(params, arg);
		break;
	case kProtocolIPv4:
		success = FieldSetIPAddress(&params->ipv4.address,
					    kOptAddress, arg);
		break;
	case kProtocolIPv6:
		success = FieldSetIPv6Address(&params->ipv6.address,
					      kOptAddress, arg);
		break;
	case kProtocolNone:
		break;
	}
	return (success);
}

static Boolean
ProtocolParamsAddSubnetMask(ProtocolParamsRef params, const char * arg)
{
	Boolean	success = FALSE;

	switch (params->protocol) {
	case kProtocolIPv4:
		success = FieldSetIPAddress(&params->ipv4.subnet_mask,
					    kOptSubnetMask, arg);
		break;
	case kProtocolNone:
	case kProtocolIPv6:
	case kProtocolDNS:
		fprintf(stderr,
			"%s only valid with IPv4\n",
			kOptSubnetMask);
	}
	return (success);
}

static Boolean
ProtocolParamsAddRouter(ProtocolParamsRef params, const char * arg)
{
	Boolean	success = FALSE;

	switch (params->protocol) {
	case kProtocolIPv4:
		success = FieldSetIPAddress(&params->ipv4.router,
					    kOptRouter, arg);
		break;
	case kProtocolIPv6:
		success = FieldSetIPv6Address(&params->ipv6.router,
					      kOptRouter, arg);
		break;
	case kProtocolNone:
	case kProtocolDNS:
		fprintf(stderr,
			"%s only valid with IPv4/IPv6\n",
			kOptRouter);
		break;
	}
	return (success);
}

static Boolean
ProtocolParamsAddSearchDomain(ProtocolParamsRef params, const char * arg)
{
	Boolean		success = FALSE;

	if (params->protocol != kProtocolDNS) {
		fprintf(stderr,	"%s only applies to dns\n",
			kOptDNSSearchDomains);
	}
	else {
		CFArrayRef	array;
		CFStringRef	domain;
		CFRange		range;

		domain = my_CFStringCreate(arg);
		array = CFStringCreateArrayBySeparatingStrings(NULL, domain,
							       CFSTR(","));
		CFRelease(domain);
		if (params->dns.search_domains == NULL) {
			params->dns.search_domains = array_create();
		}
		range.location = 0;
		range.length = CFArrayGetCount(array);
		CFArrayAppendArray(params->dns.search_domains, array, range);
		success = TRUE;
		CFRelease(array);
	}
	return (success);
}

static Boolean
ProtocolParamsAddDomainName(ProtocolParamsRef params, const char * arg)
{
	Boolean		success = FALSE;

	if (params->protocol != kProtocolDNS) {
		fprintf(stderr, "%s only applies to dns\n",
			kOptDNSDomainName);
	}
	else if (params->dns.domain_name != NULL) {
		fprintf(stderr,	"%s specified multiple times\n",
			kOptDNSDomainName);
	}
	else {
		params->dns.domain_name = arg;
		success = TRUE;
	}
	return (success);
}

static Boolean
ProtocolParamsValidateIPv4(ProtocolParamsRef params)
{
	IPv4ConfigMethod 	method = params->ipv4.config_method;
	Boolean			success = FALSE;

	if (method != kIPv4ConfigMethodDHCP
	    && params->ipv4.dhcp_client_id != NULL) {
		fprintf(stderr,
			"%s not valid with %s\n",
			kOptDHCPClientID,
			IPv4ConfigMethodGetString(method));
		goto done;
	}
	if (method != kIPv4ConfigMethodManual
	    && params->ipv4.router.s_addr != 0) {
		fprintf(stderr,
			"%s not valid with %s\n",
			kOptRouter,
			IPv4ConfigMethodGetString(method));
		goto done;
	}
	switch (method) {
	case kIPv4ConfigMethodNone:
		fprintf(stderr,
			"%s must be specified\n",
			kOptConfigMethod);
		break;
	case kIPv4ConfigMethodInform:
	case kIPv4ConfigMethodManual:
		if (params->ipv4.address.s_addr == 0) {
			fprintf(stderr,
				"%s requires %s\n",
				IPv4ConfigMethodGetString(method),
				kOptAddress);
			break;
		}
		success = TRUE;
		break;
	case kIPv4ConfigMethodDHCP:
	case kIPv4ConfigMethodLinkLocal:
		if (params->ipv4.address.s_addr != 0) {
			fprintf(stderr,
				"%s not valid with %s\n",
				kOptAddress,
				IPv4ConfigMethodGetString(method));
			break;
		}
		if (params->ipv4.subnet_mask.s_addr != 0) {
			fprintf(stderr,
				"%s not valid with %s\n",
				kOptSubnetMask,
				IPv4ConfigMethodGetString(method));
			break;
		}
		success = TRUE;
		break;
	}
 done:
	return (success);
	
}

static Boolean
ProtocolParamsValidateIPv6(ProtocolParamsRef params)
{
	IPv6ConfigMethod 	method = params->ipv6.config_method;
	Boolean			success = FALSE;

	if (method != kIPv6ConfigMethodManual
	    && !IN6_IS_ADDR_UNSPECIFIED(&params->ipv6.router)) {
		fprintf(stderr,
			"%s not valid with %s\n",
			kOptRouter,
			IPv6ConfigMethodGetString(method));
		goto done;
	}
	switch (params->ipv6.config_method) {
	case kIPv6ConfigMethodNone:
		fprintf(stderr,
			"%s must be specified\n",
			kOptConfigMethod);
		break;
	case kIPv6ConfigMethodManual:
		if (IN6_IS_ADDR_UNSPECIFIED(&params->ipv6.address)) {
			fprintf(stderr,
				"%s requires %s\n",
				IPv6ConfigMethodGetString(method),
				kOptAddress);
			break;
		}
		success = TRUE;
		break;
	case kIPv6ConfigMethodAutomatic:
	case kIPv6ConfigMethodLinkLocal:
		success = TRUE;
		break;
	}

 done:
	return (success);
}

static Boolean
ProtocolParamsValidate(ProtocolParamsRef params)
{
	Boolean		success = FALSE;

	switch (params->protocol) {
	case kProtocolIPv4:
		success = ProtocolParamsValidateIPv4(params);
		break;
	case kProtocolIPv6:
		success = ProtocolParamsValidateIPv6(params);
		break;
	case kProtocolDNS:
		if (params->dns.addresses == NULL) {
			fprintf(stderr,
				"dns requires at least one address\n");
			break;
		}
		success = TRUE;
		break;
	case kProtocolNone:
		break;
	}
	return (success);
}

static void
print_invalid_interface_type(const char * arg)
{
	fprintf(stderr,
		"Invalid %s '%s', must be vlan\n",
		kOptInterfaceType, arg);
}

static void
print_invalid_protocol(const char * arg)
{
	fprintf(stderr,
		"Invalid protocol '%s', must be one of ipv4, ipv6, or dns\n",
		arg);
}

static Boolean
ProtocolParamsGet(ProtocolParamsRef params,
		  int argc, char * * argv)
{
	int		ch;
	Boolean		protocol_done = FALSE;
	int		start_optind = optind;

	ch = getopt_long(argc, argv, ADD_SET_OPTSTRING, longopts, NULL);
	if (ch == -1) {
		goto done;
	}
	switch (ch) {
	case OPT_PROTOCOL:
		params->protocol = ProtocolFromString(optarg);
		if (params->protocol == kProtocolNone) {
			print_invalid_protocol(optarg);
			command_specific_help();
		}
		break;
	case OPT_DEFAULT_CONFIG:
		params->default_configuration = TRUE;
		protocol_done = TRUE;
		goto done;
	default:
		break;
	}
	if (params->protocol == kProtocolNone) {
		fprintf(stderr, "protocol must first be specified\n");
		command_specific_help();
	}
	while (!protocol_done) {
		Boolean		success = FALSE;

		ch = getopt_long(argc, argv, ADD_SET_OPTSTRING, longopts, NULL);
		if (ch == -1) {
			break;
		}
		switch (ch) {
		case OPT_ADDRESS:
			success = ProtocolParamsAddAddress(params, optarg);
			break;
		case OPT_DNS_SEARCH_DOMAINS:
			success = ProtocolParamsAddSearchDomain(params, optarg);
			break;
		case OPT_DNS_DOMAIN_NAME:
			success = ProtocolParamsAddDomainName(params, optarg);
			break;
		case OPT_CONFIG_METHOD:
			success = ProtocolParamsAddConfigMethod(params, optarg);
			break;
		case OPT_DHCP_CLIENT_ID:
			success = ProtocolParamsAddDHCPClientID(params, optarg);
			break;
		case OPT_NAME:
		case OPT_INTERFACE:
		case OPT_SERVICE:
			fprintf(stderr,
				"%s, %s, and %s may only be specified once\n",
				kOptInterface, kOptService, kOptName);
			break;
		case OPT_SUBNET_MASK:
			success = ProtocolParamsAddSubnetMask(params, optarg);
			break;
		case OPT_ROUTER:
			success = ProtocolParamsAddRouter(params, optarg);
			break;
		case OPT_PROTOCOL:
			/* we've moved onto the next protocol */
			protocol_done = TRUE;
			optind -= 2; /* backtrack */
			success = TRUE;
			break;
		case OPT_DEFAULT_CONFIG:
			params->default_configuration = TRUE;
			success = TRUE;
			break;
		default:
			break;
		}
		if (!success) {
			exit(EX_USAGE);
		}
	}
	if (!ProtocolParamsValidate(params)) {
		exit(EX_USAGE);
	}
	if (optind != start_optind) {
		protocol_done = TRUE;
	}

 done:
	return (protocol_done);
}

static Boolean
ProtocolParamsApplyIPv4(ProtocolParamsRef params, SCNetworkProtocolRef protocol)
{
	CFMutableDictionaryRef	config;
	CFStringRef		config_method;
	Boolean			success;

	config_method = IPv4ConfigMethodGetCFString(params->ipv4.config_method);
	config = dict_create();
	CFDictionarySetValue(config, kSCPropNetIPv4ConfigMethod,
			     config_method);
	if (params->ipv4.address.s_addr != 0) {
		my_CFDictionarySetIPAddressAsArrayValue(config,
							kSCPropNetIPv4Addresses,
							AF_INET,
							&params->ipv4.address);
		if (params->ipv4.subnet_mask.s_addr != 0) {
			my_CFDictionarySetIPAddressAsArrayValue(config,
								kSCPropNetIPv4SubnetMasks,
								AF_INET,
								&params->ipv4.subnet_mask);
		}
		if (params->ipv4.router.s_addr != 0) {
			my_CFDictionarySetIPAddress(config,
						    kSCPropNetIPv4Router,
						    AF_INET,
						    &params->ipv4.router);
		}
	}
	if (params->ipv4.dhcp_client_id != NULL) {
		my_CFDictionarySetCString(config,
					  kSCPropNetIPv4DHCPClientID,
					  params->ipv4.dhcp_client_id);
	}
	success = SCNetworkProtocolSetConfiguration(protocol, config);
	CFRelease(config);
	return (success);
}

static Boolean
ProtocolParamsApplyIPv6(ProtocolParamsRef params, SCNetworkProtocolRef protocol)
{
	CFMutableDictionaryRef	config;
	CFStringRef		config_method;
	Boolean			success;

	config_method = IPv6ConfigMethodGetCFString(params->ipv6.config_method);
	config = dict_create();
	CFDictionarySetValue(config, kSCPropNetIPv6ConfigMethod,
			     config_method);
	if (!IN6_IS_ADDR_UNSPECIFIED(&params->ipv6.address)) {
		my_CFDictionarySetIPAddressAsArrayValue(config,
							kSCPropNetIPv6Addresses,
							AF_INET6,
							&params->ipv6.address);
		my_CFDictionarySetNumberAsArrayValue(config,
						     kSCPropNetIPv6PrefixLength,
						     64);
		if (!IN6_IS_ADDR_UNSPECIFIED(&params->ipv6.router)) {
			my_CFDictionarySetIPAddress(config,
						    kSCPropNetIPv6Router,
						    AF_INET6,
						    &params->ipv6.router);
		}
	}
	success = SCNetworkProtocolSetConfiguration(protocol, config);
	CFRelease(config);
	return (success);
}

static Boolean
ProtocolParamsApplyDNS(ProtocolParamsRef params, SCNetworkProtocolRef protocol)
{
	CFMutableDictionaryRef	config;
	Boolean			success;

	if (params->dns.addresses == NULL) {
		fprintf(stderr, "DNS requires addresses\n");
		return (FALSE);
	}
	config = dict_create();
	CFDictionarySetValue(config,
			     kSCPropNetDNSServerAddresses,
			     params->dns.addresses);
	if (params->dns.search_domains != NULL) {
		CFDictionarySetValue(config,
				     kSCPropNetDNSSearchDomains,
				     params->dns.search_domains);
	}
	if (params->dns.domain_name != NULL) {
		my_CFDictionarySetCString(config,
					  kSCPropNetDNSDomainName,
					  params->dns.domain_name);
	}
	success = SCNetworkProtocolSetConfiguration(protocol, config);
	CFRelease(config);
	return (success);
}

static Boolean
ProtocolParamsApply(ProtocolParamsRef params, SCNetworkServiceRef service)
{
	Boolean			success = FALSE;
	CFStringRef		type;
	SCNetworkProtocolRef	protocol = NULL;

	if (params->default_configuration) {
		success = service_establish_default(service);
		if (!success) {
			fprintf(stderr,
				"Failed to establish default configuration\n");
		}
		goto done;
	}
	type = ProtocolGetCFString(params->protocol);
	if (type == NULL) {
		fprintf(stderr, "internal error: ProtocolGetCFString failed\n");
		goto done;
	}
	protocol = service_copy_protocol(service, type);
	if (protocol == NULL) {
		fprintf(stderr, "failed to add protocol\n");
		goto done;
	}
	switch (params->protocol) {
	case kProtocolIPv4:
		success = ProtocolParamsApplyIPv4(params, protocol);
		break;
	case kProtocolIPv6:
		success = ProtocolParamsApplyIPv6(params, protocol);
		break;
	case kProtocolDNS:
		success = ProtocolParamsApplyDNS(params, protocol);
		break;
	case kProtocolNone:
		break;
	}
	CFRelease(protocol);

 done:
	return (success);
}


/*
 * Function: do_add_set
 * Purpose:
 *   Add or set the service configuration for an interface.
 */
static void
do_add_set(int argc, char * argv[])
{
	Boolean			changed = FALSE;
	int			ch;
	SCNetworkInterfaceRef	netif = NULL;
	ProtocolParams		params;
	SCPreferencesRef	prefs = prefs_create();
	SCNetworkServiceRef	service = NULL;
	CFStringRef		name = NULL;
	CFStringRef		new_service_name = NULL;
	int			save_optind;
	CFStringRef		str;

	ch = getopt_long(argc, argv, ADD_SET_OPTSTRING, longopts, NULL);
	switch (ch) {
	case OPT_HELP:
		command_specific_help();

	case OPT_INTERFACE:
	case OPT_SERVICE:
		name = my_CFStringCreate(optarg);
		if (G_command == kCommandSet) {
			service = copy_configured_service(prefs, name,
							  (ch == OPT_INTERFACE));
			if (service != NULL) {
				netif = SCNetworkServiceGetInterface(service);
				if (netif != NULL) {
					CFRetain(netif);
				}
				break;
			}
		}
		netif = copy_available_interface(name, optarg);
		if (netif == NULL) {
			break;
		}
		if (service == NULL) {
			service = create_service(prefs, netif);
		}
		break;
	default:
		fprintf(stderr, "Either -%c or -%c must first be specified\n",
			OPT_INTERFACE, OPT_SERVICE);
		command_specific_help();
	}
	my_CFRelease(&name);
	if (service == NULL) {
		fprintf(stderr, "Can't find %s\n", optarg);
		exit(EX_UNAVAILABLE);
	}
	save_optind = optind;
	ch = getopt_long(argc, argv, ADD_SET_OPTSTRING, longopts, NULL);
	switch (ch) {
	case OPT_INTERFACE:
	case OPT_SERVICE:
		fprintf(stderr,
			"%s and %s may only be specified once\n",
			kOptInterface, kOptService);
		exit(EX_USAGE);

	case OPT_NAME:
		new_service_name = my_CFStringCreate(optarg);
		break;
	default:
		/* backtrack */
		optind = save_optind;
		break;
	}
	ProtocolParamsInit(&params);
	while (ProtocolParamsGet(&params, argc, argv)) {
		/* process params */
		changed = TRUE;
		if (!ProtocolParamsApply(&params, service)) {
			exit(EX_SOFTWARE);
		}
		ProtocolParamsRelease(&params);
		ProtocolParamsInit(&params);
	}
	if (new_service_name != NULL) {
		changed = SCNetworkServiceSetName(service, new_service_name);
		if (!changed) {
			SCPrint(TRUE, stderr,
				CFSTR("Failed to set service name '%@': %s\n"),
				new_service_name, SCErrorString(SCError()));
			exit(EX_USAGE);
		}
		my_CFRelease(&new_service_name);
	}
	if (optind < argc) {
		fprintf(stderr, "Extra command-line arguments\n");
		exit(EX_USAGE);
	}
	if (!changed) {
		command_specific_help();
	}
	commit_apply(prefs);
	str = service_copy_summary(service);
	SCPrint(TRUE, stdout, CFSTR("%s %@\n"),
		CommandGetString(G_command), str);
	CFRelease(prefs);
	CFRelease(str);
	CFRelease(service);
	CFRelease(netif);
}

/**
 ** Remove
 **/

#define REMOVE_ENABLE_DISABLE_OPTSTRING	"hi:p:S:"

/*
 * Function: do_remove_enable_disable
 * Purpose:
 *   Remove, enable, or disable a network service, or any of its
 *   protocols.
 */
static void
do_remove_enable_disable(int argc, char * argv[])
{
	Boolean			changed = FALSE;
	int			ch;
	Boolean			by_interface = FALSE;
	CFStringRef		name;
	SCPreferencesRef	prefs = prefs_create();
	Protocol		protocol;
	SCNetworkServiceRef	service = NULL;
	CFStringRef		str;
	Boolean			success = FALSE;
	CFStringRef		type;

	ch = getopt_long(argc, argv, REMOVE_ENABLE_DISABLE_OPTSTRING,
			 longopts, NULL);
	switch (ch) {
	case OPT_HELP:
		command_specific_help();
	case OPT_INTERFACE:
	case OPT_SERVICE:
		name = my_CFStringCreate(optarg);
		by_interface = (ch == OPT_INTERFACE);
		service = copy_configured_service(prefs, name, by_interface);
		my_CFRelease(&name);
		break;
	default:
		fprintf(stderr, "Either -%c or -%c must first be specified\n",
			OPT_INTERFACE, OPT_SERVICE);
		command_specific_help();
	}
	if (service == NULL) {
		fprintf(stderr, "Can't find %s\n", optarg);
		exit(EX_UNAVAILABLE);
	}
	while (TRUE) {
		ch = getopt_long(argc, argv, REMOVE_ENABLE_DISABLE_OPTSTRING,
				 longopts, NULL);
		if (ch == -1) {
			break;
		}
		switch (ch) {
		case OPT_INTERFACE:
		case OPT_SERVICE:
			fprintf(stderr,
				"interface/service may only "
				"be specified once\n");
			break;
		case OPT_PROTOCOL:
			protocol = ProtocolFromString(optarg);
			if (protocol == kProtocolNone) {
				print_invalid_protocol(optarg);
				command_specific_help();
			}
			type = ProtocolGetCFString(protocol);
			if (G_command == kCommandRemove) {
				success = service_remove_protocol(service, type);
			}
			else if (G_command == kCommandEnable) {
				success = service_enable_protocol(service, type);
			}
			else if (G_command == kCommandDisable) {
			      success = service_disable_protocol(service,
								 type);
			}
			if (!success) {
				fprintf(stderr,
					"Failed to %s protocol '%s'\n",
					CommandGetString(G_command),
					ProtocolGetString(protocol));
				exit(EX_USAGE);
			}
			break;
		default:
			break;
		}
		if (!success) {
			command_specific_help();
		}
		changed = TRUE;
	}
	if (optind < argc) {
		fprintf(stderr, "Extra command-line arguments\n");
		exit(EX_USAGE);
	}
	if (!changed) {
		success = FALSE;
		if (G_command == kCommandRemove) {
			success = remove_service(prefs, service);
		}
		else if (G_command == kCommandEnable) {
			success = enable_service(service);
		}
		else if (G_command == kCommandDisable) {
			success	= disable_service(service);
		}
		if (!success) {
			exit(EX_SOFTWARE);
		}
	}
	str = service_copy_summary(service);
	SCPrint(TRUE, stdout, CFSTR("%s %@\n"),
		CommandGetString(G_command), str);
	CFRelease(str);
	commit_apply(prefs);
	CFRelease(service);
}

/**
 ** Show
 **/
static void
protocol_populate_summary_dictionary(SCNetworkProtocolRef p,
				     CFMutableDictionaryRef dict)
{
	CFDictionaryRef		config;
	CFStringRef		type;

	config = SCNetworkProtocolGetConfiguration(p);
	if (config == NULL) {
		return;
	}
	type = SCNetworkProtocolGetProtocolType(p);
	CFDictionarySetValue(dict, CFSTR("type"), type);
	CFDictionarySetValue(dict, CFSTR("configuration"), config);
	if (!SCNetworkProtocolGetEnabled(p)) {
		CFDictionarySetValue(dict, CFSTR("enabled"), kCFBooleanFalse);
	}
}

static void
interface_populate_summary_dictionary(SCNetworkInterfaceRef netif,
				      CFMutableDictionaryRef dict)
{
	CFStringRef		name;
	CFStringRef		type;

	name = SCNetworkInterfaceGetBSDName(netif);
	if (name != NULL) {
		CFDictionarySetValue(dict, CFSTR("bsd_name"), name);
	}
	name = SCNetworkInterfaceGetLocalizedDisplayName(netif);
	if (name != NULL) {
		CFDictionarySetValue(dict, CFSTR("name"), name);
	}
	if (CFDictionaryGetCount(dict) == 0) {
		SCNetworkInterfaceRef	child;

		child = SCNetworkInterfaceGetInterface(netif);
		if (child != NULL) {
			name = SCNetworkInterfaceGetBSDName(child);
			if (name != NULL) {
				CFDictionarySetValue(dict, CFSTR("bsd_name"),
						     name);
			}
			name = SCNetworkInterfaceGetLocalizedDisplayName(netif);
			if (name != NULL) {
				CFDictionarySetValue(dict, CFSTR("name"), name);
			}
		}
	}
	type = SCNetworkInterfaceGetInterfaceType(netif);
	CFDictionarySetValue(dict, CFSTR("type"), type);
}

static void
service_populate_summary_dictionary(SCNetworkServiceRef service,
				    CFMutableDictionaryRef dict)
{
	CFArrayRef		list;
	SCNetworkInterfaceRef	netif;
	CFMutableDictionaryRef	sub_dict;

	dict_set_val(dict, CFSTR("name"),
		     SCNetworkServiceGetName(service));
	dict_set_val(dict, CFSTR("serviceID"),
		     SCNetworkServiceGetServiceID(service));
	if (!SCNetworkServiceGetEnabled(service)) {
		CFDictionarySetValue(dict,
				     CFSTR("enabled"),
				     kCFBooleanFalse);
		
	}
	netif = SCNetworkServiceGetInterface(service);
	sub_dict = dict_create();
	interface_populate_summary_dictionary(netif, sub_dict);
	if (CFDictionaryGetCount(sub_dict) != 0) {
		CFDictionarySetValue(dict,
				     CFSTR("interface"),
				     sub_dict);
	}
	my_CFRelease(&sub_dict);
	list = SCNetworkServiceCopyProtocols(service);
	if (list != NULL) {
		CFMutableArrayRef	descriptions;

		descriptions = array_create();
		for (CFIndex i = 0, count = CFArrayGetCount(list);
		     i < count; i++) {
			SCNetworkProtocolRef	p;

			sub_dict = dict_create();
			p = (SCNetworkProtocolRef)
				CFArrayGetValueAtIndex(list, i);
			protocol_populate_summary_dictionary(p, sub_dict);
			if (CFDictionaryGetCount(sub_dict) != 0) {
				CFArrayAppendValue(descriptions,
						   sub_dict);
			}
			CFRelease(sub_dict);
		}
		if (CFArrayGetCount(descriptions) > 0) {
			CFDictionarySetValue(dict,
					     CFSTR("protocols"),
					     descriptions);
			CFRelease(descriptions);
		}
		CFRelease(list);
	}
}

static void
show_interface(SCNetworkInterfaceRef netif, Boolean verbose)
{
	CFTypeRef	descr;

	if (verbose) {
		CFMutableDictionaryRef	dict;

		descr = dict = dict_create();
		interface_populate_summary_dictionary(netif, dict);
	}
	else {
		descr = (CFTypeRef)interface_copy_summary(netif);
	}
	SCPrint(TRUE, stdout, CFSTR("%@\n"), descr);
	CFRelease(descr);
}

static void
show_service(SCNetworkServiceRef service, Boolean verbose)
{
	CFTypeRef	descr;

	if (verbose) {
		CFMutableDictionaryRef	dict;

		descr = dict = dict_create();
		service_populate_summary_dictionary(service, dict);
	}
	else {
		descr = (CFTypeRef)service_copy_summary(service);
	}
	SCPrint(TRUE, stdout, CFSTR("%@\n"), descr);
	CFRelease(descr);
}

static void
find_and_show_service(SCNetworkSetRef set, const char * arg, Boolean verbose)
{
	CFStringRef		name;
	SCNetworkServiceRef	service = NULL;

	name = my_CFStringCreate(arg);
	service = copy_configured_service_in_set(set, name, FALSE);
	my_CFRelease(&name);
	if (service == NULL) {
		fprintf(stderr, "Can't find %s\n", arg);
		exit(EX_UNAVAILABLE);
	}
	show_service(service, verbose);
	CFRelease(service);
}

static void
find_and_show_interface(SCNetworkSetRef set, const char * arg, Boolean verbose)
{
	CFStringRef		name;
	SCNetworkInterfaceRef	netif;
	SCNetworkServiceRef	service = NULL;

	name = my_CFStringCreate(arg);
	service = copy_configured_service_in_set(set, name, TRUE);
	if (service != NULL) {
		netif = SCNetworkServiceGetInterface(service);
		if (netif != NULL) {
			CFRetain(netif);
		}
	}
	else {
		netif = copy_available_interface(name, arg);
	}
	my_CFRelease(&name);
	my_CFRelease(&service);
	if (netif == NULL) {
		fprintf(stderr, "Can't find %s\n", arg);
		exit(EX_UNAVAILABLE);
	}
	show_interface(netif, verbose);
	CFRelease(netif);
}

static void
show_all_interfaces(Boolean verbose)
{
	CFArrayRef	list;

	list = SCNetworkInterfaceCopyAll();
	if (list == NULL) {
		return;
	}
	for (CFIndex i = 0, count = CFArrayGetCount(list); i < count; i++) {
		CFTypeRef		descr;
		SCNetworkInterfaceRef	netif = CFArrayGetValueAtIndex(list, i);

		if (verbose) {
			CFMutableDictionaryRef	dict;

			dict = dict_create();
			interface_populate_summary_dictionary(netif, dict);
			descr = dict;
		}
		else {
			CFStringRef	str;

			str = interface_copy_summary(netif);
			descr = str;
		}
		SCPrint(TRUE, stdout, CFSTR("%d. %@\n"), (int)(i + 1), descr);
		CFRelease(descr);
	}
	CFRelease(list);
}

static void
show_all_services(SCNetworkSetRef set, Boolean verbose)
{
	CFArrayRef	services = NULL;

	services = copy_sorted_services(set);
	if (services == NULL) {
		return;
	}
	for (CFIndex i = 0, count = CFArrayGetCount(services);
	     i < count; i++) {
		CFTypeRef		descr;
		SCNetworkServiceRef	s;

		s = (SCNetworkServiceRef)CFArrayGetValueAtIndex(services, i);
		if (verbose) {
			CFMutableDictionaryRef	dict;

			dict = dict_create();
			service_populate_summary_dictionary(s, dict);
			descr = dict;
		}
		else {
			CFStringRef		str;

			str = service_copy_summary(s);
			descr = str;
		}
		SCPrint(TRUE, stdout, CFSTR("%d. %@\n"), (int)(i + 1), descr);
		CFRelease(descr);
	}
	my_CFRelease(&services);

}

static void
show_all_sets(SCPreferencesRef prefs)
{
	CFArrayRef	sets = NULL;

	sets = SCNetworkSetCopyAll(prefs);
	if (sets == NULL) {
		return;
	}
	for (CFIndex i = 0, count = CFArrayGetCount(sets);
	     i < count; i++) {
		CFStringRef	name;
		CFStringRef	setID;
		SCNetworkSetRef	s;

		s = (SCNetworkSetRef)CFArrayGetValueAtIndex(sets, i);
		name = SCNetworkSetGetName(s);
		setID = SCNetworkSetGetSetID(s);
		SCPrint(TRUE, stdout, CFSTR("%d. %@ (%@)\n"), (int)(i + 1),
			name, setID);
	}
	my_CFRelease(&sets);
}

#define SHOW_OPTSTRING	"e:f:hi:S:v"

/*
 * Function: do_show
 * Purpose:
 *   Display information about sets, services and interfaces.
 */

static void
do_show(int argc, char * argv[])
{
	Boolean			all = FALSE;
	int			ch;
	CFStringRef		filename;
	const char *		name = NULL;
	SCPreferencesRef	prefs = NULL;
	SCNetworkSetRef		set = NULL;
	const char *		set_name = NULL;
	Boolean			verbose = FALSE;
	int			which = -1;

	while (1) {
		ch = getopt_long(argc, argv, SHOW_OPTSTRING, longopts, NULL);
		if (ch == -1) {
			break;
		}
		switch (ch) {
		case OPT_SET:
			if (set_name != NULL) {
				fprintf(stderr,
					"-%c specified multiple times\n",
					OPT_SET);
				command_specific_help();
			}
			set_name = optarg;
			break;
		case OPT_INTERFACE:
		case OPT_SERVICE:
			if (which != -1) {
				fprintf(stderr,
					"-%c/-%c specified multiple times\n",
					OPT_INTERFACE, OPT_SERVICE);
				command_specific_help();
			}
			which = ch;
			name = optarg;
			if (optarg[0] == '\0') {
				all = TRUE;
			}
			break;
		case OPT_FILE:
			if (prefs != NULL) {
				fprintf(stderr,
					"-%c specified multiple times\n",
					OPT_FILE);
				command_specific_help();
			}
			if (!file_exists(optarg)) {
				exit(EX_SOFTWARE);
			}
			filename = createPath(optarg);
			if (filename == NULL) {
				exit(EX_SOFTWARE);
			}
			prefs = prefs_create_with_file(filename);
			CFRelease(filename);
			break;
		case OPT_VERBOSE:
			verbose = TRUE;
			break;
		default:
			command_specific_help();
		}
	}
	if (optind < argc) {
		fprintf(stderr, "Extra command-line arguments\n");
		exit(EX_USAGE);
	}
	if (prefs == NULL) {
		prefs = prefs_create();
	}
	if (set_name != NULL) {
		CFStringRef	set_name_cf;

		if (set_name[0] == '\0') {
			if (which != -1) {
				fprintf(stderr,
					"Can't specify -%c when showing "
					"all sets\n", which);
				command_specific_help();
			}
			show_all_sets(prefs);
			goto done;
		}
		set_name_cf = my_CFStringCreate(set_name);
		set = set_copy(prefs, set_name_cf);
		CFRelease(set_name_cf);
		if (set == NULL) {
			fprintf(stderr, "Can't find set '%s'\n", set_name);
			exit(EX_SOFTWARE);
		}
	}
	else {
		set = SCNetworkSetCopyCurrent(prefs);
		if (set == NULL) {
			fprintf(stderr, "No configuration\n");
			exit(EX_SOFTWARE);
		}
	}
	if (which == -1) {
		which = OPT_SERVICE;
		all = TRUE;
	}
	switch (which) {
	case OPT_INTERFACE:
		if (all) {
			show_all_interfaces(verbose);
		}
		else {
			find_and_show_interface(set, name, verbose);
		}
		break;
	case OPT_SERVICE:
		if (all) {
			show_all_services(set, verbose);
		}
		else {
			find_and_show_service(set, name, verbose);
		}
		break;
	default:
		break;
	}

 done:
	my_CFRelease(&set);
	my_CFRelease(&prefs);
	return;
}


/**
 ** Create
 **/
#define CREATE_OPTSTRING	"hI:N:P:t:"

static SCNetworkInterfaceRef
create_vlan(SCPreferencesRef prefs, int argc, char * argv[])
{
	int			ch;
	CFStringRef		name = NULL;
	SCNetworkInterfaceRef	netif;
	const char *		vlan_device = NULL;
	const char *		vlan_id = NULL;
	CFNumberRef		vlan_id_cf = NULL;
	SCVLANInterfaceRef	vlan_netif;

	while (TRUE) {
		Boolean	success = FALSE;

		ch = getopt_long(argc, argv, CREATE_OPTSTRING, longopts, NULL);
		if (ch == -1) {
			break;
		}
		switch (ch) {
		case OPT_NAME:
			if (name != NULL) {
				fprintf(stderr, "%s specified multiple times\n",
					kOptName);
				break;
			}
			name = my_CFStringCreate(optarg);
			success = TRUE;
			break;
		case OPT_INTERFACE_TYPE:
			fprintf(stderr,
				"%s may only be specified once\n",
				kOptInterfaceType);
			break;
		case OPT_VLAN_ID:
			if (vlan_id != NULL) {
				fprintf(stderr,
					"%s specified multiple times\n",
					kOptVLANID);
			}
			vlan_id = optarg;
			success = TRUE;
			break;
		case OPT_VLAN_DEVICE:
			if (vlan_device != NULL) {
				fprintf(stderr,
					"%s specified multiple times\n",
					kOptVLANDevice);
			}
			vlan_device = optarg;
			success = TRUE;
			break;
		default:
			break;
		}
		if (!success) {
			command_specific_help();
		}
	}
	if (optind < argc) {
		fprintf(stderr, "Extra command-line arguments\n");
		exit(EX_USAGE);
	}
	if (vlan_id == NULL || vlan_device == NULL) {
		fprintf(stderr, "Both -%c and -%c must be specified\n",
			OPT_VLAN_ID, OPT_VLAN_DEVICE);
		command_specific_help();
	}
	netif = copy_vlan_device_and_id(vlan_id, vlan_device, &vlan_id_cf);
	if (netif == NULL) {
		exit(EX_USAGE);
	}
	vlan_netif = SCVLANInterfaceCreate(prefs, netif, vlan_id_cf);
	if (vlan_netif == NULL) {
		fprintf(stderr, "Failed to create VLAN interface: %s\n",
			SCErrorString(SCError()));
		exit(EX_SOFTWARE);
	}
	if (name != NULL
	    && !SCVLANInterfaceSetLocalizedDisplayName(vlan_netif, name)) {
		SCPrint(TRUE, stderr,
			CFSTR("Failed to set VLAN name to '%@', %s\n"),
			name, SCErrorString(SCError()));
		exit(EX_SOFTWARE);
	}
	my_CFRelease(&name);
	my_CFRelease(&netif);
	my_CFRelease(&vlan_id_cf);
	return (vlan_netif);
}

/*
 * Function: do_create
 * Purpose:
 *   Create a virtual interface.
 */
static void
do_create(int argc, char * argv[])
{
	int			ch;
	SCNetworkInterfaceRef	netif;
	SCPreferencesRef	prefs = prefs_create();

	ch = getopt_long(argc, argv, CREATE_OPTSTRING, longopts, NULL);
	switch (ch) {
	case OPT_HELP:
		command_specific_help();

	case OPT_INTERFACE_TYPE:
		if (strcasecmp(optarg, "vlan") != 0) {
			print_invalid_interface_type(optarg);
			exit(EX_USAGE);
		}
		break;
	default:
		fprintf(stderr, "-%c must first be specified\n",
			OPT_INTERFACE_TYPE);
		command_specific_help();
	}
	netif = create_vlan(prefs, argc, argv);
	commit_apply(prefs);
	SCPrint(TRUE, stdout,
		CFSTR("Created %@\n"),
		SCNetworkInterfaceGetBSDName(netif));
	CFRelease(netif);
	my_CFRelease(&prefs);
	return;
}

/**
 ** Destroy
 **/

#define DESTROY_OPTSTRING	"hi:"

/*
 * Function: do_destroy
 * Purpose:
 *   Destroy a virtual interface.
 */
static void
do_destroy(int argc, char * argv[])
{
	int			ch;
	CFStringRef		name;
	SCNetworkInterfaceRef	netif;
	SCPreferencesRef	prefs = prefs_create();

	ch = getopt_long(argc, argv, DESTROY_OPTSTRING, longopts, NULL);
	switch (ch) {
	case OPT_HELP:
		command_specific_help();

	case OPT_INTERFACE:
		name = my_CFStringCreate(optarg);
		break;
	default:
		fprintf(stderr, "-%c must be specified\n",
			OPT_INTERFACE);
		command_specific_help();
	}
	if (optind < argc) {
		fprintf(stderr, "Extra command-line arguments\n");
		exit(EX_USAGE);
	}
	netif = copy_vlan_interface(prefs, name);
	if (netif == NULL) {
		SCPrint(TRUE, stderr, 
			CFSTR("Can't find VLAN %@\n"),
			name);
		exit(EX_SOFTWARE);
	}
	if (!SCVLANInterfaceRemove(netif)) {
		SCPrint(TRUE, stderr,
			CFSTR("Failed to remove %@: %s\n"),
			name, SCErrorString(SCError()));
		exit(EX_USAGE);
	}
	commit_apply(prefs);
}

/**
 ** SetVLAN
 **/

#define SET_VLAN_OPTSTRING	"hi:I:N:P:"

/*
 * Function: do_set_vlan
 * Purpose:
 *   Enables setting the VLAN ID and physical interface on a VLAN interface.
 */

static void
do_set_vlan(int argc, char * argv[])
{
	int			ch;
	CFStringRef		name = NULL;
	SCNetworkInterfaceRef	netif;
	SCPreferencesRef	prefs = prefs_create();
	const char *		vlan_device = NULL;
	CFStringRef		vlan_name = NULL;
	const char *		vlan_id = NULL;
	CFNumberRef		vlan_id_cf = NULL;
	SCNetworkInterfaceRef	vlan_netif;

	while (1) {
		Boolean		success = FALSE;

		ch = getopt_long(argc, argv, SET_VLAN_OPTSTRING, longopts, NULL);
		if (ch == -1) {
			break;
		}
		switch (ch) {
		case OPT_NAME:
			if (name != NULL) {
				fprintf(stderr, "%s specified multiple times\n",
					kOptName);
				break;
			}
			name = my_CFStringCreate(optarg);
			success = TRUE;
			break;
		case OPT_HELP:
			command_specific_help();

		case OPT_INTERFACE:
			if (vlan_name != NULL) {
				fprintf(stderr, "%s specified multiple times\n",
					kOptInterface);
				break;
			}
			vlan_name = my_CFStringCreate(optarg);
			success = TRUE;
			break;
		case OPT_VLAN_ID:
			if (vlan_id != NULL) {
				fprintf(stderr,
					"%s specified multiple times\n",
					kOptVLANID);
			}
			vlan_id = optarg;
			success = TRUE;
			break;
		case OPT_VLAN_DEVICE:
			if (vlan_device != NULL) {
				fprintf(stderr,
					"%s specified multiple times\n",
					kOptVLANDevice);
			}
			vlan_device = optarg;
			success = TRUE;
			break;
		default:
			break;
		}
		if (!success) {
			command_specific_help();
		}
	}
	
	if (vlan_name == NULL || vlan_id == NULL || vlan_device == NULL) {
		fprintf(stderr, "All of -%c, -%c, and -%c must be specified\n",
			OPT_INTERFACE, OPT_VLAN_ID, OPT_VLAN_DEVICE);
		command_specific_help();
	}
	netif = copy_vlan_device_and_id(vlan_id, vlan_device, &vlan_id_cf);
	if (netif == NULL) {
		exit(EX_USAGE);
	}
	vlan_netif = copy_vlan_interface(prefs, vlan_name);
	if (vlan_netif == NULL) {
		SCPrint(TRUE, stderr, 
			CFSTR("Can't find VLAN %@\n"),
			name);
		exit(EX_SOFTWARE);
	}
	if (!SCVLANInterfaceSetPhysicalInterfaceAndTag(vlan_netif,
						       netif, vlan_id_cf)) {
		fprintf(stderr, "Failed to set vlan tag/device: %s\n",
			SCErrorString(SCError()));
		exit(EX_SOFTWARE);
	}
	if (name != NULL
	    && !SCVLANInterfaceSetLocalizedDisplayName(vlan_netif, name)) {
		SCPrint(TRUE, stderr,
			CFSTR("Failed to set VLAN name to '%@', %s\n"),
			name, SCErrorString(SCError()));
		exit(EX_SOFTWARE);
	}
	commit_apply(prefs);
	my_CFRelease(&name);
	CFRelease(vlan_netif);
	CFRelease(netif);
	CFRelease(vlan_id_cf);
}

/**
 ** Help/usage
 **/
#define _IDENTIFIER "( -i <interface> | -S <service> ) [ -N <service-name> ]"

static void
help_add_set(const char * command_str) __dead2;

static void
help_remove_enable_disable(const char * command_str) __dead2;

static void
help_show(const char * command_str) __dead2;

static void
help_create(const char * command_str) __dead2;

static void
help_destroy(const char * command_str) __dead2;

static void
help_setvlan(const char * command_str) __dead2;

static void
help_add_set(const char * command_str)
{
	fprintf(stderr,
		"%s %s " _IDENTIFIER " -D\n",
		G_argv0, command_str);
	fprintf(stderr,
		"%s %s " _IDENTIFIER " -p ipv4 -c dhcp\n",
		G_argv0, command_str);
	fprintf(stderr,
		"%s %s " _IDENTIFIER " -p ipv4 -c manual -A <ip> -m <mask>\n",
		G_argv0, command_str);
	fprintf(stderr,
		"%s %s " _IDENTIFIER " -p ipv6 -c automatic\n",
		G_argv0, command_str);
	fprintf(stderr,
		"%s %s " _IDENTIFIER " -p ipv6 -c manual -A <ip>\n",
		G_argv0, command_str);
	fprintf(stderr,
		"%s %s " _IDENTIFIER " -p dns -A <dns-server> "
		"-n <domain> -s <domain>\n",
		G_argv0, command_str);
	exit(EX_USAGE);
}

static void
help_remove_enable_disable(const char * command_str)
{
	fprintf(stderr,
		"%s %s ( -%c <interface> | -%c <service> ) "
		"[ -p ( ipv4 | ipv6 | dns ) ]\n",
		G_argv0, command_str,
		OPT_INTERFACE, OPT_SERVICE);
	exit(EX_USAGE);
}

static void
help_show(const char * command_str)
{
	fprintf(stderr,
		"%s %s [ -e <set> | -e \"\" ] [ -%c <interface> | -%c \"\" | "
		"-%c <service> | -%c \"\" ] [ -%c ] [ -%c <filename> ]\n",
		G_argv0, command_str,
		OPT_INTERFACE, OPT_INTERFACE, 
		OPT_SERVICE, OPT_SERVICE, OPT_VERBOSE,
		OPT_FILE);
	exit(EX_USAGE);
}

static void
help_create(const char * command_str)
{
	fprintf(stderr,
		"%s %s -t vlan -%c <1..4095> -%c <interface> [ -N <name> ]\n",
		G_argv0, command_str,
		OPT_VLAN_ID, OPT_VLAN_DEVICE);
	exit(EX_USAGE);
}

static void
help_destroy(const char * command_str)
{
	fprintf(stderr,
		"%s %s -%c <interface>\n",
		G_argv0, command_str, OPT_INTERFACE);
	exit(EX_USAGE);
}

static void
help_setvlan(const char * command_str)
{
	fprintf(stderr,
		"%s %s -i <interface> -%c <1..4095> -%c <interface>\n",
		G_argv0, command_str, OPT_VLAN_ID, OPT_VLAN_DEVICE);
	exit(EX_USAGE);
}

static void
usage(void) __dead2;

static void
usage(void)
{
	fprintf(stderr,
		"Usage: %s [command] [args]\n"
		"\ncommands:\n"
		"\tadd\n"
		"\tset\n"
		"\tremove\n"
		"\tdisable\n"
		"\tenable\n"
		"\tshow\n"
		"\tcreate\n"
		"\tdestroy\n"
		"\tsetvlan\n"
		"\noptions:\n"
		"\t--address, -A             IP address\n"
		"\t--config-method, -c       configuration method\n"
		"\t--dhcp-client-id, -C      DHCP client identifier\n"
		"\t--default-config, -D      establish default configuration\n"
		"\t--file, -f                filename\n"
		"\t--help, -h                get help\n"
		"\t--dns-domain-name, -n     DNS domain name\n"
		"\t--dns-search-domains, -S  DNS search domains\n"
		"\t--interface, -i           interface name e.g. en0\n"
		"\t--interface-type, -t      interface type e.g. vlan\n"
		"\t--new-name, -N            new name\n"
		"\t--subnet-mask, -m         subnet mask e.g. 255.255.255.0\n"
		"\t--protocol, -p            protocol e.g. ipv4, ipv6, dns\n"
		"\t--service, -s             service name/identifier\n"
		"\t--verbose, -v             be verbose\n"
		"\t--vlan-id, -I             VLAN identifier (1..4096)\n"
		"\t--vlan-device, -P         VLAN physical device e.g. en0\n",
		G_argv0);
	exit(EX_USAGE);
}

static void
command_specific_help(void)
{
	const char *	str = CommandGetString(G_command);
	
	switch (G_command) {
	case kCommandAdd:
	case kCommandSet:
		help_add_set(str);

	case kCommandRemove:
	case kCommandEnable:
	case kCommandDisable:
		help_remove_enable_disable(str);

	case kCommandShow:
		help_show(str);

	case kCommandCreate:
		help_create(str);

	case kCommandDestroy:
		help_destroy(str);

	case kCommandSetVLAN:
		help_setvlan(str);

	case kCommandNone:
		break;
	}
	usage();
}

int
main(int argc, char *argv[])
{
	const char *	slash;

	G_argv0 = argv[0];
	slash = strrchr(G_argv0, '/');
	if (slash != NULL) {
		G_argv0 = slash + 1;
	}
	if (argc < 2) {
		usage();
	}
	G_command = CommandFromString(argv[1]);
	if (G_command == kCommandNone) {
		usage();
	}
	switch (G_command) {
	case kCommandAdd:
	case kCommandSet:
		do_add_set(argc - 1, argv + 1);
		break;
	case kCommandRemove:
	case kCommandEnable:
	case kCommandDisable:
		do_remove_enable_disable(argc - 1, argv + 1);
		break;
	case kCommandShow:
		do_show(argc - 1, argv + 1);
		break;
	case kCommandCreate:
		do_create(argc - 1, argv + 1);
		break;
	case kCommandDestroy:
		do_destroy(argc - 1, argv + 1);
		break;
	case kCommandSetVLAN:
		do_set_vlan(argc - 1, argv + 1);
		break;
	case kCommandNone:
		break;
	}
	exit(0);
}
