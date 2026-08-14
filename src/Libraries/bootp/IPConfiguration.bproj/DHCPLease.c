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
 * DHCPLease.c
 * - routines to handle the DHCP in-memory lease list and the lease
 *   stored in the filesystem
 */

/* 
 * Modification History
 *
 * June 11, 2009		Dieter Siegmund (dieter@apple.com)
 * - split out from ipconfigd.c
 */

#include <CoreFoundation/CFString.h>
#include <SystemConfiguration/SCValidation.h>
#include <SystemConfiguration/SCPrivate.h>
#include "DHCPLease.h"
#include "util.h"
#include "host_identifier.h"
#include "globals.h"
#include "cfutil.h"
#include "dhcp_thread.h"
#include "cfutil.h"

#define DHCPCLIENT_LEASE_FILE_FMT	DHCPCLIENT_LEASES_DIR "/%s.plist"

/* required properties: */
#define kLeaseStartDate			CFSTR("LeaseStartDate")
#define kPacketData			CFSTR("PacketData")
#define kRouterHardwareAddress		CFSTR("RouterHardwareAddress")
#define kSSID				CFSTR("SSID") /* wi-fi only */
#define kClientIdentifier		CFSTR("ClientIdentifier")

/* informative properties: */
#define kLeaseLength			CFSTR("LeaseLength")
#define kIPAddress			CFSTR("IPAddress")
#define kRouterIPAddress		CFSTR("RouterIPAddress")

static Boolean
client_id_data_matches(CFDataRef client_id,
		       uint8_t cid_type, const void * cid, int cid_length)
{
    const UInt8 *	bytes;
    CFIndex		len;
    Boolean		match = FALSE;

    if (cid_length == 0) {
	goto done;
    }
    if (isA_CFData(client_id) == NULL) {
	goto done;
    }
    len = CFDataGetLength(client_id);
    if (len != (cid_length + 1)) {
	goto done;
    }
    bytes = CFDataGetBytePtr(client_id);
    if (*bytes != cid_type) {
	goto done;
    }
    if (bcmp(bytes + 1, cid, cid_length) == 0) {
	match = TRUE;
    }

 done:
    return (match);
}

static CFDataRef
client_id_data_create(uint8_t cid_type, const void * cid, int cid_length)
{
    CFMutableDataRef	client_id_data;

    client_id_data = CFDataCreateMutable(NULL, cid_length + 1);
    CFDataAppendBytes(client_id_data, &cid_type, 1);
    CFDataAppendBytes(client_id_data, cid, cid_length);
    return (client_id_data);
}

/*
 * Function: DHCPLeaseCreateWithDictionary
 * Purpose:
 *   Instantiate a new DHCPLease structure corresponding to the given
 *   dictionary.  Validates that required properties are present,
 *   returns NULL if those checks fail.
 */
