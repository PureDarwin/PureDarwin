/*
 * Copyright (c) 1999-2020 Apple Inc. All rights reserved.
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
 * client.c
 * - client side program to talk to ipconfigd
 */
/* 
 * Modification History
 *
 * September, 1999 	Dieter Siegmund (dieter@apple.com)
 * - initial revision
 * July 31, 2000	Dieter Siegmund (dieter@apple.com)
 * - changed to add set, and new implementation of waitall
 * - removed waitif
 * February 24, 2003	Dieter Siegmund (dieter@apple.com)
 * - added support to retrieve information about the netboot (bsdp)
 *   packet stored in the device tree
 * September 10, 2009	Dieter Siegmund (dieter@apple.com)
 * - added IPv6 support
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <servers/bootstrap.h>
#include <ctype.h>
#include <string.h>
#include <sysexits.h>

#include <netinet/in.h>
#include <netinet/udp.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/bootp.h>
#include <arpa/inet.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCValidation.h>
#include <SystemConfiguration/SCPrivate.h>
#include <TargetConditionals.h>

#include "ipconfig_ext.h"
#include "ipconfig_types.h"
#include "dhcp_options.h"
#include "dhcplib.h"
#include "bsdp.h"
#include "bsdplib.h"
#include "ioregpath.h"
#include "ipconfig.h"
#include "cfutil.h"
#include "DHCPv6.h"
#include "DHCPv6Options.h"
#include "RouterAdvertisement.h"
#include "IPConfigurationControlPrefs.h"
#include "IPConfigurationUtil.h"

#define METHOD_LIST_V4			"BOOTP, MANUAL, DHCP, INFORM"
#define METHOD_LIST_V4_WITH_NONE	METHOD_LIST_V4 ", NONE"
#define METHOD_LIST_V6			"AUTOMATIC-V6, MANUAL-V6, 6TO4"
#define METHOD_LIST_V6_WITH_NONE	METHOD_LIST_V6 ", NONE-V6"
#define METHOD_LIST			METHOD_LIST_V4 ", " METHOD_LIST_V6
#define METHOD_LIST_WITH_NONE		METHOD_LIST_V4_WITH_NONE ", " METHOD_LIST_V6_WITH_NONE

typedef int func_t(mach_port_t server, int argc, char * argv[]);
typedef func_t * funcptr_t;

static char * progname;
static char * command_name;

#define STARTUP_KEY	CFSTR("Plugin:IPConfiguration")

#define SHADOW_MOUNT_PATH_COMMAND	"shadow_mount_path"
#define SHADOW_FILE_PATH_COMMAND	"shadow_file_path"
#define MACHINE_NAME_COMMAND		"machine_name"

static void
on_alarm(int sigraised)
{
    exit(0);
}

#define WAIT_ALL_DEFAULT_TIMEOUT	90
#define WAIT_ALL_MAX_TIMEOUT		120

static void
key_appeared(SCDynamicStoreRef session, CFArrayRef changes, void * arg)
{
    exit(0);
}

static int
S_wait_all(mach_port_t server, int argc, char * argv[])
{
    CFMutableArrayRef	keys;
    SCDynamicStoreRef 	session;
    CFRunLoopSourceRef	rls;
    unsigned long	t = WAIT_ALL_DEFAULT_TIMEOUT;
    CFPropertyListRef	value;
    struct itimerval 	v;

    if (argc > 0) {
	t = strtoul(argv[0], 0, 0);
	if (t > WAIT_ALL_MAX_TIMEOUT) {
	    t = WAIT_ALL_MAX_TIMEOUT;
	}
    }

    session = SCDynamicStoreCreate(NULL, CFSTR("ipconfig command"), 
				   key_appeared, NULL);
    if (session == NULL) {
	fprintf(stderr, "SCDynamicStoreCreate failed: %s\n", 
		SCErrorString(SCError()));
	return (0);
    }
    keys = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    CFArrayAppendValue(keys, STARTUP_KEY);
    SCDynamicStoreSetNotificationKeys(session, keys, NULL);
    CFRelease(keys);

    rls = SCDynamicStoreCreateRunLoopSource(NULL, session, 0);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);
    CFRelease(rls);

    signal(SIGALRM, on_alarm);
    bzero(&v, sizeof(v));
    v.it_value.tv_sec = t;
    if (setitimer(ITIMER_REAL, &v, NULL) < 0) {
	perror("setitimer");
	return (0);
    }
    value = SCDynamicStoreCopyValue(session, STARTUP_KEY);
    if (value == NULL) {
	CFRunLoopRun();
	return (0);
    }
    CFRelease(value);
    CFRelease(session);
    return (0);
}

static int
S_bsdp_get_packet(mach_port_t server, int argc, char * argv[])
{
    CFDictionaryRef	chosen = NULL;
    struct dhcp *	dhcp;
    int			length;
    CFDataRef		response = NULL;
    int			ret = 1;

    if (getuid() != 0) {
	return (EX_NOPERM);
    }

    chosen = myIORegistryEntryCopyValue("IODeviceTree:/chosen");
    if (chosen == NULL) {
	goto done;
    }
    response = CFDictionaryGetValue(chosen, CFSTR("bsdp-response"));
    if (isA_CFData(response) == NULL) {
	response = CFDictionaryGetValue(chosen, CFSTR("bootp-response"));
	if (isA_CFData(response) == NULL) {
	    goto done;
	}
    }
    /* ALIGN: CFDataGetBytePtr is aligned to at least sizeof(uint64) */
    dhcp = (struct dhcp *)(void *)CFDataGetBytePtr(response);
    length = (int)CFDataGetLength(response);
    bsdp_print_packet(dhcp, length, 0);
    ret = 0;
 done:
    if (chosen != NULL) {
	CFRelease(chosen);
    }
    return (ret);
}

