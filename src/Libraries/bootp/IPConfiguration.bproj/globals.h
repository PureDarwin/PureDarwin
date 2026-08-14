/*
 * Copyright (c) 2003-2016 Apple Inc. All rights reserved.
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

#ifndef _S_GLOBALS_H
#define _S_GLOBALS_H
#include <mach/boolean.h>
#include <stdint.h>
#include "timer.h"
#include "IPConfigurationControlPrefs.h"

extern uint16_t 		G_client_port;
extern boolean_t		G_dhcp_accepts_bootp;
extern boolean_t		G_dhcp_failure_configures_linklocal;
extern boolean_t		G_dhcp_success_deconfigures_linklocal;
extern int			G_dhcp_init_reboot_retry_count;
extern int			G_dhcp_select_retry_count;
extern int			G_dhcp_allocate_linklocal_at_retry_count;
extern int			G_dhcp_generate_failure_symptom_at_retry_count;
extern int			G_dhcp_router_arp_at_retry_count;
extern uint16_t			G_server_port;
extern int			G_gather_secs;
extern int			G_initial_wait_secs;
extern int			G_max_wait_secs;
extern int			G_gather_secs;
extern int			G_link_inactive_secs;
extern int			G_max_retries;
extern boolean_t 		G_must_broadcast;
extern Boolean			G_IPConfiguration_verbose;
extern boolean_t		G_router_arp;
extern int			G_router_arp_wifi_lease_start_threshold_secs;
extern int			G_dhcp_defend_ip_address_interval_secs;
extern int			G_dhcp_defend_ip_address_count;
extern int			G_dhcp_lease_write_t1_threshold_secs;
extern int			G_manual_conflict_retry_interval_secs;
extern boolean_t		G_dhcpv6_enabled;
extern boolean_t		G_dhcpv6_stateful_enabled;
extern int			G_dhcp_duid_type;
extern boolean_t		G_is_netboot;
extern int			G_min_short_wake_interval_secs;
extern int			G_min_wake_interval_secs;
extern int			G_wake_skew_secs;

extern const unsigned char	G_rfc_magic[4];
extern const struct in_addr	G_ip_broadcast;
extern const struct in_addr	G_ip_zeroes;
extern IPConfigurationInterfaceTypes G_awd_interface_types;

#include "ipconfigd_globals.h"

#endif /* _S_GLOBALS_H */
