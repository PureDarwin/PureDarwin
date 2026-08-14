/*
 * Copyright (c) 2009-2018, 2020 Apple Inc. All rights reserved.
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
 * DHCPv6Client.c
 * - API's to instantiate and interact with DHCPv6Client
 */

/* 
 * Modification History
 *
 * September 22, 2009		Dieter Siegmund (dieter@apple.com)
 * - created
 *
 * May 14, 2010			Dieter Siegmund (dieter@apple.com)
 * - implemented stateful support
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <mach/boolean.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>
#include "DHCPv6.h"
#include "DHCPv6Client.h"
#include "DHCPv6Options.h"
#include "DHCPv6Socket.h"
#include "DHCPDUIDIAID.h"
#include "timer.h"
#include "ipconfigd_threads.h"
#include "interfaces.h"
#include "cfutil.h"
#include "DNSNameList.h"
#include "symbol_scope.h"
#include "ipconfigd_threads.h"

typedef void
(DHCPv6ClientEventFunc)(DHCPv6ClientRef client, IFEventID_t event_id, 
			void * event_data);
typedef DHCPv6ClientEventFunc * DHCPv6ClientEventFuncRef;

typedef enum {
    kDHCPv6ClientStateInactive = 0,
    kDHCPv6ClientStateSolicit,
    kDHCPv6ClientStateRequest,
    kDHCPv6ClientStateBound,
    kDHCPv6ClientStateRenew,
    kDHCPv6ClientStateRebind,
    kDHCPv6ClientStateConfirm,
    kDHCPv6ClientStateRelease,
    kDHCPv6ClientStateUnbound,
    kDHCPv6ClientStateDecline,
    kDHCPv6ClientStateInform,
 } DHCPv6ClientState;

STATIC const char *
DHCPv6ClientStateGetName(DHCPv6ClientState cstate)
{
    STATIC const char * names[] = {
	"Inactive",
	"Solicit",
	"Request",
	"Bound",
	"Renew",
	"Rebind",
	"Confirm",
	"Release",
	"Unbound",
	"Decline",
	"Inform",
    };
    const int 		names_count = sizeof(names) / sizeof(names[0]);

    if (cstate >= 0 && cstate < names_count) {
	return (names[cstate]);
    }
    return ("<unknown>");
}

#if 0
STATIC const char *
DHCPv6ClientModeGetName(DHCPv6ClientMode mode)
{
    STATIC const char * names[] = {
	"Idle",
	"Stateless",
	"Stateful",
    };
    const int 		names_count = sizeof(names) / sizeof(names[0]);

    if (mode < names_count) {
	return (names[mode]);
    }
    return ("<unknown>");
}
#endif

typedef struct {
    CFAbsoluteTime		start;

    /* these times are all relative to start */
    uint32_t			t1;
    uint32_t			t2;
    uint32_t			valid_lifetime;
    uint32_t			preferred_lifetime;
    bool			valid;
} lease_info_t;

typedef struct {
    DHCPv6PacketRef		pkt;
    int				pkt_len;
    DHCPv6OptionListRef		options;
} dhcpv6_info_t;

struct DHCPv6Client {
    CFRunLoopSourceRef			callback_rls;
    DHCPv6ClientNotificationCallBack 	callback;
    void *				callback_arg;
    struct in6_addr			our_ip;
    int					our_prefix_length;
    DHCPv6ClientMode			mode;
    DHCPv6ClientState			cstate;
    DHCPv6SocketRef			sock;
    timer_callout_t *			timer;
    uint32_t				transaction_id;
    int					try;
    CFAbsoluteTime			start_time;
    CFTimeInterval			retransmit_time;
    dhcpv6_info_t			saved;
    bool				saved_verified;
    DHCPDUIDRef				server_id; /* points to saved */
    DHCPv6OptionIA_NARef		ia_na;	   /* points to saved */	
    DHCPv6OptionIAADDRRef		ia_addr;   /* points to saved */
    lease_info_t			lease;
};

STATIC DHCPv6ClientEventFunc	DHCPv6Client_Bound;
STATIC DHCPv6ClientEventFunc	DHCPv6Client_Unbound;
STATIC DHCPv6ClientEventFunc	DHCPv6Client_Solicit;
STATIC DHCPv6ClientEventFunc	DHCPv6Client_Request;
STATIC DHCPv6ClientEventFunc	DHCPv6Client_RenewRebind;

INLINE interface_t *
DHCPv6ClientGetInterface(DHCPv6ClientRef client)
{
    return (DHCPv6SocketGetInterface(client->sock));
}

STATIC const uint16_t	DHCPv6RequestedOptionsStatic[] = {
    kDHCPv6OPTION_DNS_SERVERS,
    kDHCPv6OPTION_DOMAIN_LIST,
    kDHCPv6OPTION_CAPTIVE_PORTAL_URL
};
#define kDHCPv6RequestedOptionsStaticCount 	(sizeof(DHCPv6RequestedOptionsStatic) / sizeof(DHCPv6RequestedOptionsStatic[0]))

STATIC uint16_t *	DHCPv6RequestedOptionsDefault = (uint16_t *)DHCPv6RequestedOptionsStatic;
STATIC int		DHCPv6RequestedOptionsDefaultCount = kDHCPv6RequestedOptionsStaticCount;

STATIC uint16_t *	DHCPv6RequestedOptions = (uint16_t *)DHCPv6RequestedOptionsStatic;
STATIC int		DHCPv6RequestedOptionsCount =  kDHCPv6RequestedOptionsStaticCount;

STATIC int
S_get_prefix_length(const struct in6_addr * addr, int if_index)
{
    int	prefix_length;

    prefix_length = inet6_get_prefix_length(addr, if_index);
    if (prefix_length == 0) {
#define DHCPV6_PREFIX_LENGTH		128
	prefix_length = DHCPV6_PREFIX_LENGTH;
    }
    return (prefix_length);
}

PRIVATE_EXTERN void
DHCPv6ClientSetRequestedOptions(uint16_t * requested_options,
				int requested_options_count)
{
    if (requested_options != NULL && requested_options_count != 0) {
	DHCPv6RequestedOptionsDefault = requested_options;
	DHCPv6RequestedOptionsDefaultCount = requested_options_count;
    }
    else {
	DHCPv6RequestedOptionsDefault 
	    = (uint16_t *)DHCPv6RequestedOptionsStatic;
	DHCPv6RequestedOptionsDefaultCount 
	    = kDHCPv6RequestedOptionsStaticCount;
    }
    DHCPv6RequestedOptions = DHCPv6RequestedOptionsDefault;
    DHCPv6RequestedOptionsCount = DHCPv6RequestedOptionsDefaultCount;
    return;
}

PRIVATE_EXTERN bool
DHCPv6ClientOptionIsOK(int option)
{
    int i;

    switch (option) {
    case kDHCPv6OPTION_CLIENTID:
    case kDHCPv6OPTION_SERVERID:
    case kDHCPv6OPTION_ORO:
    case kDHCPv6OPTION_ELAPSED_TIME:
    case kDHCPv6OPTION_UNICAST:
    case kDHCPv6OPTION_RAPID_COMMIT:
    case kDHCPv6OPTION_IA_NA:
    case kDHCPv6OPTION_IAADDR:
    case kDHCPv6OPTION_STATUS_CODE:
    case kDHCPv6OPTION_IA_TA:
    case kDHCPv6OPTION_PREFERENCE:
    case kDHCPv6OPTION_RELAY_MSG:
    case kDHCPv6OPTION_AUTH:
    case kDHCPv6OPTION_USER_CLASS:
    case kDHCPv6OPTION_VENDOR_CLASS:
    case kDHCPv6OPTION_VENDOR_OPTS:
    case kDHCPv6OPTION_INTERFACE_ID:
    case kDHCPv6OPTION_RECONF_MSG:
    case kDHCPv6OPTION_RECONF_ACCEPT:
	return (TRUE);
    default:
	break;
    }

    for (i = 0; i < DHCPv6RequestedOptionsCount; i++) {
	if (DHCPv6RequestedOptions[i] == option) {
	    return (TRUE);
	}
    }
    return (FALSE);
}

STATIC double
random_double_in_range(double bottom, double top)
{
    double		r = (double)arc4random() / (double)UINT32_MAX;
    
    return (bottom + (top - bottom) * r);
}

STATIC uint32_t
get_new_transaction_id(void)
{
    uint32_t	r = arc4random();

#define LOWER_24_BITS	((uint32_t)0x00ffffff)
    /* return the lower 24 bits */
    return (r & LOWER_24_BITS);
}

STATIC bool
S_insert_duid(DHCPv6OptionAreaRef oa_p)
{
    CFDataRef			data;
    DHCPv6OptionErrorString 	err;

    data = DHCPDUIDGet(get_interface_list());
    if (data == NULL) {
	return (FALSE);
    }
    if (DHCPv6OptionAreaAddOption(oa_p, kDHCPv6OPTION_CLIENTID,
				  (int)CFDataGetLength(data),
				  CFDataGetBytePtr(data),
				  &err) == FALSE) {
	my_log(LOG_NOTICE, "DHCPv6Client: failed to add CLIENTID, %s",
	       err.str);
	return (FALSE);
    }
    return (TRUE);
}

STATIC bool
S_duid_matches(DHCPv6OptionListRef options)
{
    CFDataRef		data;
    DHCPDUIDRef		duid;
    int			option_len;

    data = DHCPDUIDGet(get_interface_list());
    duid = (DHCPDUIDRef)
	DHCPv6OptionListGetOptionDataAndLength(options,
					       kDHCPv6OPTION_CLIENTID,
					       &option_len, NULL);
    if (duid == NULL
	|| CFDataGetLength(data) != option_len
	|| bcmp(duid, CFDataGetBytePtr(data), option_len) != 0) {
	return (FALSE);
    }
    return (TRUE);
}

