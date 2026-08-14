/*
 * Copyright (c) 2000-2018 Apple Inc. All rights reserved.
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
 * NICache.c
 * - netinfo host cache routines
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/types.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <netinet/bootp.h>
#include <net/ethernet.h>
#include <netinet/if_ether.h>
#include <net/if_arp.h>
#include <mach/boolean.h>
#include <errno.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <string.h>
#include <syslog.h>
#include "dprintf.h"
#include "NICache.h"
#include "NICachePrivate.h"
#include "util.h"
#include "netinfo.h"
#include "symbol_scope.h"

#ifdef NICACHE_TEST
#define TIMESTAMPS
#endif /* NICACHE_TEST */
#ifdef READ_TEST
#define TIMESTAMPS
#endif /* READ_TEST */

#ifdef TIMESTAMPS
STATIC void
timestamp_printf(char * msg)
{
    STATIC struct timeval	tvp = {0,0};
    struct timeval		tv;

    gettimeofday(&tv, 0);
    if (tvp.tv_sec) {
	struct timeval result;
	
	timeval_subtract(tv, tvp, &result);
	printf("%d.%06d (%d.%06d): %s\n", 
	       (int)tv.tv_sec, (int)tv.tv_usec, 
	       (int)result.tv_sec, (int)result.tv_usec, msg);
    }
    else 
	printf("%d.%06d (%d.%06d): %s\n", 
	       (int)tv.tv_sec, (int)tv.tv_usec, 0, 0, msg);
    tvp = tv;
}
STATIC __inline__ void
S_timestamp(char * msg)
{
    timestamp_printf(msg);
}
#endif /* TIMESTAMPS */

/**
 ** Module: PLCacheEntry
 **/

PRIVATE_EXTERN PLCacheEntry_t *
PLCacheEntry_create(ni_proplist pl)
{
    PLCacheEntry_t * entry = malloc(sizeof(*entry));

    if (entry == NULL)
	return (NULL);
    entry->pl = ni_proplist_dup(pl);
    entry->next = entry->prev = NULL;
    return (entry);
}

PRIVATE_EXTERN void
PLCacheEntry_free(PLCacheEntry_t * ent)
{
    ni_proplist_free(&ent->pl);
    bzero(ent, sizeof(*ent));
    free(ent);
    return;
}

/**
 ** Module: PLCache
 **/

PRIVATE_EXTERN void
PLCache_print(PLCache_t * cache)
{
    int			i;
    PLCacheEntry_t *	scan;

    printf("PLCache contains %d elements\n", cache->count);
    for (i = 0, scan = cache->head; scan; scan = scan->next, i++) {
	printf("\nEntry %d\n", i);
	ni_proplist_dump(&scan->pl);
    }
}

PRIVATE_EXTERN void
PLCache_init(PLCache_t * cache)
{
    bzero(cache, sizeof(*cache));
    cache->max_entries = CACHE_MAX;
    return;
}

PRIVATE_EXTERN int
PLCache_count(PLCache_t * c)
{
    return (c->count);
}

PRIVATE_EXTERN void
PLCache_set_max(PLCache_t * c, int m)
{
    if (m < CACHE_MIN)
	m = CACHE_MIN;
    c->max_entries = m;
    if (c->count > c->max_entries) {
	int 			i;
	int 			num = c->count - c->max_entries;
	PLCacheEntry_t * 	prev;
	PLCacheEntry_t * 	scan;

	dprintf(("Count %d max %d, removing %d\n", c->count, c->max_entries,
		 num));

	/* drop num items from the cache */
	prev = NULL;
	scan = c->tail;
	for (i = 0; i < num; i++) {
	    dprintf(("Deleting %d\n", i));
	    prev = scan->prev;
	    PLCacheEntry_free(scan);
	    scan = prev;
	}
	c->tail = prev;
	if (c->tail) {
	    c->tail->next = NULL;
	}
	else {
	    c->head = NULL;
	}
	c->count = c->max_entries;
    }
    return;
}

PRIVATE_EXTERN void
PLCache_free(PLCache_t * cache)
{
    PLCacheEntry_t * scan;

    for (scan = cache->head; scan; ) {
	PLCacheEntry_t * next;

	next = scan->next;
	PLCacheEntry_free(scan);
	scan = next;
	dprintf(("deleting %d\n", ++i));
    }
    bzero(cache, sizeof(*cache));
    return;
}