static DHCPLeaseRef
DHCPLeaseCreateWithDictionary(CFDictionaryRef dict,
			      uint8_t cid_type, const void * cid,
			      int cid_length, bool is_wifi)
{
    CFDataRef			client_id_data;
    CFDataRef			hwaddr_data;
    dhcp_lease_time_t		lease_time;
    DHCPLeaseRef		lease_p;
    dhcpol_t			options;
    CFDataRef			pkt_data;
    CFRange			pkt_data_range;
    CFStringRef			ssid = NULL;
    CFDateRef			start_date;
    dhcp_lease_time_t		t1_time;
    dhcp_lease_time_t		t2_time;


    /* get the client identifier */
    client_id_data = CFDictionaryGetValue(dict, kClientIdentifier);
    if (!client_id_data_matches(client_id_data, cid_type, cid, cid_length)) {
	goto failed;
    }

    /* get the lease start time */
    start_date = CFDictionaryGetValue(dict, kLeaseStartDate);
    if (isA_CFDate(start_date) == NULL) {
	goto failed;
    }
    /* get the packet data */
    pkt_data = CFDictionaryGetValue(dict, kPacketData);
    if (isA_CFData(pkt_data) == NULL) {
	goto failed;
    }
    /* if Wi-Fi, get the SSID */
    if (is_wifi) {
	ssid = CFDictionaryGetValue(dict, kSSID);
	if (isA_CFString(ssid) == NULL) {
	    goto failed;
	}
    }

    pkt_data_range.location = 0;
    pkt_data_range.length = CFDataGetLength(pkt_data);
    if (pkt_data_range.length < sizeof(struct dhcp)) {
	goto failed;
    }
    lease_p = (DHCPLeaseRef)
	malloc(offsetof(DHCPLease, pkt) + pkt_data_range.length);
    bzero(lease_p, offsetof(DHCPLease, pkt));

    /* copy the packet data */
    CFDataGetBytes(pkt_data, pkt_data_range, lease_p->pkt);
    lease_p->pkt_length = (int)pkt_data_range.length;

    /* get the lease information and router IP address */
    lease_p->lease_start = (absolute_time_t)CFDateGetAbsoluteTime(start_date);
    /* parse/retrieve options */
    (void)dhcpol_parse_packet(&options, (void *)lease_p->pkt,
			      (int)pkt_data_range.length, NULL);
    dhcp_get_lease_from_options(&options, &lease_time, &t1_time, &t2_time);
    lease_p->lease_length = lease_time;

    /* get the IP address */
    /* ALIGN: lease_p->pkt is aligned, cast ok. */
    lease_p->our_ip = ((struct dhcp *)(void *)lease_p->pkt)->dp_yiaddr;

    /* get the router information */
    if (dhcp_get_router_address(&options, lease_p->our_ip,
				&lease_p->router_ip)) {
	CFRange		hwaddr_range;

	/* get the router hardware address */
	hwaddr_data = CFDictionaryGetValue(dict, kRouterHardwareAddress);
	hwaddr_range.length = 0;
	if (isA_CFData(hwaddr_data) != NULL) {
	    hwaddr_range.length = CFDataGetLength(hwaddr_data);
	}
	if (hwaddr_range.length > 0) {
	    hwaddr_range.location = 0;
	    if (hwaddr_range.length > sizeof(lease_p->router_hwaddr)) {
		hwaddr_range.length = sizeof(lease_p->router_hwaddr);
	    }
	    lease_p->router_hwaddr_length = (int)hwaddr_range.length;
	    CFDataGetBytes(hwaddr_data, hwaddr_range, lease_p->router_hwaddr);
	}
    }
    dhcpol_free(&options);
    if (ssid != NULL) {
	CFRetain(ssid);
	lease_p->ssid = ssid;
    }
    return (lease_p);

 failed:
    return (NULL);
}

static void
DHCPLeaseDeallocate(void * arg)
{
    DHCPLeaseRef lease_p = (DHCPLeaseRef)arg;

    if (lease_p->ssid != NULL) {
	CFRelease(lease_p->ssid);
    }
    free(lease_p);
    return;
}

void
DHCPLeaseSetNAK(DHCPLeaseRef lease_p, int nak)
{
    lease_p->nak = nak;
    return;
}

/*
 * Function: DHCPLeaseCreate
 * Purpose:
 *   Instantiate a new DHCPLease structure corresponding to the given
 *   information.
 */
