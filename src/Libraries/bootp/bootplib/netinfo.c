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
 * netinfo.c - Routines for dealing with the NetInfo database.
 *
 **********************************************************************
 * HISTORY
 * 10-Jun-89  Peter King
 *	Created.
 * 23-Feb-98  Dieter Siegmund (dieter@apple.com)
 *      Removed all of the promiscous-related stuff,
 *	left with routines to do host creation/lookup.
 * 11-Sep-06  Dieter Siegmund (dieter@apple.com)
 *	Imported netinfo code from Libinfo
 **********************************************************************
 */

/*
 * Include Files
 */
#include <ctype.h>
#include <pwd.h>
#include <netdb.h>
#include <string.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include "host_identifier.h"
#include "netinfo.h"
#include "symbol_scope.h"

/*
 * Imported from Libinfo START ---v
 */
#define  mm_used() mstats()

#define MM_ALLOC(obj) obj = ((void *)malloc(sizeof(*(obj))))

#define MM_FREE(obj)  free((void *)(obj))

#define MM_ZERO(obj)  bzero((void *)(obj), sizeof(*(obj)))

#define MM_BCOPY(b1, b2, size) bcopy((void *)(b1), (void *)(b2), \
				     (unsigned)(size))

#define MM_BEQ(b1, b2, size) (bcmp((void *)(b1), (void *)(b2), \
				   (unsigned)(size)) == 0)

#define MM_ALLOC_ARRAY(obj, len)  \
	obj = ((void *)malloc(sizeof(*(obj)) * (len)))

#define MM_ZERO_ARRAY(obj, len) bzero((void *)(obj), sizeof(*obj) * len)

#define MM_FREE_ARRAY(obj, len) free((void *)(obj))

#define MM_GROW_ARRAY(obj, len) \
	((obj == NULL) ? (MM_ALLOC_ARRAY((obj), (len) + 1)) : \
	 (obj = (void *)realloc((void *)(obj), \
				sizeof(*(obj)) * ((len) + 1))))

#define MM_SHRINK_ARRAY(obj, len) \
	obj = (void *)realloc((void *)(obj), \
			      sizeof(*(obj)) * ((len) - 1))

PRIVATE_EXTERN void
ni_proplist_insert(
		ni_proplist *pl,
		const ni_property prop,
		ni_index where
		)
{
	ni_index i;
	
	MM_GROW_ARRAY(pl->nipl_val, pl->nipl_len);
	for (i = pl->nipl_len; i > where; i--) {
		pl->nipl_val[i] = pl->nipl_val[i - 1];
	}
	pl->nipl_val[i] = ni_prop_dup(prop);
	pl->nipl_len++;
}

PRIVATE_EXTERN void
ni_proplist_delete(
		ni_proplist *pl,
		ni_index which
		)
{
	int i;

	ni_prop_free(&pl->nipl_val[which]);
	for (i = (int)which + 1; i < pl->nipl_len; i++) {
		pl->nipl_val[i - 1] = pl->nipl_val[i];
	}
	MM_SHRINK_ARRAY(pl->nipl_val, pl->nipl_len--);
}

PRIVATE_EXTERN void
ni_proplist_free(
		 ni_proplist *pl
		 )
{
	ni_index i;

	if (pl->nipl_val == NULL) {
		return;
	}
	for (i = 0; i < pl->nipl_len; i++) {
		ni_prop_free(&pl->nipl_val[i]);
	}
	MM_FREE_ARRAY(pl->nipl_val, pl->nipl_len);
	NI_INIT(pl);
}

PRIVATE_EXTERN ni_proplist
ni_proplist_dup(
	     const ni_proplist pl
	     )
{
	ni_proplist newlist;
	ni_index i;

	newlist.nipl_len = pl.nipl_len;
	MM_ALLOC_ARRAY(newlist.nipl_val, pl.nipl_len);
	for (i = 0; i < pl.nipl_len; i++) {
		newlist.nipl_val[i].nip_name = ni_name_dup(pl.nipl_val[i].nip_name);
		newlist.nipl_val[i].nip_val = ni_namelist_dup(pl.nipl_val[i].nip_val);
	}
	return (newlist);
}

PRIVATE_EXTERN ni_index
ni_proplist_match(
	       const ni_proplist pl,
	       ni_name_const pname,
	       ni_name_const pval
	   )
{
	ni_index i;
	ni_index j;
	ni_namelist nl;

	for (i = 0; i < pl.nipl_len; i++) {
		if (ni_name_match(pname, pl.nipl_val[i].nip_name)) {
			if (pval == NULL) {
				return (i);
			}
			nl = pl.nipl_val[i].nip_val;
			for (j = 0; j < nl.ninl_len; j++) {
				if (ni_name_match(pval, nl.ninl_val[j])) {
					return (i);
				}
			}
			break;
		}
	}
	return (NI_INDEX_NULL);
}


