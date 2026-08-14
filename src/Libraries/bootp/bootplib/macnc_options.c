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
 * macnc_options.c
 * - handle dhcp/bootp options specific to the macNC
 */

/*
 * Modification History:
 *
 * December 15, 1997	Dieter Siegmund (dieter@apple)
 * - created
 * November 19, 1999 	Dieter Siegmund (dieter@apple)
 * - converted to regular C
 */
#import <unistd.h>
#import <stdlib.h>
#import <stdio.h>
#import <sys/types.h>
#import <netinet/in.h>
#import <arpa/inet.h>
#import <string.h>
#import "rfc_options.h"
#import "macnc_options.h"

typedef struct {
    macNCtype_t		type;
    dhcptype_info_t	info;
} macNCtype_info_t;

static const macNCtype_info_t macNCtype_info_table[] = {
    { macNCtype_pstring_e, 	{ 1, dhcptype_none_e, "PString" } },
    { macNCtype_afp_path_e, 	{ 0, dhcptype_none_e, "AFP path" } },
    { macNCtype_afp_password_e, { 8, dhcptype_none_e, "AFP password" } },
};

static const int macNCtype_info_size = sizeof(macNCtype_info_table) 
     / sizeof(macNCtype_info_t);

typedef struct {
    macNCtag_t		tag;
    dhcptag_info_t	info;
} macNCtag_info_t;

static const macNCtag_info_t macNCtag_info_table[] = {
 { macNCtag_client_version_e, {dhcptype_uint32_e, "macNC_client_version" } },
 { macNCtag_client_info_e, { dhcptype_opaque_e, "macNC_client_info" } },
 { macNCtag_server_version_e, { dhcptype_uint32_e, "macNC_server_version" } },
 { macNCtag_server_info_e, { dhcptype_opaque_e, "macNC_server_info" } },
 { macNCtag_user_name_e, { dhcptype_string_e, "macNC_user_name" } },
 { macNCtag_password_e, { macNCtype_afp_password_e, "macNC_password" } },
 { macNCtag_shared_system_file_e, 
       { macNCtype_afp_path_e, "macNC_shared_system_file" } },
 { macNCtag_private_system_file_e, 
       { macNCtype_afp_path_e, "macNC_private_system_file" } },
 { macNCtag_page_file_e, 
       { macNCtype_afp_path_e, "macNC_page_file" } },
 { macNCtag_MacOS_machine_name_e, 
       { dhcptype_string_e, "macNC_MacOS_machine_name" } },
 { macNCtag_shared_system_shadow_file_e,
       { macNCtype_afp_path_e, "macNC_shared_system_shadow_file" } },
 { macNCtag_private_system_shadow_file_e,
       { macNCtype_afp_path_e, "macNC_private_system_shadow_file" } },
};

static int macNCtag_info_size = sizeof(macNCtag_info_table) 
     / sizeof(macNCtag_info_t);

static const dhcptype_info_t *
macNCtype_info(int type)
{
    int 			i;
    const dhcptype_info_t * 	t;

    for (i = 0; i < macNCtype_info_size; i++) {
	if (type == macNCtype_info_table[i].type)
	    return (&macNCtype_info_table[i].info);
    }
    t = dhcptype_info(type);
    if (t)
	return (t);
    return (NULL);
}

static const dhcptag_info_t * 
macNCtag_info(int tag)
{
    int 			i;
    const dhcptag_info_t * 	t;

    for (i = 0; i < macNCtag_info_size; i++) {
	if (tag == macNCtag_info_table[i].tag)
	    return (&macNCtag_info_table[i].info);
    }
    t = dhcptag_info(tag);
    if (t)
	return (t);
    return (NULL);
}

