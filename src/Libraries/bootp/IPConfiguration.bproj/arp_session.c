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
 * arp_session.c
 */
/* 
 * Modification History
 *
 * May 11, 2000		Dieter Siegmund (dieter@apple.com)
 * - created
 *
 * March 21, 2001	Dieter Siegmund (dieter@apple.com)
 * - process multiple ARP responses from one bpf read instead of
 *   assuming there's just one response per read
 *
 * June 16, 2003	Dieter Siegmund (dieter@apple.com)
 * - added support for firewire
 *
 * December 22, 2003	Dieter Siegmund (dieter@apple.com)
 * - handle multiple arp client probe requests over multiple
 *   interfaces concurrently
 */

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/sockio.h>
#include <net/if.h>
#include <net/ethernet.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <net/if_arp.h>
#include <net/firewire.h>
#include <netinet/if_ether.h>
#include <arpa/inet.h>
#include <net/if_types.h>
#include <net/bpf.h>
#include <CoreFoundation/CFRunLoop.h>
#include <syslog.h>

#include "ipconfigd_globals.h"
#include "util.h"
#include "bpflib.h"
#include "util.h"
#include "dynarray.h"
#include "timer.h"
#include "FDSet.h"
#include "arp_session.h"
#include "ioregpath.h"
#include "ipconfigd_threads.h"
#include "symbol_scope.h"

struct firewire_arp {
    struct arphdr 	fw_hdr;	/* fixed-size header */
    uint8_t		arp_sha[FIREWIRE_ADDR_LEN];
    uint8_t		arp_spa[4];
    uint8_t		arp_tpa[4];
};

struct probe_info {
    CFTimeInterval		retry_interval;
    int				probe_count;
    int				gratuitous_count;
    boolean_t			skip_first;
};

struct arp_session {
    int				debug;
    struct probe_info		default_probe_info;
    int				default_detect_count;
    CFTimeInterval		default_detect_retry;
    int				default_conflict_retry_count;
    CFTimeInterval		default_conflict_delay;
    arp_our_address_func_t *	is_our_address;
    dynarray_t			if_sessions;
#ifdef TEST_ARP_SESSION
    int				next_client_index;
#endif /* TEST_ARP_SESSION */
};

struct arp_if_session {
    arp_session_t *		session;
    interface_t *		if_p;
    dynarray_t			clients;
    char *			receive_buf;
    int				receive_bufsize;
    FDCalloutRef		read_fd;
    int				read_fd_refcount;
    struct firewire_address	fw_addr;
};

typedef struct arp_if_session arp_if_session_t;

typedef enum {
    arp_client_command_none_e = 0,
    arp_client_command_probe_e = 1,
    arp_client_command_resolve_e = 2,
    arp_client_command_detect_e = 3
} arp_client_command_t;

typedef enum {
    arp_status_none_e = 0,
    arp_status_not_in_use_e = 1,
    arp_status_in_use_e = 2,
    arp_status_error_e = 3,
    arp_status_unknown_e = 4,
} arp_status_t;

struct arp_client {
#ifdef TEST_ARP_SESSION
    int				client_index; /* unique ID */
#endif /* TEST_ARP_SESSION */
    arp_client_command_t	command;
    arp_status_t		command_status;
    boolean_t			fd_open;
    arp_if_session_t *		if_session;
    arp_result_func_t *		func;
    void *			arg1;
    void *			arg2;
    struct in_addr		sender_ip;
    struct in_addr		target_ip;
    int				try;
    int				conflict_count;
    timer_callout_t *		timer_callout;
    arp_address_info_t		in_use_addr;
    char			errmsg[128];
    struct probe_info		probe_info;
    boolean_t			probes_are_collisions;
    uint32_t			resolve_secs;
    arp_address_info_t *	detect_list;
    int				detect_list_count;
    CFRunLoopObserverRef	callback_rls;
};

#ifdef TEST_ARP_SESSION
#undef my_log
#define my_log		arp_session_log
static void arp_session_log(int priority, const char * message, ...);
#define G_IPConfiguration_verbose TRUE
#endif /* TEST_ARP_SESSION */

#include <CoreFoundation/CFDictionary.h>
#include <CoreFoundation/CFRunLoop.h>
#include <SystemConfiguration/SCValidation.h>

#define ARP_STR "ARP "

static Boolean
getFireWireAddress(const char * ifname, struct firewire_address * addr_p)
{
    CFDictionaryRef	dict = NULL;
    CFDataRef		data;
    Boolean		found = FALSE;

    dict = myIORegistryEntryBSDNameMatchingCopyValue(ifname, TRUE);
    if (dict == NULL) {
	return (FALSE);
    }
    data = CFDictionaryGetValue(dict, CFSTR("IOFWHWAddr"));
    if (isA_CFData(data) == NULL || CFDataGetLength(data) != sizeof(*addr_p)) {
	goto done;
    }
    CFDataGetBytes(data, CFRangeMake(0, sizeof(*addr_p)), (void *)addr_p);

    /* put it in network byte order */
    addr_p->unicastFifoHi = htons(addr_p->unicastFifoHi);
    addr_p->unicastFifoLo = htonl(addr_p->unicastFifoLo);
    found = TRUE;

 done:
    if (dict != NULL) {
	CFRelease(dict);
    }
    return (found);
}

/* forward-declarations: */

static arp_if_session_t *
arp_session_new_if_session(arp_session_t * session, interface_t * if_p);

static void
arp_client_free_element(void * arg);

static void
arp_client_probe_retransmit(void * arg1, void * arg2, void * arg3);

static void
arp_client_probe_start(void * arg1, void * arg2, void * arg3);

static void
arp_client_resolve_retransmit(void * arg1, void * arg2, void * arg3);

static boolean_t
arp_client_open_fd(arp_client_t * client);

static void
arp_client_close_fd(arp_client_t * client);

static void
arp_if_session_free(arp_if_session_t * * if_session_p);

static void
arp_if_session_free_element(void * arg);

static void
arp_if_session_read(void * arg1, void * arg2);

static boolean_t
arp_is_our_address(interface_t * if_p, int hwtype, void * hwaddr, int hwlen);

static __inline__ char *
arpop_name(u_int16_t op)
{
    switch (op) {
    case ARPOP_REQUEST:
	return "ARP REQUEST";
    case ARPOP_REPLY:
	return "ARP REPLY";
    case ARPOP_REVREQUEST:
	return "REVARP REQUEST";
    case ARPOP_REVREPLY:
	return "REVARP REPLY";
    default:
	break;
    }
    return ("<unknown>");
}

/* NOTE: caller should make sure arp_p pointed structure
 * be at least 4 byte aligned */
static void
dump_arp(struct arphdr * arp_p)

{
    int arphrd = ntohs(arp_p->ar_hrd);

    printf("\n");
    printf("%s type=0x%x proto=0x%x\n", arpop_name(ntohs(arp_p->ar_op)),
	   arphrd, ntohs(arp_p->ar_pro));

    switch (arphrd) {
    case ARPHRD_ETHER:
	{
	    /* ALIGN: alignment not assumed, using bcopy */
	    struct ether_arp * earp = (struct ether_arp *)(void *)arp_p;
	    struct in_addr iaddr;

	    if (arp_p->ar_hln == sizeof(earp->arp_sha)) {
		struct ether_addr eaddr;
		
		bcopy(earp->arp_sha, &eaddr, sizeof(eaddr));
		printf("Sender H/W\t%s\n", 
		       ether_ntoa((const struct ether_addr *)&eaddr));

		bcopy(earp->arp_tha, &eaddr, sizeof(eaddr));
		printf("Target H/W\t%s\n", 
		       ether_ntoa((const struct ether_addr *)&eaddr));
	    }
	    bcopy(earp->arp_spa, &iaddr, sizeof(iaddr));
	    printf("Sender IP\t%s\n", 
		   inet_ntoa(iaddr));

	    bcopy(earp->arp_tpa, &iaddr, sizeof(iaddr));
	    printf("Target IP\t%s\n", 
		   inet_ntoa(iaddr));
	}
	break;
    case ARPHRD_IEEE1394:
	{
	    /* ALIGN: arp_p is aligned, cast ok. */
	    struct firewire_arp * farp = (struct firewire_arp *)(void *)arp_p;

	    if (arp_p->ar_hln == sizeof(farp->arp_sha)) {
		printf("Sender H/W\t" FWA_FORMAT "\n",
		       FWA_LIST(farp->arp_sha));
	    }
	    /* ALIGN: arp_p is aligned, cast ok. */
	    printf("Sender IP\t%s\n", 
		   inet_ntoa(*((struct in_addr *)(void *)farp->arp_spa)));
	    /* ALIGN: arp_p is aligned, cast ok. */
	    printf("Target IP\t%s\n", 
		   inet_ntoa(*((struct in_addr *)(void *)farp->arp_tpa)));
	}
	break;
    }
    fflush(stdout);
    return;
}