STATIC DHCPv6OptionIA_NARef
get_ia_na_addr(DHCPv6ClientRef client, int msg_type,
	       DHCPv6OptionListRef options,
	       DHCPv6OptionIAADDRRef * ret_ia_addr)
{
    DHCPv6OptionErrorString 	err;
    DHCPv6OptionIA_NARef	ia_na;
    DHCPv6OptionListRef		ia_na_options;
    interface_t *		if_p = DHCPv6ClientGetInterface(client);
    int				option_len;
    uint32_t			preferred_lifetime;
    int				start_index;
    uint32_t			t1;
    uint32_t			t2;
    uint32_t			valid_lifetime;

    *ret_ia_addr = NULL;
    ia_na = (DHCPv6OptionIA_NARef)
	DHCPv6OptionListGetOptionDataAndLength(options,
					       kDHCPv6OPTION_IA_NA,
					       &option_len, NULL);
    if (ia_na == NULL
	|| option_len <= DHCPv6OptionIA_NA_MIN_LENGTH) {
	/* no IA_NA option */
	return (NULL);
    }
    t1 = DHCPv6OptionIA_NAGetT1(ia_na);
    t2 = DHCPv6OptionIA_NAGetT2(ia_na);
    if (t1 != 0 && t2 != 0) {
	if (t1 > t2) {
	    /* server is confused */
	    return (NULL);
	}
    }
    option_len -= DHCPv6OptionIA_NA_MIN_LENGTH;
    ia_na_options = DHCPv6OptionListCreate(ia_na->options, option_len, &err);
    if (ia_na_options == NULL) {
	my_log(LOG_INFO,
	       "DHCPv6 %s: %s IA_NA contains no options",
	       if_name(if_p), DHCPv6MessageName(msg_type));
	/* no options */
	return (NULL);
    }
    /* find the first ia_addr with non-zero lifetime */
    start_index = 0;
    for (start_index = 0; TRUE; start_index++) {
	DHCPv6OptionIAADDRRef	ia_addr;
	
	ia_addr = (DHCPv6OptionIAADDRRef)
	    DHCPv6OptionListGetOptionDataAndLength(ia_na_options,
						   kDHCPv6OPTION_IAADDR,
						   &option_len, &start_index);
	if (ia_addr == NULL
	    || option_len < DHCPv6OptionIAADDR_MIN_LENGTH) {
	    my_log(LOG_INFO,
		   "DHCPv6 %s: %s IA_NA contains no valid IAADDR option",
		   if_name(if_p), DHCPv6MessageName(msg_type));
	    /* missing/invalid IAADDR option */
	    break;
	}
	valid_lifetime = DHCPv6OptionIAADDRGetValidLifetime(ia_addr);
	preferred_lifetime 
	    = DHCPv6OptionIAADDRGetPreferredLifetime(ia_addr);
	if (valid_lifetime == 0) {
	    my_log(LOG_INFO,
		   "DHCP %s: %s IA_ADDR has valid/preferred lifetime is 0,"
		   " skipping",
		   if_name(if_p), DHCPv6MessageName(msg_type));
	}
	else if (preferred_lifetime > valid_lifetime) {
	    /* server is confused */
	    my_log(LOG_INFO,
		   "DHCP %s: %s IA_ADDR preferred %d > valid lifetime %d",
		   if_name(if_p), DHCPv6MessageName(msg_type),
		   preferred_lifetime, valid_lifetime);
	    break;
	}
	else {
	    *ret_ia_addr = ia_addr;
	    break;
	}
    }

    DHCPv6OptionListRelease(&ia_na_options);

    /* if we didn't find a suitable IAADDR, then ignore the IA_NA */
    if (*ret_ia_addr == NULL) {
	ia_na = NULL;
    }
    return (ia_na);
}

STATIC uint8_t
get_preference_value_from_options(DHCPv6OptionListRef options)
{
    int				option_len;
    DHCPv6OptionPREFERENCERef	pref;
    uint8_t			value = kDHCPv6OptionPREFERENCEMinValue;

    pref = (DHCPv6OptionPREFERENCERef)
	DHCPv6OptionListGetOptionDataAndLength(options,
					       kDHCPv6OPTION_PREFERENCE,
					       &option_len, NULL);
    if (pref != NULL 
	&& option_len >= DHCPv6OptionPREFERENCE_MIN_LENGTH) {
	value = pref->value;
    }
    return (value);
}

#define OUR_IA_NA_SIZE	(DHCPv6OptionIA_NA_MIN_LENGTH + DHCPV6_OPTION_HEADER_SIZE + DHCPv6OptionIAADDR_MIN_LENGTH)

STATIC bool
add_ia_na_option(DHCPv6ClientRef client, DHCPv6OptionAreaRef oa_p,
		 DHCPv6OptionErrorStringRef err_p)
{
    char				buf[OUR_IA_NA_SIZE];
    DHCPv6OptionIA_NARef		ia_na_p;
    DHCPv6OptionRef			option;
    DHCPv6OptionIAADDRRef		ia_addr_p;
    interface_t *			if_p = DHCPv6ClientGetInterface(client);

    ia_na_p = (DHCPv6OptionIA_NARef)buf;
    DHCPv6OptionIA_NASetIAID(ia_na_p, DHCPIAIDGet(if_name(if_p)));
    DHCPv6OptionIA_NASetT1(ia_na_p, 0);
    DHCPv6OptionIA_NASetT2(ia_na_p, 0);
    option = (DHCPv6OptionRef)(buf + DHCPv6OptionIA_NA_MIN_LENGTH);
    DHCPv6OptionSetCode(option, kDHCPv6OPTION_IAADDR);
    DHCPv6OptionSetLength(option, DHCPv6OptionIAADDR_MIN_LENGTH);
    ia_addr_p = (DHCPv6OptionIAADDRRef)
	(((char *)option) + DHCPV6_OPTION_HEADER_SIZE);
    DHCPv6OptionIAADDRSetAddress(ia_addr_p, 
				 DHCPv6OptionIAADDRGetAddress(client->ia_addr));
    DHCPv6OptionIAADDRSetPreferredLifetime(ia_addr_p, 0);
    DHCPv6OptionIAADDRSetValidLifetime(ia_addr_p, 0);
    return (DHCPv6OptionAreaAddOption(oa_p, kDHCPv6OPTION_IA_NA,
				      OUR_IA_NA_SIZE, ia_na_p, err_p));
}


/*
 * Function: option_data_get_length
 * Purpose:
 *   Given a pointer to the option data, return its length, which is stored
 *   in the previous 2 bytes.
 */
STATIC int
option_data_get_length(const void * option_data)
{
    const uint16_t *	len_p;

    len_p = (const uint16_t *)(option_data - sizeof(uint16_t));
    return (ntohs(*len_p));
}

STATIC CFTimeInterval
DHCPv6_RAND(void)
{
    return (random_double_in_range(-0.1, 0.1));
}

STATIC CFTimeInterval
DHCPv6SubsequentTimeout(CFTimeInterval RTprev, CFTimeInterval MRT)
{
    CFTimeInterval	RT;

    RT = 2 * RTprev + DHCPv6_RAND() * RTprev;
    if (MRT != 0 && RT > MRT) {
	RT = MRT + DHCPv6_RAND() * MRT;
    }
    return (RT);
}

STATIC CFTimeInterval
DHCPv6InitialTimeout(CFTimeInterval IRT)
{
    return (IRT + DHCPv6_RAND() * IRT);
}

STATIC uint16_t
get_elapsed_time(DHCPv6ClientRef client)
{
    uint16_t	elapsed_time;

    if (client->try == 1) {
	elapsed_time = 0;
    }
    else {
	uint32_t	elapsed;

	/* elapsed time is in 1/100ths of a second */
	elapsed = (timer_get_current_time() - client->start_time) * 100;
#define MAX_ELAPSED	0xffff
	if (elapsed > MAX_ELAPSED) {
	    elapsed_time = MAX_ELAPSED;
	}
	else {
	    elapsed_time = htons(elapsed);
	}
    }
    return (elapsed_time);
}

/**
 ** DHCPv6Client routines
 **/

PRIVATE_EXTERN bool
DHCPv6ClientIsActive(DHCPv6ClientRef client)
{
    return (DHCPv6SocketReceiveIsEnabled(client->sock));
}

PRIVATE_EXTERN bool
DHCPv6ClientHasDNS(DHCPv6ClientRef client, bool * search_available)
{
    const uint8_t *	search;
    int			search_len;
    const uint8_t *	servers;
    int			servers_len;

    *search_available = FALSE;

    /* check for DNSServers, DNSDomainList options */
    if (client->saved.options == NULL) {
	return (FALSE);
    }
    search = DHCPv6OptionListGetOptionDataAndLength(client->saved.options,
						    kDHCPv6OPTION_DOMAIN_LIST,
						    &search_len, NULL);
    if (search != NULL && search_len > 0) {
	*search_available = TRUE;
    }
    servers = DHCPv6OptionListGetOptionDataAndLength(client->saved.options,
						     kDHCPv6OPTION_DNS_SERVERS,
						     &servers_len, NULL);
    return (servers != NULL && (servers_len / sizeof(struct in6_addr)) != 0);
}

STATIC void
DHCPv6ClientSetState(DHCPv6ClientRef client, DHCPv6ClientState cstate)
{
    interface_t *	if_p = DHCPv6ClientGetInterface(client);

    client->cstate = cstate;
    my_log(LOG_INFO, "DHCPv6 %s: %s", if_name(if_p),
	   DHCPv6ClientStateGetName(cstate));
}

STATIC void
DHCPv6ClientRemoveAddress(DHCPv6ClientRef client, const char * label)
{
    interface_t *	if_p = DHCPv6ClientGetInterface(client);
    char 		ntopbuf[INET6_ADDRSTRLEN];
    int			s;

    if (IN6_IS_ADDR_UNSPECIFIED(&client->our_ip)) {
	return;
    }
    my_log(LOG_INFO, "DHCPv6 %s: %s: removing %s",
	   if_name(if_p), label,
	   inet_ntop(AF_INET6, &client->our_ip,
		     ntopbuf, sizeof(ntopbuf)));
    s = inet6_dgram_socket();
    if (s < 0) {
	my_log(LOG_NOTICE,
	       "DHCPv6ClientRemoveAddress(%s):socket() failed, %s (%d)",
	       if_name(if_p), strerror(errno), errno);
    }
    else {
	if (inet6_difaddr(s, if_name(if_p), &client->our_ip) < 0) {
	    my_log(LOG_INFO,
		   "DHCPv6ClientRemoveAddress(%s): remove %s failed, %s (%d)",
		   if_name(if_p),
		   inet_ntop(AF_INET6, &client->our_ip,
			     ntopbuf, sizeof(ntopbuf)),
		   strerror(errno), errno);
	}
	close(s);
    }
    bzero(&client->our_ip, sizeof(client->our_ip));
    client->our_prefix_length = 0;
    return;
}