static DHCPLeaseRef
DHCPLeaseCreate(struct in_addr our_ip, struct in_addr router_ip,
		const uint8_t * router_hwaddr, int router_hwaddr_length,
		absolute_time_t lease_start, 
		dhcp_lease_time_t lease_length,
		const uint8_t * pkt, int pkt_size,
		CFStringRef ssid)
{
    DHCPLeaseRef		lease_p = NULL;

    lease_p = (DHCPLeaseRef)
	malloc(offsetof(DHCPLease, pkt) + pkt_size);
    bzero(lease_p, offsetof(DHCPLease, pkt));
    lease_p->our_ip = our_ip;
    lease_p->router_ip = router_ip;
    lease_p->lease_start = lease_start;
    lease_p->lease_length = lease_length;
    bcopy(pkt, lease_p->pkt, pkt_size);
    lease_p->pkt_length = pkt_size;
    if (router_hwaddr != NULL && router_hwaddr_length != 0) {
	if (router_hwaddr_length > sizeof(lease_p->router_hwaddr)) {
	    router_hwaddr_length = sizeof(lease_p->router_hwaddr);
	}
	lease_p->router_hwaddr_length = router_hwaddr_length;
	bcopy(router_hwaddr, lease_p->router_hwaddr, router_hwaddr_length);
    }
    if (ssid != NULL) {
	CFRetain(ssid);
	lease_p->ssid = ssid;
    }
    return (lease_p);
}

static CFDictionaryRef
DHCPLeaseCopyDictionary(DHCPLeaseRef lease_p,
			uint8_t cid_type, const void * cid, int cid_length)
{
    CFDataRef			client_id_data;
    CFDataRef			data;
    CFDateRef			date;
    CFMutableDictionaryRef	dict;
    CFNumberRef			num;
    CFStringRef			str;

    dict = CFDictionaryCreateMutable(NULL, 0,
				     &kCFTypeDictionaryKeyCallBacks,
				     &kCFTypeDictionaryValueCallBacks);
    /* set the Client Identifier */
    client_id_data = client_id_data_create(cid_type, cid, cid_length);
    CFDictionarySetValue(dict, kClientIdentifier, client_id_data);
    CFRelease(client_id_data);

    /* set the IP address */
    str = CFStringCreateWithFormat(NULL, NULL, CFSTR(IP_FORMAT),
				   IP_LIST(&lease_p->our_ip));
    CFDictionarySetValue(dict, kIPAddress, str);
    CFRelease(str);

    /* set the lease start date */
    date = CFDateCreate(NULL, lease_p->lease_start);
    CFDictionarySetValue(dict, kLeaseStartDate, date);
    CFRelease(date);

    /* set the lease length */
    num = CFNumberCreate(NULL, kCFNumberSInt32Type, &lease_p->lease_length);
    CFDictionarySetValue(dict, kLeaseLength, num);
    CFRelease(num);

    /* set the SSID */
    if (lease_p->ssid != NULL) {
	CFDictionarySetValue(dict, kSSID, lease_p->ssid);
    }

    /* set the packet data */
    data = CFDataCreateWithBytesNoCopy(NULL, lease_p->pkt, lease_p->pkt_length,
				       kCFAllocatorNull);
    CFDictionarySetValue(dict, kPacketData, data);
    CFRelease(data);

    if (lease_p->router_ip.s_addr != 0) {
	/* set the router IP address */
	str = CFStringCreateWithFormat(NULL, NULL, CFSTR(IP_FORMAT),
				       IP_LIST(&lease_p->router_ip));
	CFDictionarySetValue(dict, kRouterIPAddress, str);
	CFRelease(str);

	if (lease_p->router_hwaddr_length > 0) {
	    /* set the router hardware address */
	    data = CFDataCreateWithBytesNoCopy(NULL, lease_p->router_hwaddr,
					       lease_p->router_hwaddr_length,
					       kCFAllocatorNull);
	    CFDictionarySetValue(dict, kRouterHardwareAddress, data);
	    CFRelease(data);
	}
    }
    return (dict);
}