PRIVATE_EXTERN void
PLCache_add(PLCache_t * cache, PLCacheEntry_t * entry)
{
    if (entry == NULL)
	return;

    entry->next = cache->head;
    entry->prev = NULL;
    if (cache->head == NULL) {
	cache->head = cache->tail = entry;
    }
    else {
	cache->head->prev = entry;
	cache->head = entry;
    }
    cache->count++;
    return;
}

PRIVATE_EXTERN void
PLCache_append(PLCache_t * cache, PLCacheEntry_t * entry)
{
    if (entry == NULL)
	return;
    
    entry->next = NULL;
    entry->prev = cache->tail;
    if (cache->head == NULL) {
	cache->head = cache->tail = entry;
    }
    else {
	cache->tail->next = entry;
	cache->tail = entry;
    }
    cache->count++;
    return;
}

/*
 * Function: my_fgets
 * Purpose:
 *   like fgets() but consumes/discards characters until the next newline 
 *   once the line buffer is full.
 */
STATIC char *
my_fgets(char * buf, int buf_size, FILE * f)
{
    boolean_t	done = FALSE;
    int		left = buf_size - 1;
    char *	scan;

    scan = buf;
    while (!done) {
	int		this_char;

	this_char = fgetc(f);
	switch (this_char) {
	case 0:
	case EOF:
	    done = TRUE;
	    break;
	default:
	    if (left > 0) {
		*scan++ = (char)this_char;
		left--;
	    }
	    if (this_char == '\n') {
		done = TRUE;
	    }
	    break;
	}
    }
    if (scan == buf) {
	/* we didn't read anything */
	return (NULL);
    }
    *scan = '\0';
    return (buf);
}

PRIVATE_EXTERN boolean_t
PLCache_read(PLCache_t * cache, const char * filename)
{
    FILE *	file = NULL;
    int		line_number = 0;
    char	line[1024];
    ni_proplist	pl;
    enum { 
	nowhere_e,
	start_e, 
	body_e, 
	end_e 
    }		where = nowhere_e;

    NI_INIT(&pl);
    file = fopen(filename, "r");
    if (file == NULL) {
	perror(filename);
	goto failed;
    }

    while (1) {
	if (my_fgets(line, sizeof(line), file) != line) {
	    if (where == start_e || where == body_e) {
		fprintf(stderr, "file ends prematurely\n");
	    }
	    break;
	}
	line_number++;
	if (strcmp(line, "{\n") == 0) {
	    if (where != end_e && where != nowhere_e) {
		fprintf(stderr, "unexpected '{' at line %d\n", 
			line_number);
		goto failed;
	    }
	    where = start_e;
	}
	else if (strcmp(line, "}\n") == 0) {
	    if (where != start_e && where != body_e) {
		fprintf(stderr, "unexpected '}' at line %d\n", 
			line_number);
		goto failed;
	    }
	    if (pl.nipl_len > 0) {
		PLCache_append(cache, PLCacheEntry_create(pl));
		ni_proplist_free(&pl);
	    }
	    where = end_e;
	}
	else {
	    char	propname[128];
	    char	propval[768] = "";
	    int 	len = (int)strlen(line);
	    char *	sep = strchr(line, '=');
	    int 	whitespace_len = (int)strspn(line, " \t\n");

	    if (whitespace_len == len) {
		continue;
	    }
	    if (sep) {
		int nlen = (int)(sep - line) - whitespace_len;
		int vlen = len - whitespace_len - nlen - 2;

		if (nlen >= sizeof(propname)) {
		    fprintf(stderr,
			    "property name truncated to %d bytes at line %d\n",
			    (int)sizeof(propname) - 1,
			    line_number);
		    nlen = sizeof(propname) - 1;
		}
		if (vlen >= sizeof(propval)) {
		    fprintf(stderr, 
			    "value truncated to %d bytes at line %d\n",
			    (int)sizeof(propval) - 1,
			    line_number);
		    vlen = sizeof(propval) - 1;
		}
		strncpy(propname, line + whitespace_len, nlen);
		propname[nlen] = '\0';
		strncpy(propval, sep + 1, vlen);
		propval[vlen] = '\0';
		ni_proplist_insertprop(&pl, propname, propval, NI_INDEX_NULL);
	    }
	    else {
		int nlen = len - whitespace_len - 1;

		if (nlen >= sizeof(propname)) {
		    fprintf(stderr,
			    "property name truncated to %d bytes at line %d\n",
			    (int)sizeof(propname) - 1,
			    line_number);
		    nlen = sizeof(propname) - 1;
		}
		strncpy(propname, line + whitespace_len, nlen);
		propname[nlen] = '\0';
		ni_proplist_insertprop(&pl, propname, NULL, NI_INDEX_NULL);
	    }
	    where = body_e;
	}
    }

 failed:
    if (file)
	fclose(file);
    ni_proplist_free(&pl);
    return (TRUE);
}

