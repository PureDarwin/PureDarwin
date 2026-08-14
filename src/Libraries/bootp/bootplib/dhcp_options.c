/*
 * Copyright (c) 1999-2018 Apple Inc. All rights reserved.
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
 * dhcp_options.c
 * - routines to parse and access dhcp options
 *   and create new dhcp option areas
 * - handles overloaded areas as well as vendor-specific options
 *   that are encoded using the RFC 2132 encoding
 */

/* 
 * Modification History
 *
 * November 23, 1999	Dieter Siegmund (dieter@apple)
 * - created from objective C version (dhcpOptions.m)
 * - generalized code so that the file, sname and vendor specific
 *   areas could be parsed using the same routines
 * - cleanly separated functions that deal with existing option areas
 *   (dhcpol_*) from those that create new option areas (dhpoa_*).
 */
#include "dhcp.h"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <strings.h>

#include "rfc_options.h"
#include "gen_dhcp_types.h"

#include "dhcp_options.h"
#include "gen_dhcp_parse_table.h"
#include "util.h"
#include "DNSNameList.h"
#include "IPv4ClasslessRoute.h"
#include "nbo.h"
#include "cfutil.h"
#include "symbol_scope.h"

/*
 * Module: dhcpoa (dhcp options area)
 *
 * Purpose:
 *   Types and functions to create new dhcp option areas.
 */

#define DHCPOA_MAGIC	0x11223344


#define MAX_TAG (sizeof(dhcptag_info_table) / sizeof(dhcptag_info_table[0]))


PRIVATE_EXTERN const uint8_t	rfc_magic[] = RFC_OPTIONS_MAGIC;

/*
 * Functions: dhcptag_*, dhcptype_*
 *
 * Purpose:
 *   Functions to deal with dhcp option tag and type information.
 */

/*
 * Function: dhcptype_info
 *
 * Purpose:
 *   Return the type information for the given type.
 */
PRIVATE_EXTERN const dhcptype_info_t *
dhcptype_info(dhcptype_t type) 
{
    if (type > dhcptype_last_e)
	return (NULL);
    return dhcptype_info_table + type;
}

/*
 * Function: dhcptype_from_str
 *
 * Purpose:
 *   Convert from a string to the appropriate internal representation
 *   for the given type.  Calls the appropriate strto<type> function.
 */
PRIVATE_EXTERN boolean_t
dhcptype_from_str(const char * str, int type, void * buf, int * len_p,
		  dhcpo_err_str_t * err)
{
    const dhcptype_info_t * 	type_info = dhcptype_info(type);

    if (err)
	err->str[0] = '\0';

    if (type_info == NULL) {
	if (err) {
	    snprintf(err->str, sizeof(err->str), "type %d unknown", type);
	}
	return (FALSE);
    }

    if (*len_p < type_info->size) {
	if (err) {
	    snprintf(err->str, sizeof(err->str),
		     "type %s buffer too small (%d < %d)", 
		     type_info->name, *len_p, type_info->size);
	}
	return (FALSE);
    }

    switch (type) {
      case dhcptype_bool_e: {
	  long l = strtol(str, 0, 0);
	  *len_p = type_info->size;
	  *((uint8_t *)buf) = ((l == 0) ? 0 : 1);
	  break;
      }
      case dhcptype_uint8_e: {
	  unsigned long ul = strtoul(str, 0, 0);
	  *len_p = type_info->size;
	  if (ul > 255) {
	      if (err) {
		  snprintf(err->str, sizeof(err->str),
			   "value %ld too large for %s",
			   ul, type_info->name);
	      }
	      return (FALSE);
	  }
	  *((uint8_t *)buf) = ul;
	  break;
      }
      case dhcptype_uint16_e: {
	  unsigned long ul = strtoul(str, 0, 0);
	  unsigned short us = ul;

	  if (ul > 65535) {
	      if (err) {
		  snprintf(err->str, sizeof(err->str),
			   "value %ld too large for %s",
			   ul, type_info->name);
	      }
	      return (FALSE);
	  }
	  net_uint16_set(buf, us);
	  *len_p = type_info->size;
	  break;
      }
      case dhcptype_int32_e: {
	  int32_t l = (int32_t)strtol(str, 0, 0);
	  net_uint32_set(buf, l);
	  *len_p = type_info->size;
	  break;
      }
      case dhcptype_uint32_e: {
	  uint32_t l = (uint32_t)strtoul(str, 0, 0);
	  net_uint32_set(buf, l);
	  *len_p = type_info->size;
	  break;
      }
      case dhcptype_ip_e: {
	  struct in_addr ip;

	  if (inet_aton(str, &ip) == 0) {
	      if (err) {
		  snprintf(err->str, sizeof(err->str), "%s not valid ip",
			   str);
	      }
	      return (FALSE);
	  }
	  bcopy(&ip, buf, sizeof(ip));
	  *len_p = type_info->size;
	  break;
      }
      case dhcptype_string_e: {
	  int len = (int)strlen(str);
	  if (*len_p < len) {
	      if (err) {
		  snprintf(err->str, sizeof(err->str), "string too long");
	      }
	      return (FALSE);
	  }
	  if (len)
	      bcopy(str, buf, len);
	  *len_p = len;
	  break;
      }
      case dhcptype_dns_namelist_e: {
	  (void)DNSNameListBufferCreate(&str, 1, buf, len_p, TRUE);
	  if (*len_p == 0) {
	      if (err != NULL) {
		  strlcpy(err->str, "no DNS names added",
			  sizeof(err->str));
	      }
	      return (FALSE);
	  }
	  break;
      }
      default:
	  if (err) {
	      snprintf(err->str, sizeof(err->str), "type %s not yet supported",
		       type_info->name);
	  }
	  return (FALSE);
    }
    return (TRUE);
}

/*
 * Function: dhcptype_from_strlist
 *
 * Purpose:
 *   Convert from a string list to the appropriate internal representation
 *   for the given type.
 */