static int
S_bsdp_option(mach_port_t server, int argc, char * argv[])
{
    CFDictionaryRef	chosen = NULL;
    void *		data = NULL;
    int			data_len;
    struct dhcp *	dhcp;
    int			length;
    dhcpol_t		options;
    CFDataRef		response = NULL;
    int			ret = 1;
    int			tag = 0;
    dhcpol_t		vendor_options;
    int			vendor_tag = 0;

    if (getuid() != 0) {
	return (EX_NOPERM);
    }

    chosen = myIORegistryEntryCopyValue("IODeviceTree:/chosen");
    if (chosen == NULL) {
	goto done;
    }
    response = CFDictionaryGetValue(chosen, CFSTR("bsdp-response"));
    if (isA_CFData(response) == NULL) {
	response = CFDictionaryGetValue(chosen, CFSTR("bootp-response"));
	if (isA_CFData(response) == NULL) {
	    goto done;
	}
    }

    /* ALIGN: CFDataGetBytePtr is aligned to at least sizeof(uint64) */
    dhcp = (struct dhcp *)(void *)CFDataGetBytePtr(response);
    length = (int)CFDataGetLength(response);
    if (dhcpol_parse_packet(&options, dhcp, length, NULL) == FALSE) {
	goto done;
    }
    if (strcmp(argv[0], SHADOW_MOUNT_PATH_COMMAND) == 0) {
	tag = dhcptag_vendor_specific_e;
	vendor_tag = bsdptag_shadow_mount_path_e;
    }
    else if (strcmp(argv[0], SHADOW_FILE_PATH_COMMAND) == 0) {
	tag = dhcptag_vendor_specific_e;
	vendor_tag = bsdptag_shadow_file_path_e;
    }
    else if (strcmp(argv[0], MACHINE_NAME_COMMAND) == 0) {
	tag = dhcptag_vendor_specific_e;
	vendor_tag = bsdptag_machine_name_e;
    }
    else {
	tag = atoi(argv[0]);
	if (argc == 2) {
	    vendor_tag = atoi(argv[1]);
	}
    }
    if (tag == dhcptag_vendor_specific_e && vendor_tag != 0) {
	if (dhcpol_parse_vendor(&vendor_options, &options, NULL) == FALSE) {
	    goto done;
	}
	data = dhcpol_option_copy(&vendor_options, vendor_tag, &data_len);
	if (data != NULL) {
	    dhcptype_print(bsdptag_type(vendor_tag), data, data_len);
	    ret = 0;
	}
	dhcpol_free(&vendor_options);
    }
    else {
	const dhcptag_info_t * entry;

	entry = dhcptag_info(tag);
	if (entry == NULL) {
	    goto done;
	}
	data = dhcpol_option_copy(&options, tag, &data_len);
	if (data != NULL) {
	    dhcptype_print(entry->type, data, data_len);
	    ret = 0;
	}
    }
 done:
    if (data != NULL) {
	free(data);
    }
    if (chosen != NULL) {
	CFRelease(chosen);
    }
    return (ret);
}

static int
S_if_addr(mach_port_t server, int argc, char * argv[])
{
    struct in_addr	ip;
    kern_return_t	kret;
    InterfaceName	name;
    ipconfig_status_t	status;

    InterfaceNameInit(name, argv[0]);
    kret = ipconfig_if_addr(server, name, (ip_address_t *)&ip, &status);
    if (kret != KERN_SUCCESS) {
	fprintf(stderr, "get if addr %s failed, %s\n", name,
		mach_error_string(kret));
    }
    if (status == ipconfig_status_success_e) {
	printf("%s\n", inet_ntoa(ip));
	return (0);
    }
    return (1);
}

static int
S_if_count(mach_port_t server, int argc, char * argv[])
{
    int 		count = 0;
    kern_return_t	status;

    status = ipconfig_if_count(server, &count);
    if (status == KERN_SUCCESS) {
	printf("%d\n", count);
	return (0);
    }
    fprintf(stderr, "get if count failed, %s\n", mach_error_string(status));
    return (1);
}

static int
S_get_option(mach_port_t server, int argc, char * argv[])
{
    char		buf[1024];
    dhcpo_err_str_t	err;
    kern_return_t	kret;
    InterfaceName	name;
    dataOut_t		option;
    mach_msg_type_number_t option_cnt;
    ipconfig_status_t	status;
    boolean_t		success;
    int			tag;

    InterfaceNameInit(name, argv[0]);
    tag = dhcptag_with_name(argv[1]);
    if (tag == -1) {
	tag = atoi(argv[1]);
    }
    kret = ipconfig_get_option(server, name, tag, &option, &option_cnt,
			       &status);
    if (kret != KERN_SUCCESS) {
	fprintf(stderr, "ipconfig_get_option failed, %s\n", 
		mach_error_string(kret));
	goto done;
    }
    if (status != ipconfig_status_success_e) {
	goto done;
    }
    success = dhcptag_to_str(buf, sizeof(buf), tag, option, option_cnt, &err);
    (void)vm_deallocate(mach_task_self(), (vm_address_t)option, option_cnt);
    if (success) {
	printf("%s\n", buf);
	return (0);
    }
    fprintf(stderr, "couldn't convert the option, %s\n",
	    err.str);
 done:
    return (1);
}