static void
arp_client_close_fd(arp_client_t * client)
{
    arp_if_session_t * 	if_session = client->if_session;

    if (client->fd_open == FALSE) {
	return;
    }
    if (if_session->read_fd_refcount <= 0) {
	my_log(LOG_NOTICE, "arp_client_close_fd(%s): bpf open fd count is %d",
	       if_name(if_session->if_p), if_session->read_fd_refcount);
	return;
    }
    if_session->read_fd_refcount--;
    my_log(LOG_DEBUG, "arp_client_close_fd(%s): bpf open fd count is %d",
	   if_name(if_session->if_p), if_session->read_fd_refcount);
    client->fd_open = FALSE;
    if (if_session->read_fd_refcount == 0) {
	if (if_session->read_fd != NULL) {
	    my_log(LOG_DEBUG, "arp_client_close_fd(%s): closing bpf fd %d",
		   if_name(if_session->if_p),
		   FDCalloutGetFD(if_session->read_fd));
	    /* this closes the file descriptor */
	    FDCalloutRelease(&if_session->read_fd);
	}
	if (if_session->receive_buf != NULL) {
	    free(if_session->receive_buf);
	    if_session->receive_buf = NULL;
	}
    }
    return;
}

/* 
 * Function: arp_client_is_active
 * Purpose:
 *   Returns whether the arp_client is active.
 */
PRIVATE_EXTERN boolean_t
arp_client_is_active(arp_client_t * client)
{
    return (client->func != NULL);
}

/*
 * Function: arp_client_cancel_callback
 * Purpose:
 *   Invalidate/release the callback function.
 */
static void
arp_client_cancel_callback(arp_client_t * client)
{
    if (client->callback_rls != NULL) {
	CFRunLoopObserverInvalidate(client->callback_rls);
	CFRelease(client->callback_rls);
	client->callback_rls = NULL;
    }
    return;
}

/*
 * Function: arp_client_callback
 *
 * Purpose:
 *   Call the supplied function with the appropriate result.
 */
static void
arp_client_callback(arp_client_t * client)
{
    void *		c_arg1;
    void * 		c_arg2;
    arp_result_func_t *	func;
    arp_result_t	result;

    /* remember the client parameters, then clear them */
    c_arg1 = client->arg1;
    c_arg2 = client->arg2;
    func = client->func;
    client->func = client->arg1 = client->arg2 = NULL;

    arp_client_close_fd(client);
    timer_cancel(client->timer_callout);

    /* return the results */
    bzero(&result, sizeof(result));
    switch (client->command_status) {
    default:
    case arp_status_none_e:
	/* not possible */
	printf("No result for %s?\n", 
	       if_name(client->if_session->if_p));
	break;
    case arp_status_error_e:
	result.error = TRUE;
	break;
    case arp_status_not_in_use_e:
	break;
    case arp_status_in_use_e:
	result.in_use = TRUE;
	result.addr = client->in_use_addr;
	break;
    }

    /* return the results to the client */
    result.client = client;
    (*func)(c_arg1, c_arg2, &result);
    return;
}

/*
 * Function: arp_client_do_callback
 * Purpose:
 *   Invalidate the runloop observer then invoke arp_client_callback().
 */
static void
arp_client_do_callback(CFRunLoopObserverRef observer, 
		       CFRunLoopActivity activity,
		       void * info)
{
    arp_client_t * 	client = (arp_client_t *)info;

    /* de-activate the observer */
    arp_client_cancel_callback(client);
    arp_client_callback(client);
    return;
}

/*
 * Function: arp_client_schedule_callback
 * Purpose:
 *   Call the arp_client_callback via a runloop observer.
 */
static void
arp_client_schedule_callback(arp_client_t * client)
{
    CFRunLoopObserverContext	context = { 0, client, NULL, NULL, NULL };

    arp_client_cancel_callback(client);
    client->callback_rls
	= CFRunLoopObserverCreate(NULL, kCFRunLoopAllActivities,
				  TRUE, 0, arp_client_do_callback, &context);
    
    CFRunLoopAddObserver(CFRunLoopGetCurrent(), client->callback_rls,
			 kCFRunLoopDefaultMode);
    return;
}

/*
 * Function: arp_is_our_address
 *
 * Purpose:
 *   Returns whether the given hardware address matches the given
 *   network interface.
 */
static boolean_t
arp_is_our_address(interface_t * if_p, int hwtype, void * hwaddr, int hwlen)
{
    int		link_length = if_link_length(if_p);

    if (hwlen != link_length || hwtype != if_link_arptype(if_p)) {
	return (FALSE);
    }
    if (bcmp(hwaddr, if_link_address(if_p), link_length) == 0) {
	return (TRUE);
    }
    return (FALSE);
}

static void
arp_if_session_update_hardware_address(arp_if_session_t * if_session)
{
    if (if_link_type(if_session->if_p) != IFT_IEEE1394) {
	return;
    }
    /* copy in the latest firewire address */
    if (getFireWireAddress(if_name(if_session->if_p), 
			   &if_session->fw_addr) == FALSE) {
	my_log(LOG_NOTICE,
	       "arp_if_session_update_hardware_address(%s):"
	       "could not retrieve firewire address",
	       if_name(if_session->if_p));
    }
    return;
}

/*
 * Function: arp_if_session_read
 * Purpose:
 *   Called when data is available on the bpf fd.
 *   Check the arp packet, and see if it matches
 *   any of the clients' probe criteria.  If it does,
 *   call the client with an in_use result structure.
 */
static void
arp_if_session_read(void * arg1, void * arg2)
{
    arp_client_t *	client;
    int			client_count;
    boolean_t		debug;
    char		errmsg[128];
    int			hwlen = 0;
    int			hwtype;
    int			i;
    arp_if_session_t * 	if_session;
    int			link_header_size;
    int			link_arp_size;
    int			link_length;
    ssize_t		n;
    char *		offset;
    arp_session_t *	session;

    if_session = (arp_if_session_t *)arg1;
    session = if_session->session;

    errmsg[0] = '\0';

    if (if_session->read_fd_refcount == 0) {
	my_log(LOG_NOTICE, "arp_if_session_read: no pending clients?");
	return;
    }

    debug = session->debug;
    client_count = dynarray_count(&if_session->clients);

    link_length = if_link_length(if_session->if_p);
    hwtype = if_link_arptype(if_session->if_p);
    switch (hwtype) {
    default:
	/* default clause will never match */
    case ARPHRD_ETHER:
	link_header_size = sizeof(struct ether_header);
	link_arp_size = sizeof(struct ether_arp);
	hwlen = ETHER_ADDR_LEN;
	break;
    case ARPHRD_IEEE1394:
	link_header_size = sizeof(struct firewire_header);
	link_arp_size = sizeof(struct firewire_arp);
	hwlen = FIREWIRE_ADDR_LEN;
	break;
    }
    n = read(FDCalloutGetFD(if_session->read_fd), if_session->receive_buf, 
	     if_session->receive_bufsize);
    if (n < 0) {
	if (errno == EAGAIN) {
	    return;
	}
	my_log(LOG_ERR, "arp_if_session_read: read(%s) failed, %s (%d)", 
	       if_name(if_session->if_p), strerror(errno), errno);
	snprintf(errmsg, sizeof(errmsg),
		 "arp_if_session_read: read(%s) failed, %s (%d)", 
		 if_name(if_session->if_p), strerror(errno), errno);
	goto failed;
    }
    for (offset = if_session->receive_buf; n > 0; ) {
	struct arphdr *		arp_p;
	struct bpf_hdr * 	bpf = (struct bpf_hdr *)(void *)offset;
	void *			hwaddr;
	boolean_t		is_our_address;
	short			op;
	char *			pkt_start;
	struct in_addr		source_ip_aligned;
	struct in_addr *	source_ip_p;
	struct in_addr		target_ip_aligned;
	struct in_addr *	target_ip_p;
	int			skip;
	
	/* ALIGN: offset is aligned to sizeof(int) bytes */
	pkt_start = offset + bpf->bh_hdrlen;
	arp_p = (struct arphdr *)(void *)(pkt_start + link_header_size);
	if (debug) {
	    dump_arp(arp_p);
	}
	op = ntohs(arp_p->ar_op);
	if (bpf->bh_caplen < (link_header_size + link_arp_size)
	    || arp_p->ar_hln != hwlen
	    || (op != ARPOP_REPLY && op != ARPOP_REQUEST)
	    || ntohs(arp_p->ar_hrd) != hwtype
	    || ntohs(arp_p->ar_pro) != ETHERTYPE_IP) {
	    goto next_packet;
	}
	switch (hwtype) {
	default:
	case ARPHRD_ETHER:
	    {
		struct ether_arp * earp;
		
		earp = (struct ether_arp *)arp_p;
	
		/* ALIGN: don't assume fields in earp are aligned */
		source_ip_p = &source_ip_aligned;
		target_ip_p = &target_ip_aligned;
		bcopy(earp->arp_spa, source_ip_p, sizeof(struct in_addr));
		bcopy(earp->arp_tpa, target_ip_p, sizeof(struct in_addr));
		hwaddr = earp->arp_sha;
	    }
	    break;
	case ARPHRD_IEEE1394:
	    {
		struct firewire_arp * farp;

		farp = (struct firewire_arp *)arp_p;
		/* ALIGN: arp_p aligned, cast ok. */
		source_ip_p = (struct in_addr *)(void *)farp->arp_spa;
		target_ip_p = (struct in_addr *)(void *)farp->arp_tpa;
		hwaddr = farp->arp_sha;
	    }
	    break;
	}
	is_our_address 
	    = (*session->is_our_address)(if_session->if_p,
					 hwtype,
					 hwaddr,
					 link_length);
	for (i = 0; i < client_count; i++) {
	    int			addr_index;
	    arp_client_t *	client;
	    boolean_t		got_match;

	    client = dynarray_element(&if_session->clients, i);
	    if (client->func == NULL) {
		continue;
	    }
	    if (client->command_status == arp_status_in_use_e) {
		/* we already found a match for this client */
		continue;
	    }
	    got_match = FALSE;
	    switch (client->command) {
	    case arp_client_command_probe_e:
		if (is_our_address) {
		    /* don't report conflicts against our own h/w addresses */
		}
		/* IP is in use by some other host */
		else if (client->target_ip.s_addr == source_ip_p->s_addr
		    || (client->probes_are_collisions
			&& op == ARPOP_REQUEST
			&& source_ip_p->s_addr == 0
			&& client->target_ip.s_addr == target_ip_p->s_addr)) {
		    client->in_use_addr.sender_ip = client->sender_ip;
		    client->in_use_addr.target_ip = client->target_ip;
		    bcopy(hwaddr, client->in_use_addr.target_hardware,
			  link_length);
		    got_match = TRUE;
		}
		break;
	    case arp_client_command_resolve_e:
		if (client->target_ip.s_addr == source_ip_p->s_addr
		    && op == ARPOP_REPLY) {
		    client->in_use_addr.sender_ip = client->sender_ip;
		    client->in_use_addr.target_ip = client->target_ip;
		    bcopy(hwaddr, client->in_use_addr.target_hardware, 
			  link_length);
		    got_match = TRUE;
		}
		break;
	    case arp_client_command_detect_e:
		if (op != ARPOP_REPLY) {
		    break;
		}
		for (addr_index = 0; addr_index < client->detect_list_count;
		     addr_index++) {
		    arp_address_info_t *	info_p;

		    info_p = client->detect_list + addr_index;
		    if (info_p->sender_ip.s_addr == target_ip_p->s_addr
			&& info_p->target_ip.s_addr == source_ip_p->s_addr
			&& (bcmp(info_p->target_hardware, hwaddr, link_length)
			    == 0)) {
			client->in_use_addr = *info_p;
			got_match = TRUE;
			break;
		    }
		}
		break;
	    default:
		break;
	    }
	    if (got_match) {
		client->command_status = arp_status_in_use_e;
		if (client->command == arp_client_command_probe_e
		    && client->probes_are_collisions == FALSE) {
		    client->conflict_count++;
		    my_log(LOG_INFO,
			   "arp_session: encountered conflict,"
			   " trying again %d (of %d)",
			   client->conflict_count,
			   session->default_conflict_retry_count + 1);
		    if (client->conflict_count
			<= session->default_conflict_retry_count) {
			/* schedule another probe cycle */
			timer_callout_set(client->timer_callout,
					  session->default_conflict_delay,
					  (timer_func_t *)
					  arp_client_probe_start,
					  client, NULL, NULL);
			goto next_packet;
		    }
		}
		/* match found, provide results via callback */
		arp_client_schedule_callback(client);
	    }
	}
    next_packet:
	skip = BPF_WORDALIGN(bpf->bh_caplen + bpf->bh_hdrlen);
	if (skip == 0) {
	    break;
	}
	offset += skip;
	n -= skip;
    }
    return;
 failed:
    for (i = 0; i < client_count; i++) {
	client = dynarray_element(&if_session->clients, i);
	if (client->func == NULL) {
	    continue;
	}
	strncpy(client->errmsg, errmsg, sizeof(client->errmsg));
	/* report back an error to the caller */
	client->command_status = arp_status_error_e;
	arp_client_schedule_callback(client);
    }
    return;
}