PRIVATE_EXTERN boolean_t
dhcptype_from_strlist(const char * * strlist, int strlist_count,
		      int type, void * buf, int * len_p,
		      dhcpo_err_str_t * err)
{
    if (err != NULL) {
	err->str[0] = '\0';
    }

    switch (type) {
      case dhcptype_dns_namelist_e: {
	  (void)DNSNameListBufferCreate(strlist, strlist_count, buf, len_p,
					TRUE);
	  if (*len_p == 0) {
	      if (err != NULL) {
		  strlcpy(err->str, "no DNS names added",
			  sizeof(err->str));
	      }
	      return (FALSE);
	  }
	  break;
      }
      case dhcptype_classless_route_e: {
	  CFArrayRef		cf_strlist;
	  IPv4ClasslessRouteRef	route_list = NULL;
	  int			route_list_count;

	  cf_strlist = my_CFStringArrayCreate(strlist, strlist_count);
	  if (cf_strlist != NULL) {
	      route_list
		  = IPv4ClasslessRouteListCreateWithArray(cf_strlist,
							  &route_list_count);
	      CFRelease(cf_strlist);
	  }
	  if (route_list != NULL) {
	      IPv4ClasslessRouteListBufferCreate(route_list, route_list_count,
						 buf, len_p);
	      CFRelease(route_list);
	  }
	  else {
	      *len_p = 0;
	  }
	  if (*len_p == 0) {
	      if (err != NULL) {
		  strlcpy(err->str,
			  "no IPv4 classless routes added",
			  sizeof(err->str));
	      }
	      return (FALSE);
	  }
	  break;
      }
      default:
	  if (err != NULL) {
	      snprintf(err->str, sizeof(err->str), "type %d not yet supported",
		       type);
	  }
	  return (FALSE);
    }
    return (TRUE);
}

/*
 * Function: dhcptype_to_str
 *
 * Purpose:
 *   Give a string representation to the given type.
 */
PRIVATE_EXTERN boolean_t
dhcptype_to_str(char * tmp, size_t maxtmplen,
		const void * opt, int len, int type,
		dhcpo_err_str_t * err)
{
    switch (type) {
    case dhcptype_bool_e:
	snprintf(tmp, maxtmplen, "%d", *((boolean_t *)opt));
	break;
    case dhcptype_uint8_e:
	snprintf(tmp, maxtmplen, "%d", *((uint8_t *)opt));
	break;
    case dhcptype_uint16_e:
	snprintf(tmp, maxtmplen, "%d", net_uint16_get(opt));
	break;
    case dhcptype_int32_e:
	snprintf(tmp, maxtmplen, "%d", net_uint32_get(opt));
	break;
    case dhcptype_uint32_e:
	snprintf(tmp, maxtmplen, "%u", net_uint32_get(opt));
	break;
    case dhcptype_ip_e:
	snprintf(tmp, maxtmplen, IP_FORMAT, IP_LIST(opt));
	break;
    case dhcptype_string_e:
	if (len < maxtmplen) {
	    strncpy(tmp, opt, len);
	    tmp[len] = '\0';
	}
	else {
	    snprintf(err->str, sizeof(err->str),
		     "type dhcptype_string_e: string too long");
	    return (FALSE);
	}
	break;
    default:
	if (err) {
	    snprintf(err->str, sizeof(err->str),
		     "type %d: not supported", type);
	    return (FALSE);
	}
    }
    return (TRUE);
}

PRIVATE_EXTERN void
dhcptype_print_cfstr_simple(CFMutableStringRef str, dhcptype_t type,
			    const void * opt, int option_len)
{
    const uint8_t *	option = opt;

    switch (type) {
      case dhcptype_bool_e:
	  STRING_APPEND(str, "%s", *option ? "TRUE" : "FALSE");
	  break;
	
      case dhcptype_ip_e:
	  STRING_APPEND(str, IP_FORMAT, IP_LIST(opt));
	  break;
	
      case dhcptype_string_e: {
	  STRING_APPEND(str, "%.*s", option_len, (char *)option);
	  break;
      }
	
      case dhcptype_opaque_e:
	STRING_APPEND(str, "\n");
	print_data_cfstr(str, option, option_len);
	break;

      case dhcptype_uint8_e:
	STRING_APPEND(str, "0x%x", *option);
	break;

      case dhcptype_uint16_e:
	STRING_APPEND(str, "0x%x", net_uint16_get(option));
	break;

      case dhcptype_int32_e:
      case dhcptype_uint32_e:
	STRING_APPEND(str, "0x%x", net_uint32_get(option));
	break;

      case dhcptype_dns_namelist_e: {
	  int			i;
	  const char * *	namelist;
	  int			namelist_length = 0;

	  namelist = DNSNameListCreate(option, option_len, &namelist_length);
	  STRING_APPEND(str, "{");
	  if (namelist != NULL) {
	      for (i = 0; i < namelist_length; i++) {
		  STRING_APPEND(str, "%s%s", (i == 0) ? "" : ", ",
			  namelist[i]);
	      }
	      free(namelist);
	  }
	  STRING_APPEND(str, "}");
	  break;
      }
      case dhcptype_classless_route_e: {
	  int			i;
	  IPv4ClasslessRouteRef	route_list;
	  int			route_list_count;

	  route_list = IPv4ClasslessRouteListCreate(option, option_len,
						    &route_list_count);
	  STRING_APPEND(str, "{");
	  if (route_list != NULL) {
	      IPv4ClasslessRouteRef	scan;

	      for (i = 0, scan = route_list;
		   i < route_list_count;
		   i++, scan++) {
		  STRING_APPEND(str, "%s" IP_FORMAT "/%d, " IP_FORMAT,
				i == 0 ? "" : "; ",
				IP_LIST(&scan->dest), scan->prefix_length,
				IP_LIST(&scan->gate));
	      }
	      free(route_list);
	  }
	  STRING_APPEND(str, "}");
	  break;
      }
      case dhcptype_none_e:
      default:
	break;
    }
    return;
}

PRIVATE_EXTERN void
dhcptype_fprint_simple(FILE * f, dhcptype_t type,
		       const void * opt, int option_len)
{
    CFMutableStringRef		str;
    
    str = CFStringCreateMutable(NULL, 0);
    dhcptype_print_cfstr_simple(str, type, opt, option_len);
    my_CFStringPrint(f, str);
    CFRelease(str);
    return;
}

/*
 * Function: dhcptype_print_simple
 * Purpose:
 *   Display (to stdout) an option with a simple type.
 */
PRIVATE_EXTERN void
dhcptype_print_simple(dhcptype_t type, const void * opt, int option_len)
{
    dhcptype_fprint_simple(stdout, type, opt, option_len);
    return;
}

