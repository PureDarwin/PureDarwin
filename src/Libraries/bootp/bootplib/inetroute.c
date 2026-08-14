/*
 * Copyright (c) 1998-2018 Apple Inc. All rights reserved.
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
 * inetroute.c
 * - get a list of internet network routes
 */

/*
 * Modification History
 *
 * Dieter Siegmund (dieter@apple.com) Tue Jul 14 11:33:50 PDT 1998
 * - created
 */
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/mbuf.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/route.h>
#include <netinet/in.h>
#include <sys/sysctl.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h> /* has inet_ntoa, etc. */

#include "inetroute.h"
#include "cfutil.h"
#include "util.h"

#define INDEX_NONE	-1

static __inline__ void *
next_sockaddr(struct sockaddr * sa)
{
    int		offset;

    offset = (sa->sa_len != 0)
	? roundup(sa->sa_len, sizeof(uint32_t))
	: sizeof(uint32_t);
    return (((caddr_t)(sa) + offset));
}

inetroute_list_t *
inetroute_list_init()
{
    char *		buf = NULL;
    char *		lim;
    inetroute_list_t *	list_p = NULL;
    int			list_size = 2;
    int 		mib[6];
    size_t 		needed;
    char * 		next;
    struct rt_msghdr *	rtm;

    mib[0] = CTL_NET;
    mib[1] = PF_ROUTE;
    mib[2] = 0;
    mib[3] = 0;
    mib[4] = NET_RT_DUMP;
    mib[5] = 0;
    if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0) { 
	perror("route-sysctl-estimate"); 
	goto err;
    }
    if ((buf = malloc(needed)) == 0) {
	goto err;
    }
    if (sysctl(mib, 6, buf, &needed, NULL, 0) < 0) {
	perror("sysctl of routing table");
	goto err;
    }
    list_p = (inetroute_list_t *)malloc(sizeof(*list_p));
    list_p->def_index = INDEX_NONE;
    if (list_p == NULL)
	goto err;
    list_p->count = 0;
    list_p->list = (inetroute_t *)malloc(sizeof(*(list_p->list)) * list_size);
    if (list_p->list == NULL)
	goto err;
    lim = buf + needed;
    for (next = buf; next < lim; next += rtm->rtm_msglen) {
	void *		  addrs;
	struct sockaddr * dst = NULL;
	struct sockaddr * gateway = NULL;
	struct sockaddr * mask = NULL;

	/* ALIGN: kernel ensures that next will be aligned, cast ok */	
	rtm = (struct rt_msghdr *)(void *)next;
	addrs = (void *)(&rtm[1]);
	if (rtm->rtm_addrs & RTA_DST) {
	    dst = (struct sockaddr *)addrs;
	    addrs = next_sockaddr(dst);
	}
	if (rtm->rtm_addrs & RTA_GATEWAY) {
	    gateway = (struct sockaddr *)addrs;
	    addrs = next_sockaddr(gateway);
	}
	if (rtm->rtm_addrs & RTA_NETMASK) {
	    mask = (struct sockaddr *)addrs;
	}

	if (dst && dst->sa_family == AF_INET 
	    && gateway 
	    && mask
	    && !(rtm->rtm_flags & RTF_HOST)) {
	    /* ALIGN: dst aligned, after cast
	     * dst_p is aligned to fields of dst. */
	    struct sockaddr_in * dst_p = (struct sockaddr_in *)(void *)dst;
	
	    /* ALIGN: mask_p aligned, after cast, mask_p aligned to fields of
	     * mask. */
	    struct sockaddr_in * mask_p = (struct sockaddr_in *)(void *)mask;

	    inetroute_t * entry;
	    if (list_p->count == list_size) {
		list_size *= 2;
		list_p->list = (inetroute_t *) 
		    realloc(list_p->list, sizeof(*(list_p->list)) * list_size);
		if (list_p->list == NULL)
		    goto err;
	    }
	    entry = list_p->list + list_p->count;
	    bzero(entry, sizeof(*entry));
	    entry->dest = dst_p->sin_addr;
	    if (mask_p->sin_len != 0) {
		entry->mask = mask_p->sin_addr;
	    }
	    /* remember the non-interface-scoped default route */
	    if ((rtm->rtm_flags & RTF_IFSCOPE) == 0 
		&& dst_p->sin_addr.s_addr == INADDR_ANY) {
		list_p->def_index = list_p->count;
	    }
	    if (gateway->sa_family == AF_LINK) {
		/* ALIGN: gateway aligned.  After cast, 
		 * sdl fields should be aligned */
		struct sockaddr_dl * sdl = (struct sockaddr_dl *)(void *)gateway;
		entry->gateway.link = *sdl;
	    }
	    else {
		struct sockaddr_in * in_p = (struct sockaddr_in *)(void *)gateway;
		entry->gateway.inet = *in_p;
	    }
	    list_p->count++;
	}
    }
    free(buf);
    return (list_p);
  err:
    if (buf)
	free(buf);
    inetroute_list_free(&list_p);
    return (NULL);
}

void
inetroute_list_free(inetroute_list_t * * list)
{
    if (list != NULL && *list != NULL) {
	if ((*list)->list)
	    free((*list)->list);
	(*list)->list = NULL;
	free(*list);
	*list = NULL;
    }
}

struct in_addr *
inetroute_default(inetroute_list_t * list_p)
{
    inetroute_t * 	entry;
    
    if (list_p->def_index == INDEX_NONE) {
	return (NULL);
    }
    entry = list_p->list + list_p->def_index;
    if (entry->gateway.inet.sin_family == AF_INET) {
	return (&(entry->gateway.inet.sin_addr));
    }
    return (NULL);
}

void
inetroute_list_print_cfstr(CFMutableStringRef str, inetroute_list_t * list_p)
{
    int i;

    for (i = 0; i < list_p->count; i++) {
	inetroute_t * entry = list_p->list + i;

	if (entry->gateway.link.sdl_family == AF_LINK) {
	    STRING_APPEND(str, "%s ==> link %d\n", 
			  inet_nettoa(entry->dest, entry->mask),
			  entry->gateway.link.sdl_index);
	}
	else {
	    STRING_APPEND(str, "%s ==> %s\n", 
			  inet_nettoa(entry->dest, entry->mask),
			  inet_ntoa(entry->gateway.inet.sin_addr));
	}
    }
}

void
inetroute_list_print(inetroute_list_t * list_p)
{
    CFMutableStringRef	str;

    str = CFStringCreateMutable(NULL, 0);
    inetroute_list_print_cfstr(str, list_p);
    my_CFStringPrint(stdout, str);
    CFRelease(str);
}

#ifdef TEST_INETROUTE
int
main()
{
    inetroute_list_t *	list_p;
    struct in_addr *	def;

    list_p = inetroute_list_init();
    if (list_p == NULL)
	exit(0);
    inetroute_list_print(list_p);
    def = inetroute_default(list_p);
    if (def) {
	printf("default route: %s\n", inet_ntoa(*def));
    }
    inetroute_list_free(&list_p);
    exit(0);
}
#endif /* TEST_INETROUTE */
