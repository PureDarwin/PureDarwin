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
 * hostlist.c
 * - in-core host entry list manipulation routines
 * - these are used for storing the in-core version of the 
 *   file-based host list and the in-core ignore list
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <mach/boolean.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <net/if.h>
#include <netinet/if_ether.h>
#include <arpa/inet.h>
#include <signal.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <syslog.h>
#include "hostlist.h"

void
hostinsert(struct hosts * * hosts, struct hosts * hp)
{
    hp->next = *hosts;
    hp->prev = NULL;
    if (*hosts)
	(*hosts)->prev = hp;
    *hosts = hp;
}

void
hostremove(struct hosts * * hosts, struct hosts * hp)
{
    if (hp->prev)
	hp->prev->next = hp->next;
    else
	*hosts = hp->next;
    if (hp->next)
	hp->next->prev = hp->prev;
}

void
hostprint(struct hosts * hp)
{
    if (hp)
	syslog(LOG_INFO, 
	       "hw %s type %d len %d ip %s host '%s' bootfile '%s'\n", 
	       ether_ntoa((struct ether_addr *)&hp->haddr), hp->htype,
	       hp->hlen, inet_ntoa(hp->iaddr), 
	       hp->hostname ? hp->hostname : "", 
	       hp->bootfile ? hp->bootfile : "");
}

void
hostfree(struct hosts * * hosts,
	 struct hosts *hp
	 )
{
    hostremove(hosts, hp);
    if (hp->hostname) {
	free(hp->hostname);
	hp->hostname = NULL;
    }
    if (hp->bootfile) {
	free(hp->bootfile);
	hp->bootfile = NULL;
    }
    free((char *)hp);
}

struct hosts * 
hostadd(struct hosts * * hosts, struct timeval * tv_p, int htype,
	char * haddr, int hlen, struct in_addr * iaddr_p, 
	char * hostname, char * bootfile)
{
    struct hosts * hp;

    hp = (struct hosts *)malloc(sizeof(*hp));
    if (!hp)
	return (NULL);
    bzero(hp, sizeof(*hp));
    if (tv_p)
	hp->tv = *tv_p;
    hp->htype = htype;
    hp->hlen = hlen;
    if (hlen > sizeof(hp->haddr)) {
	hlen = sizeof(hp->haddr);
    }
    bcopy(haddr, &hp->haddr, hlen);
    if (iaddr_p)
	hp->iaddr = *iaddr_p;
    if (hostname)
	hp->hostname = strdup(hostname);
    if (bootfile)
	hp->bootfile = strdup(bootfile);
    hostinsert(hosts, hp);
    return (hp);
}