STATIC void
DHCPv6ClientClearRetransmit(DHCPv6ClientRef client)
{
    client->try = 0;
    return;
}

STATIC CFTimeInterval
DHCPv6ClientNextRetransmit(DHCPv6ClientRef client,
			   CFTimeInterval IRT, CFTimeInterval MRT)
{
    client->try++;
    if (client->try == 1) {
	client->retransmit_time = DHCPv6InitialTimeout(IRT);
    }
    else {
	client->retransmit_time
	    = DHCPv6SubsequentTimeout(client->retransmit_time, MRT);
    }
    return (client->retransmit_time);
}

STATIC void
DHCPv6ClientPostNotification(DHCPv6ClientRef client)
{
    if (client->callback_rls != NULL) {
	CFRunLoopSourceSignal(client->callback_rls);
    }
    return;
}

STATIC void
DHCPv6ClientCancelPendingEvents(DHCPv6ClientRef client)
{
    DHCPv6SocketDisableReceive(client->sock);
    timer_cancel(client->timer);
    return;
}

STATIC void
DHCPv6ClientClearPacket(DHCPv6ClientRef client)
{
    if (client->saved.pkt_len == 0) {
	return;
    }
    bzero(&client->lease, sizeof(client->lease));
    client->saved.pkt_len = 0;
    if (client->saved.pkt != NULL) {
	free(client->saved.pkt);
	client->saved.pkt = NULL;
    }
    DHCPv6OptionListRelease(&client->saved.options);
    client->server_id = NULL;
    client->ia_na = NULL;
    client->ia_addr = NULL;
    client->saved_verified = FALSE;
    return;
}

STATIC void
DHCPv6ClientInactive(DHCPv6ClientRef client)
{
    DHCPv6ClientCancelPendingEvents(client);
    DHCPv6ClientClearPacket(client);
    DHCPv6ClientRemoveAddress(client, "Inactive");
    DHCPv6ClientPostNotification(client);
    return;
}

STATIC bool
DHCPv6ClientLeaseStillValid(DHCPv6ClientRef client)
{
    CFAbsoluteTime		current_time;
    lease_info_t *		lease_p = &client->lease;

    if (lease_p->valid == FALSE) {
	goto done;
    }
    if (lease_p->valid_lifetime == DHCP_INFINITE_LEASE) {
	goto done;
    }
    current_time = timer_get_current_time();
    if (current_time < lease_p->start) {
	/* time went backwards */
	DHCPv6ClientClearPacket(client);
	lease_p->valid = FALSE;
	goto done;
    }
    if ((current_time - lease_p->start) >= lease_p->valid_lifetime) {
	/* expired */
	DHCPv6ClientClearPacket(client);
	lease_p->valid = FALSE;
    }

 done:
    return (lease_p->valid);
}

STATIC void
DHCPv6ClientSavePacket(DHCPv6ClientRef client, DHCPv6SocketReceiveDataRef data)
{
    CFAbsoluteTime		current_time = timer_get_current_time();
    DHCPv6OptionErrorString 	err;
    lease_info_t *		lease_p = &client->lease;
    int				option_len;
    uint32_t			preferred_lifetime;
    uint32_t			t1;
    uint32_t			t2;
    uint32_t			valid_lifetime;

    DHCPv6ClientClearPacket(client);
    client->saved.pkt_len = data->pkt_len;
    client->saved.pkt = malloc(client->saved.pkt_len);
    bcopy(data->pkt, client->saved.pkt, client->saved.pkt_len);
    client->saved.options 
	= DHCPv6OptionListCreateWithPacket(client->saved.pkt,
					   client->saved.pkt_len, &err);
    client->server_id = (DHCPDUIDRef)
	DHCPv6OptionListGetOptionDataAndLength(client->saved.options,
					       kDHCPv6OPTION_SERVERID,
					       &option_len, NULL);
    client->ia_na = get_ia_na_addr(client, client->saved.pkt->msg_type,
				   client->saved.options,  &client->ia_addr);
    if (client->ia_na != NULL) {
	t1 = DHCPv6OptionIA_NAGetT1(client->ia_na);
	t2 = DHCPv6OptionIA_NAGetT2(client->ia_na);
	valid_lifetime = DHCPv6OptionIAADDRGetValidLifetime(client->ia_addr);
	preferred_lifetime 
	    = DHCPv6OptionIAADDRGetPreferredLifetime(client->ia_addr);
	if (preferred_lifetime == 0) {
	    preferred_lifetime = valid_lifetime;
	}
	if (t1 == 0 || t2 == 0) {
	    if (preferred_lifetime == DHCP_INFINITE_LEASE) {
		t1 = t2 = 0;
	    }
	    else {
		t1 = preferred_lifetime * 0.5;
		t2 = preferred_lifetime * 0.8;
	    }
	}
	else if (t1 == DHCP_INFINITE_LEASE || t2 == DHCP_INFINITE_LEASE) {
	    t1 = t2 = 0;
	    preferred_lifetime = DHCP_INFINITE_LEASE;
	    valid_lifetime = DHCP_INFINITE_LEASE;
	}
	lease_p->start = current_time;
	if (valid_lifetime == DHCP_INFINITE_LEASE) {
	    lease_p->t1 = lease_p->t2 = 0;
	    preferred_lifetime = DHCP_INFINITE_LEASE;
	}
	else {
	    lease_p->t1 = t1;
	    lease_p->t2 = t2;
	}
	lease_p->preferred_lifetime = preferred_lifetime;
	lease_p->valid_lifetime = valid_lifetime;
    }
    client->saved_verified = TRUE;
    return;
}

STATIC DHCPv6PacketRef
DHCPv6ClientMakePacket(DHCPv6ClientRef client, int message_type,
		       char * buf, int buf_size,
		       DHCPv6OptionAreaRef oa_p)
{
    uint16_t			elapsed_time;
    DHCPv6OptionErrorString 	err;
    DHCPv6PacketRef		pkt;

    pkt = (DHCPv6PacketRef)buf;
    DHCPv6PacketSetMessageType(pkt, message_type);
    DHCPv6PacketSetTransactionID(pkt, client->transaction_id);
    DHCPv6OptionAreaInit(oa_p, pkt->options, 
			 buf_size - DHCPV6_PACKET_HEADER_LENGTH);
    if (S_insert_duid(oa_p) == FALSE) {
	return (NULL);
    }
    if (DHCPv6OptionAreaAddOptionRequestOption(oa_p, 
					       DHCPv6RequestedOptions,
					       DHCPv6RequestedOptionsCount,
					       &err) == FALSE) {
	my_log(LOG_NOTICE, "DHCPv6Client: failed to add ORO, %s",
	       err.str);
	return (NULL);
    }
    elapsed_time = get_elapsed_time(client);
    if (DHCPv6OptionAreaAddOption(oa_p, kDHCPv6OPTION_ELAPSED_TIME,
				  sizeof(elapsed_time), &elapsed_time,
				  &err) == FALSE) {
	my_log(LOG_NOTICE, "DHCPv6Client: failed to add ELAPSED_TIME, %s",
	       err.str);
	return (NULL);
    }
    return (pkt);
}


STATIC void
DHCPv6ClientSendInform(DHCPv6ClientRef client)
{
    char			buf[1500];
    int				error;
    interface_t *		if_p = DHCPv6ClientGetInterface(client);
    DHCPv6OptionArea		oa;
    DHCPv6PacketRef		pkt;

    pkt = DHCPv6ClientMakePacket(client, kDHCPv6MessageINFORMATION_REQUEST,
				 buf, sizeof(buf), &oa);
    if (pkt == NULL) {
	return;
    }
    error = DHCPv6SocketTransmit(client->sock, pkt,
				 DHCPV6_PACKET_HEADER_LENGTH 
				 + DHCPv6OptionAreaGetUsedLength(&oa));
    switch (error) {
    case 0:
    case ENXIO:
    case ENETDOWN:
	break;
    default:
	my_log(LOG_NOTICE, "DHCPv6 %s: SendInformRequest transmit failed, %s", 
	       if_name(if_p), strerror(error));
	break;
    }
    return;
}

STATIC void
DHCPv6ClientSendSolicit(DHCPv6ClientRef client)
{
    char			buf[1500];
    DHCPv6OptionErrorString 	err;
    int				error;
    DHCPv6OptionIA_NA		ia_na;
    interface_t *		if_p = DHCPv6ClientGetInterface(client);
    DHCPv6OptionArea		oa;
    DHCPv6PacketRef		pkt;

    pkt = DHCPv6ClientMakePacket(client, kDHCPv6MessageSOLICIT,
				 buf, sizeof(buf), &oa);
    if (pkt == NULL) {
	return;
    }
    DHCPv6OptionIA_NASetIAID(&ia_na, DHCPIAIDGet(if_name(if_p)));
    DHCPv6OptionIA_NASetT1(&ia_na, 0);
    DHCPv6OptionIA_NASetT2(&ia_na, 0);
    if (DHCPv6OptionAreaAddOption(&oa, kDHCPv6OPTION_IA_NA,
				  DHCPv6OptionIA_NA_MIN_LENGTH,
				  &ia_na, &err) == FALSE) {
	my_log(LOG_NOTICE, "DHCPv6Client: failed to add IA_NA, %s",
	       err.str);
	return;
    }
    error = DHCPv6SocketTransmit(client->sock, pkt,
				 DHCPV6_PACKET_HEADER_LENGTH 
				 + DHCPv6OptionAreaGetUsedLength(&oa));
    switch (error) {
    case 0:
    case ENXIO:
    case ENETDOWN:
	break;
    default:
	my_log(LOG_NOTICE, "DHCPv6 %s: SendSolicit transmit failed, %s", 
	       if_name(if_p), strerror(error));
	break;
    }
    return;
}

