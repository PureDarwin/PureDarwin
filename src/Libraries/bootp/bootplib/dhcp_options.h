/*
 * Copyright (c) 1999-2013 Apple Inc. All rights reserved.
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
 * dhcp_options.h
 * - routines to parse and access dhcp options
 *   and create new dhcp option areas
 */


#ifndef _S_DHCP_OPTIONS_H
#define _S_DHCP_OPTIONS_H

/*
 * Modification History
 *
 * November 23, 1999	Dieter Siegmund (dieter@apple)
 * - created
 */

#include <stdio.h>
#include <mach/boolean.h>
#include <CoreFoundation/CFString.h>
#include "ptrlist.h"
#include "dhcp.h"
#include "gen_dhcp_tags.h"
#include "gen_dhcp_types.h"

#define DHCP_OPTION_TAG_MAX	255

/*
 * DHCP_OPTION_SIZE_MAX
 * - the largest size that an option can be (limited to an 8-bit quantity)
 */
#define DHCP_OPTION_SIZE_MAX	255

#define TAG_OFFSET	0
#define LEN_OFFSET	1
#define OPTION_OFFSET	2

typedef struct {
    char	str[256];
} dhcpo_err_str_t;


/*
 * Module: dhcpoa (dhcp options area)
 *
 * Purpose:
 *   Types and functions to create new dhcp option areas.
 */

/*
 * Struct: dhcpoa_s
 * Purpose:
 *   To record information about a dhcp option data area.
 */
struct dhcpoa_s {
    u_long	oa_magic;	/* magic number to ensure it's been init'd */
    void *	oa_buffer;	/* data area to hold options */
    int		oa_size;	/* size of buffer */
    int		oa_offset;	/* offset of next option to write */
    boolean_t	oa_end_tag;	/* to mark when options are terminated */
    int		oa_last;	/* the offset of the last option added */
    int		oa_prev_last;	/* the offset of the option previous to last */
    int		oa_option_count;/* number of options present */
    dhcpo_err_str_t oa_err;	/* error string */
    int		oa_reserve; 	/* space to reserve, either 0 or 1 */
};

/*
 * Type: dhcpoa_t
 *
 * Purpose:
 *   To record information about a dhcp option data area.
 */
typedef struct dhcpoa_s dhcpoa_t;

/* 
 * Type:dhcpoa_ret_t
 *
 * Purpose:
 *  outine return codes 
 */
typedef enum {
    dhcpoa_success_e = 0,
    dhcpoa_failed_e,
    dhcpoa_full_e,
} dhcpoa_ret_t;

void
dhcpoa_init(dhcpoa_t * opt, void * buffer, int size);

void
dhcpoa_init_no_end(dhcpoa_t * opt, void * buffer, int size);

dhcpoa_ret_t
dhcpoa_add(dhcpoa_t * oa_p, dhcptag_t tag, int len, const void * option);

dhcpoa_ret_t
dhcpoa_add_from_strlist(dhcpoa_t * oa_p, dhcptag_t tag, 
			const char * * strlist, int strlist_len);

dhcpoa_ret_t
dhcpoa_add_from_str(dhcpoa_t * oa_p, dhcptag_t tag, 
		    const char * str);

dhcpoa_ret_t
dhcpoa_add_dhcpmsg(dhcpoa_t * oa_p, dhcp_msgtype_t msgtype);

dhcpoa_ret_t
dhcpoa_vendor_add(dhcpoa_t * oa_p, dhcpoa_t * vendor_oa_p,
		  dhcptag_t tag, int len, void * option);

char *
dhcpoa_err(dhcpoa_t * oa_p);

int
dhcpoa_used(dhcpoa_t * oa_p);

int
dhcpoa_count(dhcpoa_t * oa_p);

void *
dhcpoa_buffer(dhcpoa_t * oa_p);

int
dhcpoa_freespace(dhcpoa_t * oa_p);

int
dhcpoa_size(dhcpoa_t * oa_p);

/*
 * Module: dhcpol (dhcp options list)
 *
 * Purpose:
 *   Routines to parse and retrieve dhcp options.
 */

typedef ptrlist_t dhcpol_t;

void			dhcpol_init(dhcpol_t * list);
void			dhcpol_free(dhcpol_t * list);
int			dhcpol_count(dhcpol_t * list);
boolean_t		dhcpol_add(dhcpol_t * list, void * element);
void *			dhcpol_element(dhcpol_t * list, int i);
boolean_t		dhcpol_concat(dhcpol_t * list, dhcpol_t * extra);
boolean_t		dhcpol_parse_buffer(dhcpol_t * list, void * buffer, 
					    int length, dhcpo_err_str_t * err);
void *			dhcpol_find(dhcpol_t * list, int tag, int * len_p, 
				    int * start);
void *			dhcpol_find_with_length(dhcpol_t * options,
						dhcptag_t tag, int min_length);
void *			dhcpol_option_copy(dhcpol_t * list, int tag,
					   int * len_p);
boolean_t		dhcpol_parse_packet(dhcpol_t * options, 
					    struct dhcp * pkt, int len,
					    dhcpo_err_str_t * err);
boolean_t		dhcpol_parse_vendor(dhcpol_t * vendor, 
					    dhcpol_t * options,
					    dhcpo_err_str_t * err);
void			dhcpol_print(dhcpol_t * list);
void			dhcpol_print_cfstr(CFMutableStringRef str,
					   dhcpol_t * list);
int			dhcpol_count_params(dhcpol_t * options, 
					    const uint8_t * tags, int size);

/*
 * Functions: dhcptype_*, dhcptag_*
 *
 * Purpose:
 *   Get tag and type information as well as do conversions.
 */

const dhcptype_info_t *	dhcptype_info(dhcptype_t type);
boolean_t		dhcptype_from_str(const char * str, 
					  int type, void * buf, int * len_p,
					  dhcpo_err_str_t * err);
boolean_t		dhcptype_to_str(char * str, size_t str_len, const void * opt, 
					int len, int type, 
					dhcpo_err_str_t * err);
void			dhcptype_print_simple(dhcptype_t type, 
					      const void * opt, int option_len);
void			dhcptype_print(dhcptype_t type, const void * option, 
				       int option_len);

const dhcptag_info_t *	dhcptag_info(dhcptag_t tag);
dhcptag_t		dhcptag_with_name(const char * name);
const char *		dhcptag_name(int tag);
boolean_t		dhcptag_from_strlist(const char * * slist, 
					     int num, int tag, void * buf, 
					     int * len_p, 
					     dhcpo_err_str_t * err);
boolean_t		dhcptag_to_str(char * tmp, size_t tmplen, int tag, 
				       const void * opt, int len, 
				       dhcpo_err_str_t * err);
boolean_t		dhcptag_print(const void * vopt);

#endif /* _S_DHCP_OPTIONS_H */