static boolean_t
arp_client_open_fd(arp_client_t * client)
{
    int			bpf_fd;
    arp_if_session_t *	if_session = client->if_session;
    int 		opt;
    int 		status;

    if (client->fd_open) {
	return (TRUE);
    }
    if_session->read_fd_refcount++;
    my_log(LOG_DEBUG, "arp_client_open_fd (%s): refcount %d", 
	   if_name(if_session->if_p), if_session->read_fd_refcount);
    client->fd_open = TRUE;
    if (if_session->read_fd_refcount > 1) {
	/* already open */
	return (TRUE);
    }
    bpf_fd = bpf_new();
    if (bpf_fd < 0) {
	my_log(LOG_ERR, "arp_client_open_fd: bpf_new(%s) failed, %s (%d)", 
	       if_name(if_session->if_p), strerror(errno), errno);
	snprintf(client->errmsg, sizeof(client->errmsg),
		 "arp_client_open_fd: bpf_new(%s) failed, %s (%d)", 
		 if_name(if_session->if_p), strerror(errno), errno);
	goto failed;
    }
    opt = 1;
    status = ioctl(bpf_fd, FIONBIO, &opt);
    if (status < 0) {
	my_log(LOG_ERR, "ioctl FIONBIO failed %s", strerror(errno));
	goto failed;
    }

    /* associate it with the given interface */
    status = bpf_setif(bpf_fd, if_name(if_session->if_p));
    if (status < 0) {
	my_log(LOG_ERR, "arp_client_open_fd: bpf_setif(%s) failed: %s (%d)", 
	       if_name(if_session->if_p), strerror(errno), errno);
	snprintf(client->errmsg, sizeof(client->errmsg),
		 "arp_client_open_fd: bpf_setif(%s) failed: %s (%d)", 
		 if_name(if_session->if_p), strerror(errno), errno);
	goto failed;
    }

    /* don't wait for packets to be buffered */
    bpf_set_immediate(bpf_fd, 1);

    /* set the filter to return only ARP packets */
    switch (if_link_type(if_session->if_p)) {
    default:
    case IFT_ETHER:
	status = bpf_arp_filter(bpf_fd, 12, ETHERTYPE_ARP,
				sizeof(struct ether_arp) 
				+ sizeof(struct ether_header));
	break;
    case IFT_IEEE1394:
	status = bpf_arp_filter(bpf_fd, 16, ETHERTYPE_ARP,
				sizeof(struct firewire_arp) 
				+ sizeof(struct firewire_header));
	break;
    }
    if (status < 0) {
	my_log(LOG_ERR, 
	       "arp_client_open_fd: bpf_arp_filter(%s) failed: %s (%d)", 
	       if_name(if_session->if_p), strerror(errno), errno);
	snprintf(client->errmsg, sizeof(client->errmsg),
		 "arp_client_open_fd: bpf_arp_filter(%s) failed: %s (%d)", 
		 if_name(if_session->if_p), strerror(errno), errno);
	goto failed;
    }
    /* get the receive buffer size */
    status = bpf_get_blen(bpf_fd, &if_session->receive_bufsize);
    if (status < 0) {
	my_log(LOG_ERR, 
	       "arp_client_open_fd: bpf_get_blen(%s) failed, %s (%d)", 
	       if_name(if_session->if_p), strerror(errno), errno);
	snprintf(client->errmsg, sizeof(client->errmsg),
		 "arp_client_open_fd: bpf_get_blen(%s) failed, %s (%d)", 
		 if_name(if_session->if_p), strerror(errno), errno);
	goto failed;
    }
    if_session->receive_buf = malloc(if_session->receive_bufsize);
    if_session->read_fd 
	= FDCalloutCreate(bpf_fd,
			  arp_if_session_read, if_session, NULL);
    if (if_session->read_fd == NULL) {
	goto failed;
    }
    my_log(LOG_DEBUG, "arp_client_open_fd (%s): opened bpf fd %d",
	   if_name(if_session->if_p), bpf_fd);
    return (TRUE);

 failed:
    if (bpf_fd >= 0) {
	close(bpf_fd);
    }
    arp_client_close_fd(client);
    return (FALSE);
}

static char			link_broadcast[8] = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff 
};