PRIVATE_EXTERN void
dhcptype_print_cfstr(CFMutableStringRef str,
		     dhcptype_t type, const void * option, int option_len)
{
    const dhcptype_info_t * 	type_info = dhcptype_info(type);

    if (type_info && type_info->multiple_of != dhcptype_none_e) {
	int 			i;
	int 			number;
	const void *		offset;
	int 			size;
	const dhcptype_info_t * subtype_info;

	subtype_info = dhcptype_info(type_info->multiple_of);
	if (subtype_info == NULL)
	    return;
	size = subtype_info->size;
	number = option_len / size;
	STRING_APPEND(str, "{");
	for (i = 0, offset = option; i < number; i++) {
	    if (i != 0)
		STRING_APPEND(str, ", ");
	    dhcptype_print_cfstr_simple(str, type_info->multiple_of,
					offset, size);
	    offset += size;
	}
	STRING_APPEND(str, "}");
    }
    else {
	dhcptype_print_cfstr_simple(str, type, option, option_len);
    }
    return;
}

PRIVATE_EXTERN void
dhcptype_fprint(FILE * f, dhcptype_t type, const void * option, int option_len)
{
    CFMutableStringRef		str;
    
    str = CFStringCreateMutable(NULL, 0);
    dhcptype_print_cfstr(str, type, option, option_len);
    my_CFStringPrint(f, str);
    CFRelease(str);
    return;
}

/*
 * Function: dhcptype_print
 * Purpose:
 *   Display (to stdout) an option with the given type.
 */
PRIVATE_EXTERN void
dhcptype_print(dhcptype_t type, const void * option, int option_len)
{
    dhcptype_fprint(stdout, type, option, option_len);
    return;
}

STATIC boolean_t
dhcptag_print_cfstr(CFMutableStringRef str, const void * vopt)
{
    const dhcptag_info_t * entry;
    const uint8_t *	opt = vopt;
    uint8_t 		tag = opt[TAG_OFFSET];
    uint8_t 		option_len = opt[LEN_OFFSET];
    const uint8_t * 	option = opt + OPTION_OFFSET;

    entry = dhcptag_info(tag);
    if (entry == NULL)
	return (FALSE);
    {	
	const dhcptype_info_t * type = dhcptype_info(entry->type);
	
	if (type == NULL) {
	    STRING_APPEND(str, "unknown type %d\n", entry->type);
	    return (FALSE);
	}
	STRING_APPEND(str, "%s (%s): ", entry->name, type->name);
	if (tag == dhcptag_dhcp_message_type_e)
	    STRING_APPEND(str, "%s ", dhcp_msgtype_names(*option));
	dhcptype_print_cfstr(str, entry->type, option, option_len);
	STRING_APPEND(str, "\n");
    }
    return (TRUE);
}

PRIVATE_EXTERN boolean_t
dhcptag_fprint(FILE * f, const void * vopt)
{
    boolean_t			printed;
    CFMutableStringRef		str;
    
    str = CFStringCreateMutable(NULL, 0);
    printed = dhcptag_print_cfstr(str, vopt);
    CFRelease(str);
    return (printed);
}

/*
 * Function: dhcptag_print
 * Purpose:
 *   Display (to stdout) the given option.
 * Returns:
 *   TRUE it could be displayed, FALSE otherwise.
 */
PRIVATE_EXTERN boolean_t
dhcptag_print(const void * vopt)
{
    return (dhcptag_fprint(stdout, vopt));
}

/*
 * Function: dhcptag_info
 *
 * Purpose:
 *   Return the tag information for the give tag.
 */
PRIVATE_EXTERN const dhcptag_info_t *
dhcptag_info(dhcptag_t tag)
{
    if (tag >= MAX_TAG)
	return (NULL);
    return dhcptag_info_table + tag;
}

/*
 * Function: dhcptag_with_name
 *
 * Purpose:
 *   Given a name, return a corresponding tag.  This comes from
 *   the table, as well as the default name of the option number itself.
 */

PRIVATE_EXTERN dhcptag_t
dhcptag_with_name(const char * name)
{
    dhcptag_t 		i;
    unsigned int 	v;

    for (i = 0; i < MAX_TAG; i++) {
	if (dhcptag_info_table[i].name) {
	    if (strcmp(dhcptag_info_table[i].name, name) == 0)
		return (i);
	}
    }
    if (strncmp(name, "option_", 7) == 0) {
	v = (unsigned int)strtoul(name + 7, NULL, 10);
    }
    else {
	v = (unsigned int)strtoul(name, NULL, 10);
    }
    if (v <= MAX_TAG) {
	return (v);
    }
    return (-1);
}

/*
 * Function: dhcptag_name
 * Purpose:
 *   Return the name of the tag.
 */
PRIVATE_EXTERN const char *
dhcptag_name(int tag)
{
    const dhcptag_info_t * info;

    info = dhcptag_info(tag);
    if (info == NULL)
	return (NULL);
    return (info->name);
}

/*
 * Function: dhcptag_from_strlist
 *
 * Purpose:
 *   Convert from a string list using the appropriate
 *   type conversion for the tag's type.
 */
PRIVATE_EXTERN boolean_t
dhcptag_from_strlist(const char * * slist, int num, 
		     int tag, void * buf, int * len_p,
		     dhcpo_err_str_t * err)
{
    int				i;
    int				n_per_type;
    const dhcptag_info_t *	tag_info;
    const dhcptype_info_t * 	type_info;
    const dhcptype_info_t * 	base_type_info;

    if (err)
	err->str[0] = '\0';
    tag_info = dhcptag_info(tag);
    if (tag_info == NULL) {
	if (err)
	    snprintf(err->str, sizeof(err->str), "tag %d unknown", tag);
	return (FALSE);
    }

    type_info = dhcptype_info(tag_info->type);
    if (type_info == NULL) {
	if (err)
	    snprintf(err->str, sizeof(err->str), "tag %d type %d unknown", tag,
		     tag_info->type);
	return (FALSE);
    }
    if (type_info->string_list == FALSE) {
	return (dhcptype_from_str(slist[0], tag_info->type,
				  buf, len_p, err));
    }
    if (type_info->multiple_of == dhcptype_none_e) {
	return (dhcptype_from_strlist(slist, num, tag_info->type, 
				      buf, len_p, err));
    }
    base_type_info = dhcptype_info(type_info->multiple_of);
    if (base_type_info == NULL) {
	if (err)
	    snprintf(err->str, sizeof(err->str), "tag %d base type %d unknown", tag,
		     type_info->multiple_of);
	return (FALSE);
    }

    n_per_type = type_info->size / base_type_info->size;
    if (num & (n_per_type - 1)) {
	if (err)
	    snprintf(err->str, sizeof(err->str), "type %s not a multiple of %d",
		     type_info->name, n_per_type);
	return (FALSE);
    }

    if ((num * base_type_info->size) > *len_p) /* truncate if necessary */
	num = *len_p / base_type_info->size;

    for (i = 0, *len_p = 0; i < num; i++) {
	int 		l;

	l = base_type_info->size;
	if (dhcptype_from_str(slist[i], type_info->multiple_of, buf, &l,
			      err) == FALSE)
	    return (FALSE);
	buf += l;
	*len_p += l;
    }
    return (TRUE);
}