static int
S_get_packet(mach_port_t server, int argc, char * argv[])
{
    kern_return_t	kret;
    InterfaceName	name;
    dataOut_t		packet;
    mach_msg_type_number_t packet_cnt;
    int			ret;
    ipconfig_status_t	status;

    ret = 1;
    InterfaceNameInit(name, argv[0]);
    kret = ipconfig_get_packet(server, name, &packet, &packet_cnt, &status);
    if (kret != KERN_SUCCESS) {
	fprintf(stderr, "ipconfig_get_packet failed, %s\n", 
		mach_error_string(kret));
	goto done;
    }
    if (status != ipconfig_status_success_e) {
	goto done;
    }
    dhcp_packet_print((struct dhcp *)(void *)packet, packet_cnt);
    (void)vm_deallocate(mach_task_self(), (vm_address_t)packet, packet_cnt);
    ret = 0;

 done:
    return (ret);
}

static int
S_get_v6_packet(mach_port_t server, int argc, char * argv[])
{
    DHCPv6OptionErrorString 	err;
    kern_return_t		kret;
    InterfaceName		name;
    DHCPv6OptionListRef		options;
    dataOut_t			packet;
    mach_msg_type_number_t	packet_cnt;
    int				ret;
    ipconfig_status_t		status;

    ret = 1;
    InterfaceNameInit(name, argv[0]);
    kret = ipconfig_get_v6_packet(server, name, &packet, &packet_cnt, &status);
    if (kret != KERN_SUCCESS) {
	fprintf(stderr, "ipconfig_get_v6_packet failed, %s\n", 
		mach_error_string(kret));
	goto done;
    }
    if (status != ipconfig_status_success_e) {
	goto done;
    }
    DHCPv6PacketFPrint(stdout, (DHCPv6PacketRef)packet, packet_cnt);
    options = DHCPv6OptionListCreateWithPacket((DHCPv6PacketRef)packet,
					       packet_cnt, &err);
    if (options != NULL) {
	DHCPv6OptionListFPrint(stdout, options);
	DHCPv6OptionListRelease(&options);
    }
    ret = 0;
    (void)vm_deallocate(mach_task_self(), (vm_address_t)packet, packet_cnt);

 done:
    return (ret);
}

static int
S_get_ra(mach_port_t server, int argc, char * argv[])
{
    CFDictionaryRef		dict = NULL;
    kern_return_t		kret;
    InterfaceName		name;
    RouterAdvertisementRef	ra;
    xmlDataOut_t		ra_data = NULL;
    mach_msg_type_number_t	ra_data_cnt;
    int				ret;
    ipconfig_status_t		status;

    ret = 1;
    InterfaceNameInit(name, argv[0]);
    kret = ipconfig_get_ra(server, name, &ra_data, &ra_data_cnt, &status);
    if (kret != KERN_SUCCESS) {
	fprintf(stderr, "ipconfig_get_ra failed, %s\n",
		mach_error_string(kret));
	goto done;
    }
    if (status != ipconfig_status_success_e) {
	goto done;
    }
    dict = my_CFPropertyListCreateWithBytePtrAndLength(ra_data, ra_data_cnt);
    if (dict == NULL) {
	fprintf(stderr, "not available\n");
	goto done;
    }
    if (isA_CFDictionary(dict) == NULL) {
	fprintf(stderr, "internal error: bad format\n");
	goto done;
    }
    ra = RouterAdvertisementCreateWithDictionary(dict);
    if (ra != NULL) {
	CFStringRef	description;
	CFDateRef	receive_date;

	receive_date
	    = CFDateCreate(NULL, RouterAdvertisementGetReceiveTime(ra));
	description = RouterAdvertisementCopyDescription(ra);
	SCPrint(TRUE, stdout, CFSTR("RA Received %@ %@\n"),
		receive_date, description);
	CFRelease(receive_date);
	CFRelease(description);
	CFRelease(ra);
    }
    ret = 0;

 done:
    my_CFRelease(&dict);
    if (ra_data != NULL) {
	(void)vm_deallocate(mach_task_self(),
			    (vm_address_t)ra_data, ra_data_cnt);
    }

    return (ret);
}

#ifndef kSCValNetIPv6ConfigMethodLinkLocal
static const CFStringRef kIPConfigurationIPv6ConfigMethodLinkLocal = CFSTR("LinkLocal");
#define kSCValNetIPv6ConfigMethodLinkLocal kIPConfigurationIPv6ConfigMethodLinkLocal
#endif /* kSCValNetIPv6ConfigMethodLinkLocal */

#ifndef kSCValNetIPv4ConfigMethodFailover
static const CFStringRef kIPConfigurationConfigMethodFailover = CFSTR("Failover");
#define kSCValNetIPv4ConfigMethodFailover kIPConfigurationConfigMethodFailover
#endif /* kSCValNetIPv4ConfigMethodFailover */

#ifndef kSCPropNetIPv4FailoverAddressTimeout
static const CFStringRef kIPConfigurationFailoverAddressTimeout = CFSTR("FailoverAddressTimeout");
#define kSCPropNetIPv4FailoverAddressTimeout	kIPConfigurationFailoverAddressTimeout
#endif /* kSCPropNetIPv4FailoverAddressTimeout */