boolean_t
macNCopt_str_to_type(const char * str, 
		     int type, void * buf, int * len_p,
		     dhcpo_err_str_t * err)
{
    const dhcptype_info_t * 	type_info = macNCtype_info(type);

    if (err)
	err->str[0] = '\0';

    switch (type) {
      case macNCtype_afp_password_e: {
	  int len = (int)strlen(str);
	  if (*len_p < AFP_PASSWORD_LEN) {
	      if (err)
		  snprintf(err->str, sizeof(err->str),
			   "%s: buffer too small (%d < %d)",
			   type_info->name, *len_p, AFP_PASSWORD_LEN);
	      return (FALSE);
	  }
	  if (len > AFP_PASSWORD_LEN) {
	      if (err)
		  snprintf(err->str, sizeof(err->str),
			   "%s: string too large (%d > %d)",
			   type_info->name, len, AFP_PASSWORD_LEN);
	      return (FALSE);
	  }
	  *len_p = AFP_PASSWORD_LEN;
	  bzero(buf, AFP_PASSWORD_LEN);
	  strncpy((char *)buf, str, len);
        }
	break;
      case macNCtype_pstring_e: {
	  int len = (int)strlen(str);
	  if (*len_p < (len + 1)) {
	      if (err)
		  snprintf(err->str, sizeof(err->str),
			   "%s: buffer too small (%d < %d)",
			   type_info->name, *len_p, len + 1);
	      return (FALSE);
	  }
	  ((u_char *)buf)[0] = len;			/* string length */
	  bcopy(str, buf + 1, len);
	  *len_p = len + 1;
        }
	break;
      case macNCtype_afp_path_e:
	if (err)
	    snprintf(err->str, sizeof(err->str),
		     "%s: not supported", type_info->name);
	return (FALSE);
      default:
	return (dhcptype_from_str(str, type, buf, len_p, err));
    }
    return (TRUE);
}

static void
S_replace_separators(u_char * buf, int len, u_char sep, u_char new_sep)
{
    int i;

    for (i = 0; i < len; i++) {
	if (buf[i] == sep)
	    buf[i] = new_sep;
    }
    return;
}

boolean_t
macNCopt_encodeAFPPath(struct in_addr iaddr, uint16_t port,
		       const char * volname, uint32_t dirID,
		       uint8_t pathtype, const char * pathname,
		       char separator, void * buf,
		       int * len_p, dhcpo_err_str_t * err)
{
    void * 	buf_p = buf;
    int 	l;

    l = (int)strlen(volname) + (int)strlen(pathname);
    if (l > AFP_PATH_LIMIT) {
	if (err)
	    snprintf(err->str, sizeof(err->str),
		     "volume/path name length %d > %d-byte limit", l, 
		     AFP_PATH_LIMIT);
	return (FALSE);
    }

    if ((l + AFP_PATH_OVERHEAD) > *len_p) {
	if (err)
	    snprintf(err->str, sizeof(err->str),
		     "buffer too small: %d > %d", l + AFP_PATH_OVERHEAD, 
		    *len_p);
	return (FALSE);
    }
    *len_p = l + AFP_PATH_OVERHEAD;			/* option len */

    *((struct in_addr *)buf_p) = iaddr;		/* ip */
    buf_p += sizeof(iaddr);

    *((u_short *)buf_p) = port;			/* port */
    buf_p += sizeof(port);

    l = (int)strlen(volname);			/* VolName */
    *((u_char *)buf_p) = l;
    buf_p++;
    if (l)
	bcopy(volname, (u_char *)buf_p, l);
    buf_p += l;

    *((uint32_t *)buf_p) = dirID;			/* DirID */
    buf_p += sizeof(dirID);

    *((uint8_t *)buf_p) = pathtype;		/* AFPPathType */
    buf_p += sizeof(pathtype);

    l = (int)strlen(pathname);			/* PathName */
    *((uint8_t *)buf_p) = l;
    buf_p++;
    if (l) {
	bcopy(pathname, (u_char *)buf_p, l);
	S_replace_separators(buf_p, l, separator, AFP_PATH_SEPARATOR);
    }

    return (TRUE);
}

static void
print_pstring(const uint8_t * option)

{
    int 	i;
    int		len = option[0];

    for (i = 0; i < len; i++) {
	char ch = option[1 + i];
	printf("%c", ch ? ch : '.');
    }
}

static void
macNC_print_type(dhcptype_t type, void * opt, int option_len)
{
    int 	offset;
    uint8_t * 	option = opt;

    switch (type) {
      case macNCtype_afp_password_e:
	if (option_len != AFP_PASSWORD_LEN)
	    printf("bad password field\n");
	else {
	    char buf[9];
	    strncpy(buf, (char *)opt, AFP_PASSWORD_LEN);
	    buf[8] = '\0';
	    printf("%s", buf);
	}
	break;
      case macNCtype_afp_path_e:
	offset = 0;
	printf("(");

	dhcptype_print(dhcptype_ip_e, option, option_len);
	offset += 4;

	printf(", ");
	dhcptype_print(dhcptype_uint16_e, option + offset, option_len);
	offset += 2;

	printf(", ");
	print_pstring(option + offset);
	offset += option[offset] + 1;

	printf(", ");
	dhcptype_print(dhcptype_uint32_e, option + offset, option_len);
	offset += 4;

	printf(", ");
	dhcptype_print(dhcptype_uint8_e, option + offset, option_len);
	offset += 1;

	printf(", ");
	print_pstring(option + offset);
	printf(")");
	break;

      case macNCtype_pstring_e: {
	print_pstring(option);
	break;
      }
      default:
	dhcptype_print(type, option, option_len);
	break;
    }
    return;
}