/*
 * Function: dhcptag_to_str
 *
 * Purpose:
 *   Give a string representation to the given tag.
 * Note:
 *   For tags whose type is a multiple of a type, this routine
 *   only returns the string representation for the first
 *   occurrence.
 */
PRIVATE_EXTERN boolean_t
dhcptag_to_str(char * tmp, size_t tmplen, int tag, const void * opt,
	       int len, dhcpo_err_str_t * err)
{
    const dhcptag_info_t * 	tag_info;
    const dhcptype_info_t * 	type_info;
    dhcptype_t			type;

    tag_info = dhcptag_info(tag);
    if (tag_info == NULL)
	return (FALSE);
    type_info = dhcptype_info(tag_info->type);
    if (type_info->multiple_of == dhcptype_none_e)
	type = tag_info->type;
    else
	type = type_info->multiple_of;
    return (dhcptype_to_str(tmp, tmplen, opt, len, type, err));
}

/*
 * Functions: dhcpol_* 
 *
 * Purpose:
 *   Routines to parse/access existing options buffers.
 */
PRIVATE_EXTERN boolean_t
dhcpol_add(dhcpol_t * list, void * element)
{
    return (ptrlist_add((ptrlist_t *)list, element));
}

PRIVATE_EXTERN int
dhcpol_count(dhcpol_t * list)
{
    return (ptrlist_count((ptrlist_t *)list));
}

PRIVATE_EXTERN void *
dhcpol_element(dhcpol_t * list, int i)
{
    return (ptrlist_element((ptrlist_t *)list, i));
}

PRIVATE_EXTERN void
dhcpol_init(dhcpol_t * list)
{
    ptrlist_init((ptrlist_t *)list);
}

PRIVATE_EXTERN void
dhcpol_free(dhcpol_t * list)
{
    ptrlist_free((ptrlist_t *)list);
}

PRIVATE_EXTERN boolean_t
dhcpol_concat(dhcpol_t * list, dhcpol_t * extra)
{
    return (ptrlist_concat((ptrlist_t *)list, (ptrlist_t *)extra));
}

/*
 * Function: dhcpol_parse_buffer
 *
 * Purpose:
 *   Parse the given buffer into DHCP options, returning the
 *   list of option pointers in the given dhcpol_t.
 *   Parsing continues until we hit the end of the buffer or
 *   the end tag.
 */
PRIVATE_EXTERN boolean_t
dhcpol_parse_buffer(dhcpol_t * list, void * buffer, int length,
		    dhcpo_err_str_t * err)
{
    int		len;
    uint8_t *	scan;
    uint8_t	tag;

    if (err)
	err->str[0] = '\0';

    dhcpol_init(list);

    len = length;
    tag = dhcptag_pad_e;
    for (scan = (uint8_t *)buffer; tag != dhcptag_end_e && len > 0; ) {

	tag = scan[TAG_OFFSET];

	switch (tag) {
	  case dhcptag_end_e:
	      dhcpol_add(list, scan); /* remember that it was terminated */
	      scan++;
	      len--;
	      break;
	  case dhcptag_pad_e: /* ignore pad */
	      scan++;
	      len--;
	      break;
	  default:
	      if (len > LEN_OFFSET) {
		  uint8_t	option_len;

		  option_len = scan[LEN_OFFSET];
		  dhcpol_add(list, scan);
		  len -= (option_len + OPTION_OFFSET);
		  scan += (option_len + OPTION_OFFSET);
	      }
	      else {
		  len = -1;
	      }
	      break;
	}
    }
    if (len < 0) {
	/* ran off the end */
	if (err)
	    snprintf(err->str, sizeof(err->str), "parse failed near tag %d", tag);
	dhcpol_free(list);
	return (FALSE);
    }
    return (TRUE);
}

/*
 * Function: dhcpol_find
 *
 * Purpose:
 *   Finds the first occurence of the given option, and returns its
 *   length and the option data pointer.
 *
 *   The optional start parameter allows this function to 
 *   return the next start point so that successive
 *   calls will retrieve the next occurence of the option.
 *   Before the first call, *start should be set to 0.
 */
PRIVATE_EXTERN void *
dhcpol_find(dhcpol_t * list, int tag, int * len_p, int * start)
{
    int 	i = 0;

    if (tag == dhcptag_end_e || tag == dhcptag_pad_e)
	return (NULL);

    if (start)
	i = *start;

    for (; i < dhcpol_count(list); i++) {
	uint8_t * option = dhcpol_element(list, i);
	
	if (option[TAG_OFFSET] == tag) {
	    if (len_p)
		*len_p = option[LEN_OFFSET];
	    if (start)
		*start = i + 1;
	    return (option + OPTION_OFFSET);
	}
    }
    return (NULL);
}

/*
 * Function: dhcpol_find_with_length
 * Purpose:
 *   Find a DHCP option with the specified tag that is at least as long
 *   as the passed in min_length value.  Return NULL if such an option
 *   does not exist.
 */
PRIVATE_EXTERN void *
dhcpol_find_with_length(dhcpol_t * options, dhcptag_t tag, int min_length)
{
    int		real_length;
    void * 	val;

    val = dhcpol_find(options, tag, &real_length, NULL);
    if (val == NULL) {
	return (NULL);
    }
    if (real_length < min_length) {
	return (NULL);
    }
    return (val);
}

/*
 * Function: dhcpol_option_copy
 * 
 * Purpose:
 *   Accumulate all occurences of the given option into a
 *   malloc'd buffer, and return its length.  Used to get
 *   all occurrences of a particular option in a single
 *   data area.
 * Note:
 *   Use free() to free the returned data area.
 */
PRIVATE_EXTERN void *
dhcpol_option_copy(dhcpol_t * list, int tag, int * len_p)
{
    int 	i;
    void *	data = NULL;
    int		data_len = 0;

    if (tag == dhcptag_end_e || tag == dhcptag_pad_e)
	return (NULL);

    for (i = 0; i < dhcpol_count(list); i++) {
	uint8_t * option = dhcpol_element(list, i);
	
	if (option[TAG_OFFSET] == tag) {
	    int len = option[LEN_OFFSET];

	    if (data == NULL) {
		data = malloc(len);
	    }
	    else {
		data = reallocf(data, data_len + len);
	    }
	    bcopy(option + OPTION_OFFSET, data + data_len, len);
	    data_len += len;
	}
    }
    *len_p = data_len;
    return (data);
}

