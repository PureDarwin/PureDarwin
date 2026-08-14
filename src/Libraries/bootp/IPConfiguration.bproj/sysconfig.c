/*
 * Copyright (c) 2000-2020 Apple Inc. All rights reserved.
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
 * sysconfig.c
 * - system configuration related functions
 */

/* 
 * Modification History
 *
 * June 23, 2009	Dieter Siegmund (dieter@apple.com)
 * - split out from ipconfigd.c
 */

#include "sysconfig.h"
#include "globals.h"
#include <CoreFoundation/CFArray.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCValidation.h>
#include <SystemConfiguration/SCPrivate.h>
#include "DNSNameList.h"
#include "cfutil.h"
#include "symbol_scope.h"
#include "dhcp_thread.h"
#include "DHCPv6Client.h"
#include "IPConfigurationServiceInternal.h"

PRIVATE_EXTERN CFDictionaryRef
my_SCDynamicStoreCopyDictionary(SCDynamicStoreRef session, CFStringRef key)
{
    CFDictionaryRef 		dict;

    dict = SCDynamicStoreCopyValue(session, key);
    if (dict) {
	if (isA_CFDictionary(dict) == NULL) {
	    my_CFRelease(&dict);
	}
    }
    return (dict);
}

STATIC boolean_t
store_key_different(SCDynamicStoreRef session, CFStringRef key, 
		    CFDictionaryRef value)
{
    CFDictionaryRef	store_value;
    boolean_t		ret = TRUE;

    store_value = my_SCDynamicStoreCopyDictionary(session, key);
    if (store_value != NULL) {
	if (CFEqual(value, store_value)) {
	    ret = FALSE;
	}
	my_CFRelease(&store_value);
    }
    return (ret);
}

STATIC CFMutableDictionaryRef	S_keys_to_set = NULL;
STATIC CFMutableArrayRef	S_keys_to_remove = NULL;
STATIC CFRange			S_keys_to_remove_range;

STATIC void
update_key(SCDynamicStoreRef session,
	   CFStringRef key, CFDictionaryRef dict)
{
    if (dict != NULL) {
	CFIndex		index;

	/* if we're setting the value, don't remove it anymore */
	index = CFArrayGetFirstIndexOfValue(S_keys_to_remove,
					    S_keys_to_remove_range, key);
	if (index != kCFNotFound
	    || store_key_different(session, key, dict)) {
	    if (index != kCFNotFound) {
		CFArrayRemoveValueAtIndex(S_keys_to_remove, index);
		S_keys_to_remove_range.length--;
	    }
	    CFDictionarySetValue(S_keys_to_set, key, dict);
	}
    }
    else {
	if (CFArrayContainsValue(S_keys_to_remove, 
				 S_keys_to_remove_range, key) == FALSE) {
	    CFArrayAppendValue(S_keys_to_remove, key);
	    S_keys_to_remove_range.length++;
	}
	/* if we're removing the value, don't set it anymore */
	CFDictionaryRemoveValue(S_keys_to_set, key);
    }
    return;
}

PRIVATE_EXTERN void
my_SCDynamicStoreSetService(SCDynamicStoreRef store, 
			    CFStringRef serviceID,
			    CFStringRef entities[],
			    CFDictionaryRef values[],
			    int count,
			    boolean_t alternate_location)
{
    int				i;

    if (count == 0) {
	/* nothing to do */
	return;
    }
    if (S_keys_to_set == NULL) {
	S_keys_to_set
	    = CFDictionaryCreateMutable(NULL, 0,
					&kCFTypeDictionaryKeyCallBacks,
					&kCFTypeDictionaryValueCallBacks);
    }
    if (S_keys_to_remove == NULL) {
	S_keys_to_remove
	    = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	S_keys_to_remove_range.location = 0;
	S_keys_to_remove_range.length = 0;
    }
    if (alternate_location) {
	CFMutableDictionaryRef	dict = NULL;
	CFStringRef		key;

	for (i = 0; i < count; i++) {
	    if (values[i] == NULL) {
		continue;
	    }
	    if (dict == NULL) {
		dict = CFDictionaryCreateMutable(NULL, 0,
						 &kCFTypeDictionaryKeyCallBacks,
						 &kCFTypeDictionaryValueCallBacks);
	    }
	    CFDictionarySetValue(dict, entities[i], values[i]);
	}
	key = IPConfigurationServiceKey(serviceID);
	update_key(store, key, dict);
	CFRelease(key);
	if (dict != NULL) {
	    CFRelease(dict);
	}
    }
    else {
	for (i = 0; i < count; i++) {
	    CFStringRef	key;
	    
	    key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
							      kSCDynamicStoreDomainState,
							      serviceID, 
							      entities[i]);
	    update_key(store, key, values[i]);
	    CFRelease(key);
	}
    }
    return;
}