static CFStringRef
IPv4ConfigMethodGet(const char * m)
{
    if (strcasecmp("bootp", m) == 0) {
	return (kSCValNetIPv4ConfigMethodBOOTP);
    }
    if (strcasecmp("dhcp", m) == 0) {
	return (kSCValNetIPv4ConfigMethodDHCP);
    }
    if (strcasecmp("manual", m) == 0) {
	return (kSCValNetIPv4ConfigMethodManual);
    }
    if (strcasecmp("inform", m) == 0) {
	return (kSCValNetIPv4ConfigMethodINFORM);
    }
    if (strcasecmp("linklocal", m) == 0) {
	return (kSCValNetIPv4ConfigMethodLinkLocal);
    }
    if (strcasecmp("failover", m) == 0) {
	return (kSCValNetIPv4ConfigMethodFailover);
    }
    return (NULL);
}

static CFDictionaryRef
IPv4ConfigDictCreate(const char * ifname,
		     int argc, char * argv[], const char * cmd,
		     const char * method_name,
		     CFStringRef config_method)
{
    CFMutableDictionaryRef	dict;

    dict = CFDictionaryCreateMutable(NULL, 0,
				     &kCFTypeDictionaryKeyCallBacks,
				     &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(dict, kSCPropNetIPv4ConfigMethod,
			 config_method);
    if (config_method == kSCValNetIPv4ConfigMethodFailover
	|| config_method == kSCValNetIPv4ConfigMethodINFORM
	|| config_method == kSCValNetIPv4ConfigMethodManual) {
	struct in_addr		ip_address;

	if (config_method == kSCValNetIPv4ConfigMethodFailover) {
	    if (argc < 1 || argc > 3) {
		fprintf(stderr, "usage: ipconfig %s %s "
			"%s <ip-address> [ <subnet-mask> [ <timeout> ] ]\n",
			cmd, ifname, method_name);
		goto failed;
	    }
	}
	else if (argc < 1 || argc > 2) {
	    fprintf(stderr, "usage: ipconfig %s %s "
		    "%s <ip-address> [ <subnet-mask> ]\n",
		    cmd, ifname, method_name);
	    goto failed;
	}
	if (inet_aton(argv[0], &ip_address) == 0) {
	    fprintf(stderr, "Invalid IP address %s\n", argv[0]);
	    goto failed;
	}
	my_CFDictionarySetIPAddressAsArrayValue(dict,
						kSCPropNetIPv4Addresses,
						ip_address);
	if (argc >= 2) {
	    if (inet_aton(argv[1], &ip_address) != 1) {
		fprintf(stderr, "Invalid IP mask %s\n", argv[1]);
		goto failed;
	    }
	    my_CFDictionarySetIPAddressAsArrayValue(dict,
						    kSCPropNetIPv4SubnetMasks,
						    ip_address);
	}
	if (argc >= 3) {
	    int32_t	timeout;
	    
	    timeout = (int32_t)strtol(argv[2], NULL, 0);
	    if (timeout > 0) {
		CFNumberRef	num;
		
		num = CFNumberCreate(NULL, kCFNumberSInt32Type, &timeout);
		CFDictionarySetValue(dict, 
				     kSCPropNetIPv4FailoverAddressTimeout,
				     num);
		CFRelease(num);
	    }
	}
    }
    else if (argc != 0) {
	fprintf(stderr, "too many arguments for method\n");
	goto failed;
    }
    return (dict);

 failed:
    my_CFRelease(&dict);
    return (NULL);
}

static CFStringRef
IPv6ConfigMethodGet(const char * m)
{
    if (strcasecmp(m, "automatic-v6") == 0) {
	return (kSCValNetIPv6ConfigMethodAutomatic);
    }
    else if (strcasecmp(m, "manual-v6") == 0) {
	return (kSCValNetIPv6ConfigMethodManual);
    }
    else if (strcasecmp(m, "6to4") == 0) {
	return (kSCValNetIPv6ConfigMethod6to4);
    }
    if (strcasecmp(m, "linklocal-v6") == 0) {
	return (kSCValNetIPv6ConfigMethodLinkLocal);
    }
    return (NULL);
}

static void
my_CFDictionarySetIPv6AddressAsArrayValue(CFMutableDictionaryRef dict,
					  CFStringRef prop,
					  const struct in6_addr * ipaddr_p)
{
    CFStringRef		str;

    str = my_CFStringCreateWithIPv6Address(ipaddr_p);
    my_CFDictionarySetTypeAsArrayValue(dict, prop, str);
    CFRelease(str);
    return;
}

static void
my_CFDictionarySetIntegerAsArrayValue(CFMutableDictionaryRef dict,
				      CFStringRef prop, int val)
{
    CFNumberRef		num;

    num = CFNumberCreate(NULL, kCFNumberIntType, &val);
    my_CFDictionarySetTypeAsArrayValue(dict, prop, num);
    CFRelease(num);
    return;
}


static CFDictionaryRef
IPv6ConfigDictCreate(const char * ifname, 
		     int argc, char * argv[], const char * cmd,
		     const char * method_name,
		     CFStringRef config_method)
{
    CFDictionaryRef		dict;
    CFMutableDictionaryRef	ipv6_dict;
    CFStringRef			ipv6_key;

    ipv6_dict = CFDictionaryCreateMutable(NULL, 0,
					  &kCFTypeDictionaryKeyCallBacks,
					  &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(ipv6_dict, kSCPropNetIPv6ConfigMethod,
			 config_method);
    if (config_method == kSCValNetIPv6ConfigMethodManual) {
	struct in6_addr		ip_address;
	int			prefix_length;

	if (argc != 2) {
	    fprintf(stderr, "usage: ipconfig %s %s "
		    "%s6 <ipv6-address> <prefix-length>\n", 
		    cmd, ifname, method_name);
	    goto failed;
	}
	if (inet_pton(AF_INET6, argv[0], &ip_address) != 1) {
	    fprintf(stderr, "Invalid IPv6 address %s\n", argv[0]);
	    goto failed;
	}
	my_CFDictionarySetIPv6AddressAsArrayValue(ipv6_dict,
						  kSCPropNetIPv6Addresses,
						  &ip_address);
	prefix_length = (int)strtol(argv[1], NULL, 0);
	if (prefix_length < 0 || prefix_length > 128) {
	    fprintf(stderr, "Invalid prefix_length %s\n", argv[1]);
	    goto failed;
	}
	my_CFDictionarySetIntegerAsArrayValue(ipv6_dict,
					      kSCPropNetIPv6PrefixLength,
					      prefix_length);
    }
    else if (argc != 0) {
	fprintf(stderr, "too many arguments for method\n");
	goto failed;
    }
    ipv6_key = kSCEntNetIPv6;
    dict = CFDictionaryCreate(NULL,
			      (const void * *)&ipv6_key,
			      (const void * *)&ipv6_dict, 1,
			      &kCFTypeDictionaryKeyCallBacks,
			      &kCFTypeDictionaryValueCallBacks);
    CFRelease(ipv6_dict);
    return (dict);

 failed:
    my_CFRelease(&ipv6_dict);
    return (NULL);
}

static CFDictionaryRef
ConfigDictCreate(const char * ifname, int argc, char * argv[], const char * cmd,
		 const char * method_name, boolean_t allow_none)
{
    CFStringRef		config_method;
    
    config_method = IPv4ConfigMethodGet(method_name);
    if (config_method != NULL) {
	return (IPv4ConfigDictCreate(ifname, argc, argv, cmd, method_name, 
				     config_method));
    }
    config_method = IPv6ConfigMethodGet(method_name);
    if (config_method == NULL) {
	fprintf(stderr,
		"ipconfig %s: invalid method '%s'\n<method> is one of %s\n", 
		cmd, method_name,
		(allow_none) ? METHOD_LIST_WITH_NONE : METHOD_LIST);
	return (NULL);
    }
    return (IPv6ConfigDictCreate(ifname, argc, argv, cmd, 
				 method_name, config_method));
}

static int
S_set(mach_port_t server, int argc, char * argv[])
{
    CFDataRef			data = NULL;
    CFDictionaryRef		dict = NULL;
    const char *		method_name;
    InterfaceName		name;
    kern_return_t		kret;
    ipconfig_status_t		status = ipconfig_status_success_e;
    void *			xml_data_ptr = NULL;
    int				xml_data_len = 0;

    InterfaceNameInit(name, argv[0]);
    method_name = argv[1];
    argv += 2;
    argc -= 2;

    if (strcasecmp(method_name, "NONE") == 0) {
	/* nothing to do, NONE implies NULL method data */
    }
    else if (strcasecmp(method_name, "NONE-V6") == 0
	     || strcasecmp(method_name, "NONE-V4") == 0) {

	CFDictionaryRef		empty_dict;
	CFStringRef		ip_key;

	/* NONE-V{4,6} is represented as an empty IPv{4,6} dictionary */
	empty_dict = CFDictionaryCreate(NULL,
					NULL, NULL, 0,
					&kCFTypeDictionaryKeyCallBacks,
					&kCFTypeDictionaryValueCallBacks);
	if (strcasecmp(method_name, "NONE-V6") == 0) {
	    ip_key = kSCEntNetIPv6;
	}
	else {
	    ip_key = kSCEntNetIPv4;
	}
	dict = CFDictionaryCreate(NULL,
				  (const void * *)&ip_key,
				  (const void * *)&empty_dict, 1,
				  &kCFTypeDictionaryKeyCallBacks,
				  &kCFTypeDictionaryValueCallBacks);
	CFRelease(empty_dict);
    }
    else {
	dict = ConfigDictCreate(name, argc, argv, command_name, method_name,
				TRUE);
	if (dict == NULL) {
	    return (1);
	}
    }
    if (dict != NULL) {
	data = CFPropertyListCreateData(NULL,
					dict,
					kCFPropertyListBinaryFormat_v1_0,
					0,
					NULL);
	if (data == NULL) {
	    CFRelease(dict);
	    fprintf(stderr, "failed to allocate memory\n");
	    return (1);
	}
	xml_data_ptr = (void *)CFDataGetBytePtr(data);
	xml_data_len = (int)CFDataGetLength(data);
    }
    kret = ipconfig_set(server, name, xml_data_ptr, xml_data_len, &status);
    my_CFRelease(&dict);
    my_CFRelease(&data);
    if (kret != KERN_SUCCESS) {
	mach_error("ipconfig_set failed", kret);
	return (1);
    }
    if (status != ipconfig_status_success_e) {
	fprintf(stderr, "ipconfig_set %s %s failed: %s\n",
		name, method_name, ipconfig_status_string(status));
	return (1);
    }
    return (0);
}

static int
S_set_verbose(mach_port_t server, int argc, char * argv[])
{
    int			verbose;

    verbose = (int)strtol(argv[0], NULL, 0);
    errno = 0;
    if (verbose == 0 && errno != 0) {
	fprintf(stderr, "conversion to integer of %s failed\n", argv[0]);
	return (1);
    }
    if (IPConfigurationControlPrefsSetVerbose(verbose != 0) == FALSE) {
	fprintf(stderr, "failed to set verbose\n");
	return (1);
    }
    return (0);
}

static int
S_set_cellular_clat46_autoenable(mach_port_t server, int argc, char * argv[])
{
	int			enable;

	errno = 0;
	enable = (int)strtol(argv[0], NULL, 0);
	if (enable == 0 && errno != 0) {
		fprintf(stderr, "conversion to integer of %s failed\n", argv[0]);
		return (1);
	}
	if (IPConfigurationControlPrefsSetCellularCLAT46AutoEnable(enable != 0) == FALSE) {
		fprintf(stderr, "failed to set cellular CLAT46 auto-enable\n");
		return (1);
	}
	return (0);
}

#ifdef IPCONFIG_TEST_NO_ENTRY
static int
S_set_something(mach_port_t server, int argc, char * argv[])
{
    kern_return_t		kret;
    ipconfig_status_t		status = ipconfig_status_success_e;
    int				verbose;

    verbose = strtol(argv[0], NULL, 0);
    errno = 0;
    if (verbose == 0 && errno != 0) {
	fprintf(stderr, "conversion to integer of %s failed\n", argv[0]);
	return (1);
    }
    kret = ipconfig_set_something(server, verbose, &status);
    if (kret != KERN_SUCCESS) {
	return (1);
    }
    if (status != ipconfig_status_success_e) {
	fprintf(stderr, "setsomething failed: %s\n",
		ipconfig_status_string(status));
	return (1);
    }
    return (0);
}
#endif /* IPCONFIG_TEST_NO_ENTRY */

static int
S_add_or_set_service(mach_port_t server, int argc, char * argv[], bool add)
{
    CFDataRef			data = NULL;
    CFDictionaryRef		dict;
    char *			method_name;
    InterfaceName		name;
    kern_return_t		kret;
    ServiceID			service_id;
    ipconfig_status_t		status = ipconfig_status_success_e;
    void *			xml_data_ptr = NULL;
    int				xml_data_len = 0;

    InterfaceNameInit(name, argv[0]);
    method_name = argv[1];
    argv += 2;
    argc -= 2;

    dict = ConfigDictCreate(name, argc, argv, command_name, method_name,
			    FALSE);
    if (dict == NULL) {
	return (1);
    }
    data = CFPropertyListCreateData(NULL,
				    dict,
				    kCFPropertyListBinaryFormat_v1_0,
				    0,
				    NULL);
    if (data == NULL) {
	CFRelease(dict);
	fprintf(stderr, "failed to allocate memory\n");
	return (1);
    }
    ServiceIDClear(service_id);
    xml_data_ptr = (void *)CFDataGetBytePtr(data);
    xml_data_len = (int)CFDataGetLength(data);
    if (add) {
	kret = ipconfig_add_service(server, name, xml_data_ptr, xml_data_len,
				    service_id, &status);
    }
    else {
	kret = ipconfig_set_service(server, name, xml_data_ptr, xml_data_len,
				    service_id, &status);
    }
    CFRelease(dict);
    CFRelease(data);
    
    if (kret != KERN_SUCCESS) {
	fprintf(stderr, "ipconfig_%s_service failed, %s\n", add ? "add" : "set",
		mach_error_string(kret));
	return (1);
    }
    if (status != ipconfig_status_success_e) {
	fprintf(stderr, "ipconfig_%s_service %s %s failed: %s\n",
		add ? "add" : "set",
		name, method_name, ipconfig_status_string(status));
	return (1);
    }
    printf("%s\n", service_id);
    return (0);
}

static int
S_add_service(mach_port_t server, int argc, char * argv[])
{
    return (S_add_or_set_service(server, argc, argv, TRUE));
}

static int
S_set_service(mach_port_t server, int argc, char * argv[])
{
    return (S_add_or_set_service(server, argc, argv, FALSE));
}

static int
S_remove_service_with_id(mach_port_t server, int argc, char * argv[])
{
    InterfaceName		name;
    kern_return_t		kret;
    ServiceID			service_id;
    ipconfig_status_t		status = ipconfig_status_success_e;

    ServiceIDInit(service_id, argv[0]);
    if (argc > 1) {
	InterfaceNameInit(name, argv[1]);
    }
    else {
	InterfaceNameClear(name);
    }
    kret = ipconfig_remove_service_on_interface(server, name, service_id,
						&status);
    if (kret != KERN_SUCCESS) {
	mach_error("ipconfig_remove_service_on_interface failed", kret);
	return (1);
    }
    if (status != ipconfig_status_success_e) {
	fprintf(stderr,
		"ipconfig_remove_service_on_interface '%s' '%s' failed: %s\n",
		name, service_id, ipconfig_status_string(status));
	return (1);
    }
    return (0);
}

static int
S_find_service(mach_port_t server, int argc, char * argv[])
{
    CFDataRef			data = NULL;
    CFDictionaryRef		dict;
    boolean_t			exact = FALSE;
    char *			method_name;
    InterfaceName		name;
    kern_return_t		kret;
    ServiceID			service_id;
    ipconfig_status_t		status = ipconfig_status_success_e;
    void *			xml_data_ptr = NULL;
    int				xml_data_len = 0;

    ServiceIDClear(service_id);
    InterfaceNameInit(name, argv[0]);
    argv++;
    argc--;
    if (argc > 1 && strcasecmp(argv[0], "exact") == 0) {
	exact = TRUE;
	argc--;
	argv++;
    }
    method_name = argv[0];
    argc--;
    argv++;
    dict = ConfigDictCreate(name, argc, argv, command_name, method_name,
			    FALSE);
    if (dict == NULL) {
	return (1);
    }
    data = CFPropertyListCreateData(NULL,
				    dict,
				    kCFPropertyListBinaryFormat_v1_0,
				    0,
				    NULL);
    if (data == NULL) {
	CFRelease(dict);
	fprintf(stderr, "failed to allocate memory\n");
	return (1);
    }
    xml_data_ptr = (void *)CFDataGetBytePtr(data);
    xml_data_len = (int)CFDataGetLength(data);
    kret = ipconfig_find_service(server, name, exact,
				 xml_data_ptr, xml_data_len,
				 service_id, &status);
    my_CFRelease(&dict);
    my_CFRelease(&data);
    if (kret != KERN_SUCCESS) {
	mach_error("ipconfig_find_service failed", kret);
	return (1);
    }
    if (status != ipconfig_status_success_e) {
	fprintf(stderr, "ipconfig_find_service %s %s failed: %s\n",
		name, method_name, ipconfig_status_string(status));
	return (1);
    }
    printf("%s\n", service_id);
    return (0);
}

static int
S_remove_service(mach_port_t server, int argc, char * argv[])
{
    CFDataRef			data = NULL;
    CFDictionaryRef		dict;
    char *			method_name;
    InterfaceName		name;
    kern_return_t		kret;
    ipconfig_status_t		status = ipconfig_status_success_e;
    void *			xml_data_ptr = NULL;
    int				xml_data_len = 0;

    InterfaceNameInit(name, argv[0]);
    method_name = argv[1];
    argv += 2;
    argc -= 2;

    dict = ConfigDictCreate(name, argc, argv, command_name, method_name,
			    FALSE);
    if (dict == NULL) {
	return (1);
    }
    data = CFPropertyListCreateData(NULL,
				    dict,
				    kCFPropertyListBinaryFormat_v1_0,
				    0,
				    NULL);
    if (data == NULL) {
	CFRelease(dict);
	fprintf(stderr, "failed to allocate memory\n");
	return (1);
    }
    xml_data_ptr = (void *)CFDataGetBytePtr(data);
    xml_data_len = (int)CFDataGetLength(data);
    kret = ipconfig_remove_service(server, name, xml_data_ptr, xml_data_len,
				   &status);
    my_CFRelease(&dict);
    my_CFRelease(&data);
    if (kret != KERN_SUCCESS) {
	mach_error("ipconfig_remove_service failed", kret);
	return (1);
    }
    if (status != ipconfig_status_success_e) {
	fprintf(stderr, "ipconfig_remove_service %s %s failed: %s\n",
		name, method_name, ipconfig_status_string(status));
	return (1);
    }
    return (0);
}

static int
S_refresh_service(mach_port_t server, int argc, char * argv[])
{
    InterfaceName		name;
    kern_return_t		kret;
    ServiceID			service_id;
    ipconfig_status_t		status = ipconfig_status_success_e;

    ServiceIDInit(service_id, argv[0]);
    InterfaceNameInit(name, argv[1]);
    kret = ipconfig_refresh_service(server, name, service_id,
				    &status);
    if (kret != KERN_SUCCESS) {
	mach_error("ipconfig_refresh_service", kret);
	return (1);
    }
    if (status != ipconfig_status_success_e) {
	fprintf(stderr, "ipconfig_refresh_service(%s, %s) failed: %s\n",
		service_id, name, ipconfig_status_string(status));
	return (1);
    }
    return (0);
}

static int
S_set_awd_interfaces(mach_port_t server, int argc, char * argv[])
{
    IPConfigurationInterfaceTypes	types;

    if (argc == 0) {
	types = kIPConfigurationInterfaceTypesNone;
    }
    else {
	CFStringRef			str;

	str = CFStringCreateWithCString(NULL, argv[0], kCFStringEncodingASCII);
	types = IPConfigurationInterfaceTypesFromString(str);
	my_CFRelease(&str);
	if (types == kIPConfigurationInterfaceTypesUnspecified) {
	    fprintf(stderr, "invalid type '%s'\n", argv[0]);
	    return (1);
	}
    }
    if (IPConfigurationControlPrefsSetAWDReportInterfaceTypes(types) == FALSE) {
	fprintf(stderr, "failed to set AWD interface types\n");
	return (1);
    }
    return (0);
}

static int
S_get_awd_interfaces(mach_port_t server, int argc, char * argv[])
{
    char *				cstr;
    CFStringRef				str;
    IPConfigurationInterfaceTypes	types;

    types = IPConfigurationControlPrefsGetAWDReportInterfaceTypes();
    if (types == kIPConfigurationInterfaceTypesUnspecified) {
	types = kIPConfigurationInterfaceTypesCellular;
    }
    str = IPConfigurationInterfaceTypesToString(types);
    cstr = my_CFStringToCString(str, kCFStringEncodingASCII);
    printf("%s\n", cstr);
    free(cstr);
    return (0);
}

#if TARGET_OS_OSX
extern Boolean
IPConfigurationForgetNetwork(CFStringRef ifname, CFStringRef ssid)
__attribute__((weak_import));
#endif /* TARGET_OS_OSX */

static int
S_forget_network(mach_port_t server, int argc, char * argv[])
{
    CFStringRef		ifname;
    CFStringRef		ssid;
    Boolean		success;

#if TARGET_OS_OSX
    if (IPConfigurationForgetNetwork == NULL) {
	fprintf(stderr, "IPConfigurationForgetNetwork unavailable\n");
	return (1);
    }
#endif /* TARGET_OS_OSX */
    ifname = CFStringCreateWithCString(NULL, argv[0], kCFStringEncodingUTF8);
    ssid = CFStringCreateWithCString(NULL, argv[1], kCFStringEncodingUTF8);
    success = IPConfigurationForgetNetwork(ifname, ssid);
    my_CFRelease(&ifname);
    my_CFRelease(&ssid);
    if (!success) {
	fprintf(stderr, "%s: failed to forget network %s\n",
		argv[0], argv[1]);
	return (1);
    }
    return (0);
}

static const struct command_info {
    const char *command;
    funcptr_t	func;
    int		argc;
    const char *usage;
    int		display;
    int		no_server;
} commands[] = {
    { "waitall", S_wait_all, 0, "[ timeout secs ]", 1, 1 },
    { "getifaddr", S_if_addr, 1, "<interface name>", 1, 0 },
    { "ifcount", S_if_count, 0, "", 1, 0 },
    { "getoption", S_get_option, 2, 
      " <interface name | \"\" > <option name> | <option code>", 1, 0 },
    { "getpacket", S_get_packet, 1, " <interface name>", 1, 0 },
    { "getv6packet", S_get_v6_packet, 1, " <interface name>", 1, 0 },
    { "getra", S_get_ra, 1, "<interface name>", 1, 0},
    { "set", S_set, 2, 
      "<interface name> <method> <method args>\n"
      "<method> is one of " METHOD_LIST_WITH_NONE,
      1, 0 },
    { "netbootoption", S_bsdp_option, 1, "<option> [<vendor option>] | " 
      SHADOW_MOUNT_PATH_COMMAND " | " SHADOW_FILE_PATH_COMMAND
      " | " MACHINE_NAME_COMMAND, 0, 1 },
    { "netbootpacket", S_bsdp_get_packet, 0, "", 0, 1 },
    { "setclat46enable", S_set_cellular_clat46_autoenable, 1, "0 | 1", 0, 1 },
    { "setverbose", S_set_verbose, 1, "0 | 1", 1, 1 },
#ifdef IPCONFIG_TEST_NO_ENTRY
    { "setsomething", S_set_something, 1, "0 | 1", 1, 0 },
#endif /* IPCONFIG_TEST_NO_ENTRY */
    { "addService", S_add_service, 2, 
      "<interface name> <method> <method args>\n"
      "<method> is one of " METHOD_LIST,
      0, 0 },
    { "setService", S_set_service, 2, 
      "<interface name> <method> <method args>\n"
      "<method> is one of " METHOD_LIST,
      0, 0 },
    { "findService", S_find_service, 2, 
      "<interface name> [ exact ] <method> <method args>\n"
      "<method> is one of " METHOD_LIST,
      0, 0 },
    { "removeServiceWithId", S_remove_service_with_id, 1, 
      "<service ID> [ <interface name> ]", 0, 0 },
    { "removeService", S_remove_service, 2, 
      "<interface name> <method> <method args>\n" 
      "<method> is one of " METHOD_LIST,
      0, 0 },
    { "refreshService", S_refresh_service, 2, 
      "<service ID> <interface name>", 0, 0 },
    { "setawdinterfaces", S_set_awd_interfaces, 0, "[ All | Cellular | None ]", 0, 1 },
    { "getawdinterfaces", S_get_awd_interfaces, 0, NULL, 0, 1 },
    { "forgetNetwork", S_forget_network, 2, "<interface name> <ssid>", 0, 0},
    { NULL, NULL, 0, NULL, 0, 0 },
};

void
usage()
{
    int i;
    fprintf(stderr, "usage: %s <command> <args>\n", progname);
    fprintf(stderr, "where <command> is one of ");
    for (i = 0; commands[i].command; i++) {
	if (commands[i].display) {
	    fprintf(stderr, "%s%s",  i == 0 ? "" : ", ",
		    commands[i].command);
	}
    }
    fprintf(stderr, "\n");
    exit(1);
}

static const struct command_info *
S_lookup_command(char * cmd, int argc)
{
    int i;

    for (i = 0; commands[i].command; i++) {
	if (strcasecmp(cmd, commands[i].command) == 0) {
	    if (argc < commands[i].argc) {
		fprintf(stderr, "usage: ipconfig %s %s\n", commands[i].command,
			commands[i].usage ? commands[i].usage : "");
		exit(1);
	    }
	    return commands + i;
	}
    }
    return (NULL);
}

int
main(int argc, char * argv[])
{
    const struct command_info *	command;
    mach_port_t			server = MACH_PORT_NULL;
    kern_return_t		kret;

    progname = argv[0];
    if (argc < 2)
	usage();

    argv++; argc--;
    command_name = argv[0];
    command = S_lookup_command(command_name, argc - 1);
    if (command == NULL) {
	usage();
	exit(1);
    }
    argv++; argc--;
    if (command->no_server == 0) {
	kret = ipconfig_server_port(&server);
	switch (kret) {
	case BOOTSTRAP_SUCCESS:
	    break;
	case BOOTSTRAP_UNKNOWN_SERVICE:
	    fprintf(stderr, "ipconfig server not active\n");
	    /* start it maybe??? */
	    exit(1);
	default:
	    mach_error("ipconfig_server_port failed", kret);
	    exit(1);
	}
    }
    exit ((*command->func)(server, argc, argv));
}