PRIVATE_EXTERN ni_property
ni_prop_dup(
	 const ni_property prop
	 )
{
	ni_property newprop;

	newprop.nip_name = ni_name_dup(prop.nip_name);
	newprop.nip_val = ni_namelist_dup(prop.nip_val);
	return (newprop);
}

PRIVATE_EXTERN void
ni_prop_free(
	     ni_property *prop
	     )
{
	ni_name_free(&prop->nip_name);
	ni_namelist_free(&prop->nip_val);
}

PRIVATE_EXTERN int
ni_name_match(
	   ni_name_const nm1,
	   ni_name_const nm2
	   )
{
	return (strcmp(nm1, nm2) == 0);
}

PRIVATE_EXTERN ni_name
ni_name_dup(
	 ni_name_const nm
	 )
{
	return (strdup(nm));
}


PRIVATE_EXTERN void
ni_name_free(
	     ni_name *nm
	     )
{
	if (*nm != NULL) {
		free(*nm);
		*nm = NULL;
	}
}

PRIVATE_EXTERN ni_namelist
ni_namelist_dup(
	     const ni_namelist nl
	     )
{
	ni_namelist newlist;
	ni_index i;

	newlist.ninl_len = nl.ninl_len;
	MM_ALLOC_ARRAY(newlist.ninl_val, newlist.ninl_len);
	for (i = 0; i < nl.ninl_len; i++) {
		newlist.ninl_val[i] = ni_name_dup(nl.ninl_val[i]);
	}
	return (newlist);
}

PRIVATE_EXTERN void
ni_namelist_free(
	      ni_namelist *nl
	      )
{
	ni_index i;

	if (nl->ninl_val == NULL) {
		return;
	}
	for (i = 0; i < nl->ninl_len; i++) {
		ni_name_free(&nl->ninl_val[i]);
	}
	MM_FREE_ARRAY(nl->ninl_val, nl->ninl_len);
	NI_INIT(nl);
}

PRIVATE_EXTERN void
ni_namelist_insert(
		ni_namelist *nl,
		ni_name_const nm,
		ni_index where
		)
{
	ni_index i;

	MM_GROW_ARRAY(nl->ninl_val, nl->ninl_len);
	for (i = nl->ninl_len; i > where; i--) {
		nl->ninl_val[i] = nl->ninl_val[i - 1];
	}
	nl->ninl_val[i] = ni_name_dup(nm);
	nl->ninl_len++;
}

PRIVATE_EXTERN void
ni_namelist_delete(
		ni_namelist *nl,
		ni_index which
		)
{
	int i;

	ni_name_free(&nl->ninl_val[which]);
	for (i = (int)which + 1; i < nl-> ninl_len; i++) {
		nl->ninl_val[i - 1] = nl->ninl_val[i];
	}
	MM_SHRINK_ARRAY(nl->ninl_val, nl->ninl_len--);
}

PRIVATE_EXTERN ni_index
ni_namelist_match(
	       const ni_namelist nl,
	       ni_name_const nm
	       )
{
	ni_index i;
	
	for (i = 0; i < nl.ninl_len; i++) {
		if (ni_name_match(nl.ninl_val[i], nm)) {
			return (i);
		}
	}
	return (NI_INDEX_NULL);
}

/*
 * Imported from Libinfo END ---^
 */

/*
 * Exported routines
 */

PRIVATE_EXTERN void
ni_proplist_dump(ni_proplist * pl)
{
    int i, j;

    for (i = 0; i < pl->nipl_len; i++) {
	ni_property * prop = &(pl->nipl_val[i]);
	ni_namelist * nl_p = &prop->nip_val;
	if (nl_p->ninl_len == 0) {
	    printf("\"%s\"\n", prop->nip_name);
	}
	else {
	    printf("\"%s\" = ", prop->nip_name);
	    for (j = 0; j < nl_p->ninl_len; j++)
		printf("%s\"%s\"", (j == 0) ? "" : ", ", nl_p->ninl_val[j]);
	    printf("\n");
	}
    }
}

PRIVATE_EXTERN void
ni_set_prop(ni_proplist * pl_p, ni_name prop, ni_name value, 
	    boolean_t * modified)
{
    ni_index		where;
    
    where = ni_proplist_match(*pl_p, prop, NULL);
    if (where != NI_INDEX_NULL) {
	if (value != NULL && where == ni_proplist_match(*pl_p, prop, value)) {
	    return; /* already set */
	}
	ni_proplist_delete(pl_p, where);
    }
    ni_proplist_insertprop(pl_p, prop, value, where);
    if (modified)
	*modified = TRUE;
    return;
}

PRIVATE_EXTERN void
ni_delete_prop(ni_proplist * pl_p, ni_name prop, boolean_t * modified)
{
    int where;

    where = (int)ni_proplist_match(*pl_p, prop, NULL);
    if (where != NI_INDEX_NULL) {
	ni_proplist_delete(pl_p, where);
	if (modified)
	    *modified = TRUE;
    }
    return;
}