PRIVATE_EXTERN void
my_SCDynamicStoreSetInterface(SCDynamicStoreRef store,
			      CFStringRef ifname,
			      CFStringRef entity,
			      CFDictionaryRef value)
{
    CFStringRef	key;
	
    if (S_keys_to_set == NULL) {
	S_keys_to_set
	    = CFDictionaryCreateMutable(NULL, 0,
					&kCFTypeDictionaryKeyCallBacks,
					&kCFTypeDictionaryValueCallBacks);
    }
    if (S_keys_to_remove == NULL) {
	S_keys_to_remove
	    = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	S_keys_to_remove_range.location = 0;
	S_keys_to_remove_range.length = 0;
    }
    key = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
							kSCDynamicStoreDomainState,
							ifname,
							entity);
    update_key(store, key, value);
    CFRelease(key);
    return;
}

PRIVATE_EXTERN void
my_SCDynamicStorePublish(SCDynamicStoreRef store)
{
    if (S_keys_to_remove != NULL) {
	if (CFArrayGetCount(S_keys_to_remove) == 0) {
	    my_CFRelease(&S_keys_to_remove);
	}
    }
    if (S_keys_to_set != NULL) {
	if (CFDictionaryGetCount(S_keys_to_set) == 0) {
	    my_CFRelease(&S_keys_to_set);
	}
    }
    if (S_keys_to_remove != NULL || S_keys_to_set != NULL) {
	SCDynamicStoreSetMultiple(store,
				  S_keys_to_set,
				  S_keys_to_remove,
				  NULL);
	if (G_IPConfiguration_verbose) {
	    if (S_keys_to_set != NULL) {
		my_log(~LOG_INFO,
		       "DynamicStore Publish\n%@",
		       S_keys_to_set);
	    }
	    if (S_keys_to_remove != NULL) {
		my_log(~LOG_INFO,
		       "DynamicStore Remove\n%@",
		       S_keys_to_remove);
	    }
	}
	my_CFRelease(&S_keys_to_remove);
	my_CFRelease(&S_keys_to_set);
    }
    return;
}


/**
 ** DHCP
 **/

PRIVATE_EXTERN CFDictionaryRef
DHCPInfoDictionaryCreate(ipconfig_method_t method, dhcpol_t * options_p,
			 absolute_time_t start_time,
			 absolute_time_t expiration_time)
{
    CFMutableDictionaryRef	dict;
    int				tag;

    dict = CFDictionaryCreateMutable(NULL, 0,
				     &kCFTypeDictionaryKeyCallBacks,
				     &kCFTypeDictionaryValueCallBacks);
    for (tag = 1; tag < 255; tag++) {
	CFDataRef	data;
	CFStringRef	key;
	int		len;
	void * 		option;

	if (tag == dhcptag_host_name_e 
	    && method == ipconfig_method_bootp_e) {
	}
	else if (dhcp_parameter_is_ok(tag) == FALSE) {
	    continue;
	}
	option = dhcpol_option_copy(options_p, tag, &len);
	if (option == NULL) {
	    continue;
	}
	key = CFStringCreateWithFormat(NULL, NULL, CFSTR("Option_%d"), tag);
	data = CFDataCreate(NULL, option, len);
	if (key != NULL && data != NULL) {
	    CFDictionarySetValue(dict, key, data);
	}
	my_CFRelease(&key);
	my_CFRelease(&data);
	free(option);
    }

    if (method == ipconfig_method_dhcp_e) {
	CFDateRef	date;

	/* start */
	date = CFDateCreate(NULL, (CFAbsoluteTime)start_time);
	CFDictionarySetValue(dict, CFSTR("LeaseStartTime"), date);
	CFRelease(date);

	/* expiration */
	if (expiration_time != 0) {
	    date = CFDateCreate(NULL, (CFAbsoluteTime)expiration_time);
	    CFDictionarySetValue(dict, CFSTR("LeaseExpirationTime"), date);
	    CFRelease(date);
	}
    }
    if (CFDictionaryGetCount(dict) == 0) {
	my_CFRelease(&dict);
    }
    return (dict);
}