static void
DHCPLeasePrintToString(CFMutableStringRef str, DHCPLeaseRef lease_p)
{
    STRING_APPEND(str, "IP " IP_FORMAT " Start %d Length", 
		  IP_LIST(&lease_p->our_ip), (int)lease_p->lease_start);
    if (lease_p->lease_length == DHCP_INFINITE_LEASE) {
	STRING_APPEND(str, " infinite");
    }
    else {
	STRING_APPEND(str, " %d", (int)lease_p->lease_length);
    }
    
    if (lease_p->router_ip.s_addr != 0) {
	STRING_APPEND(str, " Router IP " IP_FORMAT,
		      IP_LIST(&lease_p->router_ip));
	if (lease_p->router_hwaddr_length > 0) {
	    char	link_string[MAX_LINK_ADDR_LEN * 3];

	    link_addr_to_string(link_string, sizeof(link_string),
				lease_p->router_hwaddr,
				lease_p->router_hwaddr_length);
	    STRING_APPEND(str, " MAC %s", link_string);
	}
    }
    if (lease_p->ssid != NULL) {
	STRING_APPEND(str, " SSID '%@'", lease_p->ssid);
    }
    return;
}

static void
DHCPLeaseListLog(DHCPLeaseListRef list_p)
{
    int			count;
    int			i;
    CFMutableStringRef	str;
	
    str = CFStringCreateMutable(NULL, 0);
    count = dynarray_count(list_p);
    for (i = 0; i < count; i++) {
	DHCPLeaseRef	lease_p = dynarray_element(list_p, i);

	STRING_APPEND(str, "\n%d. ", i + 1);
	DHCPLeasePrintToString(str, lease_p);
    }
    my_log(~LOG_DEBUG, "DHCPLeaseList has %d element(s)%@", count, str);
    CFRelease(str);
    return;
}

static void
DHCPLeaseListGetPath(const char * ifname,
		     char * filename, int filename_size)
{
    snprintf(filename, filename_size, DHCPCLIENT_LEASE_FILE_FMT, ifname);
    return;
}

void
DHCPLeaseListInit(DHCPLeaseListRef list_p)
{
    dynarray_init(list_p, DHCPLeaseDeallocate, NULL);
    return;
}

void
DHCPLeaseListFree(DHCPLeaseListRef list_p)
{
    dynarray_free(list_p);
}

/*
 * Function: DHCPLeaseListRemoveStaleLeases
 * Purpose:
 *   Scans the list of leases removing any that are no longer valid.
 */
static void
DHCPLeaseListRemoveStaleLeases(DHCPLeaseListRef list_p)
{
    int				count;
    absolute_time_t 		current_time;
    int				i;

    count = dynarray_count(list_p);
    if (count == 0) {
	return;
    }
    current_time = timer_current_secs();
    i = 0;
    while (i < count) {
	DHCPLeaseRef	lease_p = dynarray_element(list_p, i);

	/* check the lease expiration */
	if (lease_p->lease_length != DHCP_INFINITE_LEASE
	    && current_time >= (lease_p->lease_start + lease_p->lease_length)) {
	    /* lease is expired */
	    my_log(LOG_INFO, "Removing Stale Lease "
		   IP_FORMAT " Router " IP_FORMAT,
		   IP_LIST(&lease_p->our_ip),
		   IP_LIST(&lease_p->router_ip));
	    dynarray_free_element(list_p, i);
	    count--;
	}
	else {
	    i++;
	}
    }
    return;
}

/* 
 * Function: DHCPLeaseListRead
 *
 * Purpose:
 *   Read the single DHCP lease for the given interface/client_id into a
 *   DHCPLeaseList structure.  This lease is marked as "tentative" because
 *   we have no idea whether the lease is still good or not, since another
 *   version of the OS (or another OS) could have had additional communication
 *   with the DHCP server, invalidating our notion of the lease.  It also
 *   affords a simple, self-cleansing mechanism to clear out the set of
 *   leases we keep track of.
 */