/*
 * Function: dhcpol_parse_packet
 *
 * Purpose:
 *    Parse the option areas in the DHCP packet.
 *    Verifies that the packet has the right magic number,
 *    then parses and accumulates the option areas.
 *    First the pkt->dp_options is parsed.  If that contains
 *    the overload option, it parses pkt->dp_file if specified,
 *    then parses pkt->dp_sname if specified.
 */
PRIVATE_EXTERN boolean_t
dhcpol_parse_packet(dhcpol_t * options, struct dhcp * pkt, int len,
		    dhcpo_err_str_t * err)
{
    dhcpol_init(options);	/* make sure it's empty */

    if (err)
	err->str[0] = '\0';

    if (len < (sizeof(*pkt) + RFC_MAGIC_SIZE)) {
	if (err) {
	    snprintf(err->str, sizeof(err->str), "packet is too short: %d < %d",
		    len, (int)sizeof(*pkt) + RFC_MAGIC_SIZE);
	}
	return (FALSE);
    }
    if (bcmp(pkt->dp_options, rfc_magic, RFC_MAGIC_SIZE)) {
	if (err)
	    snprintf(err->str, sizeof(err->str), "missing magic number");
	return (FALSE);
    }
    if (dhcpol_parse_buffer(options, pkt->dp_options + RFC_MAGIC_SIZE,
			    len - sizeof(*pkt) - RFC_MAGIC_SIZE, err) == FALSE)
	return (FALSE);
    { /* get overloaded options */
	uint8_t *	overload;
	int		overload_len;

	overload = (uint8_t *)dhcpol_find(options, dhcptag_option_overload_e, 
					  &overload_len, NULL);
	if (overload && overload_len == 1) { /* has overloaded options */
	    dhcpol_t	extra;

	    dhcpol_init(&extra);
	    if (*overload == DHCP_OVERLOAD_FILE
		|| *overload == DHCP_OVERLOAD_BOTH) {
		if (dhcpol_parse_buffer(&extra, pkt->dp_file, 
					 sizeof(pkt->dp_file), NULL)) {
		    dhcpol_concat(options, &extra);
		    dhcpol_free(&extra);
		}
	    }
	    if (*overload == DHCP_OVERLOAD_SNAME
		|| *overload == DHCP_OVERLOAD_BOTH) {
		if (dhcpol_parse_buffer(&extra, pkt->dp_sname, 
					 sizeof(pkt->dp_sname), NULL)) {
		    dhcpol_concat(options, &extra);
		    dhcpol_free(&extra);
		}
	    }
	}
    }
    return (TRUE);
}

/*
 * Function: dhcpol_parse_vendor
 *
 * Purpose:
 *   Given a set of options, find the vendor specific option(s)
 *   and parse all of them into a single option list.
 *  
 * Return value:
 *   TRUE if vendor specific options existed and were parsed succesfully,
 *   FALSE otherwise.
 */
PRIVATE_EXTERN boolean_t
dhcpol_parse_vendor(dhcpol_t * vendor, dhcpol_t * options,
		    dhcpo_err_str_t * err)
{
    dhcpol_t		extra;
    boolean_t		ret = FALSE;
    int 		start = 0;

    if (err)
	err->str[0] = '\0';

    dhcpol_init(vendor);
    dhcpol_init(&extra);

    for (;;) {
	void *		data;
	int		len;

	data = dhcpol_find(options, dhcptag_vendor_specific_e, &len, &start);
	if (data == NULL) {
	    break; /* out of for */
	}

	if (dhcpol_parse_buffer(&extra, data, len, err) == FALSE) {
	    goto failed;
	}

	if (dhcpol_concat(vendor, &extra) == FALSE) {
	    if (err)
		snprintf(err->str, sizeof(err->str),
			 "dhcpol_concat() failed at %d\n", start);
	    goto failed;
	}
	dhcpol_free(&extra);
	ret = TRUE;
    }
    if (ret == FALSE) {
	if (err)
	    strlcpy(err->str, "missing vendor specific options",
		    sizeof(err->str));
    }
    return (ret);

 failed:
    dhcpol_free(vendor);
    dhcpol_free(&extra);
    return (FALSE);
}

PRIVATE_EXTERN void
dhcpol_print_cfstr(CFMutableStringRef str, dhcpol_t * list)
{
    int 		i;

    STRING_APPEND(str, "Options count is %d\n", dhcpol_count(list));
    for (i = 0; i < dhcpol_count(list); i++) {
	uint8_t * option = dhcpol_element(list, i);

	if (dhcptag_print_cfstr(str, option) == FALSE) {
	    STRING_APPEND(str, "undefined tag %d len %d\n", option[TAG_OFFSET], 
			  option[LEN_OFFSET]);
	}
    }
    return;
}

PRIVATE_EXTERN void
dhcpol_fprint(FILE * f, dhcpol_t * list)
{
    CFMutableStringRef	str;

    str = CFStringCreateMutable(NULL, 0);
    dhcpol_print_cfstr(str, list);
    my_CFStringPrint(f, str);
    CFRelease(str);
    return;
}

PRIVATE_EXTERN void
dhcpol_print(dhcpol_t * list)
{
    dhcpol_fprint(stdout, list);
    return;
}


PRIVATE_EXTERN int
dhcpol_count_params(dhcpol_t * options, const uint8_t * tags, int size)
{
    int		i;
    int		param_count = 0;

    for (i = 0; i < size; i++) {
	if (dhcpol_find(options, tags[i], NULL, NULL) != NULL)
	    param_count++;
    }
    return (param_count);
}


/*
 * Module: dhcpoa
 *
 * Purpose:
 *   Types and functions to create new dhcp option areas.
 */

/*
 * Function: dhcpoa_{init_common, init_no_end, init}
 *
 * Purpose:
 *   Initialize an option area structure so that it can be used
 *   in calling the dhcpoa_* routines.
 */

STATIC void
dhcpoa_init_common(dhcpoa_t * oa_p, void * buffer, int size, int reserve)
{
    bzero(buffer, size);	/* fill option area with pad tags */

    bzero(oa_p, sizeof(*oa_p));
    oa_p->oa_magic = DHCPOA_MAGIC;
    oa_p->oa_buffer = buffer;
    oa_p->oa_size = size;
    oa_p->oa_reserve = reserve;
}