static boolean_t
arp_client_transmit(arp_client_t * client, boolean_t send_gratuitous,
		    const arp_address_info_t * info_p)
{
    arp_if_session_t *		if_session = client->if_session;
    struct arphdr *		hdr;
    int 			status = 0;
    /* 
     * txbuf is cast to some struct types containing short fields;
     * force it to be aligned as much as an int
     */
    int				txbuf_aligned[32];
    char *			txbuf = (char *)txbuf_aligned;
    int				size;

    bzero(txbuf_aligned, sizeof(txbuf_aligned));

    /* fill in the ethernet header */
    switch (if_link_arptype(if_session->if_p)) {
    case ARPHRD_ETHER:
	{
	    struct ether_header *	eh_p;
	    struct ether_arp *		earp;

	    /* fill in the ethernet header */
	    /* ALIGN: txbuf is aligned to sizeof(int) bytes */
	    eh_p = (struct ether_header *)(void *)txbuf;
	    eh_p->ether_type = htons(ETHERTYPE_ARP);
	    if (info_p != NULL) {
		bcopy(info_p->target_hardware, eh_p->ether_dhost,
		      sizeof(eh_p->ether_dhost));
	    }
	    else {
		bcopy(link_broadcast, eh_p->ether_dhost,
		      sizeof(eh_p->ether_dhost));
	    }
	    /* fill in the arp packet contents */
	    /* ALIGN: txbuf is aligned to sizeof(int) bytes */
	    earp = (struct ether_arp *)(void *)(txbuf + sizeof(*eh_p));
	    hdr = &earp->ea_hdr;
	    hdr->ar_hrd = htons(ARPHRD_ETHER);
	    hdr->ar_pro = htons(ETHERTYPE_IP);
	    hdr->ar_hln = sizeof(earp->arp_sha);;
	    hdr->ar_pln = sizeof(struct in_addr);
	    hdr->ar_op = htons(ARPOP_REQUEST);
	    bcopy(if_link_address(if_session->if_p), earp->arp_sha,
		  sizeof(earp->arp_sha));
	    if (info_p != NULL) {
		*((struct in_addr *)(void *)earp->arp_spa) = info_p->sender_ip;
		*((struct in_addr *)(void *)earp->arp_tpa) = info_p->target_ip;
	    }
	    else {
		if (send_gratuitous == TRUE
		    && client->sender_ip.s_addr == 0) {
		    *((struct in_addr *)(void *)earp->arp_spa) = client->target_ip;
		}
		else {
		    *((struct in_addr *)(void *)earp->arp_spa) = client->sender_ip;
		}
		*((struct in_addr *)(void *)earp->arp_tpa) = client->target_ip;
	    }
	    size = sizeof(*eh_p) + sizeof(*earp);
	}
	break;
    case ARPHRD_IEEE1394:
	{
	    struct firewire_header *	fh_p;
	    struct firewire_arp *	farp;

	    /* fill in the firewire header */
	    /* ALIGN: txbuf is aligned to sizeof(int) bytes */
	    fh_p = (struct firewire_header *)(void *)txbuf;
	    fh_p->firewire_type = htons(ETHERTYPE_ARP);
	    if (info_p != NULL) {
		bcopy(info_p->target_hardware, fh_p->firewire_dhost,
		      sizeof(fh_p->firewire_dhost));
	    }
	    else {
		bcopy(link_broadcast, fh_p->firewire_dhost,
		      sizeof(fh_p->firewire_dhost));
	    }

	    /* fill in the arp packet contents */
	    /* ALIGN: txbuf is aligned to sizeof(int) bytes */
	    farp = (struct firewire_arp *)(void *)(txbuf + sizeof(*fh_p));
	    hdr = &farp->fw_hdr;
	    hdr->ar_hrd = htons(ARPHRD_IEEE1394);
	    hdr->ar_pro = htons(ETHERTYPE_IP);
	    hdr->ar_hln = sizeof(farp->arp_sha);;
	    hdr->ar_pln = sizeof(struct in_addr);
	    hdr->ar_op = htons(ARPOP_REQUEST);
	    bcopy(&if_session->fw_addr, farp->arp_sha,
		  sizeof(farp->arp_sha));
	    if (info_p != NULL) {
		*((struct in_addr *)(void *)farp->arp_spa) = info_p->sender_ip;
		*((struct in_addr *)(void *)farp->arp_tpa) = info_p->target_ip;
	    }
	    else {
		if (send_gratuitous == TRUE
		    && client->sender_ip.s_addr == 0) {
		    *((struct in_addr *)(void *)farp->arp_spa) = client->target_ip;
		}
		else {
		    *((struct in_addr *)(void *)farp->arp_spa) = client->sender_ip;
		}
		*((struct in_addr *)(void *)farp->arp_tpa) = client->target_ip;
	    }
	    size = sizeof(*fh_p) + sizeof(*farp);
	}
	break;
    default:
	snprintf(client->errmsg, sizeof(client->errmsg),
		 "arp_client_transmit(%s): "
		 "interface hardware type not yet known", 
		 if_name(if_session->if_p));
	goto failed;
    }

    status = bpf_write(FDCalloutGetFD(if_session->read_fd), txbuf, size);
    if (status < 0) {
	my_log(LOG_ERR, "arp_client_transmit(%s) failed, %s (%d)", 
	       if_name(if_session->if_p), strerror(errno), errno);
	snprintf(client->errmsg, sizeof(client->errmsg),
		 "arp_client_transmit(%s) failed, %s (%d)", 
		 if_name(if_session->if_p), strerror(errno), errno);
	goto failed;
    }
#ifdef TEST_ARP_SESSION
    arp_session_log(LOG_INFO, "TX ARP Packet");
#endif
    return (TRUE);

 failed:
    return (FALSE);
}

static void
arp_client_probe_start(void * arg1, void * arg2, void * arg3)
{
    arp_client_t * 	client = (arp_client_t *)arg1;

    client->try = 0;
    client->command_status = arp_status_unknown_e;
    arp_client_probe_retransmit(arg1, arg2, arg3);
    return;
}

/*
 * Function: arp_client_probe_retransmit
 *
 * Purpose:
 *   Transmit an ARP packet with timeout retry.
 *   Uses callback to invoke arp_client_report_error if the transmit failed.
 *   When we've tried often enough, call the client function with a result
 *   structure indicating no errors and the IP is not in use.
 */
static void
arp_client_probe_retransmit(void * arg1, void * arg2, void * arg3)
{
    arp_client_t *      client = (arp_client_t *)arg1;
    struct probe_info * probe_info = &client->probe_info;
    int                 tries_left;
    arp_if_session_t *  if_session = client->if_session;

    tries_left = (probe_info->probe_count + probe_info->gratuitous_count)
        - client->try;
    
    if (tries_left <= 0) {
        /* not in use */
        client->command_status = arp_status_not_in_use_e;
        arp_client_schedule_callback(client);
        return;
    }
    
    client->try++;

    if (client->probe_info.skip_first
	|| arp_client_transmit(client,
			       (tries_left <= probe_info->gratuitous_count),
			       NULL)) {
	if (client->probe_info.skip_first) {
	    my_log(LOG_INFO, ARP_STR 
		   "(%s): skipping the first arp announcement.",
		   if_name(if_session->if_p));
	}
	else if (tries_left <= probe_info->gratuitous_count) {
	    my_log(LOG_INFO, 
		   ARP_STR 
		   "(%s): sending (%d of %d) arp announcements ", 
		   if_name(if_session->if_p), 
		   probe_info->gratuitous_count - tries_left + 1, 
		   probe_info->gratuitous_count); 
	}
	else {
	    my_log(LOG_INFO, ARP_STR "(%s): sending (%d of %d) "
		   "arp probes ", if_name(if_session->if_p),
		   client->try, probe_info->probe_count);
	}
        timer_callout_set(client->timer_callout,
			  probe_info->retry_interval,
			  (timer_func_t *)arp_client_probe_retransmit,
			  client, NULL, NULL);
        client->probe_info.skip_first = FALSE;
    }
    else {
        /* report back an error to the caller */
        client->command_status = arp_status_error_e;
        arp_client_schedule_callback(client);
    }
}
/*
 * Function: arp_client_resolve_retransmit
 *
 * Purpose:
 *   Transmit an ARP request packet in an attempt to resolve the IP address.
 *   Uses callback to invoke arp_client_report_error if the transmit failed.
 *   If we can't resolve the address, call the callback indicating the IP
 *   address is not in use.
 */
static void
arp_client_resolve_retransmit(void * arg1, void * arg2, void * arg3)
{
    arp_client_t * 	client = (arp_client_t *)arg1;
    int			tries_left;

    tries_left = client->resolve_secs - client->try;
    if (tries_left <= 0) {
	/* not in use */
	client->command_status = arp_status_not_in_use_e;
	arp_client_schedule_callback(client);
	return;
    }
    client->try++;
    if (arp_client_transmit(client, FALSE, NULL)) {
#define ARP_RESOLVE_RETRY_INTERVAL		(1.0)
	timer_callout_set(client->timer_callout, ARP_RESOLVE_RETRY_INTERVAL,
			  (timer_func_t *)arp_client_resolve_retransmit,
			  client, NULL, NULL);
    }
    else {
	/* report back an error to the caller */
	client->command_status = arp_status_error_e;
	arp_client_schedule_callback(client);
    }
    return;
}

/*
 * Function: arp_client_detect_retransmit
 *
 * Purpose:
 *   Transmit a set of ARP requests, one request for each host of interest,
 *   in an attempt to detect which one is present.  The ARP requests are sent
 *   using unicast to a specific hardware address and should only be visible
 *   received/processed by that specific host.
 */
static void
arp_client_detect_retransmit(void * arg1, void * arg2, void * arg3)
{
    arp_client_t * 	client = (arp_client_t *)arg1;
    int			i;
    boolean_t		keep_going = TRUE;
    arp_session_t *	session = client->if_session->session;
    int			tries_left;

    tries_left = session->default_detect_count - client->try;
    if (tries_left <= 0) {
	/* not in use */
	client->command_status = arp_status_not_in_use_e;
	arp_client_schedule_callback(client);
	return;
    }
    client->try++;
    for (i = 0; i < client->detect_list_count; i++) {
	if (arp_client_transmit(client, FALSE, client->detect_list + i)
	    == FALSE) {
	    keep_going = FALSE;
	    break;
	}
    }
    if (keep_going) {
	double		backoff;
	CFTimeInterval	timeout;

	if (client->try < 2) {
	    backoff = 1;
	}
	else {
	    backoff = 2 << (client->try - 2);
	}
	timeout = session->default_detect_retry * backoff;
	timer_callout_set(client->timer_callout, timeout,
			   (timer_func_t *)
			   arp_client_detect_retransmit,
			   client, NULL, NULL);
    }
    else {
	/* report back an error to the caller */
	client->command_status = arp_status_error_e;
	arp_client_schedule_callback(client);
    }
    return;
}