void
DHCPLeaseListRead(DHCPLeaseListRef list_p,
		  const char * ifname, bool is_wifi,
		  uint8_t cid_type, const void * cid, int cid_length)
{
    char			filename[PATH_MAX];
    CFDictionaryRef		lease_dict = NULL;
    DHCPLeaseRef		lease_p;
    struct in_addr		lease_ip;

    DHCPLeaseListInit(list_p);
    DHCPLeaseListGetPath(ifname, filename, sizeof(filename));
    lease_dict = my_CFPropertyListCreateFromFile(filename);
    if (isA_CFDictionary(lease_dict) == NULL) {
	goto done;
    }
    lease_p = DHCPLeaseCreateWithDictionary(lease_dict,
					    cid_type, cid, cid_length,
					    is_wifi);
    if (lease_p == NULL) {
	goto done;
    }
    lease_p->tentative = TRUE;
    dynarray_add(list_p, lease_p);
    if (G_IPConfiguration_verbose) {
	DHCPLeaseListLog(list_p);
    }
    lease_ip = lease_p->our_ip;
    DHCPLeaseListRemoveStaleLeases(list_p);
    if (DHCPLeaseListCount(list_p) == 0) {
	remove_unused_ip(ifname, lease_ip);
    }

 done:
    my_CFRelease(&lease_dict);
    return;
}

/* 
 * Function: DHCPLeaseListWrite
 *
 * Purpose:
 *   Write the last DHCP lease in the list for the given interface/client_id.
 *   We only save the last (current) lease.  See the comments for 
 *   DHCPLeaseListRead above for more information.
 */
void
DHCPLeaseListWrite(DHCPLeaseListRef list_p,
		   const char * ifname,
		   uint8_t cid_type, const void * cid, int cid_length)
{
    int			count;
    char		filename[PATH_MAX];
    CFDictionaryRef	lease_dict;
    DHCPLeaseRef	lease_p;
    
    DHCPLeaseListGetPath(ifname, filename, sizeof(filename));
    DHCPLeaseListRemoveStaleLeases(list_p);
    count = dynarray_count(list_p);
    if (count == 0) {
	unlink(filename);
	return;
    }
    lease_p = dynarray_element(list_p, count - 1);
    lease_dict = DHCPLeaseCopyDictionary(lease_p, cid_type, cid, cid_length);
    if (my_CFPropertyListWriteFile(lease_dict, filename, 0644) < 0) {
	/*
	 * An ENOENT error is expected on a read-only filesystem.  All 
	 * other errors should be reported.
	 */
	if (errno != ENOENT) {
	    my_log(LOG_NOTICE, "my_CFPropertyListWriteFile(%s) failed, %s",
		   filename, strerror(errno));
	}
    }
    my_CFRelease(&lease_dict);
    return;
}

/*
 * Function: DHCPLeaseListCopyARPAddressInfo
 * Purpose:
 *   Returns a list of arp_address_info_t's corresponding to each
 *   discoverable lease.
 */