PRIVATE_EXTERN void *
bytesFromColonHexString(CFStringRef colon_hex, int * len)
{
    CFArrayRef	arr = NULL;
    uint8_t *	bytes = NULL;
    char	hexstr[4];
    int 	i;
    int		n_bytes = 0;
 
    arr = CFStringCreateArrayBySeparatingStrings(NULL, colon_hex, CFSTR(":"));
    if (arr != NULL) {
	n_bytes = (int)CFArrayGetCount(arr);
    }
    if (n_bytes == 0) {
	goto failed;
    }
    bytes = (uint8_t *)malloc(n_bytes);
#define BASE_16		16
    for (i = 0; i < n_bytes; i++) {
	CFStringRef	str = CFArrayGetValueAtIndex(arr, i);
	my_CFStringToCStringAndLength(str, hexstr, sizeof(hexstr));
	bytes[i] = (uint8_t)strtoul(hexstr, NULL, BASE_16);
    }
    my_CFRelease(&arr);
    *len = n_bytes;
    return (bytes);

 failed:
    my_CFRelease(&arr);
    return (NULL);
}

CF_RETURNS_RETAINED PRIVATE_EXTERN CFStringRef
IPv4ARPCollisionKeyParse(CFStringRef cache_key, struct in_addr * ipaddr_p,
			 void * * hwaddr, int * hwlen)
{
    CFArrayRef			components = NULL;
    CFStringRef			ifn_cf = NULL;
    CFStringRef			ip_cf = NULL;
    CFStringRef			hwaddr_cf = NULL;

    ipaddr_p->s_addr = 0;
    *hwaddr = NULL;
    *hwlen = 0;

    /* 
     * Turn
     *   State:/Network/Interface/ifname/IPv4ARPCollision/ipaddr/hwaddr 
     * into
     *   { "State:", "Network", "Interface", ifname, "IPv4ARPCollision",
     *      ipaddr, hwaddr }
     */
    components = CFStringCreateArrayBySeparatingStrings(NULL, cache_key, 
							CFSTR("/"));
    if (components == NULL || CFArrayGetCount(components) < 7) {
	goto failed;
    }
    ifn_cf = CFArrayGetValueAtIndex(components, 3);
    ip_cf = CFArrayGetValueAtIndex(components, 5);
    hwaddr_cf = CFArrayGetValueAtIndex(components, 6);
    (void)my_CFStringToIPAddress(ip_cf, ipaddr_p);
    if (ipaddr_p->s_addr == 0) {
	goto failed;
    }
    *hwaddr = bytesFromColonHexString(hwaddr_cf, hwlen);
    CFRetain(ifn_cf);
    my_CFRelease(&components);
    return (ifn_cf);

 failed:
    my_CFRelease(&components);
    return (NULL);
}

/**
 ** DNS
 **/