PRIVATE_EXTERN boolean_t
PLCache_write(PLCache_t * cache, const char * filename)
{
    FILE *		file = NULL;
    PLCacheEntry_t *	scan;
    char		tmp_filename[256];

    snprintf(tmp_filename, sizeof(tmp_filename), "%s-", filename);
    file = fopen(tmp_filename, "w");
    if (file == NULL) {
	perror(tmp_filename);
	return (FALSE);
    }

    for (scan = cache->head; scan; scan = scan->next) {
	int i;

	fprintf(file, "{\n");
	for (i = 0; i < scan->pl.nipl_len; i++) {
	    ni_property * prop = &(scan->pl.nipl_val[i]);
	    ni_namelist * nl_p = &prop->nip_val;
	    if (nl_p->ninl_len == 0) {
		fprintf(file, "\t%s\n", prop->nip_name);
	    }
	    else {
		fprintf(file, "\t%s=%s\n", prop->nip_name,
			nl_p->ninl_val[0]);
	    }
	}
	fprintf(file, "}\n");
    }
    fclose(file);
    rename(tmp_filename, filename);
    return (TRUE);
}

PRIVATE_EXTERN void
PLCache_remove(PLCache_t * cache, PLCacheEntry_t * entry)
{
    if (entry == NULL)
	return;

    if (entry->prev)
	entry->prev->next = entry->next;
    if (entry->next)
	entry->next->prev = entry->prev;
    if (entry == cache->head) {
	cache->head = cache->head->next;
    }
    if (entry == cache->tail) {
	cache->tail = cache->tail->prev;
    }
    entry->next = entry->prev = NULL;
    cache->count--;
    return;
}

PRIVATE_EXTERN void
PLCache_make_head(PLCache_t * cache, PLCacheEntry_t * entry)
{
    if (entry == cache->head)
	return; /* already the head */

    PLCache_remove(cache, entry);
    PLCache_add(cache, entry);
}

PRIVATE_EXTERN PLCacheEntry_t *
PLCache_lookup_prop(PLCache_t * PLCache, char * prop, char * value, boolean_t make_head)
{
    PLCacheEntry_t * scan;

    for (scan = PLCache->head; scan; scan = scan->next) {
	int		name_index;

	name_index = (int)ni_proplist_match(scan->pl, prop, value);
	if (name_index != NI_INDEX_NULL) {
	    if (make_head) {
		PLCache_make_head(PLCache, scan);
	    }
	    return (scan);
	}
    }
    return (NULL);
}

PRIVATE_EXTERN PLCacheEntry_t *
PLCache_lookup_hw(PLCache_t * PLCache, 
		  uint8_t hwtype, void * hwaddr, int hwlen,
		  NICacheFunc_t * func, void * arg,
		  struct in_addr * client_ip,
		  boolean_t * has_binding)
{
    struct ether_addr *	en_search = (struct ether_addr *)hwaddr;
    PLCacheEntry_t *	scan;

    if (has_binding)
	*has_binding = FALSE;
    for (scan = PLCache->head; scan; scan = scan->next) {
	ni_namelist *	en_nl_p;
	int 		n;
	int		en_index;
	int		ip_index;
	ni_namelist *	ip_nl_p;

	en_index = (int)ni_proplist_match(scan->pl, NIPROP_ENADDR, NULL);
	if (en_index == NI_INDEX_NULL)
	    continue;
	ip_index = (int)ni_proplist_match(scan->pl, NIPROP_IPADDR, NULL);
	if (ip_index == NI_INDEX_NULL)
	    continue;

	en_nl_p = &scan->pl.nipl_val[en_index].nip_val;
	ip_nl_p = &scan->pl.nipl_val[ip_index].nip_val;
	if (en_nl_p->ninl_len == 0 || ip_nl_p->ninl_len == 0)
	    continue;
	for (n = 0; n < en_nl_p->ninl_len; n++) {
	    struct ether_addr * en_p = ether_aton(en_nl_p->ninl_val[n]);
	    if (en_p == NULL)
		continue;
	    if (bcmp(en_p, en_search, sizeof(*en_search)) == 0) {
		int		which_ip; 

		which_ip = n % ip_nl_p->ninl_len;
		if (inet_aton(ip_nl_p->ninl_val[which_ip], client_ip) == 0)
		    continue;
		if (has_binding)
		    *has_binding = TRUE;
		if (func == NULL || (*func)(arg, *client_ip)) {
		    PLCache_make_head(PLCache, scan);
		    return (scan);
		}
	    }
	}
    }
    return (NULL);
}