arp_address_info_t *
DHCPLeaseListCopyARPAddressInfo(DHCPLeaseListRef list_p,
				CFStringRef ssid,
				absolute_time_t * start_time_threshold_p,
				bool tentative_ok,
				int * ret_count)
{
    int				arp_info_count;
    arp_address_info_t *	arp_info_p;
    int				count;
    int				i;
    arp_address_info_t *	info_p;

    DHCPLeaseListRemoveStaleLeases(list_p);
    count = dynarray_count(list_p);
    if (count == 0) {
	*ret_count = 0;
	return (NULL);
    }
    arp_info_p = (arp_address_info_t *)malloc(sizeof(*arp_info_p) * count);
    arp_info_count = 0;
    info_p = arp_info_p;
    for (i = 0; i < count; i++) {
	DHCPLeaseRef	lease_p = dynarray_element(list_p, i);

	if (ssid != NULL) {
	    if (lease_p->ssid == NULL || !CFEqual(lease_p->ssid, ssid)) {
		my_log(LOG_INFO,
		       "ignoring lease with SSID %@",
		       lease_p->ssid);
		continue;
	    }
	    
	}
	if (lease_p->router_ip.s_addr == 0
	    || lease_p->router_hwaddr_length == 0) {
	    /* can't use this with ARP discovery */
	    my_log(LOG_INFO, "ignoring lease for " IP_FORMAT,
		   IP_LIST(&lease_p->our_ip));
	    continue;
	}
	if (lease_p->tentative && tentative_ok == FALSE) {
	    /* ignore tentative lease */
	    continue;
	}
	if (start_time_threshold_p != NULL
	    && lease_p->lease_start < *start_time_threshold_p) {
	    my_log(LOG_INFO,
		   "start time on lease " IP_FORMAT " too old (%ld < %ld)",
		   IP_LIST(&lease_p->our_ip),
		   lease_p->lease_start, *start_time_threshold_p);
	    continue;
	}
	info_p->sender_ip = lease_p->our_ip;
	info_p->target_ip = lease_p->router_ip;
	bcopy(lease_p->router_hwaddr, info_p->target_hardware,
	      lease_p->router_hwaddr_length);
	arp_info_count++;
	info_p++;
    }
    if (arp_info_count == 0) {
	free(arp_info_p);
	arp_info_p = NULL;
    }
    *ret_count = arp_info_count;
    return (arp_info_p);
}

/*
 * Function: DHCPLeaseListFindLease
 * Purpose:
 *   Find a lease corresponding to the supplied information.
 */
int
DHCPLeaseListFindLease(DHCPLeaseListRef list_p, struct in_addr our_ip,
		       struct in_addr router_ip,
		       const uint8_t * router_hwaddr, int router_hwaddr_length)
{
    int			count;
    int			i;
    bool		private_ip = ip_is_private(our_ip);

    count = dynarray_count(list_p);
    for (i = 0; i < count; i++) {
	DHCPLeaseRef	lease_p = dynarray_element(list_p, i);

	if (lease_p->our_ip.s_addr != our_ip.s_addr) {
	    /* IP doesn't match */
	    continue;
	}
	if (private_ip == FALSE) {
	    /* lease for public IP is unique */
	    return (i);
	}
	if (lease_p->router_ip.s_addr != router_ip.s_addr) {
	    /* router IP doesn't match (or one is set the other isn't)*/
	    continue;
	}
	if (router_ip.s_addr == 0) {
	    /* found lease with no router information */
	    return (i);
	}
	if (lease_p->router_hwaddr_length != router_hwaddr_length) {
	    /* one has router hwaddr, other doesn't */
	    continue;
	}
	if (router_hwaddr == NULL || router_hwaddr_length == 0) {
	    /* found lease with router IP but no router hwaddr */
	    return (i);
	}
	if (bcmp(lease_p->router_hwaddr, router_hwaddr, router_hwaddr_length)
	    == 0) {
	    /* exact match on IP, router IP, router hwaddr */
	    return (i);
	}
    }
    return (DHCP_LEASE_NOT_FOUND);
}

/*
 * Function: DHCPLeaseListFindLeaseWithSSID
 * Purpose:
 *   Find the most recently used lease for the given SSID. Traverses the
 *   list in reverse order to grab the most recently used lease.
 */
int
DHCPLeaseListFindLeaseWithSSID(DHCPLeaseListRef list_p, CFStringRef ssid)
{
    int			i;

    for (i = dynarray_count(list_p) - 1; i >= 0; i--) {
	DHCPLeaseRef	lease_p = dynarray_element(list_p, i);

	if (lease_p->ssid == NULL) {
	    continue;
	}
	if (CFEqual(lease_p->ssid, ssid)) {
	    return (i);
	}
    }
    return (DHCP_LEASE_NOT_FOUND);
}

/*
 * Function: DHCPLeaseListRemoveLeaseWithSSID
 * Purpose:
 *   Find all leases with the given SSID, and remove them.
 */