static void
process_domain_name(const uint8_t * dns_domain, int dns_domain_len,
		    boolean_t search_present, CFMutableDictionaryRef dns_dict)
{
    CFMutableArrayRef	array = NULL;
    int 		i;
    const uint8_t *	name_start = NULL;
    const uint8_t *	scan;

    for (i = 0, scan = dns_domain; i < dns_domain_len; i++, scan++) {
	uint8_t		ch = *scan;

	if (ch == '\0' || isspace(ch)) {
	    if (name_start != NULL) {
		CFStringRef		str;

		if (search_present || ch == '\0') {
		    break;
		}
		str = CFStringCreateWithBytes(NULL, (UInt8 *)name_start,
					      scan - name_start,
					      kCFStringEncodingUTF8, FALSE);
		if (str == NULL) {
		    goto done;
		}
		if (array == NULL) {
		    array = CFArrayCreateMutable(NULL, 0,
						 &kCFTypeArrayCallBacks);
		}
		CFArrayAppendValue(array, str);
		CFRelease(str);
		name_start = NULL;
	    }
	}
	else if (name_start == NULL) {
	    name_start = scan;
	}
    }
    if (name_start != NULL) {
	CFStringRef		str;

	str = CFStringCreateWithBytes(NULL, (UInt8 *)name_start,
				      scan - name_start,
				      kCFStringEncodingUTF8, FALSE);
	if (str == NULL) {
	    goto done;
	}
	if (array == NULL) {
	    CFDictionarySetValue(dns_dict, 
				 kSCPropNetDNSDomainName, str);
	}
	else {
	    CFArrayAppendValue(array, str);
	}
	CFRelease(str);
    }
    if (array != NULL) {
	if (CFArrayGetCount(array) == 1) {
	    CFDictionarySetValue(dns_dict, 
				 kSCPropNetDNSDomainName,
				 CFArrayGetValueAtIndex(array, 0));
	}
	else {
	    CFDictionarySetValue(dns_dict,
				 kSCPropNetDNSSearchDomains, 
				 array);
	}
    }
 done:
    my_CFRelease(&array);
    return;
}


STATIC CFDictionaryRef
DNSEntityCreateWithDHCPInfo(dhcp_info_t * info_p)
{
    CFMutableArrayRef		array = NULL;
    CFMutableDictionaryRef	dns_dict = NULL;
    const uint8_t *		dns_domain = NULL;
    int				dns_domain_len = 0;
    struct in_addr *		dns_server = NULL;
    int				dns_server_len = 0;
    uint8_t *			dns_search = NULL;
    int				dns_search_len = 0;
    int				i;
    dhcpol_t *			options;

    if (info_p == NULL || info_p->options == NULL) {
	return (NULL);
    }
    options = info_p->options;
    if (dhcp_parameter_is_ok(dhcptag_domain_name_server_e)) {
	dns_server = (struct in_addr *)
	    dhcpol_find(options, 
			dhcptag_domain_name_server_e,
			&dns_server_len, NULL);
    }
    if (dhcp_parameter_is_ok(dhcptag_domain_name_e)) {
	dns_domain = (const uint8_t *) 
	    dhcpol_find(options, 
			dhcptag_domain_name_e,
			&dns_domain_len, NULL);
    }
    /* search can span multiple options, allocate contiguous buffer */
    if (dhcp_parameter_is_ok(dhcptag_domain_search_e)) {
	dns_search = (uint8_t *)
	    dhcpol_option_copy(options, 
			       dhcptag_domain_search_e,
			       &dns_search_len);
    }
    if (dns_server && dns_server_len >= sizeof(struct in_addr)) {
	dns_dict 
	    = CFDictionaryCreateMutable(NULL, 0,
					&kCFTypeDictionaryKeyCallBacks,
					&kCFTypeDictionaryValueCallBacks);
	array = CFArrayCreateMutable(NULL, 
				     dns_server_len / sizeof(struct in_addr),
				     &kCFTypeArrayCallBacks);
	for (i = 0; i < (dns_server_len / sizeof(struct in_addr)); i++) {
	    CFStringRef		str;
	    str = my_CFStringCreateWithIPAddress(dns_server[i]);
	    CFArrayAppendValue(array, str);
	    CFRelease(str);
	}
	CFDictionarySetValue(dns_dict, kSCPropNetDNSServerAddresses, 
			     array);
	CFRelease(array);
	
	if (dns_domain != NULL) {
	    process_domain_name(dns_domain, dns_domain_len,
				(dns_search != NULL), dns_dict);
	}
	if (dns_search != NULL) {
	    CFArrayRef	dns_search_array;

	    dns_search_array
		= DNSNameListCreateArray(dns_search, dns_search_len);
	    if (dns_search_array != NULL) {
		CFDictionarySetValue(dns_dict, kSCPropNetDNSSearchDomains,
				     dns_search_array);
		CFRelease(dns_search_array);
	    }
	}
    }
    if (dns_search != NULL) {
	free(dns_search);
    }
    return (dns_dict);
}