static arp_client_t *
arp_if_session_new_client(arp_if_session_t * if_session)
{
    arp_client_t *		client;
    char			timer_name[32];

    client = malloc(sizeof(*client));
    if (client == NULL) {
	return (NULL);
    }
    bzero(client, sizeof(*client));
    if (dynarray_add(&if_session->clients, client) == FALSE) {
	free(client);
	return (NULL);
    }
#ifdef TEST_ARP_SESSION
    client->client_index = if_session->session->next_client_index++;
#endif /* TEST_ARP_SESSION */
    client->if_session = if_session;
    client->probe_info = if_session->session->default_probe_info;
    snprintf(timer_name, sizeof(timer_name), "arp-%s",
	     if_name(if_session->if_p));
    client->timer_callout = timer_callout_init(timer_name);
    return (client);

}

static arp_client_t *
arp_session_new_client(arp_session_t * session, interface_t * if_p)
{
    arp_if_session_t *		if_session;

    if_session = arp_session_new_if_session(session, if_p);
    if (if_session == NULL) {
	return (NULL);
    }
    return (arp_if_session_new_client(if_session));
}

#ifdef TEST_ARP_SESSION
static arp_client_t *
arp_session_find_client_with_index(arp_session_t * session, int index)
{
    int		if_sessions_count;
    int		i;
    int		j;

    if_sessions_count = dynarray_count(&session->if_sessions);
    for (i = 0; i < if_sessions_count; i++) {
	int			clients_count;
	arp_if_session_t *	if_session;

	if_session = dynarray_element(&session->if_sessions, i);
	clients_count = dynarray_count(&if_session->clients);
	for (j = 0; j < clients_count; j++) {
	    arp_client_t *	client;

	    client = dynarray_element(&if_session->clients, j);
	    if (client->client_index == index) {
		return (client);
	    }
	}
    }
    return (NULL);
}

STATIC void
arp_client_set_probe_info(arp_client_t * client, 
			  CFTimeInterval * retry_interval,
			  const int * probe_count, 
			  const int * gratuitous_count)
{
    struct probe_info *		probe_info = &client->probe_info;

    if (retry_interval != NULL) {
	probe_info->retry_interval = *retry_interval;
    }
    if (probe_count != NULL) {
	probe_info->probe_count = *probe_count;
    }
    if (gratuitous_count != NULL) {
	probe_info->gratuitous_count = *gratuitous_count;
    }
    return;
}

STATIC void
arp_client_restore_default_probe_info(arp_client_t * client)
{
    client->probe_info = client->if_session->session->default_probe_info;
    return;
}

#endif /* TEST_ARP_SESSION */

PRIVATE_EXTERN arp_client_t *
arp_client_init(arp_session_t * session, interface_t * if_p)
{
    return (arp_session_new_client(session, if_p));
}

static void
arp_client_free_element(void * arg)
{
    arp_client_t * 	client = (arp_client_t *)arg;

    arp_client_cancel(client);
    timer_callout_free(&client->timer_callout);
    free(client);
    return;
}

PRIVATE_EXTERN void
arp_client_free(arp_client_t * * client_p)
{
    arp_client_t * 		client = NULL;
    int 			i;
    arp_if_session_t * 		if_session;

    if (client_p != NULL) {
	client = *client_p;
    }
    if (client == NULL) {
	return;
    }

    /* remove from list of clients */
    if_session = client->if_session;
    i = dynarray_index(&if_session->clients, client);
    if (i != -1) {
	dynarray_remove(&if_session->clients, i, NULL);
    }
    else {
	my_log(LOG_NOTICE, "arp_client_free(%s) not in list?",
	       if_name(if_session->if_p));
    }

    /* free resources */
    arp_client_free_element(client);
    *client_p = NULL;

    /* if we're the last client, if_session can go too */
    if (dynarray_count(&if_session->clients) == 0) {
	arp_if_session_free(&if_session);
    }
    return;
}

PRIVATE_EXTERN void
arp_client_set_probes_are_collisions(arp_client_t * client, 
				     boolean_t probes_are_collisions)
{
    client->probes_are_collisions = probes_are_collisions;
    return;
}

static inline void
arp_client_setup_context(arp_client_t * client,
                         arp_result_func_t * func, void * arg1, void * arg2,
                    	 struct in_addr sender_ip, struct in_addr target_ip,
                    	 boolean_t skip)
{
    arp_if_session_t *  if_session = client->if_session;

    arp_client_cancel(client);
    arp_if_session_update_hardware_address(if_session);
    client->sender_ip = sender_ip;
    client->target_ip = target_ip;
    client->func = func;
    client->arg1 = arg1;
    client->arg2 = arg2;
    client->errmsg[0] = '\0';
    client->try = 0;
    client->conflict_count = 0;
    
    /* We might need to skip the first arp announcement since it
     * may have already been sent. */
    client->probe_info.skip_first = skip;
}

PRIVATE_EXTERN void
arp_client_announce(arp_client_t * client,
                    arp_result_func_t * func, void * arg1, void * arg2,
                    struct in_addr sender_ip, struct in_addr target_ip, 
		    boolean_t skip) 
{
    arp_client_setup_context(client, func, arg1, arg2, 
 			     sender_ip, target_ip, skip);
    
    /* Send announce only, get rid of the probe count. */
    client->try = client->probe_info.probe_count;
    if (!arp_client_open_fd(client)) {
        /* report back an error to the caller */
        client->command_status = arp_status_error_e;
        arp_client_schedule_callback(client);
        return;
    }
    
    client->command_status = arp_status_unknown_e;
    client->command = arp_client_command_probe_e;
    arp_client_probe_retransmit(client, NULL, NULL);
}

PRIVATE_EXTERN void
arp_client_probe(arp_client_t * client,
		 arp_result_func_t * func, void * arg1, void * arg2,
		 struct in_addr sender_ip, struct in_addr target_ip)
{
    arp_client_setup_context(client, func, arg1, arg2, 
			     sender_ip, target_ip, FALSE);

    if (!arp_client_open_fd(client)) {
	/* report back an error to the caller */
	client->command_status = arp_status_error_e;
	arp_client_schedule_callback(client);
	return;
    }
    client->command_status = arp_status_unknown_e;
    client->command = arp_client_command_probe_e;
    arp_client_probe_retransmit(client, NULL, NULL);
    return;
}

PRIVATE_EXTERN void
arp_client_resolve(arp_client_t * client,
		   arp_result_func_t * func, void * arg1, void * arg2,
		   struct in_addr sender_ip, struct in_addr target_ip,
		   uint32_t resolve_secs)
{
    
    arp_if_session_t * 	if_session = client->if_session;

    arp_client_cancel(client);
    arp_if_session_update_hardware_address(if_session);
    client->sender_ip = sender_ip;
    client->target_ip = target_ip;
    client->func = func;
    client->arg1 = arg1;
    client->arg2 = arg2;
    client->errmsg[0] = '\0';
    client->try = 0;
    client->conflict_count = 0;
    if (!arp_client_open_fd(client)) {
	/* report back an error to the caller */
	client->command_status = arp_status_error_e;
	arp_client_schedule_callback(client);
	return;
    }
    client->command_status = arp_status_unknown_e;
#define DEFAULT_RESOLVE_SECS	16
    client->resolve_secs 
	= (resolve_secs > 0) ? resolve_secs : DEFAULT_RESOLVE_SECS;
    client->command = arp_client_command_resolve_e;
    arp_client_resolve_retransmit(client, NULL, NULL);
    return;
}

PRIVATE_EXTERN void
arp_client_detect(arp_client_t * client,
		  arp_result_func_t * func, void * arg1, void * arg2,
		  const arp_address_info_t * list, int list_count)
{
    arp_if_session_t * 	if_session = client->if_session;
    int			list_size;

    arp_client_cancel(client);
    arp_if_session_update_hardware_address(if_session);
    client->func = func;
    client->arg1 = arg1;
    client->arg2 = arg2;
    client->errmsg[0] = '\0';
    client->try = 0;
    client->conflict_count = 0;
    if (list_count == 0 || !arp_client_open_fd(client)) {
	/* report back an error to the caller */
	client->command_status = arp_status_error_e;
	arp_client_schedule_callback(client);
	return;
    }
    list_size = sizeof(*client->detect_list) * list_count;
    client->detect_list = (arp_address_info_t *)malloc(list_size);
    bcopy(list, client->detect_list, list_size);
    client->detect_list_count = list_count;
    client->command_status = arp_status_unknown_e;
    client->command = arp_client_command_detect_e;
    arp_client_detect_retransmit(client, NULL, NULL);
    return;
}

PRIVATE_EXTERN const char *
arp_client_errmsg(arp_client_t * client)
{
    return ((const char *)client->errmsg);
}

PRIVATE_EXTERN void
arp_client_cancel(arp_client_t * client)
{
    client->errmsg[0] = '\0';
    client->func = client->arg1 = client->arg2 = NULL;
    client->command_status = arp_status_none_e;
    arp_client_close_fd(client);
    timer_cancel(client->timer_callout);
    if (client->detect_list != NULL) {
	free(client->detect_list);
	client->detect_list = NULL;
    }
    arp_client_cancel_callback(client);
    return;
}