boolean_t
macNC_print_option(void * vopt)
{
    u_char *    		opt = vopt;
    u_char 			tag = opt[TAG_OFFSET];
    u_char 			option_len = opt[LEN_OFFSET];
    u_char * 			option = opt + OPTION_OFFSET;
    const dhcptag_info_t * 	entry;

    entry = macNCtag_info(tag);
    if (entry == NULL)
	return (FALSE);
    {	
	const dhcptype_info_t * type = macNCtype_info(entry->type);
	
	if (type == NULL) {
	    printf("unknown type %d\n", entry->type);
	    return (FALSE);
	}
	printf("%s (%s): ", entry->name, type->name);
	if (tag == dhcptag_dhcp_message_type_e)
	    printf("%s ", dhcp_msgtype_names(*option));
	macNC_print_type(entry->type, option, option_len);
	printf("\n");
    }
    return (TRUE);
}

void
macNCopt_print(dhcpol_t * list)
{
    int 		i;

    printf("Options count is %d\n", dhcpol_count(list));
    for (i = 0; i < dhcpol_count(list); i++) {
	unsigned char * option = dhcpol_element(list, i);
	if (macNC_print_option(option) == FALSE) 
	    printf("undefined tag %d len %d\n", option[TAG_OFFSET], 
		   option[LEN_OFFSET]);
    }
}

#ifdef TEST_MACNC_OPTIONS

/**
 **
 ** Testing 1 2 3
 **
 **/
u_char test[] = 
{
    dhcptag_subnet_mask_e,
    4,
    255, 255, 252, 0,
    
    dhcptag_router_e,
    12,
    17, 202, 40, 1,
    17, 202, 41, 1,
    17, 202, 42, 1, 
    
    dhcptag_domain_name_server_e,
    4,
    17, 128, 100, 12,
    
    dhcptag_host_name_e,
    7,
    's', 'i', 'e', 'g', 'd', 'i', '7',
    
    dhcptag_pad_e,
    
    dhcptag_all_subnets_local_e,
    1,
    0,
    
    dhcptag_vendor_specific_e,
    24,
    't', 'h', 'i', 's', ' ', 'i', 's', ' ', 'a', ' ', 't', 'e', 's', 't',
    234, 212, 0, 1, 2, 3, 4, 5, 6, 7,

    macNCtag_user_name_e,
    10,
    'M', 'a', 'c', 'N', 'C', ' ', '#', ' ', '1', '9',

    macNCtag_shared_system_file_e,
    29,
    17, 202, 40, 191,
    0x20, 0x00,
    4, 'a', 'b', 'c', 'd',
    0, 0, 0, 0,
    2,
    12, 0, 'e', 0, 'f', 0, 'g', 'h', 'i', 0, 'j', 'k', 'l',
    
    dhcptag_end_e,
};

#if 0
u_char test[] = {
0x01, 0x04, 0xff, 0x00, 0x00, 0x00, 0x03, 0x04, 0x0f, 0x03, 0x03, 0x09, 0x06, 
0x04, 0x0f, 0x03, 0x03, 0x09, 0xe8, 0x09, 0x08, 0x6d, 0x61, 0x63, 0x6e, 0x63, 0x30, 0x30, 0x30,
0xed, 0x09, 0x08, 0x6d, 0x61, 0x63, 0x6e, 0x63, 0x30, 0x30, 0x30, 0xe9, 0x09, 0x08, 0x74, 0x65,
0x73, 0x74, 0x69, 0x6e, 0x67, 0x32, 0xea, 0x18, 0x0f, 0x03, 0x03, 0x05, 0x02, 0x24, 0x00, 0x06,
0x64, 0x75, 0x63, 0x61, 0x74, 0x73, 0x09, 0x31, 0x35, 0x2e, 0x33, 0x2e, 0x33, 0x2e, 0x31, 0x37,
0xeb, 0x18, 0x0f, 0x03, 0x03, 0x05, 0x02, 0x24, 0x00, 0x06, 0x64, 0x75, 0x63, 0x61, 0x74, 0x73,
0x09, 0x31, 0x35, 0x2e, 0x33, 0x2e, 0x33, 0x2e, 0x31, 0x37, 0xec, 0x18, 0x0f, 0x03, 0x03, 0x05,
0x02, 0x24, 0x00, 0x06, 0x64, 0x75, 0x63, 0x61, 0x74, 0x73, 0x09, 0x31, 0x35, 0x2e, 0x33, 0x2e,
0x33, 0x2e, 0x31, 0x37, 0xff,
};
#endif