STATIC void
add_ipv6_addresses_to_array(CFMutableArrayRef array, 
			    const struct in6_addr * list,
			    int list_count)
{
    int				i;
    CFRange			r;
    const struct in6_addr * 	scan;

    r.location = 0;
    r.length = CFArrayGetCount(array);
    for (i = 0, scan = list; i < list_count; i++, scan++) {
	CFStringRef		ip;
	    
	ip = my_CFStringCreateWithIPv6Address(scan);
	if (CFArrayContainsValue(array, r, ip) == FALSE) {
	    CFArrayAppendValue(array, ip);
	    r.length++;
	}
	CFRelease(ip);
    }
    return;
}

STATIC void
my_CFStringArrayMerge(CFMutableArrayRef merge, CFArrayRef list)
{
    CFIndex	count;
    CFRange	r;

    count = CFArrayGetCount(list);
    r.location = 0;
    r.length = CFArrayGetCount(merge);
    for (CFIndex i = 0; i < count; i++) {
	CFStringRef	str;

	str = CFArrayGetValueAtIndex(list, i);
	if (!CFArrayContainsValue(merge, r, str)) {
	    CFArrayAppendValue(merge, str);
	    r.length++;
	}
    }
}

STATIC CFMutableArrayRef
my_DHCPv6OptionListCopyDNSInfo(DHCPv6OptionListRef options,
			       CFArrayRef * dns_search_list_p)
{
    CFMutableArrayRef		dns_server_list = NULL;
    CFArrayRef			dns_search_list = NULL;
    const struct in6_addr *	servers;
    int				servers_count = 0;
    int				servers_length;

    /* retrieve the DNS server addresses */
    servers = (const struct in6_addr *)
	DHCPv6OptionListGetOptionDataAndLength(options,
					       kDHCPv6OPTION_DNS_SERVERS,
					       &servers_length,
					       NULL);
    if (servers != NULL) {
	servers_count = servers_length / sizeof(struct in6_addr);
    }
    if (servers_count == 0) {
	goto done;
    }
    dns_server_list = CFArrayCreateMutable(NULL,
					   servers_count,
					   &kCFTypeArrayCallBacks);
    add_ipv6_addresses_to_array(dns_server_list, servers, servers_count);

    /* check for DNS search domains */
    if (DHCPv6ClientOptionIsOK(kDHCPv6OPTION_DOMAIN_LIST)) {
	const uint8_t *		search;
	int			search_length;

	search
	    = DHCPv6OptionListGetOptionDataAndLength(options,
						     kDHCPv6OPTION_DOMAIN_LIST,
						     &search_length,
						     NULL);
	if (search != NULL) {
	    dns_search_list = DNSNameListCreateArray(search, search_length);
	}
    }

 done:
    *dns_search_list_p = dns_search_list;
    return (dns_server_list);
}

STATIC CFArrayRef
my_RouterAdvertisementCopyDNSInfo(RouterAdvertisementRef ra,
				  const char * if_name,
				  CFArrayRef * dns_search_list_p)
{
    CFMutableArrayRef		dns_server_list = NULL;
    CFArrayRef			dns_search_list = NULL;
    uint32_t 			lifetime;
    CFAbsoluteTime		now;
    const struct in6_addr *	servers;
    int				servers_count;

    /* retrieve the DNS server addresses */
    servers = RouterAdvertisementGetRDNSS(ra, &servers_count, &lifetime);
    if (servers == NULL) {
	goto done;
    }
    now = CFAbsoluteTimeGetCurrent();
    if (RouterAdvertisementLifetimeHasExpired(ra, now, lifetime)) {
	if (if_name != NULL) {
	    my_log(LOG_NOTICE, "%s: RDNSS expired", if_name);
	}
	goto done;
    }
    dns_server_list = CFArrayCreateMutable(NULL,
					   servers_count,
					   &kCFTypeArrayCallBacks);
    add_ipv6_addresses_to_array(dns_server_list, servers, servers_count);

    /* check for DNS search domains */
    if (DHCPv6ClientOptionIsOK(kDHCPv6OPTION_DOMAIN_LIST)) {
	const uint8_t *		search;
	int			search_length;

	search = RouterAdvertisementGetDNSSL(ra, &search_length, &lifetime);
	if (search != NULL) {
	    if (RouterAdvertisementLifetimeHasExpired(ra, now, lifetime)) {
		if (if_name != NULL) {
		    my_log(LOG_NOTICE, "%s: DNSSL expired", if_name);
		}
	    }
	    else {
		dns_search_list
		    = DNSNameListCreateArray(search, search_length);
	    }
	}
    }

 done:
    *dns_search_list_p = dns_search_list;
    return (dns_server_list);
}

