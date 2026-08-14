
#ifndef _S_NETINFO_H
#define _S_NETINFO_H
/*
 * Copyright (c) 1999-2014 Apple Inc. All rights reserved.
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
 * netinfo.h - Header file for netinfo routines.
 */
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <mach/boolean.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/in.h>

/*
 * Constants
 */

/*
 * Constant: DHCPD_LEASES_NOTIFICATION_KEY
 * Purpose:
 *   notify(3) notification key when the lease list changes.
 */
#define DHCPD_LEASES_NOTIFICATION_KEY	"com.apple.bootpd.DHCPLeaseList"

/*
 * Constant: DHCPD_DISABLED_INTERFACES_NOTIFICATION_KEY
 * Purpose:
 *   notify(3) notification key when the list of disabled interfaces changes.
 */
#define DHCPD_DISABLED_INTERFACES_NOTIFICATION_KEY	\
    "com.apple.bootpd.DHCPDisabledInterfaces"

/*
 * Key: DHCPD_DYNAMIC_STORE_KEY
 * Purpose:
 *   Key used in dynamic store to hold DHCP-related information.
 */
#define DHCPD_DYNAMIC_STORE_KEY		"com.apple.bootpd.DHCPServer"

/*
 * Key: DHCPD_DISABLED_INTERFACES
 * Purpose:
 *   Key used to indicate which interfaces have been disabled.
 */
#define DHCPD_DISABLED_INTERFACES	"DisabledInterfaces"


/* Important properties */
#define	NIPROP_NAME		"name"

#define NIPROP_UID		"uid"
#define NIPROP_GID		"gid"
#define NIPROP_PASSWD		"passwd"
#define NIPROP_SHELL		"shell"
#define NIPROP_REALNAME		"realname"

#define	NIPROP_BOOTFILE		"bootfile"
#define NIPROP_DHCP_RELEASED	"released"
#define NIPROP_DHCP_DECLINED	"declined"
#define NIPROP_DHCP_LEASE	"lease"
#define	NIPROP_ENADDR		"en_address"
#define	NIPROP_HWADDR		"hw_address"
#define NIPROP_IDENTIFIER	"identifier"
#define	NIPROP_IPADDR		"ip_address"
#define NIPROP_NETBOOT_AFP_USER	"afp_user"
#define NIPROP_NETBOOT_ARCH	"arch"
#define NIPROP_NETBOOT_IMAGE_ID	"image_id"
#define NIPROP_NETBOOT_IMAGE_INDEX "image_index"
#define NIPROP_NETBOOT_IMAGE_KIND "image_kind"
#define NIPROP_NETBOOT_IMAGE_IS_INSTALL "image_is_install"
#define NIPROP_NETBOOT_NUMBER	"number"
#define NIPROP_NETBOOT_SYSID	"sysid"
#define NIPROP_NETBOOT_LAST_BOOT_TIME	"last_boot_time"
#define NIPROP_NETBOOT_BOUND	"bound"
#define NIPROP_SERVES		"serves"
#define NIPROP__CREATOR		"_creator"

#define NI_INDEX_NULL ((ni_index)-1)
#define NI_INIT(objp) bzero((void *)(objp), sizeof(*(objp)))

typedef char *ni_name;
typedef const char *ni_name_const;

typedef struct {
	u_int ni_namelist_len;
	ni_name *ni_namelist_val;
} ni_namelist;

typedef unsigned long ni_index;

typedef struct ni_property {
	ni_name nip_name;
	ni_namelist nip_val;
} ni_property;

typedef struct {
	u_int ni_proplist_len;
	ni_property *ni_proplist_val;
} ni_proplist;

enum ni_status {
	NI_OK = 0,
	NI_BADID = 1,
	NI_STALE = 2,
	NI_NOSPACE = 3,
	NI_PERM = 4,
	NI_NODIR = 5,
	NI_NOPROP = 6,
	NI_NONAME = 7,
	NI_NOTEMPTY = 8,
	NI_UNRELATED = 9,
	NI_SERIAL = 10,
	NI_NETROOT = 11,
	NI_NORESPONSE = 12,
	NI_RDONLY = 13,
	NI_SYSTEMERR = 14,
	NI_ALIVE = 15,
	NI_NOTMASTER = 16,
	NI_CANTFINDADDRESS = 17,
	NI_DUPTAG = 18,
	NI_NOTAG = 19,
	NI_AUTHERROR = 20,
	NI_NOUSER = 21,
	NI_MASTERBUSY = 22,
	NI_INVALIDDOMAIN = 23,
	NI_BADOP = 24,
	NI_FAILED = 9999,
};
typedef enum ni_status ni_status;

/*
 * Define some shortcuts
 */