STATIC void
DHCPv6ClientSendPacket(DHCPv6ClientRef client)
{
    char			buf[1500];
    DHCPv6OptionErrorString 	err;
    int				error;
    interface_t *		if_p = DHCPv6ClientGetInterface(client);
    int				message_type;
    DHCPv6OptionArea		oa;
    DHCPv6PacketRef		pkt;

    if (client->ia_na == NULL || client->server_id == NULL) {
	my_log(LOG_NOTICE, "DHCPv6 %s: SendPacket given NULLs",
	       if_name(if_p));
	return;
    }
    switch (client->cstate) {
    case kDHCPv6ClientStateRequest:
	message_type = kDHCPv6MessageREQUEST;
	break;
    case kDHCPv6ClientStateRenew:
	message_type = kDHCPv6MessageRENEW;
	break;
    case kDHCPv6ClientStateRebind:
	message_type = kDHCPv6MessageREBIND;
	break;
    case kDHCPv6ClientStateRelease:
	message_type = kDHCPv6MessageRELEASE;
	break;
    case kDHCPv6ClientStateConfirm:
	message_type = kDHCPv6MessageCONFIRM;
	break;
    case kDHCPv6ClientStateDecline:
	message_type = kDHCPv6MessageDECLINE;
	break;
    default:
	my_log(LOG_NOTICE,
	       "DHCP %s: SendPacket doesn't know %s",
	       if_name(if_p), DHCPv6ClientStateGetName(client->cstate));
	return;
    }
    pkt = DHCPv6ClientMakePacket(client, message_type,
				 buf, sizeof(buf), &oa);
    if (pkt == NULL) {
	return;
    }
    switch (message_type) {
    case kDHCPv6MessageREBIND:
    case kDHCPv6MessageCONFIRM:
	break;
    default:
	if (DHCPv6OptionAreaAddOption(&oa, kDHCPv6OPTION_SERVERID,
				      option_data_get_length(client->server_id),
				      client->server_id, &err) == FALSE) {
	    my_log(LOG_NOTICE, "DHCPv6Client: %s failed to add SERVERID, %s",
		   DHCPv6ClientStateGetName(client->cstate), err.str);
	    return;
	}
	break;
    }
    if (add_ia_na_option(client, &oa, &err) == FALSE) {
	my_log(LOG_NOTICE, "DHCPv6Client: failed to add IA_NA, %s",
	       err.str);
	return;
    }
    error = DHCPv6SocketTransmit(client->sock, pkt,
				 DHCPV6_PACKET_HEADER_LENGTH 
				 + DHCPv6OptionAreaGetUsedLength(&oa));
    switch (error) {
    case 0:
    case ENXIO:
    case ENETDOWN:
	break;
    default:
	my_log(LOG_NOTICE, "DHCPv6 %s: SendPacket transmit failed, %s", 
	       if_name(if_p), strerror(error));
	break;
    }
    return;
}

STATIC void
DHCPv6Client_Inform(DHCPv6ClientRef client, IFEventID_t event_id, 
		   void * event_data)
{
    interface_t *	if_p = DHCPv6ClientGetInterface(client);

    switch (event_id) {
    case IFEventID_start_e:
	DHCPv6ClientSetState(client, kDHCPv6ClientStateInform);
	DHCPv6ClientClearPacket(client);
	DHCPv6ClientClearRetransmit(client);
	client->transaction_id = get_new_transaction_id();
	DHCPv6SocketEnableReceive(client->sock, (DHCPv6SocketReceiveFuncPtr)
				  DHCPv6Client_Inform,
				  client, (void *)IFEventID_data_e);

	if (if_ift_type(if_p) != IFT_CELLULAR) {
	    timer_callout_set(client->timer,
			      random_double_in_range(0, DHCPv6_INF_MAX_DELAY),
			      (timer_func_t *)DHCPv6Client_Inform, client,
			      (void *)IFEventID_timeout_e, NULL);
	    break;
	}
	/* FALL THROUGH */
    case IFEventID_timeout_e:
	if (client->try == 0) {
	    client->start_time = timer_get_current_time();
	}
	else {
	    link_status_t	link_status;

	    link_status = if_get_link_status(if_p);
	    if (link_status.valid && !link_status.active) {
		DHCPv6ClientInactive(client);
		break;
	    }
	}
	timer_callout_set(client->timer,
			  DHCPv6ClientNextRetransmit(client,
						     DHCPv6_INF_TIMEOUT,
						     DHCPv6_INF_MAX_RT),
			  (timer_func_t *)DHCPv6Client_Inform, 
			  client, (void *)IFEventID_timeout_e, NULL);
	my_log(LOG_INFO, "DHCPv6 %s: Inform Transmit (try=%d)",
	       if_name(if_p), client->try);
	DHCPv6ClientSendInform(client);
	break;
    case IFEventID_data_e: {
	DHCPv6SocketReceiveDataRef 	data;
	int				option_len;
	DHCPDUIDRef			server_id;

	data = (DHCPv6SocketReceiveDataRef)event_data;
	if (data->pkt->msg_type != kDHCPv6MessageREPLY
	    || (DHCPv6PacketGetTransactionID((const DHCPv6PacketRef)data->pkt)
		!= client->transaction_id)
	    || (S_duid_matches(data->options) == FALSE)) {
	    /* not a match */
	    break;
	}
	server_id = (DHCPDUIDRef)
	    DHCPv6OptionListGetOptionDataAndLength(data->options,
						   kDHCPv6OPTION_SERVERID,
						   &option_len, NULL);
	if (server_id == NULL
	    || DHCPDUIDIsValid(server_id, option_len) == FALSE) {
	    /* missing/invalid DUID */
	    break;
	}
	my_log(LOG_INFO, "DHCPv6 %s: Reply Received (try=%d)",
	       if_name(if_p), client->try);
	DHCPv6ClientSavePacket(client, data);
	DHCPv6ClientPostNotification(client);
	DHCPv6ClientCancelPendingEvents(client);
	break;
    }
    default:
	break;
    }
    return;
}

STATIC void
DHCPv6Client_Release(DHCPv6ClientRef client, IFEventID_t event_id, 
		     void * event_data)
{
    interface_t *		if_p = DHCPv6ClientGetInterface(client);

    switch (event_id) {
    case IFEventID_start_e:
	DHCPv6ClientSetState(client, kDHCPv6ClientStateRelease);
	DHCPv6ClientRemoveAddress(client, "Release");
	DHCPv6ClientCancelPendingEvents(client);
	DHCPv6ClientClearRetransmit(client);
	client->transaction_id = get_new_transaction_id();
	my_log(LOG_INFO, "DHCPv6 %s: Release Transmit",
	       if_name(if_p));
	DHCPv6ClientSendPacket(client);
	/*
	 * We're supposed to wait for a Reply.  Unfortunately, that's not
	 * possible because the code that invokes us expects the Stop
	 * event to be synchronous.
	 */
	break;
    default:
	break;
    }
    return;
}

STATIC void
DHCPv6Client_Decline(DHCPv6ClientRef client, IFEventID_t event_id, 
		     void * event_data)
{
    interface_t *		if_p = DHCPv6ClientGetInterface(client);

    switch (event_id) {
    case IFEventID_start_e:
	DHCPv6ClientSetState(client, kDHCPv6ClientStateDecline);
	DHCPv6ClientRemoveAddress(client, "Decline");
	DHCPv6ClientCancelPendingEvents(client);
	client->lease.valid = FALSE;
	client->saved_verified = FALSE;
	DHCPv6ClientPostNotification(client);
	DHCPv6ClientClearRetransmit(client);
	client->transaction_id = get_new_transaction_id();
	DHCPv6SocketEnableReceive(client->sock, (DHCPv6SocketReceiveFuncPtr)
				  DHCPv6Client_Decline,
				  client, (void *)IFEventID_data_e);
	/* FALL THROUGH */
    case IFEventID_timeout_e:
	if (client->try >= DHCPv6_DEC_MAX_RC) {
	    /* go back to Solicit */
	    DHCPv6Client_Solicit(client, IFEventID_start_e, NULL);
	    return;
	}
	timer_callout_set(client->timer,
			  DHCPv6ClientNextRetransmit(client,
						     DHCPv6_DEC_TIMEOUT, 
						     0),
			  (timer_func_t *)DHCPv6Client_Decline, 
			  client, (void *)IFEventID_timeout_e, NULL);
	my_log(LOG_INFO, "DHCPv6 %s: Decline Transmit (try=%d)",
	       if_name(if_p), client->try);
	DHCPv6ClientSendPacket(client);
	break;

    case IFEventID_data_e: {
	DHCPv6SocketReceiveDataRef 	data;
	int				option_len;
	DHCPDUIDRef			server_id;

	data = (DHCPv6SocketReceiveDataRef)event_data;
	if (data->pkt->msg_type != kDHCPv6MessageREPLY
	    || (DHCPv6PacketGetTransactionID((const DHCPv6PacketRef)data->pkt)
		!= client->transaction_id)
	    || (S_duid_matches(data->options) == FALSE)) {
	    /* not a match */
	    break;
	}
	server_id = (DHCPDUIDRef)
	    DHCPv6OptionListGetOptionDataAndLength(data->options,
						   kDHCPv6OPTION_SERVERID,
						   &option_len, NULL);
	if (server_id == NULL
	    || DHCPDUIDIsValid(server_id, option_len) == FALSE) {
	    /* missing/invalid DUID */
	    break;
	}
	my_log(LOG_INFO, "DHCPv6 %s: Reply Received (try=%d)",
	       if_name(if_p), client->try);

	/* back to Solicit */
	DHCPv6Client_Solicit(client, IFEventID_start_e, NULL);
	break;
    }
    default:
	break;
    }
    return;
}