STATIC CFDictionaryRef
DNSEntityCreateWithIPv6Info(const char * if_name, ipv6_info_t * info_p)
{
    CFArrayRef			dns_servers;
    CFArrayRef			dhcp_dns_search = NULL;
    CFMutableArrayRef		dhcp_dns_servers = NULL;
    CFMutableDictionaryRef	dict = NULL;
    DHCPv6OptionListRef 	options;
    RouterAdvertisementRef	ra;
    CFArrayRef			ra_dns_search = NULL;
    CFArrayRef			ra_dns_servers = NULL;

    if (info_p == NULL) {
	goto done;
    }
    if (DHCPv6ClientOptionIsOK(kDHCPv6OPTION_DNS_SERVERS) == FALSE) {
	/* don't accept DNS server addresses from the network */
	goto done;
    }

    /* check DHCPv6 options */
    options = info_p->options;
    if (options != NULL) {
	dhcp_dns_servers
	    = my_DHCPv6OptionListCopyDNSInfo(options, &dhcp_dns_search);
    }

    /* check RA */
    ra = info_p->ra;
    if (ra != NULL) {
	ra_dns_servers
	    = my_RouterAdvertisementCopyDNSInfo(ra, if_name, &ra_dns_search);
    }

    /* no DNS servers, no DNS information */
    if (dhcp_dns_servers == NULL && ra_dns_servers == NULL) {
	goto done;
    }

    dict = CFDictionaryCreateMutable(NULL, 0,
				     &kCFTypeDictionaryKeyCallBacks,
				     &kCFTypeDictionaryValueCallBacks);

    /* DNS Servers */
    if (dhcp_dns_servers != NULL) {
	if (ra_dns_servers != NULL) {
	    /* need to merge */
	    my_CFStringArrayMerge(dhcp_dns_servers, ra_dns_servers);
	}
	dns_servers = dhcp_dns_servers;
    }
    else {
	dns_servers = ra_dns_servers;
    }
    CFDictionarySetValue(dict, kSCPropNetDNSServerAddresses, dns_servers);

    /* DNS Domain Search */
    if (dhcp_dns_search != NULL) {
	if (ra_dns_search != NULL) {
	    /* need to merge */
	    CFMutableArrayRef	merge;

	    merge = CFArrayCreateMutableCopy(NULL, 0, dhcp_dns_search);
	    my_CFStringArrayMerge(merge, ra_dns_search);
	    CFDictionarySetValue(dict, kSCPropNetDNSSearchDomains, merge);
	    CFRelease(merge);
	}
	else {
	    /* no merge necessary */
	    CFDictionarySetValue(dict, kSCPropNetDNSSearchDomains,
				 dhcp_dns_search);
	}
    }
    else if (ra_dns_search != NULL) {
	/* no merge required, just use DNSSL */
	CFDictionarySetValue(dict, kSCPropNetDNSSearchDomains, ra_dns_search);
    }
    my_CFRelease(&dhcp_dns_search);
    my_CFRelease(&dhcp_dns_servers);
    my_CFRelease(&ra_dns_search);
    my_CFRelease(&ra_dns_servers);

 done:
    return (dict);
}

STATIC void
merge_dict_arrays(CFMutableDictionaryRef dict, CFDictionaryRef one,
		  CFDictionaryRef two, CFStringRef prop)
{
    CFArrayRef		array_one;
    CFArrayRef		array_two;
    CFIndex		count_two;
    int			i;
    CFMutableArrayRef	merged;
    CFRange		range_one;

    array_one = CFDictionaryGetValue(one, prop);
    array_two = CFDictionaryGetValue(two, prop);
    if (array_one == NULL && array_two == NULL) {
	return;
    }
    if (array_one == NULL || array_two == NULL) {
	if (array_one != NULL) {
	    CFDictionarySetValue(dict, prop, array_one);
	}
	else {
	    CFDictionarySetValue(dict, prop, array_two);
	}
	return;
    }
    range_one = CFRangeMake(0, CFArrayGetCount(array_one));
    merged = CFArrayCreateMutableCopy(NULL, 0, array_one);
    count_two = CFArrayGetCount(array_two);
    for (i = 0; i < count_two; i++) {
	CFTypeRef	val = CFArrayGetValueAtIndex(array_two, i);

	if (CFArrayContainsValue(array_one, range_one, val)) {
	    continue;
	}
	CFArrayAppendValue(merged, val);
    }
    CFDictionarySetValue(dict, prop, merged);
    CFRelease(merged);
    return;
}