PRIVATE_EXTERN boolean_t
arp_client_defend(arp_client_t * client, struct in_addr our_ip)
{
    boolean_t		defended = FALSE;
    arp_if_session_t * 	if_session = client->if_session;

    arp_client_cancel(client);
    arp_if_session_update_hardware_address(if_session);

    if (!arp_client_open_fd(client)) {
	my_log(LOG_NOTICE, "arp_client_defend(%s): open fd failed",
	       if_name(if_session->if_p));
    }
    else {
	client->target_ip = client->sender_ip = our_ip;
	if (!arp_client_transmit(client, FALSE, NULL)) {
	    my_log(LOG_NOTICE, "arp_client_defend(%s): transmit failed",
		   if_name(if_session->if_p));
	}
	else {
	    defended = TRUE;
	}
	arp_client_close_fd(client);
    }
    return (defended);
}

PRIVATE_EXTERN arp_session_t *
arp_session_init(arp_our_address_func_t * func,
		 arp_session_values_t * values)
{
    arp_session_t * 	session;

    session = malloc(sizeof(*session));
    if (session == NULL) {
	return (NULL);
    }
    bzero(session, sizeof(*session));
    dynarray_init(&session->if_sessions, arp_if_session_free_element, NULL);
    if (func == NULL) {
	session->is_our_address = arp_is_our_address;
    }
    else {
	session->is_our_address = func;
    }
    if (values->probe_interval != NULL) {
	session->default_probe_info.retry_interval = *values->probe_interval;
    }
    else {
	session->default_probe_info.retry_interval = ARP_RETRY_SECS;
    }
    if (values->probe_count != NULL) {
	session->default_probe_info.probe_count = *values->probe_count;
    }
    else {
	session->default_probe_info.probe_count = ARP_PROBE_COUNT;
    }
    if (values->probe_gratuitous_count != NULL) {
	session->default_probe_info.gratuitous_count 
	    = *values->probe_gratuitous_count;
    }
    else {
	session->default_probe_info.gratuitous_count = ARP_GRATUITOUS_COUNT;
    }
    if (values->detect_count != NULL) {
	session->default_detect_count = *values->detect_count;
    }
    else {
	session->default_detect_count = ARP_DETECT_COUNT;
    }
    if (values->detect_interval != NULL) {
	session->default_detect_retry = *values->detect_interval;
    }
    else {
	session->default_detect_retry = ARP_DETECT_RETRY_SECS;
    }
    if (values->conflict_retry_count != NULL) {
	session->default_conflict_retry_count = *values->conflict_retry_count;
    }
    else {
	session->default_conflict_retry_count = ARP_CONFLICT_RETRY_COUNT;
    }
    if (values->conflict_delay_interval != NULL) {
	session->default_conflict_delay
	    = *values->conflict_delay_interval;
    }
    else {
	session->default_conflict_delay = ARP_CONFLICT_RETRY_DELAY_SECS;
    }
#ifdef TEST_ARP_SESSION
    session->next_client_index = 1;
#endif /* TEST_ARP_SESSION */
    return (session);
}

PRIVATE_EXTERN void
arp_session_free(arp_session_t * * session_p)
{
    arp_session_t * session = *session_p;

    dynarray_free(&session->if_sessions);
    bzero(session, sizeof(*session));
    free(session);
    *session_p = NULL;
    return;
}

static arp_if_session_t *
arp_session_find_if_session(arp_session_t * session, const char * ifn)
{
    int			count;
    int			i;

    count = dynarray_count(&session->if_sessions);
    for (i = 0; i < count; i++) {
	arp_if_session_t * 	if_session;

	if_session = dynarray_element(&session->if_sessions, i);
	if (strcmp(if_name(if_session->if_p), ifn) == 0) {
	    return (if_session);
	}
    }
    return (NULL);
}

static void
arp_if_session_free_element(void * arg)
{
    arp_if_session_t * if_session = (arp_if_session_t *)arg;

    /* free all of the clients, close file descriptor */
    dynarray_free(&if_session->clients);

    free(if_session);
    return;
}

static void
arp_if_session_free(arp_if_session_t * * if_session_p)
{
    arp_if_session_t * 		if_session = NULL;
    int 			i;
    arp_session_t * 		session;

    if (if_session_p != NULL) {
	if_session = *if_session_p;
    }
    if (if_session == NULL) {
	return;
    }

    /* remove from the list of if_sessions */
    session = if_session->session;
    i = dynarray_index(&session->if_sessions, if_session);
    if (i != -1) {
	dynarray_remove(&session->if_sessions, i, NULL);
    }
    else {
	my_log(LOG_NOTICE, "arp_if_session_free(%s) not in list?",
	       if_name(if_session->if_p));
    }

    /* release resources */
    arp_if_session_free_element(if_session);

    *if_session_p = NULL;
    return;
}

static arp_if_session_t *
arp_session_new_if_session(arp_session_t * session, interface_t * if_p)
{
    struct firewire_address	fw_addr;
    arp_if_session_t * 		if_session;

    if_session = arp_session_find_if_session(session, if_name(if_p));
    if (if_session != NULL) {
	return (if_session);
    }
    switch (if_link_type(if_p)) {
    case IFT_ETHER:
    case IFT_L2VLAN:
    case IFT_IEEE8023ADLAG:
	break;
    case IFT_IEEE1394:
	/* copy in the firewire address */
	if (getFireWireAddress(if_name(if_p), &fw_addr) == FALSE) {
	    my_log(LOG_ERR, 
		   "arp_client_init(%s): could not retrieve firewire address",
		   if_name(if_p));
	    return (NULL);
	}
	break;
    default:
	my_log(LOG_ERR, "arp_client_init(%s): unsupported network type",
	       if_name(if_p));
	return (NULL);
    }
    if_session = (arp_if_session_t *)malloc(sizeof(*if_session));
    bzero(if_session, sizeof(*if_session));
    dynarray_init(&if_session->clients, arp_client_free_element, NULL);
    if (if_link_type(if_p) == IFT_IEEE1394) {
	/* copy in the fw address */
	if_session->fw_addr = fw_addr;
    }
    if_session->if_p = if_p;
    if_session->session = session;
    dynarray_add(&session->if_sessions, if_session);
    return (if_session);
}

PRIVATE_EXTERN void
arp_session_set_debug(arp_session_t * session, int debug)
{
    session->debug = debug;
    return;
}

#ifdef TEST_ARP_SESSION
#include <stdarg.h>
#include "cfutil.h"
typedef boolean_t		func_t(int argc, const char * * argv);
typedef func_t * 		funcptr_t;

static arp_session_t *		S_arp_session;
static boolean_t 		S_debug = FALSE;
static func_t			S_do_probe;
static func_t			S_do_resolve;
static func_t			S_do_detect;
static func_t			S_cancel_probe;
static func_t			S_toggle_debug;
static func_t			S_new_client;
static func_t			S_free_client;
static func_t			S_list;
static func_t			S_quit;
static func_t			S_client_params;
static interface_list_t *	S_interfaces;

#define BASE_16		16

static uint8_t *
hexstrtobin(const char * str, int * len)
{
    int		buf_pos;
    uint8_t * 	buf = NULL;
    boolean_t	done = FALSE;
    int		max_decoded_len;
    const char * scan = str;
    int 	slen = strlen(str);

    *len = 0;
    /* the worst case we turn "1:2:3:4:5:6" into 6 bytes
     * strlen("1:2:3:4:5:6") = 11
     * so to get the approximate decoded length, 
     * we want strlen(str) / 2 + 1
     */
    max_decoded_len = (slen / 2) + 1;
    buf = (uint8_t *)malloc(max_decoded_len);
    if (buf == NULL) {
	return (buf);
    }
    for (buf_pos = 0; buf_pos < max_decoded_len && !done; buf_pos++) {
	char		tmp[4];
	const char *	colon;

	colon = strchr(scan, ':');
	if (colon == NULL) {
	    done = TRUE;
	    colon = str + slen;
	}
	if ((colon - scan) > (sizeof(tmp) - 1)) {
	    goto err;
	}
	strncpy(tmp, scan, colon - scan);
	tmp[colon - scan] = '\0';
	buf[buf_pos] = (u_char)strtol(tmp, NULL, BASE_16);
	scan = colon + 1;
    }
    *len = buf_pos;
    return (buf);
  err:
    if (buf) {
	 free(buf);
    }
    return (NULL);
}

struct timeval
timeval_diff(void)
{
    struct timeval 		result;
    static struct timeval	tvp = {0,0};
    struct timeval		tv;

    gettimeofday(&tv, 0);
    if (tvp.tv_sec) {
	timeval_subtract(tv, tvp, &result);
    }
    else {
	result = tvp;
    }
    tvp = tv;
    return (result);
}

static void
arp_session_log(int priority, const char * message, ...)
{
    va_list 		ap;
    struct timeval	delta;

    if (priority == LOG_INFO) {
	if (S_arp_session->debug == FALSE)
	    return;
    }
    delta = timeval_diff();
    fprintf(stderr, "%ld.%06d ", delta.tv_sec, delta.tv_usec);
    va_start(ap, message);
    vfprintf(stderr, message, ap);
    va_end(ap);
    fprintf(stderr, "\n");
    fflush(stderr);
    return;
}

static const struct command_info {
    char *	command;
    funcptr_t	func;
    int		argc;
    char *	usage;
    int		display;
} commands[] = {
    { "new", S_new_client, 1, "<ifname>", 1 },
    { "free", S_free_client, 1, "<client_index>", 1 },
    { "probe", S_do_probe, 3, "<client_index> <sender_ip> <target_ip>", 1 },
    { "resolve", S_do_resolve, 3, "<client_index> <sender_ip> <target_ip>", 1 },
    { "detect", S_do_detect, 4,
      "<client_index> [ <sender_ip> <target_ip> <target_hardware> ]+", 1 },
    { "cancel", S_cancel_probe, 1, "<client_index>", 1 },
    { "params", S_client_params, 1, "<client_index> [ default | <interval> <probes> [ <gratuitous> ] ]", 1 },
    { "debug", S_toggle_debug, 0, NULL, 1 },
    { "list", S_list, 0, NULL, 1 },
    { "quit", S_quit, 0, NULL, 1 },
    { NULL, NULL, 0 }
};

