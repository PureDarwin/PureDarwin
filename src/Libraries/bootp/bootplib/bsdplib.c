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

#include "dhcp.h"
#include "dhcp_options.h"
#include "dhcplib.h"
#include "bsdp.h"
#include "bsdplib.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "symbol_scope.h"

PRIVATE_EXTERN dhcptype_t
bsdptag_type(bsdptag_t tag)
{
    dhcptype_t type = dhcptype_none_e;

    switch (tag) {
    case bsdptag_message_type_e:
	type = dhcptype_uint8_e;
	break;
    case bsdptag_server_identifier_e:
	type = dhcptype_ip_e;
	break;
    case bsdptag_version_e:
    case bsdptag_server_priority_e:
    case bsdptag_reply_port_e:
	type = dhcptype_uint16_e;
	break;
    case bsdptag_machine_name_e:
    case bsdptag_boot_image_list_path_e:
    case bsdptag_shadow_file_path_e:
    case bsdptag_shadow_mount_path_e:
	type = dhcptype_string_e;
	break;
    case bsdptag_default_boot_image_e:
    case bsdptag_selected_boot_image_e:
	type = dhcptype_uint32_e;
	break;
    case bsdptag_boot_image_list_e:
	type = dhcptype_opaque_e;
	break;
    case bsdptag_netboot_1_0_firmware_e:
	type = dhcptype_none_e;
	break;
    case bsdptag_image_attributes_filter_list_e:
	type = dhcptype_uint16_mult_e;
	break;
    case bsdptag_max_message_size_e:
	type = dhcptype_uint16_e;
	break;
    }
    return (type);
}

PRIVATE_EXTERN const char *
bsdptag_name(bsdptag_t tag)
{
    STATIC const char * names[] = {
	NULL,
	"message type",			/* 1 */
	"version",			/* 2 */
	"server identifier",		/* 3 */
	"server priority",		/* 4 */
	"reply port",			/* 5 */
	"boot image list path",		/* 6 */
	"default boot image",		/* 7 */
	"selected boot image",		/* 8 */
	"boot image list",		/* 9 */
	"netboot 1.0 firmware",		/* 10 */
	"image attributes filter list",	/* 11 */
	"maximum message size",		/* 12 */
    };
    if (tag >= bsdptag_first_e && tag <= bsdptag_last_e) {
	return (names[tag]);
    }
    switch (tag) {
    case bsdptag_shadow_mount_path_e:
	return "shadow mount path";
    case bsdptag_shadow_file_path_e:
	return "shadow file path";
    case bsdptag_machine_name_e:
	return "machine name";
    default:
	break;
    }
    return ("<unknown>");
}

PRIVATE_EXTERN const char *
bsdp_msgtype_names(bsdp_msgtype_t type)
{
    STATIC const char * names[] = {
	"<none>",
	"LIST",
	"SELECT",
	"FAILED",
    };
    if (type >= bsdp_msgtype_none_e && type <= bsdp_msgtype_failed_e)
	return (names[type]);
    return ("<unknown>");
}

PRIVATE_EXTERN void
bsdp_print_packet(struct dhcp * pkt, int length, int options_only)
{
    dhcpo_err_str_t	err;
    int 	i;
    dhcpol_t	options;
    dhcpol_t	vendor_options;

    dhcpol_init(&options);
    dhcpol_init(&vendor_options);
    if (options_only == 0) {
	dhcp_packet_print(pkt, length);
    }
    if (dhcpol_parse_packet(&options, pkt, length, &err) == FALSE) {
	fprintf(stderr, "packet did not parse, %s\n", err.str);
	return;
    }
    if (dhcpol_parse_vendor(&vendor_options, &options, &err) == FALSE) {
	fprintf(stderr, "vendor options did not parse, %s\n", err.str);
	goto done;
    }
    printf("BSDP Options count is %d\n", dhcpol_count(&vendor_options));
    for (i = 0; i < dhcpol_count(&vendor_options); i++) {
	u_int8_t	code;
	u_int8_t *	opt = dhcpol_element(&vendor_options, i);
	u_int8_t	len;

	code = opt[TAG_OFFSET];
	len = opt[LEN_OFFSET];
	printf("%s: ", bsdptag_name(code));
	if (code == bsdptag_message_type_e) {
	    printf("%s (", bsdp_msgtype_names(opt[OPTION_OFFSET]));
	    dhcptype_print(bsdptag_type(code), opt + OPTION_OFFSET, len);
	    printf(")\n");
	}
	else {
	    dhcptype_print(bsdptag_type(code), opt + OPTION_OFFSET, len);
	    printf("\n");
	}
    }
 done:
    dhcpol_free(&options);
    dhcpol_free(&vendor_options);
    return;
}

PRIVATE_EXTERN boolean_t
bsdp_parse_class_id(void * buf, int buf_len, char * arch, 
		    char * sysid)
{
    int		len;
    u_char * 	scan;
    
    *arch = '\0';
    *sysid = '\0';

    len = strlen(BSDP_VENDOR_CLASS_ID);
    if (buf_len < len || memcmp(buf, BSDP_VENDOR_CLASS_ID, len))
	return (FALSE); /* not a BSDP class identifier */
    
    buf_len -= len;
    scan = (u_char *)buf + len;
    if (buf_len == 0)
	return (TRUE); /* server-generated */

    if (*scan != '/')
	return (FALSE);

    for (scan++, buf_len--; buf_len && *scan != '/'; scan++, buf_len--) {
	switch (*scan) {
	case '\n':
	case '\0':
	    return (FALSE);
	default:
	    break;
	}
	*arch++ = *scan;
    }
    *arch = '\0';
    if (*scan != '/') {
	return (FALSE);
    }
    for (scan++, buf_len--; buf_len; scan++, buf_len--) {
	switch (*scan) {
	case '\n':
	case '\0':
	    return (FALSE);
	default:
	    break;
	}
	*sysid++ = *scan;
    }
    *sysid = '\0';
    return (TRUE);
}