void
DHCPLeaseListRemoveLeaseWithSSID(DHCPLeaseListRef list_p, CFStringRef ssid)
{
    while (1) {
	DHCPLeaseRef	lease_p;
	int		where;

	where = DHCPLeaseListFindLeaseWithSSID(list_p, ssid);
	if (where == DHCP_LEASE_NOT_FOUND) {
	    break;
	}
	lease_p = dynarray_element(list_p, where);
	my_log(LOG_NOTICE, "Removing Lease SSID %@ "
	       IP_FORMAT " Router " IP_FORMAT,
	       ssid,
	       IP_LIST(&lease_p->our_ip),
	       IP_LIST(&lease_p->router_ip));
	dynarray_free_element(list_p, where);
    }
    return;
}

/*
 * Function: DHCPLeaseShouldBeRemoved
 * Purpose:
 *   Given an existing lease entry 'existing_p' and the one to be added 'new_p',
 *   determine whether the existing lease entry should be removed.
 *   
 *   The criteria for removing the lease entry:
 *   1) No router information is specified.  This entry is useless for lease
 *      detection.   If such a lease exists, it only makes sense to have
 *      one of them, the most recently used lease.  It will be added to the
 *      end of the list in that case.
 *   2) Lease was NAK'd.  The DHCP server NAK'd this lease and it's for the
 *      same network i.e. same router MAC.
 *   3) Lease is for a public IP and is the same as the new lease.
 *   4) Lease has the same router IP/MAC address as an existing lease.  We
 *      can only sensibly have a single lease for a particular network, so
 *      eliminate redundant ones.
 */
static boolean_t
DHCPLeaseShouldBeRemoved(DHCPLeaseRef existing_p, DHCPLeaseRef new_p, 
			 boolean_t private_ip)
{
    if (existing_p->router_ip.s_addr == 0
	|| existing_p->router_hwaddr_length == 0) {
	my_log(LOG_INFO,
	       "Removing lease with no router for IP address "
	       IP_FORMAT, IP_LIST(&existing_p->our_ip));
	return (TRUE);
    }
    if (existing_p->nak) {
	boolean_t		ignore = FALSE;

	existing_p->nak = FALSE;
	if (ip_is_private(existing_p->our_ip) != private_ip) {
	    /* one IP is private, the other is public, ignore NAK */
	    ignore = TRUE;
	}
	else if (new_p->router_hwaddr_length != 0
		 && bcmp(existing_p->router_hwaddr, new_p->router_hwaddr,
			 existing_p->router_hwaddr_length) != 0) {
	    /* router MAC on NAK'd lease is different, so ignore NAK */
	    ignore = TRUE;
	}
	if (ignore) {
	    my_log(LOG_INFO, "Ignoring NAK on IP address "
		       IP_FORMAT, IP_LIST(&existing_p->our_ip));
	}
	else {
	    my_log(LOG_INFO, "Removing NAK'd lease for IP address "
		   IP_FORMAT, IP_LIST(&existing_p->our_ip));
	    return (TRUE);
	}
    }
    if (private_ip == FALSE
	&& new_p->our_ip.s_addr == existing_p->our_ip.s_addr) {
	/* public IP's are the same, remove it */
	my_log(LOG_INFO, "Removing lease for public IP address "
	       IP_FORMAT, IP_LIST(&existing_p->our_ip));
	return (TRUE);
    }
    if (new_p->router_ip.s_addr == 0
	|| new_p->router_hwaddr_length == 0) {
	/* new lease doesn't have a router */
	return (FALSE);
    }
    if (bcmp(new_p->router_hwaddr, existing_p->router_hwaddr, 
	     new_p->router_hwaddr_length) == 0) {
	if (new_p->our_ip.s_addr == existing_p->our_ip.s_addr) {
	    my_log(LOG_INFO,
		   "Removing lease with same router for IP address "
		   IP_FORMAT, IP_LIST(&existing_p->our_ip));
	}
	else {
	    my_log(LOG_INFO,
		   "Removing lease with same router, old IP "
		   IP_FORMAT " new IP " IP_FORMAT,
		   IP_LIST(&existing_p->our_ip),
		   IP_LIST(&new_p->our_ip));
	}
	return (TRUE);
    }
    return (FALSE);
}

