/*
 * Copyright (c) 2003-2014 Apple Inc. All rights reserved.
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

#ifndef _S_BSDP_H
#define _S_BSDP_H

/*
 * bsdp.h
 * - Boot Server Discovery Protocol (BSDP) definitions
 */

/*
 * Modification History
 *
 * Dieter Siegmund (dieter@apple.com)		November 24, 1999
 * - created
 */

#include <mach/boolean.h>
#include <string.h>
#include <sys/types.h>
#include "dhcp.h"
#include "dhcp_options.h"

/*
 * The boot_image_id consists of: (highest byte,bit to lowest bit,byte:
 * struct boot_image_id {
 *     u_int16_t	attributes;
 *     u_int16_t	index;
 * };
 *
 * The attributes field contains:
 * struct bsdp_image_attributes {
 *     u_int16_t	install:1,
 *     			kind:7,
 *                      reserved:8;
 */
#define BOOT_IMAGE_ID_NULL	((bsdp_image_id_t)0)
#define BSDP_IMAGE_INDEX_MAX	0xffff
#define BSDP_IMAGE_ATTRIBUTES_INSTALL	((u_int16_t)0x8000)
#define BSDP_IMAGE_ATTRIBUTES_KIND_MASK	((u_int16_t)0x7f00)
#define BSDP_IMAGE_ATTRIBUTES_KIND_MAX	0x7f

#define BSDP_PRIORITY_MIN	((bsdp_priority_t) 0)
#define BSDP_PRIORITY_MAX	((bsdp_priority_t) 65535)

#define BSDP_PRIORITY_BASE	((bsdp_priority_t) 32768)

#define BSDP_VENDOR_CLASS_ID	"AAPLBSDPC"
#define BSDP_VERSION_1_0	((unsigned short)0x0100)
#define BSDP_VERSION_1_1	((unsigned short)0x0101)
#define BSDP_VERSION_0_0	((unsigned short)0x0)

typedef enum {
    bsdp_image_kind_MacOS9 = 0,
    bsdp_image_kind_MacOSX = 1,
    bsdp_image_kind_MacOSXServer = 2,
    bsdp_image_kind_Diagnostics = 3,
} bsdp_image_kind_t;

/* 
 * the maximum length of name
 * = DHCP_OPTION_SIZE_MAX - OPTION_OFFSET - OPTION_OFFSET
 * 		- sizeof(bsdp_image_description_t)
 * = 255 (max option size) 
 * 		- 2 (vendor specific option's tag/len) 
 * 		- 2 (this option's tag/len) 
 * 		- 5 (this struct's boot_image_id + length)
 * = 246
 */
#define BSDP_IMAGE_NAME_MAX 	(DHCP_OPTION_SIZE_MAX - 2 * OPTION_OFFSET - 5)
typedef struct {
    u_int8_t			boot_image_id[4];
    u_int8_t			name_length;
    u_int8_t			name[0];
} bsdp_image_description_t;

typedef u_int16_t bsdp_priority_t;
typedef u_int16_t bsdp_version_t;
typedef u_int32_t bsdp_image_id_t;

static __inline__ u_int16_t
bsdp_image_index(bsdp_image_id_t image_id)
{
    return (image_id & 0xffff);
}

static __inline__ u_int16_t
bsdp_image_attributes(bsdp_image_id_t image_id)
{
    return (image_id >> 16);
}

static __inline__ bsdp_image_id_t
bsdp_image_id_make(u_int16_t index, u_int16_t attributes)
{
    return (index | ((bsdp_image_id_t)attributes << 16));
}

static __inline__ boolean_t
bsdp_image_index_is_server_local(u_int16_t index)
{
    return (index < 4096);
}

static __inline__ boolean_t
bsdp_image_identifier_is_server_local(u_int32_t identifier)
{
    return (bsdp_image_index_is_server_local(bsdp_image_index(identifier)));
}

static __inline__ boolean_t
bsdp_image_identifier_is_install(u_int32_t identifier)
{
    if ((bsdp_image_attributes(identifier) & BSDP_IMAGE_ATTRIBUTES_INSTALL)
	!= 0) {
	return (TRUE);
    }
    return (FALSE);
}

static __inline__ bsdp_image_kind_t
bsdp_image_kind_from_attributes(u_int16_t attr)
{
    return ((attr & BSDP_IMAGE_ATTRIBUTES_KIND_MASK) >> 8);
}

static __inline__ u_int16_t
bsdp_image_attributes_from_kind(bsdp_image_kind_t kind)
{
    return ((kind << 8) & BSDP_IMAGE_ATTRIBUTES_KIND_MASK);
}

typedef enum {
    /* protocol-specific */
    bsdptag_message_type_e 		= 1,
    bsdptag_version_e 			= 2,
    bsdptag_server_identifier_e		= 3,
    bsdptag_server_priority_e		= 4,
    bsdptag_reply_port_e		= 5,
    bsdptag_boot_image_list_path_e	= 6, /* not used */
    bsdptag_default_boot_image_e	= 7,
    bsdptag_selected_boot_image_e	= 8,
    bsdptag_boot_image_list_e		= 9,
    bsdptag_netboot_1_0_firmware_e	= 10,
    bsdptag_image_attributes_filter_list_e = 11,
    bsdptag_max_message_size_e 		= 12,

    /* protocol-specific bounds */
    bsdptag_first_e			= 1,
    bsdptag_last_e			= 12,

    /* image-specific */
    bsdptag_shadow_mount_path_e		= 128,	/* string (URL) */
    bsdptag_shadow_file_path_e		= 129,	/* string (URL) */
    bsdptag_machine_name_e		= 130,  /* string */
} bsdptag_t;

typedef enum {
    bsdp_msgtype_none_e				= 0,
    bsdp_msgtype_list_e 			= 1,
    bsdp_msgtype_select_e 			= 2,
    bsdp_msgtype_failed_e			= 3,
} bsdp_msgtype_t;

dhcptype_t
bsdptag_type(bsdptag_t tag);

const char *
bsdptag_name(bsdptag_t tag);

const char *
bsdp_msgtype_names(bsdp_msgtype_t type);

/*
 * Function: bsdp_parse_class_id
 *
 * Purpose:
 *   Parse the given option into the arch and system identifier
 *   fields.
 *   
 *   The format is "AAPLBSDPC/<arch>/<system_id>" for client-generated
 *   requests and "AAPLBSDPC" for server-generated responses.
 */
boolean_t
bsdp_parse_class_id(void * buf, int buf_len, char * arch, 
		    char * sysid);
#endif /* _S_BSDP_H */