struct arg_info {
    char * *	argv;
    int		argc;
    int		argv_size;
};

static void
arg_info_init(struct arg_info * args)
{
    args->argv = NULL;
    args->argv_size = 0;
    args->argc = 0;
    return;
}

static void
arg_info_free(struct arg_info * args)
{
    if (args->argv != NULL) {
	free(args->argv);
    }
    arg_info_init(args);
    return;
}

/*
static void
arg_info_print(struct arg_info * args)
{
    int	i;

    for (i = 0; i < args->argc; i++) {
	printf("%2d. '%s'\n", i, args->argv[i]);
    }
    return;
}
*/

static void
arg_info_add(struct arg_info * args, char * new_arg)
{
    if (args->argv == NULL) {
	args->argv_size = 6;
	args->argv = (char * *)malloc(sizeof(*args->argv) * args->argv_size);
    }
    else if (args->argc == args->argv_size) {
	args->argv_size *= 2;
	args->argv = (char * *)realloc(args->argv, 
				       sizeof(*args->argv) * args->argv_size);
    }
    args->argv[args->argc++] = new_arg;
    return;
}

static int
arp_link_length(interface_t * if_p)
{
    int len;

    if (if_link_arptype(if_p) == ARPHRD_IEEE1394) {
	len = sizeof(struct firewire_eui64);
    }
    else {
	len = ETHER_ADDR_LEN;
    }
    return (len);
}


static void
arp_test(void * arg1, void * arg2, const arp_result_t * result)
{
    arp_client_t *	client = (arp_client_t *)arg1;

    if (result->error) {
	printf("ARP probe failed: '%s'\n",
	       client->errmsg);
    }
    else if (result->in_use) {
	int i;
	int len = arp_link_length(client->if_session->if_p);
	const u_char * addr = result->addr.target_hardware;

	printf("ip address " IP_FORMAT " in use by",
	       IP_LIST(&client->target_ip));
	for (i = 0; i < len; i++) {
	    printf("%c%02x", i == 0 ? ' ' : ':', addr[i]);
	}
	printf("\n");
    }
    else {
	printf("ip address " IP_FORMAT " is not in use\n", 
	       IP_LIST(&client->target_ip));
    }
}

static void
arp_detect_callback(void * arg1, void * arg2, const arp_result_t * result)
{
    arp_client_t *	client = (arp_client_t *)arg1;

    if (result->error) {
	printf("ARP detect failed: '%s'\n",
	       client->errmsg);
    }
    else if (result->in_use) {
	int i;
	int len = arp_link_length(client->if_session->if_p);
	const u_char * addr = result->addr.target_hardware;

	printf("ARP detected Sender IP " IP_FORMAT " Target IP " IP_FORMAT
	       " Target Hardware",
	       IP_LIST(&result->addr.sender_ip),
	       IP_LIST(&result->addr.target_ip));
	for (i = 0; i < len; i++) {
	    printf("%c%02x", i == 0 ? ' ' : ':', addr[i]);
	}
	printf("\n");
    }
    else {
	printf("Did not detect any IP\n");
    }
}

static boolean_t
S_toggle_debug(int argc, const char * * argv)
{
    S_debug = !S_debug;
    arp_session_set_debug(S_arp_session, S_debug);
    if (S_debug) {
	printf("debug mode enabled\n");
    }
    else {
	printf("debug mode disabled\n");
    }
    return (TRUE);
}

static boolean_t
S_quit(int argc, const char * * argv)
{
    exit(0);
    return (TRUE);
}

static boolean_t
S_new_client(int argc, const char * * argv)
{
    arp_client_t *	client = NULL;
    interface_t *	if_p;

    if_p = ifl_find_name(S_interfaces, argv[1]);
    if (if_p == NULL) {
	fprintf(stderr, "interface %s does not exist\n", argv[1]);
	goto done;
    }
    client = arp_client_init(S_arp_session, if_p);
    if (client == NULL) {
	fprintf(stderr, "Could not create a new client over %s\n", 
		if_name(if_p));
	goto done;
    }
    printf("%d\n", client->client_index);
 done:
    return (client != NULL);
}

static boolean_t
get_int_param(const char * arg, int * ret_int)
{
    char *	endptr;
    int		val;
    
    val = strtol(arg, &endptr, 0);
    if (endptr == arg || (val == 0 && errno != 0)) {
	return (FALSE);
    }
    *ret_int = val;
    return (TRUE);
}

static boolean_t
get_client_index(const char * arg, int * client_index)
{
    if (get_int_param(arg, client_index)) {
	return (TRUE);
    }
    if (strcmp(arg, ".") == 0) {
	*client_index = S_arp_session->next_client_index - 1;
	return (TRUE);
    }
    return (FALSE);
}

static boolean_t
S_do_probe(int argc, const char * * argv)
{
    arp_client_t *	client = NULL;
    int			client_index;
    struct in_addr 	sender_ip;
    struct in_addr     	target_ip;

    if (get_client_index(argv[1], &client_index) == FALSE) {
	fprintf(stderr, "invalid client index\n");
	goto done;
    }
    client = arp_session_find_client_with_index(S_arp_session, client_index);
    if (client == NULL) {
	fprintf(stderr, "no such client index\n");
	goto done;
    }
    if (inet_aton(argv[2], &sender_ip) == 0) {
	fprintf(stderr, "invalid sender ip address %s\n", argv[2]);
	client = NULL;
	goto done;
    }
    if (inet_aton(argv[3], &target_ip) == 0) {
	fprintf(stderr, "invalid target ip address %s\n", argv[3]);
	client = NULL;
	goto done;
    }
    arp_client_probe(client, arp_test, client, NULL, sender_ip, target_ip);
 done:
    return (client != NULL);
}

static boolean_t
S_do_resolve(int argc, const char * * argv)
{
    arp_client_t *	client = NULL;
    int			client_index;
    struct in_addr 	sender_ip;
    struct in_addr     	target_ip;

    if (get_client_index(argv[1], &client_index) == FALSE) {
	fprintf(stderr, "invalid client index\n");
	goto done;
    }
    client = arp_session_find_client_with_index(S_arp_session, client_index);
    if (client == NULL) {
	fprintf(stderr, "no such client index\n");
	goto done;
    }
    if (inet_aton(argv[2], &sender_ip) == 0) {
	fprintf(stderr, "invalid sender ip address %s\n", argv[2]);
	client = NULL;
	goto done;
    }
    if (inet_aton(argv[3], &target_ip) == 0) {
	fprintf(stderr, "invalid target ip address %s\n", argv[3]);
	client = NULL;
	goto done;
    }
    arp_client_resolve(client, arp_test, client, NULL, sender_ip, target_ip, 0);
 done:
    return (client != NULL);
}

static boolean_t
S_do_detect(int argc, const char * * argv)
{
    arp_client_t *	client = NULL;
    int			client_index;
    uint8_t *		hwaddr = NULL;
    int			hwaddr_len;
    int			i;
    arp_address_info_t *info = NULL;
    int			info_count;

    if (get_client_index(argv[1], &client_index) == FALSE) {
	fprintf(stderr, "invalid client index\n");
	goto done;
    }
    client = arp_session_find_client_with_index(S_arp_session, client_index);
    if (client == NULL) {
	fprintf(stderr, "no such client index\n");
	goto done;
    }
    if (((argc - 2) % 3) != 0) {
	fprintf(stderr, "incorrect number of arguments\n");
	client = NULL;
	goto done;
    }
    info_count = (argc - 2) / 3;
    info = malloc(sizeof(*info) * info_count);
    for (i = 0; i < info_count; i++) {
	if (inet_aton(argv[3 * i + 2], &info[i].sender_ip) == 0) {
	    fprintf(stderr, "invalid sender ip address %s\n", argv[3 * i + 2]);
	    client = NULL;
	    goto done;
	}
	if (inet_aton(argv[3 * i + 3], &info[i].target_ip) == 0) {
	    fprintf(stderr, "invalid target ip address %s\n", argv[3 * i + 3]);
	    client = NULL;
	    goto done;
	}
	/* get the hardware address */
	hwaddr = hexstrtobin(argv[3 * i + 4], &hwaddr_len);
	if (hwaddr == NULL
	    || (hwaddr_len != ETHER_ADDR_LEN 
		&& hwaddr_len != FIREWIRE_EUI64_LEN)) {
	    fprintf(stderr, "invalid hardware address %s\n", argv[3 * i + 4]);
	    client = NULL;
	    if (hwaddr != NULL) {
		free(hwaddr);
	    }
	    goto done;
	}
	bcopy(hwaddr, info[i].target_hardware, hwaddr_len);
	free(hwaddr);
    }
    arp_client_detect(client, 
		      arp_detect_callback, client, NULL,
		      info, info_count);
 done:
    if (info != NULL) {
	free(info);
    }
    return (client != NULL);
}