STATIC void
DHCPv6Client_RenewRebind(DHCPv6ClientRef client, IFEventID_t event_id, 
			 void * event_data)
{
    CFAbsoluteTime		current_time = timer_get_current_time();
    interface_t *		if_p = DHCPv6ClientGetInterface(client);

    switch (event_id) {
    case IFEventID_start_e:
	DHCPv6ClientSetState(client, kDHCPv6ClientStateRenew);
	DHCPv6ClientCancelPendingEvents(client);
	DHCPv6ClientClearRetransmit(client);
	client->start_time = current_time;
	client->transaction_id = get_new_transaction_id();
	DHCPv6SocketEnableReceive(client->sock, (DHCPv6SocketReceiveFuncPtr)
				  DHCPv6Client_RenewRebind,
				  client, (void *)IFEventID_data_e);
	/* FALL THROUGH */
    case IFEventID_timeout_e: {
	CFTimeInterval	time_since_start;
	CFTimeInterval	wait_time;

	if (current_time < client->lease.start) {
	    /* time went backwards? */
	    DHCPv6Client_Unbound(client, IFEventID_start_e, NULL);
	    return;
	}
	time_since_start = current_time - client->lease.start;
	if (((uint32_t)time_since_start) >= client->lease.valid_lifetime) {
	    /* expired */
	    DHCPv6Client_Unbound(client, IFEventID_start_e, NULL);
	    return;
	}
	
	if (((uint32_t)time_since_start) < client->lease.t2) {
	    CFTimeInterval	time_until_t2;

	    /* Renew (before T2) */
	    wait_time = DHCPv6ClientNextRetransmit(client,
						   DHCPv6_REN_TIMEOUT,
						   DHCPv6_REN_MAX_RT);
	    time_until_t2 = client->lease.t2 - (uint32_t)time_since_start;
	    if (wait_time > time_until_t2) {
		wait_time = time_until_t2;
	    }
	}
	else {
	    CFTimeInterval	time_until_expiration;

	    /* Rebind (T2 or later) */
	    if (client->cstate != kDHCPv6ClientStateRebind) {
		/* switch to Rebind */
		client->transaction_id = get_new_transaction_id();
		client->start_time = current_time;
		DHCPv6ClientSetState(client, kDHCPv6ClientStateRebind);
		DHCPv6ClientClearRetransmit(client);
	    }
	    wait_time = DHCPv6ClientNextRetransmit(client,
						   DHCPv6_REB_TIMEOUT,
						   DHCPv6_REB_MAX_RT);
	    time_until_expiration 
		= client->lease.valid_lifetime - (uint32_t)time_since_start;
	    if (wait_time > time_until_expiration) {
		wait_time = time_until_expiration;
	    }
	}
	timer_callout_set(client->timer,
			  wait_time,
			  (timer_func_t *)DHCPv6Client_RenewRebind, 
			  client, (void *)IFEventID_timeout_e, NULL);
	my_log(LOG_INFO, "DHCPv6 %s: %s Transmit (try=%d)",
	       if_name(if_p), DHCPv6ClientStateGetName(client->cstate),
	       client->try);
	DHCPv6ClientSendPacket(client);
	break;
    }
    case IFEventID_data_e: {
	DHCPv6SocketReceiveDataRef 	data;
	DHCPv6OptionIA_NARef		ia_na;
	DHCPv6OptionIAADDRRef		ia_addr;
	char 				ntopbuf[INET6_ADDRSTRLEN];
	int				option_len;
	DHCPDUIDRef			server_id;
	DHCPv6OptionSTATUS_CODERef	status_code;

	data = (DHCPv6SocketReceiveDataRef)event_data;
	if (data->pkt->msg_type != kDHCPv6MessageREPLY
	    || (DHCPv6PacketGetTransactionID((const DHCPv6PacketRef)data->pkt)
		!= client->transaction_id)
	    || (S_duid_matches(data->options) == FALSE)) {
	    /* not a match */
	    break;
	}
	server_id = (DHCPDUIDRef)
	    DHCPv6OptionListGetOptionDataAndLength(data->options,
						   kDHCPv6OPTION_SERVERID,
						   &option_len, NULL);
	if (server_id == NULL
	    || DHCPDUIDIsValid(server_id, option_len) == FALSE) {
	    /* missing/invalid DUID */
	    break;
	}
	status_code = (DHCPv6OptionSTATUS_CODERef)
	    DHCPv6OptionListGetOptionDataAndLength(data->options,
						   kDHCPv6OPTION_STATUS_CODE,
						   &option_len, NULL);
	if (status_code != NULL) {
	    uint16_t	code;

	    if (option_len < DHCPv6OptionSTATUS_CODE_MIN_LENGTH) {
		/* too short */
		break;
	    }
	    
	    code = DHCPv6OptionSTATUS_CODEGetCode(status_code);
	    if (code != kDHCPv6StatusCodeSuccess) {
		/* XXX check for a specific value maybe? */
		DHCPv6Client_Unbound(client, IFEventID_start_e, NULL);
		return;
	    }
	}
	ia_na = get_ia_na_addr(client, data->pkt->msg_type,
			       data->options, &ia_addr);
	if (ia_na == NULL) {
	    DHCPv6Client_Unbound(client, IFEventID_start_e, NULL);
	    break;
	}
	my_log(LOG_INFO, "DHCPv6 %s: %s Received Reply (try=%d) "
	       "IAADDR %s Preferred %d Valid=%d",
	       if_name(if_p),
	       DHCPv6ClientStateGetName(client->cstate),
	       client->try,
	       inet_ntop(AF_INET6,
			 DHCPv6OptionIAADDRGetAddress(ia_addr),
			 ntopbuf, sizeof(ntopbuf)),
	       DHCPv6OptionIAADDRGetPreferredLifetime(ia_addr),
	       DHCPv6OptionIAADDRGetValidLifetime(ia_addr));
	DHCPv6ClientSavePacket(client, data);
	DHCPv6Client_Bound(client, IFEventID_start_e, NULL);
	break;
    }
    default:
	break;
    }
}

STATIC void
DHCPv6Client_Confirm(DHCPv6ClientRef client, IFEventID_t event_id, 
		     void * event_data)
{
    CFAbsoluteTime		current_time = timer_get_current_time();
    interface_t *		if_p = DHCPv6ClientGetInterface(client);

    switch (event_id) {
    case IFEventID_start_e:
	DHCPv6ClientSetState(client, kDHCPv6ClientStateConfirm);
	DHCPv6ClientCancelPendingEvents(client);
	DHCPv6ClientClearRetransmit(client);
	client->saved_verified = FALSE;
	client->transaction_id = get_new_transaction_id();
	DHCPv6SocketEnableReceive(client->sock, (DHCPv6SocketReceiveFuncPtr)
				  DHCPv6Client_Confirm,
				  client, (void *)IFEventID_data_e);
	timer_callout_set(client->timer,
			  random_double_in_range(0, DHCPv6_CNF_MAX_DELAY),
			  (timer_func_t *)DHCPv6Client_Confirm, client,
			  (void *)IFEventID_timeout_e, NULL);
	break;
    case IFEventID_timeout_e:
	if (client->try == 0) {
	    client->start_time = current_time;
	}
	else {
	    bool		done = FALSE;
	    link_status_t	link_status;

	    link_status = if_get_link_status(if_p);
	    if (link_status.valid && !link_status.active) {
		DHCPv6ClientInactive(client);
		break;
	    }
	    if (current_time > client->start_time) {
		if ((current_time - client->start_time) >= DHCPv6_CNF_MAX_RD) {
		    done = TRUE;
		}
	    }
	    else {
		done = TRUE;
	    }
	    if (done) {
		if (DHCPv6ClientLeaseStillValid(client)) {
		    DHCPv6Client_Bound(client, IFEventID_start_e, NULL);
		    return;
		}
		DHCPv6Client_Solicit(client, IFEventID_start_e, NULL);
		return;
	    }
	}
	timer_callout_set(client->timer,
			  DHCPv6ClientNextRetransmit(client,
						     DHCPv6_CNF_TIMEOUT,
						     DHCPv6_CNF_MAX_RT),
			  (timer_func_t *)DHCPv6Client_Confirm,
			  client, (void *)IFEventID_timeout_e, NULL);
	my_log(LOG_INFO, "DHCPv6 %s: Confirm Transmit (try=%d)",
	       if_name(if_p), client->try);
	DHCPv6ClientSendPacket(client);
	break;
    case IFEventID_data_e: {
	DHCPv6SocketReceiveDataRef 	data;
	int				option_len;
	DHCPDUIDRef			server_id;
	DHCPv6OptionSTATUS_CODERef	status_code;

	data = (DHCPv6SocketReceiveDataRef)event_data;
	if (data->pkt->msg_type != kDHCPv6MessageREPLY
	    || (DHCPv6PacketGetTransactionID((const DHCPv6PacketRef)data->pkt)
		!= client->transaction_id)
	    || (S_duid_matches(data->options) == FALSE)) {
	    /* not a match */
	    break;
	}
	server_id = (DHCPDUIDRef)
	    DHCPv6OptionListGetOptionDataAndLength(data->options,
						   kDHCPv6OPTION_SERVERID,
						   &option_len, NULL);
	if (server_id == NULL
	    || DHCPDUIDIsValid(server_id, option_len) == FALSE) {
	    /* missing/invalid DUID */
	    break;
	}
	status_code = (DHCPv6OptionSTATUS_CODERef)
	    DHCPv6OptionListGetOptionDataAndLength(data->options,
						   kDHCPv6OPTION_STATUS_CODE,
						   &option_len, NULL);
	if (status_code != NULL) {
	    if (option_len < DHCPv6OptionSTATUS_CODE_MIN_LENGTH) {
		/* too short */
		break;
	    }
	    if (DHCPv6OptionSTATUS_CODEGetCode(status_code)
		!= kDHCPv6StatusCodeSuccess) {
		DHCPv6Client_Unbound(client, IFEventID_start_e, NULL);
		return;
	    }
	}
	my_log(LOG_INFO, "DHCPv6 %s: Reply Received (try=%d)",
	       if_name(if_p), client->try);
	DHCPv6Client_Bound(client, IFEventID_start_e, NULL);
	break;
    }
    default:
	break;
    }
}