PRIVATE_EXTERN CFDictionaryRef
DNSEntityCreateWithInfo(const char * if_name,
			dhcp_info_t * info_p,
			ipv6_info_t * info_v6_p)
 {
    CFMutableDictionaryRef	dict;
    CFDictionaryRef		dnsv4;
    CFDictionaryRef		dnsv6;

    dnsv4 = DNSEntityCreateWithDHCPInfo(info_p);
    dnsv6 = DNSEntityCreateWithIPv6Info(if_name, info_v6_p);
    if (dnsv4 == NULL && dnsv6 == NULL) {
	return (NULL);
    }
    if (dnsv4 == NULL || dnsv6 == NULL) {
	if (dnsv4 != NULL) {
	    return (dnsv4);
	}
	else {
	    return (dnsv6);
	}
    }
    
    /* we need to merge */
    dict = CFDictionaryCreateMutableCopy(NULL, 0, dnsv4);
    merge_dict_arrays(dict, dnsv4, dnsv6, kSCPropNetDNSServerAddresses);
    merge_dict_arrays(dict, dnsv4, dnsv6, kSCPropNetDNSSearchDomains);
    
    my_CFRelease(&dnsv4);
    my_CFRelease(&dnsv6);
    return (dict);
}


/**
 ** DHCPv6
 **/

PRIVATE_EXTERN CFDictionaryRef
DHCPv6InfoDictionaryCreate(DHCPv6OptionListRef options)
{
    int 			count = DHCPv6OptionListGetCount(options);
    CFMutableDictionaryRef	dict;
    int				i;

    if (count == 0) {
	return (NULL);
    }
    dict = CFDictionaryCreateMutable(NULL, 0,
				     &kCFTypeDictionaryKeyCallBacks,
				     &kCFTypeDictionaryValueCallBacks);
    for (i = 0; i < count; i++) {
	CFMutableArrayRef	array;
	CFDataRef		data;
	CFStringRef		key;
	DHCPv6OptionRef		option;
	int			option_code;
	int			option_len;
	const uint8_t *		option_data;

	option = DHCPv6OptionListGetOptionAtIndex(options, i);
	option_code = DHCPv6OptionGetCode(option);
	if (DHCPv6ClientOptionIsOK(option_code) == FALSE) {
	    continue;
	}
	option_len = DHCPv6OptionGetLength(option);
	option_data = DHCPv6OptionGetData(option);
	data = CFDataCreate(NULL, option_data, option_len);
	key = CFStringCreateWithFormat(NULL, NULL, CFSTR("Option_%d"),
				       option_code);
	array = (CFMutableArrayRef)CFDictionaryGetValue(dict, key);
	if (array == NULL) {
	    /* allocate an array to hold the value(s) */
	    array = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	    CFDictionarySetValue(dict, key, array);
	    CFRelease(array);
	}
	CFArrayAppendValue(array, data);
	CFRelease(key);
	CFRelease(data);
    }
    if (CFDictionaryGetCount(dict) == 0) {
	my_CFRelease(&dict);
    }
    return (dict);
}


/**
 ** Captive Portal
 **/

#ifndef kSCPropNetCaptivePortalURL

PRIVATE_EXTERN CFDictionaryRef
CaptivePortalEntityCreateWithInfo(dhcp_info_t * info_p,
				  ipv6_info_t * info_v6_p)
{
#pragma unused(info_p)
#pragma unused(info_v6_p)
    return (NULL);
}

#else /* kSCPropNetCaptivePortalURL */