static boolean_t
S_free_client(int argc, const char * * argv)
{
    arp_client_t *	client = NULL;
    int			client_index;

    if (get_client_index(argv[1], &client_index) == FALSE) {
	fprintf(stderr, "invalid client index\n");
	goto done;
    }
    client = arp_session_find_client_with_index(S_arp_session, client_index);
    if (client == NULL) {
	fprintf(stderr, "no such client index\n");
	goto done;
    }
    arp_client_free(&client);
 done:
    return (client != NULL);
}

static boolean_t
S_cancel_probe(int argc, const char * * argv)
{
    arp_client_t *	client = NULL;
    int			client_index;

    if (get_client_index(argv[1], &client_index) == FALSE) {
	fprintf(stderr, "invalid client index\n");
	goto done;
    }
    client = arp_session_find_client_with_index(S_arp_session, client_index);
    if (client == NULL) {
	fprintf(stderr, "no such client index\n");
	goto done;
    }
    arp_client_cancel(client);

 done:
    return (client != NULL);
}

static boolean_t
S_client_params(int argc, const char * * argv)
{
    arp_client_t *	client = NULL;
    int			client_index;
    struct probe_info *	probe_info;

    if (get_client_index(argv[1], &client_index) == FALSE) {
	fprintf(stderr, "invalid client index\n");
	goto done;
    }
    client = arp_session_find_client_with_index(S_arp_session, client_index);
    if (client == NULL) {
	fprintf(stderr, "no such client index\n");
	goto done;
    }
    if (argc == 2) {
	/* display only */
    }
    else if (strcmp(argv[2], "default") == 0) {
	if (argc != 3) {
	    fprintf(stderr, "Too many parameters specified\n");
	    client = NULL;
	    goto done;
	}
	arp_client_restore_default_probe_info(client);
    }
    else if (argc < 4) {
	fprintf(stderr, "insufficient args\n");
	client = NULL;
	goto done;
    }
    else {
	char *		endptr;
	int		gratuitous_count = 0;
	int		probe_count;
	CFTimeInterval	interval;

	if (argc > 5) {
	    fprintf(stderr, "Too many parameters specified\n");
	    client = NULL;
	    goto done;
	}
	interval = strtod(argv[2], &endptr);
	if (endptr == argv[2] || (interval == 0 && errno != 0)) {
	    fprintf(stderr, "Invalid probe interval specified\n");
	    client = NULL;
	    goto done;
	}
	if (get_int_param(argv[3], &probe_count) == FALSE) {
	    fprintf(stderr, "Invalid probe count specified\n");
	    client = NULL;
	    goto done;
	}
	if (argc == 5) {
	    if (get_int_param(argv[4], &gratuitous_count) == FALSE) {
		fprintf(stderr, "Invalid gratuitous count specified\n");
		client = NULL;
		goto done;
	    }
	}
	arp_client_set_probe_info(client, &interval, &probe_count,
				  &gratuitous_count);
    }
    probe_info = &client->probe_info;
    printf("Probe interval %g probes %d gratuitous %d\n",
	   probe_info->retry_interval,
	   probe_info->probe_count, probe_info->gratuitous_count);

 done:
    return (client != NULL);
}
    
static void
dump_bytes(const unsigned char * buf, int buf_len)
{
    int i;

    for (i = 0; i < buf_len; i++) {
	printf("%c%02x", i == 0 ? ' ' : ':', buf[i]);
    }
    return;
}

static boolean_t
S_list(int argc, const char * * argv)
{
    int		if_sessions_count;
    int		i;
    int		j;

    if_sessions_count = dynarray_count(&S_arp_session->if_sessions);
    for (i = 0; i < if_sessions_count; i++) {
	int			clients_count;
	arp_if_session_t *	if_session;
	int			len;

	if_session = dynarray_element(&S_arp_session->if_sessions, i);
	clients_count = dynarray_count(&if_session->clients);
	len = arp_link_length(if_session->if_p);
	for (j = 0; j < clients_count; j++) {
	    arp_client_t *	client;

	    client = dynarray_element(&if_session->clients, j);
	    printf("%d. %s", client->client_index, if_name(if_session->if_p));
	    switch (client->command_status) {
	    case arp_status_none_e:
		printf(" idle");
		break;
	    case arp_status_unknown_e:
		switch (client->command) {
		case arp_client_command_probe_e:
		    printf(" probing %s", inet_ntoa(client->target_ip));
		    break;
		case arp_client_command_resolve_e:
		    printf(" resolving %s", inet_ntoa(client->target_ip));
		    break;
		case arp_client_command_detect_e:
		    printf(" detecting");
		    break;
		default:
		    break;
		}
		break;
	    case arp_status_in_use_e:
		printf(" %s in use by", 
		       inet_ntoa(client->in_use_addr.target_ip));
		dump_bytes((unsigned char *)client->in_use_addr.target_hardware,
			   len);
		break;
	    case arp_status_not_in_use_e:
		printf(" %s not in use", inet_ntoa(client->target_ip));
		break;
	    case arp_status_error_e:
		printf(" %s error encountered", inet_ntoa(client->target_ip));
		break;
	    }
	    printf("\n");
	}
    }
    return (TRUE);
}

static void
parse_command(char * buf, struct arg_info * args)
{
    char *		arg_start = NULL;
    char *		scan;

    for (scan = buf; *scan != '\0'; scan++) {
	char ch = *scan;

	switch (ch) {
	case ' ':
	case '\n':
	case '\t':
	    *scan = '\0';
	    if (arg_start != NULL) {
		arg_info_add(args, arg_start);
		arg_start = NULL;
	    }
	    break;
	default:
	    if (arg_start == NULL) {
		arg_start = scan;
	    }
	    break;
	}
    }
    if (arg_start != NULL) {
	arg_info_add(args, arg_start);
    }
    return;
}

void
usage()
{
    int i;

    fprintf(stderr, "Available commands: ");
    for (i = 0; commands[i].command; i++) {
	if (commands[i].display) {
	    fprintf(stderr, "%s%s",  i == 0 ? "" : ", ",
		    commands[i].command);
	}
    }
    fprintf(stderr, "\n");
    return;
}

static const struct command_info *
S_lookup_command(char * cmd)
{
    int i;

    for (i = 0; commands[i].command; i++) {
	if (strcmp(cmd, commands[i].command) == 0) {
	    return commands + i;
	}
    }
    return (NULL);
}

static void
process_command(struct arg_info * args)
{
    boolean_t			ok = TRUE;
    const struct command_info *	cmd_info;

    cmd_info = S_lookup_command(args->argv[0]);
    if (cmd_info != NULL) {
	if (cmd_info->argc >= args->argc) {
	    ok = FALSE;
	    if (cmd_info->display) {
		fprintf(stderr, "insufficient args\nusage:\n\t%s %s\n", 
			args->argv[0],
			cmd_info->usage ? cmd_info->usage : "");
	    }
	}
    }
    else {
	ok = FALSE;
	usage();
    }

    if (ok) {
	if ((*cmd_info->func)(args->argc, (const char * *)args->argv)
	    == FALSE) {
	    fprintf(stderr, "usage:\n\t%s %s\n", 
		    args->argv[0],
		    cmd_info->usage ? cmd_info->usage : "");
	}
    }
    return;
}

static void
display_prompt(FILE * f)
{
    fprintf(f, "# ");
    fflush(f);
    return;
}

static void
user_input(CFSocketRef s, CFSocketCallBackType type, 
	   CFDataRef address, const void *data, void *info)
{
    struct arg_info	args;
    char 		choice[1024 * 10];

    if (fgets(choice, sizeof(choice), stdin) == NULL) {
	exit(1);
    }
    arg_info_init(&args);
    parse_command(choice, &args);
    if (args.argv == NULL) {
	goto done;
    }
    process_command(&args);

 done:
    arg_info_free(&args);
    display_prompt(stdout);
    return;
}

static void
initialize_input()
{
    CFSocketContext	context = { 0, NULL, NULL, NULL, NULL };
    CFRunLoopSourceRef	rls = NULL;
    CFSocketRef		socket = NULL;

    /* context.info = NULL; */
    socket = CFSocketCreateWithNative(NULL, fileno(stdin), 
				      kCFSocketReadCallBack,
				      user_input, &context);
    if (socket == NULL) {
	fprintf(stderr, "CFSocketCreateWithNative failed\n");
	exit(1);
    }
    rls = CFSocketCreateRunLoopSource(NULL, socket, 0);
    if (rls == NULL) {
	fprintf(stderr, "CFSocketCreateRunLoopSource failed\n");
	exit(1);
    }
    CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);
    my_CFRelease(&rls);
    my_CFRelease(&socket);
    display_prompt(stdout);
    return;
}

int
main(int argc, char * argv[])
{
    arp_session_values_t	arp_values;
    int				gratuitous = 0;

    S_interfaces = ifl_init(FALSE);
    if (S_interfaces == NULL) {
	fprintf(stderr, "couldn't get interface list\n");
	exit(1);
    }

    /* initialize the default values structure */
    bzero(&arp_values, sizeof(arp_values));
    arp_values.probe_gratuitous_count = &gratuitous;
    S_arp_session = arp_session_init(NULL, &arp_values);
    if (S_arp_session == NULL) {
	fprintf(stderr, "arp_session_init failed\n");
	exit(1);
    }
    initialize_input();
    CFRunLoopRun();
    exit(0);
}
#endif /* TEST_ARP_SESSION */