STATIC void
DHCPv6ClientHandleAddressChanged(DHCPv6ClientRef client,
				 inet6_addrlist_t * addr_list_p)
{
    int				i;
    inet6_addrinfo_t *		scan;

    if (addr_list_p == NULL || addr_list_p->count == 0) {
	/* no addresses configured, nothing to do */
	return;
    }
    if (client->cstate != kDHCPv6ClientStateBound) {
	return;
    }
    for (i = 0, scan = addr_list_p->list; 
	 i < addr_list_p->count; i++, scan++) {
	if (IN6_ARE_ADDR_EQUAL(&client->our_ip, &scan->addr)) {
	    /* someone else is using this address, decline it */
	    if ((scan->addr_flags & IN6_IFF_DUPLICATED) != 0) {
		DHCPv6Client_Decline(client, IFEventID_start_e, NULL);
		return;
	    }
	    if ((scan->addr_flags & IN6_IFF_TENTATIVE) != 0) {
		my_log(LOG_INFO, "address is still tentative");
		/* address is still tentative */
		break;
	    }
	    /* notify that we're ready */
	    DHCPv6ClientPostNotification(client);
	    DHCPv6ClientCancelPendingEvents(client);
	    
	    /* set a timer to start in Renew */
	    if (client->lease.valid_lifetime != DHCP_INFINITE_LEASE) {
		CFAbsoluteTime	current_time = timer_get_current_time();
		uint32_t	t1 = client->lease.t1;
		CFTimeInterval	time_since_start = 0;
		
		if (current_time < client->lease.start) {
		    /* time went backwards? */
		    DHCPv6Client_Unbound(client, IFEventID_start_e, NULL);
		    return;
		}
		time_since_start = current_time - client->lease.start;
		if (((uint32_t)time_since_start) < t1) {
		    t1 -= (uint32_t)time_since_start;
		}
		else {
		    t1 = 10; /* wakeup in 10 seconds */
		}
		timer_callout_set(client->timer, t1,
				  (timer_func_t *)DHCPv6Client_RenewRebind,
				  client, (void *)IFEventID_start_e, NULL);
	    }
	    break;
	}
    }
    return;
}

STATIC void
DHCPv6ClientSimulateAddressChanged(DHCPv6ClientRef client)
{
    inet6_addrlist_t	addr_list;
    interface_t *	if_p = DHCPv6ClientGetInterface(client);

    inet6_addrlist_copy(&addr_list, if_link_index(if_p));
    DHCPv6ClientHandleAddressChanged(client, &addr_list);
    inet6_addrlist_free(&addr_list);
    return;
}

STATIC void
DHCPv6Client_Bound(DHCPv6ClientRef client, IFEventID_t event_id, 
		   void * event_data)
{
    interface_t *		if_p = DHCPv6ClientGetInterface(client);
    char 			ntopbuf[INET6_ADDRSTRLEN];

    switch (event_id) {
    case IFEventID_start_e: {
	struct in6_addr *	our_ip;
	struct in6_addr         our_ip_aligned;
	uint32_t		preferred_lifetime;
	int			prefix_length;
	int			s;
	bool			same_address = FALSE;
	CFTimeInterval		time_since_start = 0;
	uint32_t		valid_lifetime;

	our_ip = &our_ip_aligned;
	bcopy((void *)DHCPv6OptionIAADDRGetAddress(client->ia_addr),
	      our_ip, sizeof(our_ip_aligned));

	DHCPv6ClientSetState(client, kDHCPv6ClientStateBound);
	client->lease.valid = TRUE;
	client->saved_verified = TRUE;
	DHCPv6ClientCancelPendingEvents(client);

	valid_lifetime = client->lease.valid_lifetime;
	preferred_lifetime = client->lease.preferred_lifetime;
	if (valid_lifetime != DHCP_INFINITE_LEASE) {
	    CFAbsoluteTime		current_time = timer_get_current_time();

	    if (current_time < client->lease.start) {
		/* time went backwards? */
		DHCPv6Client_Unbound(client, IFEventID_start_e, NULL);
		return;
	    }
	    time_since_start = current_time - client->lease.start;
	    if (((uint32_t)time_since_start) >= client->lease.valid_lifetime) {
		/* expired */
		DHCPv6Client_Unbound(client, IFEventID_start_e, NULL);
		return;
	    }
	    /* reduce the time left by the amount that's elapsed already */
	    valid_lifetime -= (uint32_t)time_since_start;
	    if (((uint32_t)time_since_start) < preferred_lifetime) {
		preferred_lifetime -= (uint32_t)time_since_start;
	    }
	    else {
		preferred_lifetime = 0; /* XXX really? */
	    }
	}
	s = inet6_dgram_socket();
	if (s < 0) {
	    my_log(LOG_NOTICE,
		   "DHCPv6ClientBound(%s):"
		   " socket() failed, %s (%d)",
		   if_name(if_p), strerror(errno), errno);
	    break;
	}
	/* if the address has changed, remove the old first */
	if (IN6_IS_ADDR_UNSPECIFIED(&client->our_ip) == FALSE) {
	    if (IN6_ARE_ADDR_EQUAL(&client->our_ip, our_ip)) {
		same_address = TRUE;
	    }
	    else {
		my_log(LOG_INFO, "DHCPv6 %s: Bound: removing %s",
		       if_name(if_p),
		       inet_ntop(AF_INET6, &client->our_ip,
				 ntopbuf, sizeof(ntopbuf)));
		if (inet6_difaddr(s, if_name(if_p), &client->our_ip) < 0) {
		    my_log(LOG_INFO,
			   "DHCPv6ClientBound(%s): remove %s failed, %s (%d)",
			   if_name(if_p),
			   inet_ntop(AF_INET6, &client->our_ip,
				     ntopbuf, sizeof(ntopbuf)),
			   strerror(errno), errno);
		}
	    }
	}
	prefix_length = S_get_prefix_length(our_ip, if_link_index(if_p));
	my_log(LOG_INFO,
	       "DHCPv6 %s: setting %s/%d valid %d preferred %d",
	       if_name(if_p),
	       inet_ntop(AF_INET6, our_ip, ntopbuf, sizeof(ntopbuf)),
	       prefix_length, valid_lifetime, preferred_lifetime);
	if (inet6_aifaddr(s, if_name(if_p), our_ip, NULL, 
			  prefix_length, IN6_IFF_DYNAMIC,
			  valid_lifetime, preferred_lifetime) < 0) {
	    my_log(LOG_INFO,
		   "DHCPv6ClientBound(%s): adding %s failed, %s (%d)",
		   if_name(if_p),
		   inet_ntop(AF_INET6, our_ip,
			     ntopbuf, sizeof(ntopbuf)),
		   strerror(errno), errno);
	}
	else if (same_address) {
	    /* notify that we're ready */
	    DHCPv6ClientPostNotification(client);
	    DHCPv6ClientCancelPendingEvents(client);

	    /* set a timer to start in Renew */
	    if (client->lease.valid_lifetime != DHCP_INFINITE_LEASE) {
		uint32_t	t1 = client->lease.t1;

		if (((uint32_t)time_since_start) < t1) {
		    t1 -= (uint32_t)time_since_start;
		}
		else {
		    t1 = 10; /* wakeup in 10 seconds */
		    
		}
		timer_callout_set(client->timer, t1,
				  (timer_func_t *)DHCPv6Client_RenewRebind,
				  client, (void *)IFEventID_start_e, NULL);
	    }
	}
	else {
	    client->our_ip = *our_ip;
	    client->our_prefix_length = prefix_length;
	    /* and see what addresses are there now */
	    DHCPv6ClientSimulateAddressChanged(client);
	}
	close(s);
	break;
    }
    default:
	break;
    }
}

STATIC void
DHCPv6Client_Unbound(DHCPv6ClientRef client, IFEventID_t event_id, 
		     void * event_data)
{
    switch (event_id) {
    case IFEventID_start_e:
	DHCPv6ClientSetState(client, kDHCPv6ClientStateUnbound);
	DHCPv6ClientCancelPendingEvents(client);
	DHCPv6ClientRemoveAddress(client, "Unbound");
	DHCPv6ClientClearPacket(client);
	DHCPv6ClientPostNotification(client);
	DHCPv6Client_Solicit(client, IFEventID_start_e, NULL);
	break;
    default:
	break;
    }
}

STATIC void
DHCPv6Client_Request(DHCPv6ClientRef client, IFEventID_t event_id, 
		     void * event_data)
{
    interface_t *	if_p = DHCPv6ClientGetInterface(client);

    switch (event_id) {
    case IFEventID_start_e:
	DHCPv6ClientSetState(client, kDHCPv6ClientStateRequest);
	DHCPv6ClientClearRetransmit(client);
	client->transaction_id = get_new_transaction_id();
	client->start_time = timer_get_current_time();
	DHCPv6SocketEnableReceive(client->sock, (DHCPv6SocketReceiveFuncPtr)
				  DHCPv6Client_Request,
				  client, (void *)IFEventID_data_e);
	/* FALL THROUGH */
    case IFEventID_timeout_e: {
	if (client->try >= DHCPv6_REQ_MAX_RC) {
	    /* go back to Solicit */
	    DHCPv6Client_Solicit(client, IFEventID_start_e, NULL);
	    return;
	}
	timer_callout_set(client->timer,
			  DHCPv6ClientNextRetransmit(client,
						     DHCPv6_REQ_TIMEOUT,
						     DHCPv6_REQ_MAX_RT),
			  (timer_func_t *)DHCPv6Client_Request, 
			  client, (void *)IFEventID_timeout_e, NULL);
	my_log(LOG_INFO, "DHCPv6 %s: Request Transmit (try=%d)",
	       if_name(if_p), client->try);
	DHCPv6ClientSendPacket(client);
	break;
    }
    case IFEventID_data_e: {
	DHCPv6SocketReceiveDataRef 	data;
	DHCPv6OptionIA_NARef		ia_na;
	DHCPv6OptionIAADDRRef		ia_addr;
	char 				ntopbuf[INET6_ADDRSTRLEN];
	int				option_len;
	DHCPDUIDRef			server_id;
	DHCPv6OptionSTATUS_CODERef	status_code;

	data = (DHCPv6SocketReceiveDataRef)event_data;
	if (data->pkt->msg_type != kDHCPv6MessageREPLY
	    || (DHCPv6PacketGetTransactionID((const DHCPv6PacketRef)data->pkt)
		!= client->transaction_id)
	    || (S_duid_matches(data->options) == FALSE)) {
	    /* not a match */
	    break;
	}
	server_id = (DHCPDUIDRef)
	    DHCPv6OptionListGetOptionDataAndLength(data->options,
						   kDHCPv6OPTION_SERVERID,
						   &option_len, NULL);
	if (server_id == NULL
	    || DHCPDUIDIsValid(server_id, option_len) == FALSE) {
	    /* missing/invalid DUID */
	    break;
	}
	status_code = (DHCPv6OptionSTATUS_CODERef)
	    DHCPv6OptionListGetOptionDataAndLength(data->options,
						   kDHCPv6OPTION_STATUS_CODE,
						   &option_len, NULL);
	if (status_code != NULL) {
	    uint16_t	code;

	    if (option_len < DHCPv6OptionSTATUS_CODE_MIN_LENGTH) {
		/* too short */
		break;
	    }
	    
	    code = DHCPv6OptionSTATUS_CODEGetCode(status_code);
	    if (code != kDHCPv6StatusCodeSuccess) {
		if (code == kDHCPv6StatusCodeNoAddrsAvail) {
		    /* must ignore it */
		    break;
		}
	    }
	}
	ia_na = get_ia_na_addr(client, data->pkt->msg_type,
			       data->options, &ia_addr);
	if (ia_na == NULL) {
	    break;
	}
	my_log(LOG_INFO, "DHCPv6 %s: Reply Received (try=%d) "
	       "IAADDR %s Preferred %d Valid=%d",
	       if_name(if_p),
	       client->try,
	       inet_ntop(AF_INET6,
			 DHCPv6OptionIAADDRGetAddress(ia_addr),
			 ntopbuf, sizeof(ntopbuf)),
	       DHCPv6OptionIAADDRGetPreferredLifetime(ia_addr),
	       DHCPv6OptionIAADDRGetValidLifetime(ia_addr));
	DHCPv6ClientSavePacket(client, data);
	DHCPv6Client_Bound(client, IFEventID_start_e, NULL);
	break;
    }
    default:
	break;
    }
    return;
}

