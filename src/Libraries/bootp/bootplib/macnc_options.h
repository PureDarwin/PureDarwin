
#ifndef _S_MACNC_OPTIONS_H
#define _S_MACNC_OPTIONS_H

/*
 * Copyright (c) 1999 Apple Inc. All rights reserved.
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
 * macNCOptions.h
 * - dhcp/bootp options specific to the macNC
 */

/*
 * Modification History:
 *
 * December 15, 1997	Dieter Siegmund (dieter@apple)
 * - created
 * November 19, 1999 	Dieter Siegmund (dieter@apple)
 * - converted to regular C
 */

#include "gen_dhcp_tags.h"
#include "gen_dhcp_types.h"
#include "dhcp_options.h"

enum {
    /* macNC client request options */
    macNCtag_client_version_e 		= 220,
    macNCtag_client_info_e 		= 221,

    /* macNC server reply options */
    macNCtag_server_version_e 		= 230,
    macNCtag_server_info_e 		= 231,
    macNCtag_user_name_e		= 232,
    macNCtag_password_e			= 233,
    macNCtag_shared_system_file_e 	= 234,
    macNCtag_private_system_file_e 	= 235,
    macNCtag_page_file_e 		= 236,
    macNCtag_MacOS_machine_name_e	= 237,
    macNCtag_shared_system_shadow_file_e = 238,
    macNCtag_private_system_shadow_file_e = 239,
};
typedef uint32_t macNCtag_t;

enum {
    macNCtype_pstring_e		= dhcptype_last_e + 1,
    macNCtype_afp_path_e	= dhcptype_last_e + 2,
    macNCtype_afp_password_e	= dhcptype_last_e + 3,
};
typedef uint32_t macNCtype_t;

#import "afp.h"

#define AFP_PATH_OVERHEAD	13
#define AFP_PATH_LIMIT		242
#define AFP_PATHTYPE_SHORT	1
#define AFP_PATHTYPE_LONG	2
#define AFP_PATH_SEPARATOR	'\0'

/*
 * MACNC_CLIENT_INFO
 */
#define MACNC_CLIENT_INFO		"Apple MacNC"

boolean_t
macNCopt_encodeAFPPath(struct in_addr iaddr, uint16_t port,
		       const char * volname, uint32_t dirID,
		       uint8_t pathtype, const char * pathname,
		       char separator, void * buf,
		       int * len_p, dhcpo_err_str_t * err);
boolean_t
macNCopt_str_to_type(const char * str, 
		     int type, void * buf, int * len_p,
		     dhcpo_err_str_t * err);
#endif /* _S_MACNC_OPTIONS_H */
