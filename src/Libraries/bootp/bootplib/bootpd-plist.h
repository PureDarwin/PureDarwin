/*
 * Copyright (c) 2006-2010 Apple Inc. All rights reserved.
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

#ifndef _S_BOOTPD_PLIST_H
#define _S_BOOTPD_PLIST_H

/* 
 * bootpd-plist.h
 * - format of /etc/bootpd.plist
 */
/* 
 * Modification History
 *
 * June 30, 2006	Dieter Siegmund (dieter@apple.com)
 * - created
 */

/*
 * /etc/bootpd.plist is an xml plist.  The structure is:
 * <dict>
 *	detect_other_dhcp_server
 *	bootp_enabled
 *	dhcp_enabled
 *	old_netboot_enabled
 *	netboot_enabled
 *	relay_enabled
 *	allow
 *	deny
 *	relay_ip_list
 *	reply_threshold_seconds
 *	use_open_directory
 *	NetBoot <dict>
 *		shadow_size_meg
 *		afp_users_max
 *		age_time_seconds
 *		afp_uid_start	
 *	Subnets <array>
 *		[0] <dict>
 *			name
 *			net_address
 *			net_mask
 *			net_range
 *			supernet
 *			allocate
 *			lease_max
 *			lease_min
 *			dhcp_* (e.g. dhcp_router)
 */

/*
 * Encoding: (root) <dict>
 *
 * --------------------------+----------------------------------------------
 * Property		     | Type
 * --------------------------+----------------------------------------------
 * detect_other_dhcp_server  | <boolean>, <integer>, <string>
 * --------------------------+----------------------------------------------
 * bootp_enabled	     | <boolean>, 
 * dhcp_enabled,	     | <string>,
 * old_netboot_enabled,	     | <array> of <string> 
 * netboot_enabled,	     |
 * relay_enabled 	     |
 * --------------------------+----------------------------------------------
 * allow, deny,		     | <array> of <string>
 * relay_ip_list	     |
 * --------------------------+----------------------------------------------
 * reply_threshold_seconds   | <integer>, <string>
 * --------------------------+----------------------------------------------
 * use_open_directory        | <boolean>, <integer>, <string>
 * --------------------------+----------------------------------------------
 */

/*
 * Encoding: NetBoot <dict>
 *
 * --------------------------+----------------------------------------------
 * Property		     | Encoding	
 * --------------------------+----------------------------------------------
 * shadow_size_meg	     | <integer>, <string>
 * afp_users_max	     |
 * age_time_seconds	     |
 * afp_uid_start	     |
 * --------------------------+----------------------------------------------
 * machine_name_format	     | <string>
 * --------------------------+----------------------------------------------
 */

/*
 * Encoding: Subnets <array> of <dict>
 *
 * <dict> contains:
 * --------------------------+----------------------------------------------
 * Property		     | Encoding	
 * --------------------------+----------------------------------------------
 * name  		     | <string>
 * net_address		     |
 * net_mask		     |
 * supernet		     |
 * --------------------------+----------------------------------------------
 * net_range		     | <array> of <string>
 * --------------------------+----------------------------------------------
 * allocate		     | <boolean>
 * --------------------------+----------------------------------------------
 * lease_min, lease_max	     | <integer>, <string>
 * --------------------------+----------------------------------------------
 * dhcp_*		     | convert using dhcp option conversion table
 * --------------------------+----------------------------------------------
 */

#include <CoreFoundation/CFString.h>

#define BOOTPD_PLIST_NETBOOT	CFSTR("NetBoot")
#define BOOTPD_PLIST_SUBNETS	CFSTR("Subnets")

#endif /* _S_BOOTPD_PLIST_H */