/* 
 * Function: DHCPLeaseListUpdateLease
 *
 * Purpose:
 *   Update the lease entry for the given lease in the in-memory lease database.
 */
void
DHCPLeaseListUpdateLease(DHCPLeaseListRef list_p, struct in_addr our_ip,
			 struct in_addr router_ip,
			 const uint8_t * router_hwaddr,
			 int router_hwaddr_length,
			 absolute_time_t lease_start,
			 dhcp_lease_time_t lease_length,
			 const uint8_t * pkt, int pkt_size,
			 CFStringRef ssid)
{
    int			count;
    int			i;
    DHCPLeaseRef	lease_p;
    boolean_t		private_ip = ip_is_private(our_ip);

    lease_p = DHCPLeaseCreate(our_ip, router_ip,
			      router_hwaddr, router_hwaddr_length,
			      lease_start, lease_length, pkt, pkt_size,
			      ssid);
    /* scan lease list to eliminate NAK'd, incomplete, and duplicate leases */
    count = dynarray_count(list_p);
    for (i = 0; i < count; i++) {
	DHCPLeaseRef	scan_p = dynarray_element(list_p, i);

	if (DHCPLeaseShouldBeRemoved(scan_p, lease_p, private_ip)) {
	    dynarray_free_element(list_p, i);
	    i--;
	    count--;
	}
    }
    dynarray_add(list_p, lease_p);
    my_log(LOG_INFO, "Saved lease for " IP_FORMAT,
	   IP_LIST(&lease_p->our_ip));
    if (G_IPConfiguration_verbose) {
	DHCPLeaseListLog(list_p);
    }
    return;
}

/* 
 * Function: DHCPLeaseListRemoveLease
 *
 * Purpose:
 *   Remove the lease entry for the given lease.
 */
void
DHCPLeaseListRemoveLease(DHCPLeaseListRef list_p,
			 struct in_addr our_ip,
			 struct in_addr router_ip,
			 const uint8_t * router_hwaddr,
			 int router_hwaddr_length)
{
    int		where;

    /* remove the old information if it's there */
    where = DHCPLeaseListFindLease(list_p, our_ip, router_ip,
				   router_hwaddr, router_hwaddr_length);
    if (where != DHCP_LEASE_NOT_FOUND) {
	DHCPLeaseRef lease_p = DHCPLeaseListElement(list_p, where);

	my_log(LOG_INFO, "Removing lease for " IP_FORMAT,
	       IP_LIST(&lease_p->our_ip));
	dynarray_free_element(list_p, where);
    }
    return;
}

/* 
 * Function: DHCPLeaseListRemoveAllButLastLease
 * Purpose:
 *   Remove all leases except the last one (the most recently used one),
 *   and mark it tentative.
 */
void
DHCPLeaseListRemoveAllButLastLease(DHCPLeaseListRef list_p)
{
    int			count;
    int			i;
    DHCPLeaseRef	lease_p;

    count = DHCPLeaseListCount(list_p);
    if (count == 0) {
	return;
    }
    for (i = 0; i < (count - 1); i++) {
	lease_p = DHCPLeaseListElement(list_p, 0);	    
	my_log(LOG_INFO, "Removing lease #%d for IP address "
	       IP_FORMAT, i + 1, IP_LIST(&lease_p->our_ip));
	dynarray_free_element(list_p, 0);
    }
    lease_p = DHCPLeaseListElement(list_p, 0);
    lease_p->tentative = TRUE;
    return;
}