STATIC CFDictionaryRef
dict_create(CFStringRef key, CFStringRef val)
{
    return (CFDictionaryCreate(NULL,
			       (const void * *)&key,
			       (const void * *)&val,
			       1,
			       &kCFTypeDictionaryKeyCallBacks,
			       &kCFTypeDictionaryValueCallBacks));
}

STATIC CFStringRef
CaptivePortalCopyWithDHCPInfo(dhcp_info_t * info_p)
{
    dhcpol_t *			options;
    const uint8_t *		url = NULL;
    int				url_len = 0;
    CFStringRef			url_string = NULL;

    if (info_p == NULL) {
	goto done;
    }
    if (!dhcp_parameter_is_ok(dhcptag_captive_portal_url_e)) {
	goto done;
    }
    options = info_p->options;
    if (options == NULL) {
	goto done;
    }
    url = (const uint8_t *)
	dhcpol_find(options, dhcptag_captive_portal_url_e, &url_len, NULL);
    url_string = my_CFStringCreateWithBytes(url, url_len);

 done:
    return (url_string);
}

STATIC CFStringRef
CaptivePortalCopyWithIPv6Info(ipv6_info_t * info_p)
{
    CFStringRef		dhcp_url = NULL;
    DHCPv6OptionListRef options;
    CFStringRef		ra_url = NULL;
    CFStringRef		ret_url = NULL;
    const uint8_t *	url;
    int			url_len;

    if (info_p == NULL) {
	goto done;
    }
    if (!DHCPv6ClientOptionIsOK(kDHCPv6OPTION_CAPTIVE_PORTAL_URL)) {
	/* don't accept Captive Portal URL from the network */
	goto done;
    }

    /* check DHCPv6 options */
    options = info_p->options;
    if (options != NULL) {
	url = DHCPv6OptionListGetOptionDataAndLength(options,
						     kDHCPv6OPTION_CAPTIVE_PORTAL_URL,
						     &url_len,
						     NULL);
	dhcp_url = my_CFStringCreateWithBytes(url, url_len);
    }

    /* check for RA value */
    if (info_p->ra != NULL) {
	ra_url = RouterAdvertisementCopyCaptivePortal(info_p->ra);
    }

    if (dhcp_url != NULL) {
	if (ra_url != NULL) {
	    /* both DHCP and RA specify captive URL, complain if different */
	    if (CFStringCompare(dhcp_url, ra_url, kCFCompareCaseInsensitive)
		!= kCFCompareEqualTo) {
		my_log(LOG_ERR,
		       "Mismatch in Captive Portal URLs: DHCPv6=%@ RA=%@",
		       dhcp_url, ra_url);
	    }
	    my_CFRelease(&ra_url);
	}
	/* prefer DHCP value */
	ret_url = dhcp_url;
    }
    else if (ra_url != NULL) {
	ret_url = ra_url;
    }

 done:
    return (ret_url);
}

PRIVATE_EXTERN CFDictionaryRef
CaptivePortalEntityCreateWithInfo(dhcp_info_t * info_p,
				  ipv6_info_t * info_v6_p)
{
    CFStringRef		dhcp_url;
    CFDictionaryRef	dict = NULL;
    CFStringRef 	v6_url;

    dhcp_url = CaptivePortalCopyWithDHCPInfo(info_p);
    v6_url = CaptivePortalCopyWithIPv6Info(info_v6_p);
    if (dhcp_url != NULL) {
	if (v6_url != NULL) {
	    /* both DHCP and V6 have a URL, complain if different */
	    if (CFStringCompare(dhcp_url, v6_url, kCFCompareCaseInsensitive)
		!= kCFCompareEqualTo) {
		my_log(LOG_ERR,
		       "Mismatch in Captive Portal URLs: IPv4=%@ IPv6=%@",
		       dhcp_url, v6_url);
	    }
	}
	/* prefer DHCP value */
	dict = dict_create(kSCPropNetCaptivePortalURL, dhcp_url);
    }
    else if (v6_url != NULL) {
	dict = dict_create(kSCPropNetCaptivePortalURL, v6_url);
    }
    my_CFRelease(&dhcp_url);
    my_CFRelease(&v6_url);
    return (dict);
}

#endif /* kSCPropNetCaptivePortalURL */