#define ninl_len ni_namelist_len
#define ninl_val ni_namelist_val

#define nipl_len ni_proplist_len
#define nipl_val ni_proplist_val


/*
 * Prototypes
 */
ni_name ni_name_dup(ni_name_const);
void ni_name_free(ni_name *);
int ni_name_match(ni_name_const, ni_name_const);

ni_namelist ni_namelist_dup(const ni_namelist);
void ni_namelist_free(ni_namelist *);
void ni_namelist_insert(ni_namelist *, ni_name_const, ni_index);
void ni_namelist_delete(ni_namelist *, ni_index);
ni_index ni_namelist_match(const ni_namelist, ni_name_const);

ni_property ni_prop_dup(const ni_property);
void ni_prop_free(ni_property *);

void ni_proplist_insert(ni_proplist *, const ni_property, ni_index);
void ni_proplist_delete(ni_proplist *, ni_index);
ni_index ni_proplist_match(const ni_proplist, ni_name_const, ni_name_const);
ni_proplist ni_proplist_dup(const ni_proplist);
void ni_proplist_free(ni_proplist *);

void		ni_proplist_dump(ni_proplist * pl);

boolean_t	ni_get_checksum(void * h, unsigned long * checksum);

void		ni_set_prop(ni_proplist * pl_p, ni_name prop, ni_name value, 
			    boolean_t * modified);
void		ni_delete_prop(ni_proplist * pl_p, ni_name prop, 
			       boolean_t * modified);
/*
 * Function: ni_proplist_insertprop
 * Purpose:
 *	Add a property with a given value to a property list at the
 *	specified index.
 */
static __inline__ void
ni_proplist_insertprop(
		       ni_proplist	*proplist,
		       ni_name	key,
		       ni_name	value,
		       ni_index	where
		       )
{
	ni_property	prop;

	NI_INIT(&prop);
	prop.nip_name = key;
	if (value) {
		ni_namelist_insert(&prop.nip_val, value, 0);
	}
	ni_proplist_insert(proplist, prop, where);
	ni_namelist_free(&prop.nip_val);
}

/*
 * Function: ni_proplist_addprop
 * Purpose:
 *	Add a property with a given value to a property list.
 */
static __inline__ void
ni_proplist_addprop(
		    ni_proplist	*proplist,
		    ni_name	key,
		    ni_name	value
		    )
{
    	ni_proplist_insertprop(proplist, key, value, proplist->nipl_len);
	return;
}

/*
 * Function: ni_proplist_addprops
 *
 * Purpose:
 *	Add a property with the given values to a property list.
 */
static __inline__ void
ni_proplist_addprops(
		    ni_proplist	*proplist,
		    ni_name	key,
		    ni_name *	values,
		    int		count
		    )
{
	ni_property	prop;
	int		i;

	NI_INIT(&prop);
	prop.nip_name = key;
	for (i = count - 1; i >= 0; i--) {
	    ni_namelist_insert(&prop.nip_val, values[i], 0);
	}
	ni_proplist_insert(proplist, prop, proplist->nipl_len);
	ni_namelist_free(&prop.nip_val);
}

static __inline__ ni_namelist *
ni_nlforprop(ni_proplist * pl, ni_name name)
{
    int i;

    for (i = 0; i < pl->nipl_len; i++) {
	ni_property * p = &(pl->nipl_val[i]);
	if (strcmp(name, p->nip_name) == 0) {
	    return (&p->nip_val);
	}
    }
    return (NULL);
}

static __inline__ ni_name
ni_valforprop(ni_proplist * pl, ni_name name)
{
    ni_namelist * nl_p = ni_nlforprop(pl, name);
    if (nl_p == NULL || nl_p->ninl_len == 0)
	return (NULL);
    return (nl_p->ninl_val[0]);
}

static __inline__ void
ni_proplist_append(ni_proplist * proplist, ni_proplist * new_pl)
{
    int i;

    for (i = 0; i < new_pl->nipl_len; i++) {
	ni_proplist_insert(proplist, new_pl->nipl_val[i], proplist->nipl_len);
    }
    return;
}

static __inline__ int
ni_nlvalindex(ni_namelist * nl_p, ni_name value)
{
    int i;
    if (nl_p) {
	for (i = 0; i < nl_p->ninl_len; i++) {
	    if (strcmp(nl_p->ninl_val[i], value) == 0)
		return (i);
	}
    }
    return (-1);
}

static __inline__ int
ni_indexforprop(ni_proplist * pl, ni_name name, ni_name value)
{
    ni_namelist * nl_p = ni_nlforprop(pl, name);
    if (nl_p == NULL)
	return (-1);
    return (ni_nlvalindex(nl_p, value));

}

#endif /* _S_NETINFO_H */