PRIVATE_EXTERN void
dhcpoa_init_no_end(dhcpoa_t * oa_p, void * buffer, int size)
{
    dhcpoa_init_common(oa_p, buffer, size, 0);
    return;
}

PRIVATE_EXTERN int
dhcpoa_size(dhcpoa_t * oa_p)
{
    return (oa_p->oa_size);
}

PRIVATE_EXTERN void
dhcpoa_init(dhcpoa_t * oa_p, void * buffer, int size)
{
    /* initialize the area, reserve space for the end tag */
    dhcpoa_init_common(oa_p, buffer, size, 1);
    return;
}

PRIVATE_EXTERN dhcpoa_ret_t
dhcpoa_vendor_add(dhcpoa_t * oa_p, dhcpoa_t * vendor_oa_p,
		  dhcptag_t tag, int len, void * option)
{
    int			freespace;
    dhcpoa_ret_t	ret = dhcpoa_success_e;

    oa_p->oa_err.str[0] = '\0';

    if (len > (DHCP_OPTION_SIZE_MAX - OPTION_OFFSET)) {
	snprintf(vendor_oa_p->oa_err.str, sizeof(vendor_oa_p->oa_err.str),
		"tag %d option %d > %d", tag, len,
		DHCP_OPTION_SIZE_MAX - OPTION_OFFSET);
	return (dhcpoa_failed_e);
    }
    freespace = dhcpoa_freespace(vendor_oa_p) - OPTION_OFFSET;
    if (freespace < len) {
	/* add the vendor-specific options to the packet to make room */
	ret = dhcpoa_add(oa_p, dhcptag_vendor_specific_e,
			 dhcpoa_used(vendor_oa_p),
			 dhcpoa_buffer(vendor_oa_p));
	if (ret != dhcpoa_success_e) {
	    snprintf(vendor_oa_p->oa_err.str, sizeof(vendor_oa_p->oa_err.str),
		    "tag %d option %d > %d", tag, len, freespace);
	    goto failed;
	}
	freespace = dhcpoa_freespace(oa_p) - OPTION_OFFSET;
	if (freespace > dhcpoa_size(vendor_oa_p)) {
	    freespace = dhcpoa_size(vendor_oa_p);
	}
	dhcpoa_init_no_end(vendor_oa_p, dhcpoa_buffer(vendor_oa_p),
			   freespace);
    }
    ret = dhcpoa_add(vendor_oa_p, tag, len, option);

 failed:
    return (ret);
}


/*
 * Function: dhcpoa_add
 *
 * Purpose:
 *   Add an option to the option area.
 */
PRIVATE_EXTERN dhcpoa_ret_t
dhcpoa_add(dhcpoa_t * oa_p, dhcptag_t tag, int len, const void * option)
{

    oa_p->oa_err.str[0] = '\0';

    if (len > DHCP_OPTION_SIZE_MAX) {
	snprintf(oa_p->oa_err.str, sizeof(oa_p->oa_err.str), 
		 "tag %d option %d > %d", tag, len, DHCP_OPTION_SIZE_MAX);
	return (dhcpoa_failed_e);
    }

    if (oa_p->oa_magic != DHCPOA_MAGIC) {
	strlcpy(oa_p->oa_err.str, "dhcpoa_t not initialized - internal error!!!", 
		sizeof(oa_p->oa_err.str));
	return (dhcpoa_failed_e);
    }

    if (oa_p->oa_end_tag) {
	strlcpy(oa_p->oa_err.str, "attempt to add data after end tag",
		sizeof(oa_p->oa_err.str));
	return (dhcpoa_failed_e);
    }

    switch (tag) {
      case dhcptag_end_e:
	if ((oa_p->oa_offset + 1) > oa_p->oa_size) {
	    /* this can't happen since we're careful to leave space */
	    snprintf(oa_p->oa_err.str, sizeof(oa_p->oa_err.str),
		     "can't add end tag %d > %d",
		     oa_p->oa_offset + oa_p->oa_reserve, oa_p->oa_size);
	    return (dhcpoa_failed_e);
	}
	((uint8_t *)oa_p->oa_buffer)[oa_p->oa_offset + TAG_OFFSET] = tag;
	oa_p->oa_offset++;
	oa_p->oa_end_tag = TRUE;
	break;

      case dhcptag_pad_e:
	/* 1 for pad tag */
	if ((oa_p->oa_offset + oa_p->oa_reserve + 1) > oa_p->oa_size) {
	    snprintf(oa_p->oa_err.str, sizeof(oa_p->oa_err.str), 
		     "can't add pad tag %d > %d",
		     oa_p->oa_offset + oa_p->oa_reserve + 1, oa_p->oa_size);
	    return (dhcpoa_full_e);
	}
	((u_char *)oa_p->oa_buffer)[oa_p->oa_offset + TAG_OFFSET] = tag;
	oa_p->oa_offset++;
	break;

      default:
	/* 2 for tag/len */
	if ((oa_p->oa_offset + len + 2 + oa_p->oa_reserve) > oa_p->oa_size) {
	    snprintf(oa_p->oa_err.str, sizeof(oa_p->oa_err.str),
		     "can't add tag %d (%d > %d)", tag,
		     oa_p->oa_offset + len + 2 + oa_p->oa_reserve, 
		     oa_p->oa_size);
	    return (dhcpoa_full_e);
	}
	((uint8_t *)oa_p->oa_buffer)[oa_p->oa_offset + TAG_OFFSET] = tag;
	((uint8_t *)oa_p->oa_buffer)[oa_p->oa_offset + LEN_OFFSET] = len;
	if (len) {
	    bcopy(option, (u_char *)oa_p->oa_buffer 
		  + (OPTION_OFFSET + oa_p->oa_offset), len);
	}
	oa_p->oa_prev_last = oa_p->oa_last;
	oa_p->oa_last = oa_p->oa_offset;
	oa_p->oa_offset += len + OPTION_OFFSET;
	break;
    }
    oa_p->oa_option_count++;
    return (dhcpoa_success_e);
}

/*
 * Function: dhcpoa_add_from_strlist
 *
 * Purpose:
 *   Convert the string list into an option with the specified tag.
 * Note:
 *   This only works for type representations that we know about.
 */
PRIVATE_EXTERN dhcpoa_ret_t
dhcpoa_add_from_strlist(dhcpoa_t * oa_p, dhcptag_t tag, 
			const char * * strlist, int strlist_len)
{
    int 		len;
    char		tmp[DHCP_OPTION_SIZE_MAX];

    len = sizeof(tmp);

    if (dhcptag_from_strlist(strlist, strlist_len, tag, tmp, &len,
			     &oa_p->oa_err) == FALSE) {
	return (dhcpoa_failed_e);
    }
    return (dhcpoa_add(oa_p, tag, len, tmp));
}