STATIC void
DHCPv6Client_Solicit(DHCPv6ClientRef client, IFEventID_t event_id, 
		     void * event_data)
{
    interface_t *	if_p = DHCPv6ClientGetInterface(client);

    switch (event_id) {
    case IFEventID_start_e:
	DHCPv6ClientSetState(client, kDHCPv6ClientStateSolicit);
	DHCPv6ClientClearRetransmit(client);
	DHCPv6ClientClearPacket(client);
	client->transaction_id = get_new_transaction_id();
	DHCPv6SocketEnableReceive(client->sock, (DHCPv6SocketReceiveFuncPtr)
				  DHCPv6Client_Solicit,
				  client, (void *)IFEventID_data_e);
	timer_callout_set(client->timer,
			  random_double_in_range(0, DHCPv6_SOL_MAX_DELAY),
			  (timer_func_t *)DHCPv6Client_Solicit, client,
			  (void *)IFEventID_timeout_e, NULL);
	break;
    case IFEventID_timeout_e: {
	if (client->try == 0) {
	    client->start_time = timer_get_current_time();
	}
	else {
	    link_status_t	link_status;

	    link_status = if_get_link_status(if_p);
	    if (link_status.valid && !link_status.active) {
		DHCPv6ClientInactive(client);
		break;
	    }
	}
	/* we received a response after waiting */
	if (client->saved.pkt_len != 0) {
	    DHCPv6Client_Request(client, IFEventID_start_e, NULL);
	    return;
	}
	timer_callout_set(client->timer,
			  DHCPv6ClientNextRetransmit(client,
						     DHCPv6_SOL_TIMEOUT,
						     DHCPv6_SOL_MAX_RT),
			  (timer_func_t *)DHCPv6Client_Solicit, 
			  client, (void *)IFEventID_timeout_e, NULL);
	my_log(LOG_INFO, "DHCPv6 %s: Solicit Transmit (try=%d)",
	       if_name(if_p), client->try);
	DHCPv6ClientSendSolicit(client);
#define GENERATE_SYMPTOM_AT_TRY		6
	if (client->try >= GENERATE_SYMPTOM_AT_TRY) {
	    /*
	     * We generally don't want to be calling the provided callback
	     * directly because of re-entrancy issues: the callback could call
	     * us, and we could call them, and enter an endless loop.
	     * This call is safe because we're running as a result of our timer
	     * and the client callback code isn't going to call back into us.
	     */
	    (*client->callback)(client, client->callback_arg,
				kDHCPv6ClientNotificationTypeGenerateSymptom);
	}
	break;
    }
    case IFEventID_data_e: {
	DHCPv6SocketReceiveDataRef 	data;
	DHCPv6OptionIA_NARef		ia_na;
	DHCPv6OptionIAADDRRef		ia_addr;
	char 				ntopbuf[INET6_ADDRSTRLEN];
	int				option_len;
	uint8_t				pref;
	DHCPDUIDRef			server_id;
	DHCPv6OptionSTATUS_CODERef	status_code;

	data = (DHCPv6SocketReceiveDataRef)event_data;
	if (data->pkt->msg_type != kDHCPv6MessageADVERTISE
	    || (DHCPv6PacketGetTransactionID((const DHCPv6PacketRef)data->pkt)
		!= client->transaction_id)
	    || (S_duid_matches(data->options) == FALSE)) {
	    /* not a match */
	    break;
	}
	server_id = (DHCPDUIDRef)
	    DHCPv6OptionListGetOptionDataAndLength(data->options,
						   kDHCPv6OPTION_SERVERID,
						   &option_len, NULL);
	if (server_id == NULL
	    || DHCPDUIDIsValid(server_id, option_len) == FALSE) {
	    /* missing/invalid DUID */
	    break;
	}
	status_code = (DHCPv6OptionSTATUS_CODERef)
	    DHCPv6OptionListGetOptionDataAndLength(data->options,
						   kDHCPv6OPTION_STATUS_CODE,
						   &option_len, NULL);
	if (status_code != NULL) {
	    uint16_t	code;

	    if (option_len < DHCPv6OptionSTATUS_CODE_MIN_LENGTH) {
		/* too short */
		break;
	    }
	    
	    code = DHCPv6OptionSTATUS_CODEGetCode(status_code);
	    if (code != kDHCPv6StatusCodeSuccess) {
		if (code == kDHCPv6StatusCodeNoAddrsAvail) {
		    /* must ignore it */
		    break;
		}
	    }
	}
	ia_na = get_ia_na_addr(client, data->pkt->msg_type,
			       data->options, &ia_addr);
	if (ia_na == NULL) {
	    break;
	}
	my_log(LOG_INFO, "DHCPv6 %s: Advertise Received (try=%d) "
	       "IAADDR %s Preferred %d Valid=%d",
	       if_name(if_p),
	       client->try,
	       inet_ntop(AF_INET6,
			 DHCPv6OptionIAADDRGetAddress(ia_addr),
			 ntopbuf, sizeof(ntopbuf)),
	       DHCPv6OptionIAADDRGetPreferredLifetime(ia_addr),
	       DHCPv6OptionIAADDRGetValidLifetime(ia_addr));

	/* check for a server preference value */
	pref = get_preference_value_from_options(data->options);

	/* if this response is "better" than one we saved, use it */
	if (client->saved.options != NULL) {
	    uint8_t		saved_pref;

	    saved_pref
		= get_preference_value_from_options(client->saved.options);
	    if (saved_pref >= pref) {
		/* saved packet is still "better" */
		break;
	    }
	}
	{
	    CFMutableStringRef	str;

	    str = CFStringCreateMutable(NULL, 0);
	    DHCPDUIDPrintToString(str, server_id, 
				  option_data_get_length(server_id));
	    my_log(LOG_INFO, "DHCPv6 %s: Saving Advertise from %@",
		   if_name(if_p), str);
	    CFRelease(str);
	}
	DHCPv6ClientSavePacket(client, data);
	if (client->try > 1 || pref == kDHCPv6OptionPREFERENCEMaxValue) {
	    /* already waited, or preference is max, move to Request */
	    DHCPv6Client_Request(client, IFEventID_start_e, NULL);
	    break;
	}
	break;
    }
    default:
	break;
    }
    return;
}

PRIVATE_EXTERN DHCPv6ClientMode
DHCPv6ClientGetMode(DHCPv6ClientRef client)
{
    return (client->mode);
}

PRIVATE_EXTERN DHCPv6ClientRef
DHCPv6ClientCreate(interface_t * if_p)
{
    DHCPv6ClientRef		client;
    char			timer_name[32];

    client = (DHCPv6ClientRef)malloc(sizeof(*client));
    bzero(client, sizeof(*client));
    client->sock = DHCPv6SocketCreate(if_p);
    snprintf(timer_name, sizeof(timer_name),
	     "DHCPv6-%s", if_name(if_p));
    client->timer = timer_callout_init(timer_name);
    return (client);
}

PRIVATE_EXTERN void
DHCPv6ClientStart(DHCPv6ClientRef client, bool allocate_address)
{
    if (allocate_address) {
	client->mode = kDHCPv6ClientModeStateful;
	/* start Stateful */
	if (DHCPv6ClientLeaseStillValid(client)) {
	    DHCPv6Client_Confirm(client, IFEventID_start_e, NULL);
	}
	else {
	    DHCPv6Client_Solicit(client, IFEventID_start_e, NULL);
	}
    }
    else {
	/* start Stateless */
	client->mode = kDHCPv6ClientModeStateless;
	DHCPv6ClientRemoveAddress(client, "Start");
	DHCPv6Client_Inform(client, IFEventID_start_e, NULL);
    }
    return;
}

PRIVATE_EXTERN void
DHCPv6ClientStop(DHCPv6ClientRef client, bool discard_information)
{
    /* remove the IP address */
    DHCPv6ClientRemoveAddress(client, "Stop");
    DHCPv6ClientCancelPendingEvents(client);
    if (discard_information) {
	DHCPv6ClientClearPacket(client);
    }
    else {
	client->saved_verified = FALSE;
    }
    client->mode = kDHCPv6ClientModeIdle;
    DHCPv6ClientPostNotification(client);
    return;
}

PRIVATE_EXTERN void
DHCPv6ClientRelease(DHCPv6ClientRef * client_p)
{
    DHCPv6ClientRef	client = *client_p;

    if (client == NULL) {
	return;
    }
    *client_p = NULL;
    if (DHCPv6ClientLeaseStillValid(client)) {
	DHCPv6Client_Release(client, IFEventID_start_e, NULL);
    }
    if (client->timer != NULL) {
	timer_callout_free(&client->timer);
    }
    DHCPv6SocketRelease(&client->sock);
    if (client->saved.pkt != NULL) {
	free(client->saved.pkt);
	client->saved.pkt = NULL;
    }
    DHCPv6ClientSetNotificationCallBack(client, NULL, NULL);
    DHCPv6OptionListRelease(&client->saved.options);
    free(client);
    return;
}