PRIVATE_EXTERN PLCacheEntry_t *
PLCache_lookup_identifier(PLCache_t * PLCache, 
			 char * idstr, NICacheFunc_t * func, void * arg,
			 struct in_addr * client_ip,
			 boolean_t * has_binding)
{
    PLCacheEntry_t *	scan;

    if (has_binding)
	*has_binding = FALSE;
    for (scan = PLCache->head; scan; scan = scan->next) {
	ni_namelist *	ident_nl_p;
	int 		n;
	int		ident_index;
	int		ip_index;
	ni_namelist *	ip_nl_p;

	ident_index = (int)ni_proplist_match(scan->pl, NIPROP_IDENTIFIER, 
					     NULL);
	if (ident_index == NI_INDEX_NULL)
	    continue;
	ident_nl_p = &scan->pl.nipl_val[ident_index].nip_val;
	if (ident_nl_p->ninl_len == 0)
	    continue;

	if (client_ip == NULL) { /* don't care about IP binding */
	    if (strcmp(ident_nl_p->ninl_val[0], idstr) == 0) {
		if (has_binding)
		    *has_binding = TRUE;
		PLCache_make_head(PLCache, scan);
		return (scan);
	    }
	}
	    
	ip_index = (int)ni_proplist_match(scan->pl, NIPROP_IPADDR, NULL);
	if (ip_index == NI_INDEX_NULL)
	    continue;
	ip_nl_p = &scan->pl.nipl_val[ip_index].nip_val;
	if (ip_nl_p->ninl_len == 0)
	    continue;

	for (n = 0; n < ident_nl_p->ninl_len; n++) {
	    if (strcmp(ident_nl_p->ninl_val[n], idstr) == 0) {
		int		which_ip; 
		
		which_ip = n % ip_nl_p->ninl_len;
		if (inet_aton(ip_nl_p->ninl_val[which_ip], client_ip) == 0)
		    continue;
		if (has_binding)
		    *has_binding = TRUE;
		if (func == NULL || (client_ip != NULL && (*func)(arg, *client_ip))) {
		    PLCache_make_head(PLCache, scan);
		    return (scan);
		}
	    }
	}
    }
    return (NULL);
}


PRIVATE_EXTERN PLCacheEntry_t *
PLCache_lookup_ip(PLCache_t * PLCache, struct in_addr iaddr)
{
    PLCacheEntry_t *	scan;

    for (scan = PLCache->head; scan; scan = scan->next) {
	int 		n;
	int		ip_index;
	ni_namelist *	ip_nl_p;

	ip_index = (int)ni_proplist_match(scan->pl, NIPROP_IPADDR, NULL);
	if (ip_index == NI_INDEX_NULL)
	    continue;
	ip_nl_p = &scan->pl.nipl_val[ip_index].nip_val;
	for (n = 0; n < ip_nl_p->ninl_len; n++) {
	    struct in_addr entry_ip;
	    if (inet_aton(ip_nl_p->ninl_val[n], &entry_ip) == 0)
		continue;
	    if (iaddr.s_addr == entry_ip.s_addr) {
		PLCache_make_head(PLCache, scan);
		return (scan);
	    }
	}
    }
    return (NULL);
}

#ifdef READ_TEST
int
main(int argc, char * argv[])
{
    PLCache_t 	PLCache;

    if (argc < 2)
	exit(1);

    PLCache_init(&PLCache);
    PLCache_set_max(&PLCache, 100 * 1024 * 1024);
    S_timestamp("before read");
    if (PLCache_read(&PLCache, argv[1]) == TRUE) {
	S_timestamp("after read");
	PLCache_print(&PLCache);

	printf("writing /tmp/readtest.out\n");
	PLCache_write(&PLCache, "/tmp/readtest.out");
    }
    else {
	S_timestamp("after read");
	printf("PLCache_read failed\n");
    }
    exit(0);
}

#endif /* READ_TEST */