/*
 * Function: dhcpoa_add_from_str
 *
 * Purpose:
 *   Convert the string into an option with the specified tag.
 */
PRIVATE_EXTERN dhcpoa_ret_t
dhcpoa_add_from_str(dhcpoa_t * oa_p, dhcptag_t tag, 
		    const char * str)
{
    return (dhcpoa_add_from_strlist(oa_p, tag, &str, 1));
}

/*
 * Function: dhcpoa_add_dhcpmsg
 *
 * Purpose:
 *   Add a dhcp message option to the option area.
 */
PRIVATE_EXTERN dhcpoa_ret_t
dhcpoa_add_dhcpmsg(dhcpoa_t * oa_p, dhcp_msgtype_t msgtype)
{
    char m = (char)msgtype;
    return (dhcpoa_add(oa_p, dhcptag_dhcp_message_type_e,
			sizeof(m), &m));
}

/*
 * Function: dhcpoa_err
 *
 * Purpose:
 *   Return an error message for the last error that occcurred.
 */
PRIVATE_EXTERN char *
dhcpoa_err(dhcpoa_t * oa_p)
{
    if (oa_p == NULL || oa_p->oa_magic != DHCPOA_MAGIC)
	return ("<bad parameter>");
    return (oa_p->oa_err.str);
}


PRIVATE_EXTERN int
dhcpoa_used(dhcpoa_t * oa_p)
{
    if (oa_p == NULL || oa_p->oa_magic != DHCPOA_MAGIC)
	return 0;
    return (oa_p->oa_offset);
}

PRIVATE_EXTERN int
dhcpoa_freespace(dhcpoa_t * oa_p)
{
    int	freespace;

    if (oa_p == NULL || oa_p->oa_magic != DHCPOA_MAGIC) {
	return 0;
    }
    freespace = oa_p->oa_size - oa_p->oa_offset - oa_p->oa_reserve;
    if (freespace < 0) {
	freespace = 0;
    }
    return (freespace);
}

PRIVATE_EXTERN int
dhcpoa_count(dhcpoa_t * oa_p)
{
    if (oa_p == NULL || oa_p->oa_magic != DHCPOA_MAGIC)
	return 0;
    return (oa_p->oa_option_count);
}

PRIVATE_EXTERN void *
dhcpoa_buffer(dhcpoa_t * oa_p) 
{
    if (oa_p == NULL || oa_p->oa_magic != DHCPOA_MAGIC)
      return (NULL);
    return (oa_p->oa_buffer);
}

#ifdef TEST_DHCP_OPTIONS
char test_empty[] = {
    99, 130, 83, 99,
    255,
};

char test_short[] = {
    99, 130, 83, 99,
    1,
};

char test_simple[] = {
    99, 130, 83, 99,
    1, 4, 255, 255, 252, 0,
    3, 4, 17, 202, 40, 1,
    255,
};

char test_vendor[] = {
    99, 130, 83, 99,
    1, 4, 255, 255, 252, 0,
    3, 4, 17, 202, 40, 1,
    43, 6, 1, 4, 1, 2, 3, 4,
    43, 6, 1, 4, 1, 2, 3, 4,
    255,
};
char test_overload_sname[] = {
    99, 130, 83, 99,
    dhcptag_option_overload_e, 1, DHCP_OVERLOAD_SNAME,
    255,
};
char test_overload_file[] = {
    99, 130, 83, 99,
    dhcptag_option_overload_e, 1, DHCP_OVERLOAD_FILE,
    255,
};
char test_overload_both[] = {
    99, 130, 83, 99,
    dhcptag_option_overload_e, 1, DHCP_OVERLOAD_BOTH,
    255,
};