PRIVATE_EXTERN bool
DHCPv6ClientGetInfo(DHCPv6ClientRef client, ipv6_info_t * info_p)
{
    if (client->saved.options == NULL || client->saved_verified == FALSE) {
	info_p->pkt = NULL;
	info_p->pkt_len = 0;
	info_p->options = NULL;
	return (FALSE);
    }
    info_p->pkt = client->saved.pkt;
    info_p->pkt_len = client->saved.pkt_len;
    info_p->options = client->saved.options;
    return (TRUE);
}

PRIVATE_EXTERN void
DHCPv6ClientCopyAddresses(DHCPv6ClientRef client, 
			  inet6_addrlist_t * addr_list_p)
{
    if (IN6_IS_ADDR_UNSPECIFIED(&client->our_ip)) {
	inet6_addrlist_init(addr_list_p);
	return;
    }
    addr_list_p->list = addr_list_p->list_static;
    addr_list_p->count = 1;
    addr_list_p->list[0].addr = client->our_ip;
    addr_list_p->list[0].prefix_length = client->our_prefix_length;
    addr_list_p->list[0].addr_flags = 0;
    return;
}

STATIC void 
DHCPv6ClientDeliverNotification(void * info)
{
    DHCPv6ClientRef	client = (DHCPv6ClientRef)info;

    if (client->callback == NULL) {
	/* this can't really happen */
	my_log(LOG_NOTICE,
	       "DHCPv6Client: runloop source signaled but callback is NULL");
	return;
    }
    (*client->callback)(client, client->callback_arg,
			kDHCPv6ClientNotificationTypeStatusChanged);
    return;
}

PRIVATE_EXTERN void
DHCPv6ClientSetNotificationCallBack(DHCPv6ClientRef client, 
				    DHCPv6ClientNotificationCallBack callback,
				    void * callback_arg)
{
    client->callback = callback;
    client->callback_arg = callback_arg;
    if (callback == NULL) {
	if (client->callback_rls != NULL) {
	    CFRunLoopSourceInvalidate(client->callback_rls);
	    my_CFRelease(&client->callback_rls);
	}
    }
    else if (client->callback_rls == NULL) {
	CFRunLoopSourceContext 	context;

	bzero(&context, sizeof(context));
	context.info = (void *)client;
	context.perform = DHCPv6ClientDeliverNotification;
	client->callback_rls = CFRunLoopSourceCreate(NULL, 0, &context);
	CFRunLoopAddSource(CFRunLoopGetCurrent(), client->callback_rls,
			   kCFRunLoopDefaultMode);
    }
    return;
}

PRIVATE_EXTERN void
DHCPv6ClientAddressChanged(DHCPv6ClientRef client, 
			   inet6_addrlist_t * addr_list_p)
{
    DHCPv6ClientHandleAddressChanged(client, addr_list_p);
    return;
}

#if TEST_DHCPV6_CLIENT
#include "sysconfig.h"

#include <SystemConfiguration/SCPrivate.h>

boolean_t G_is_netboot;
int G_dhcp_duid_type;
Boolean G_IPConfiguration_verbose = TRUE;

bool S_allocate_address;

STATIC void
DHCPv6ClientSendBadOptions(DHCPv6ClientRef client)
{
    char			buf[1500];
    int				error;
    interface_t *		if_p = DHCPv6ClientGetInterface(client);
    DHCPv6OptionArea		oa;
    DHCPv6OptionRef		opt;
    DHCPv6PacketRef		pkt;
    int				pkt_len;

    pkt = DHCPv6ClientMakePacket(client,  kDHCPv6MessageINFORMATION_REQUEST,
				 buf, sizeof(buf), &oa);
    if (pkt == NULL) {
	return;
    }

    opt = (DHCPv6OptionRef)(oa.buf + oa.used);
    DHCPv6OptionSetCode(opt, kDHCPv6OPTION_SERVERID);
    DHCPv6OptionSetLength(opt, 64); /* pretend that it's longer */
    opt->data[0] = 'X';
    opt->data[1] = 'X';
    opt->data[2] = 'X';
    opt->data[3] = 'X';
    oa.used += 8; /* only put in 8 bytes */
    pkt_len = DHCPV6_PACKET_HEADER_LENGTH + DHCPv6OptionAreaGetUsedLength(&oa);

    for (int i = 0; i < (1024 * 1024); i++) {
	error = DHCPv6SocketTransmit(client->sock, pkt, pkt_len);
	switch (error) {
	case 0:
	    break;
	case ENXIO:
	case ENETDOWN:
	    fprintf(stderr, "DHCPv6SocketTransmit failed, %d (%s)\n",
		    error, strerror(error));
	    return;
	default:
	    printf("send failed, waiting a bit\n");
	    my_log(LOG_NOTICE, "DHCPv6 %s: SendBadOptions transmit failed, %s",
		   if_name(if_p), strerror(error));
	    usleep(1000);
	    break;
	}
    }
    return;
}

STATIC void
client_notification(DHCPv6ClientRef client,
		    void * callback_arg,
		    DHCPv6ClientNotificationType type)
{
    ipv6_info_t	info;

    if (DHCPv6ClientGetInfo(client, &info) == FALSE) {
	printf("DHCPv6 updated: no info\n");
    }
    else {
	printf("DHCPv6 updated\n");
	DHCPv6OptionListFPrint(stdout, info.options);
    }
    return;
}

interface_list_t *
get_interface_list(void)
{
    STATIC interface_list_t *	S_interfaces;

    if (S_interfaces == NULL) {
	S_interfaces = ifl_init();
    }
    return (S_interfaces);
}

STATIC void
handle_change(SCDynamicStoreRef session, CFArrayRef changes, void * arg)
{
    int			count;
    DHCPv6ClientRef	client = (DHCPv6ClientRef)arg;
    int			i;
    interface_t *	if_p = DHCPv6ClientGetInterface(client);


    if (changes == NULL || (count = CFArrayGetCount(changes)) == 0) {
	return;
    }

    for (i = 0; i < count; i++) {
	CFStringRef	key = CFArrayGetValueAtIndex(changes, i);

	if (CFStringHasSuffix(key, kSCEntNetLink)) {
	    CFBooleanRef	active = kCFBooleanTrue;
	    CFDictionaryRef	dict;

	    my_log(LOG_NOTICE, "link changed");
	    dict = SCDynamicStoreCopyValue(session, key);
	    if (dict != NULL) {
		if (CFDictionaryGetValue(dict, kSCPropNetLinkDetaching)) {
		    my_log(LOG_NOTICE, "%s detaching - exiting",
			   if_name(if_p));
		    exit(0);
		}
		active = CFDictionaryGetValue(dict, kSCPropNetLinkActive);
	    }
	    if (CFEqual(active, kCFBooleanTrue)) {
		DHCPv6ClientStart(client, S_allocate_address);
	    }
	    else {
		DHCPv6ClientStop(client, FALSE);
	    }
	}
	else if (CFStringHasSuffix(key, kSCEntNetIPv6)) {
	    inet6_addrlist_t 	addr_list;

	    my_log(LOG_NOTICE, "address changed");
	    /* get the addresses from the interface and deliver the event */
	    inet6_addrlist_copy(&addr_list, if_link_index(if_p));
	    DHCPv6ClientAddressChanged(client, &addr_list);
	    inet6_addrlist_free(&addr_list);
	}
    }
    return;
}

STATIC void
notification_init(DHCPv6ClientRef client)
{
    CFArrayRef			array;
    SCDynamicStoreContext	context;
    CFStringRef			ifname_cf;
    const void *		keys[2];
    CFRunLoopSourceRef		rls;
    SCDynamicStoreRef		store;

    bzero(&context, sizeof(context));
    context.info = client;
    store = SCDynamicStoreCreate(NULL, CFSTR("DHCPv6Client"),
				 handle_change, &context);
    if (store == NULL) {
	my_log(LOG_NOTICE, "SCDynamicStoreCreate failed: %s",
	       SCErrorString(SCError()));
	return;
    }
    ifname_cf
	= CFStringCreateWithCString(NULL,
				    if_name(DHCPv6ClientGetInterface(client)),
				    kCFStringEncodingASCII);
    keys[0] = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
							    kSCDynamicStoreDomainState,
							    ifname_cf,
							    kSCEntNetIPv6);
    keys[1] = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
							    kSCDynamicStoreDomainState,
							    ifname_cf,
							    kSCEntNetLink);
    CFRelease(ifname_cf);
    array = CFArrayCreate(NULL, (const void **)keys, 2, &kCFTypeArrayCallBacks);
    SCDynamicStoreSetNotificationKeys(store, array, NULL);
    CFRelease(array);
    rls = SCDynamicStoreCreateRunLoopSource(NULL, store, 0);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);
    CFRelease(rls);
    return;
}

int
main(int argc, char * argv[])
{
    DHCPv6ClientRef		client;
    interface_t *		if_p;
    const char *		ifname;
    interface_list_t *		interfaces = NULL;
    boolean_t			send_bad_options = FALSE;

    if (argc < 2) {
	fprintf(stderr, "%s <ifname>\n", argv[0]);
	exit(1);
    }
    else if (argc >= 3) {
	switch (argv[2][0]) {
	case 'a':
	case 'A':
	default:
	    S_allocate_address = TRUE;
	    break;
	case 'b':
	case 'B':
	    send_bad_options = TRUE;
	    break;
	}
    }
    interfaces = get_interface_list();
    if (interfaces == NULL) {
	fprintf(stderr, "failed to get interface list\n");
	exit(2);
    }
    ifname = argv[1];
    if_p = ifl_find_name(interfaces, ifname);
    if (if_p == NULL) {
	fprintf(stderr, "No such interface '%s'\n", ifname);
	exit(2);
    }
    (void) openlog("DHCPv6Client", LOG_PERROR | LOG_PID, LOG_DAEMON);
    DHCPv6SocketSetVerbose(TRUE);
    client = DHCPv6ClientCreate(if_p);
    if (client == NULL) {
	fprintf(stderr, "DHCPv6ClientCreate(%s) failed\n", ifname);
	exit(2);
    }
    if (send_bad_options) {
	DHCPv6ClientSendBadOptions(client);
    }
    else {
	notification_init(client);
	DHCPv6ClientSetNotificationCallBack(client, client_notification, NULL);
	DHCPv6ClientStart(client, S_allocate_address);
	CFRunLoopRun();
    }
    exit(0);
    return (0);
}
#endif /* TEST_DHCPV6_CLIENT */