int
main()
{
    dhcpo_err_str_t 	err;
    dhcpol_t 		list;

    dhcpol_init(&list);
    if (dhcpol_parse_buffer(&list, test, sizeof(test), &err) == FALSE) {
	printf("parse test failed, %s\n", err.str);
	exit(1);
    }
    macNCopt_print(&list);
    exit(0);
#if 0
    {
	struct in_addr	iaddr;
	int i = sizeof(iaddr);
	
	if ([options str:"17.202.42.129" ToType:dhcptype_ip_e 
	     Buffer:(void *)&iaddr Length:&i] == FALSE) {
	    printf("conversion failed %s\n", [options errString]);
	}
	else {
	    printf("ip address should be 17.202.42.129: ");
	    [options printType:dhcptype_ip_e Size:i Option:(void *)&iaddr
	     Length:i];
	    printf("\n");
	}
    }
    {
	unsigned char buf[32] = "Mac NC #33";
	unsigned char buf2[34];
	int len = sizeof(buf2);
	
	if ([options str:buf ToType:macNCtype_pstring_e Buffer:buf2
	     Length:&len] == FALSE) {
	    printf("conversion failed %s\n", [options errString]);
	}
	else {
	    printf("macNCtype string should be %s:", buf);
	    [options printType:macNCtype_pstring_e Size:0 Option:buf2
	     Length:len];
	    printf("\n");
	}
    }
    {
	struct in_addr	iaddr[10];
	int l = sizeof(iaddr);
	u_char * strList[] = { "17.202.40.1", "17.202.41.1", "17.202.42.1",
			     "17.202.43.1" };
	int num = sizeof(strList) / sizeof(*strList);

	if ([options strList:strList Number:num Tag:dhcptag_router_e
	     Buffer:(void *)iaddr Length:&l] == FALSE) {
	    printf("conversion failed %s\n", [options errString]);
	}
	else {
	    [options printType:dhcptype_ip_mult_e Size:4 Option:(void *)iaddr
	     Length:l];
	    printf("\n");
	}
    }
    {
	u_char buf[100];
	u_char * strList[] = { "17.86.91.2", "0x100", "0", "greatVolumeName",
				   "/spectacular/path/name/eh" };
	int l = sizeof(buf);
	int num = sizeof(strList) / sizeof(*strList);

	if ([macNCOptions strList:strList Number:num 
	     Tag:macNCtag_system_path_shared_e Buffer:(void *)buf Length:&l
	     ErrorString:[options errString]] == FALSE) {
	    printf("conversion failed %s\n", [options errString]);
	}
	else {
	    printf("conversion OK\n");
	    [options printType:macNCtype_afp_path_e Size:0 Option:(void *)buf
	     Length:l];
	    printf("\n");
	}
    }
    {
	u_char buf[100];
	int l = sizeof(buf);
	struct in_addr iaddr;

	iaddr.s_addr = inet_addr("17.202.101.100");
	if ([macNCOptions encodeAFPPath
	     : iaddr 
	     : 0x1234
	     : "volumeName"
	     : AFP_DIRID_NULL
	     : AFP_PATHTYPE_LONG
	     : "this:is:the:path" 
	     : ':'
	     Into: (void *)buf 
	     Length: &l 
	     ErrorString: [options errString]] == FALSE) {
	    printf("conversion path failed %s\n", [options errString]);
	}
	else {
	    printf("conversion OK\n");
	    [options printType:macNCtype_afp_path_e Size:0 Option:(void *)buf
	     Length:l];
	    printf("\n");
	}
	
    }
    [options free];
    options = nil;
    { 
	unsigned char buf[300];
	int len = sizeof(buf);
	id o = [[macNCOptions alloc] initWithBuffer:(void *)buf Size:len];

	if (o == nil) {
	    printf("initWithBuffer failed\n");
	}
	else {
	    if ([o addOption:macNCtag_user_name_e FromString:"Mac NC # 22"] 
		== FALSE
		||
		[o addOption:dhcptag_subnet_mask_e FromString:"255.255.255.0"]
		== FALSE
		||
		[o addOption:dhcptag_end_e Length:0 Data:0] == FALSE
		) {
		printf("%s", [o errString]);
	    }
	    else {
		[o parse];
		[o print];
	    }
	}
    }
	
#endif
    
    exit(0);
}
#endif /* TEST_MACNC_OPTIONS */