char test_no_end[] = {
    0x63, 0x82, 0x53, 0x63, 0x35, 0x01, 0x05, 0x36, 
    0x04, 0xc0, 0xa8, 0x01, 0x01, 0x33, 0x04, 0x80,
    0x00, 0x80, 0x00, 0x01, 0x04, 0xff, 0xff, 0xff,
    0x00, 0x03, 0x04, 0xc0, 0xa8, 0x01, 0x01, 0x06,
    0x0c, 0x18, 0x1a, 0xa3, 0x21, 0x18, 0x1a, 0xa3,
    0x20, 0x18, 0x5e, 0xa3, 0x21, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

char test_no_magic[] = {
    0x1 
};
struct test {
    char * 		name;
    char *		data;
    int			len;
    boolean_t		result;
};

struct test tests[] = {
    { "empty", test_empty, sizeof(test_empty), TRUE },
    { "simple", test_simple, sizeof(test_simple), TRUE },
    { "vendor", test_vendor, sizeof(test_vendor), TRUE },
    { "no_end", test_no_end, sizeof(test_no_end), TRUE },
    { "no magic", test_no_magic, sizeof(test_no_magic), FALSE },
    { "short", test_short, sizeof(test_short), FALSE },
    { NULL, NULL, 0, FALSE },
};

struct test overload_tests[] = {
    { "overload sname", test_overload_sname, sizeof(test_overload_sname),TRUE },
    { "overload file", test_overload_file, sizeof(test_overload_file), TRUE },
    { "overload both", test_overload_both, sizeof(test_overload_both), TRUE },
    { NULL, NULL, 0, FALSE },
};

char test_string_253[253] = "test string 253 characters long (zero padded)";

STATIC char buf[2048];
STATIC char vend_buf[255];

int
main()
{
    int 		i;
    dhcpo_err_str_t	error;
    dhcpol_t 		options;
    struct dhcp * 	pkt = (struct dhcp *)buf;
    dhcpol_t		vendor_options;

    dhcpol_init(&options);
    dhcpol_init(&vendor_options);

    for (i = 0; tests[i].name; i++) {
	printf("\nTest %d: %s: ", i, tests[i].name);
	bcopy(tests[i].data, pkt->dp_options, tests[i].len);
	if (dhcpol_parse_packet(&options, pkt, 
				sizeof(*pkt) + tests[i].len,
				&error) != tests[i].result) {
	    printf("FAILED\n");
	    if (tests[i].result == TRUE) {
		printf("error: %s\n", error.str);
	    }
	}
	else {
	    printf("PASSED\n");
	    if (tests[i].result == FALSE) {
		printf("error: %s\n", error.str);
	    }
	}
	dhcpol_print(&options);
	dhcpol_free(&options);
    }
    bcopy(test_simple + RFC_MAGIC_SIZE, pkt->dp_sname,
	  sizeof(test_simple) - RFC_MAGIC_SIZE);
    bcopy(test_no_end + RFC_MAGIC_SIZE, pkt->dp_file, 
	  sizeof(test_no_end) - RFC_MAGIC_SIZE);
    for (i = 0; overload_tests[i].name; i++) {
	bcopy(overload_tests[i].data, pkt->dp_options,
	      overload_tests[i].len);
	printf("\nOption overload test %d: %s: ", i, overload_tests[i].name);
	if (dhcpol_parse_packet(&options, pkt, 
				sizeof(*pkt) + overload_tests[i].len,
				&error) != tests[i].result) {
	    printf("FAILED\n");
	    if (overload_tests[i].result == TRUE) {
		printf("error message returned was %s\n", error.str);
	    }
	}
	else {
	    printf("PASSED\n");
	    if (overload_tests[i].result == FALSE) {
		printf("error message returned was %s\n", error.str);
	    }
	}
	dhcpol_print(&options);
	dhcpol_free(&options);
    }

    printf("\nTesting dhcpoa\n");
    {
	struct in_addr	iaddr;
	dhcpoa_t	opts;
	dhcpoa_t	vend_opts;
	dhcpo_err_str_t err;
	char *		str;

	dhcpoa_init(&opts, buf, sizeof(buf));
	dhcpoa_init_no_end(&vend_opts, vend_buf, sizeof(vend_buf));
	
	iaddr.s_addr = inet_addr("255.255.252.0");
	if (dhcpoa_add(&opts, dhcptag_subnet_mask_e, sizeof(iaddr),
		       &iaddr) == dhcpoa_failed_e) {
	    printf("couldn't add subnet mask, %s\n", dhcpoa_err(&opts));
	    exit(1);
	}
	str = "17.202.40.1";
	if (dhcpoa_add_from_str(&opts, dhcptag_router_e, str) 
	    != dhcpoa_success_e) {
	    printf("couldn't add router, %s\n", dhcpoa_err(&opts));
	    exit(1);
	}
	if (dhcpoa_add_from_str(&opts, dhcptag_host_name_e, "siegdi5") 
	    != dhcpoa_success_e) {
	    printf("couldn't add hostname tag, %s\n", dhcpoa_err(&opts));
	    exit(1);
	}
	if (dhcpoa_add_from_str(&opts, dhcptag_domain_name_e, "apple.com") 
	    != dhcpoa_success_e) {
	    printf("couldn't add domain name tag, %s\n", dhcpoa_err(&opts));
	    exit(1);
	}
	{
	    const char * 	domain_names[] = {
		"euro.apple.com", 
		"eng.apple.com",
		"foo.bar",
		"thisisprettylongdontyouthink.foo.bar",
		"thisisprettylongdontyouthink.foo.bar.apple.com",
		"thisisprettylongdontyouthink.foo.bar.com",
		"a.foo.bar"
	    };
	    int			domain_names_count = sizeof(domain_names) / sizeof(domain_names[0]);
	    uint8_t		search_buf[DHCP_OPTION_SIZE_MAX];
	    int			len = sizeof(search_buf);

	    if (dhcptag_from_strlist(domain_names, domain_names_count,
				     dhcptag_domain_search_e, search_buf,
				     &len, &err) == FALSE) {
		printf("couldn't get domain search option: %s", err.str);
	    }
	    else if (dhcpoa_add(&opts, dhcptag_domain_search_e,
				len, search_buf) != dhcpoa_success_e) {
		printf("couldn't add domain search tag, %s\n",
		       dhcpoa_err(&opts));
		exit(1);
	    }
	}

	{
	    int			len;
	    const char * 	route_list[] = {
		"0.0.0.0/0",
		"17.193.12.1",
		"10.123.123.0/24",
		"17.193.12.192",
	    };
	    int			route_list_count = sizeof(route_list) / sizeof(route_list[0]);
	    uint8_t		routes_buf[DHCP_OPTION_SIZE_MAX];

	    len = sizeof(routes_buf);
	    if (dhcptag_from_strlist(route_list, route_list_count,
				     dhcptag_classless_static_route_e,
				     routes_buf, &len, &err) == FALSE) {
		fprintf(stderr, 
			"couldn't get classless route option: %s", err.str);
	    }
	    else {
		if (dhcpoa_add(&opts, dhcptag_classless_static_route_e,
			       len, routes_buf) != dhcpoa_success_e) {
		    fprintf(stderr, "couldn't add classless routes tag, %s\n",
			    dhcpoa_err(&opts));
		    exit(1);
		}
	    }
	}
	for (i = 0; i < 253; i++) {
	    if (dhcpoa_vendor_add(&opts, &vend_opts, i+1, i,
				  test_string_253) != dhcpoa_success_e) {
		printf("couldn't add vendor option, %s\n", 
		       dhcpoa_err(&vend_opts));
		break;
	    }
	}
	if (dhcpoa_used(&vend_opts) > 0) {
	    if (dhcpoa_add(&opts, dhcptag_vendor_specific_e,
			   dhcpoa_used(&vend_opts), dhcpoa_buffer(&vend_opts))
		!= dhcpoa_success_e) {
		printf("couldn't add vendor options, %s\n", dhcpoa_err(&opts));
		exit(1);
	    }
	}
	if (dhcpoa_add(&opts, dhcptag_end_e, 0, 0) != dhcpoa_success_e) {
	    printf("couldn't add end tag, %s\n", dhcpoa_err(&opts));
	    exit(1);
	}
	if (dhcpol_parse_buffer(&options, buf, sizeof(buf), &err) == FALSE) {
	    printf("parse buffer failed, %s\n", err.str);
	    exit(1);
	}
	dhcpol_print(&options);
	{
	    struct in_addr * iaddr;
	    iaddr = (struct in_addr *)
		dhcpol_find(&options, dhcptag_router_e, 0, 0);
	    if (iaddr == NULL) {
		printf("can't find router option\n");
	    }
	    else {
		printf("Found router option %s\n", inet_ntoa(*iaddr));
	    }
	}
	if (dhcpol_parse_vendor(&vendor_options, &options, NULL)) {
	    printf("vendor parsed ok\n");
	}
	else {
	    printf("parse vendor failed\n");
	}
	dhcpol_free(&options);
	dhcpol_free(&vendor_options);
    }
    exit(0);
}
#endif /* TEST_DHCP_OPTIONS */
