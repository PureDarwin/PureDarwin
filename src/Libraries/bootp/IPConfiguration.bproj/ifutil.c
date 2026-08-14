/*
 * Copyright (c) 2000-2020 Apple Inc. All rights reserved.
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
 * ifutil.c
 * - network interface utility routines
 */

/* 
 * Modification History
 *
 * June 23, 2009	Dieter Siegmund (dieter@apple.com)
 * - split out from ipconfigd.c
 */


#define INET6	1
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/icmp6.h>
#include <net/if_media.h>
#include <net/if_dl.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include "util.h"
#include "ifutil.h"
#include "rtutil.h"
#include "symbol_scope.h"
#include "mylog.h"
#include "CGA.h"
#include "cfutil.h"
#include <SystemConfiguration/SCPrivate.h>

PRIVATE_EXTERN int
inet_dgram_socket()
{
    return (socket(AF_INET, SOCK_DGRAM, 0));
}

STATIC int
interface_set_flags(int s, const char * name, 
		    uint16_t flags_to_set, uint16_t flags_to_clear)
{
    uint16_t		flags_after;
    uint16_t		flags_before;
    struct ifreq	ifr;
    int 		ret;

    bzero(&ifr, sizeof(ifr));
    strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
    ret = ioctl(s, SIOCGIFFLAGS, (caddr_t)&ifr);
    if (ret < 0) {
	return (ret);
    }
    flags_before = ifr.ifr_flags;
    ifr.ifr_flags |= flags_to_set;
    ifr.ifr_flags &= ~(flags_to_clear);
    flags_after = ifr.ifr_flags;
    if (flags_before == flags_after) {
	/* nothing to do */
	ret = 0;
    }
    else {
	/* issue the ioctl */
	ret = ioctl(s, SIOCSIFFLAGS, &ifr);
	my_log(LOG_INFO,
	       "interface_set_flags(%s, set 0x%x, clear 0x%x) 0x%x => 0x%x",
	       name, flags_to_set, flags_to_clear, flags_before, flags_after);
    }
    return (ret);
}

STATIC int
siocsifmtu(int s, const char * name, int mtu)
{
    struct ifreq	ifr;

    bzero(&ifr, sizeof(ifr));
    strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
    ifr.ifr_mtu = mtu;
    return (ioctl(s, SIOCSIFMTU, (caddr_t)&ifr));
}

STATIC int
siocgifeflags(int sockfd, const char * name, uint64_t * ret_eflags)
{
    struct ifreq	ifr;
    int			ret;

    memset(&ifr, 0, sizeof(ifr));
    strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
    ret = ioctl(sockfd, SIOCGIFEFLAGS, (caddr_t)&ifr);
    if (ret == 0) {
	*ret_eflags = ifr.ifr_eflags;
    }
    else {
	switch (errno) {
	case ENXIO:
	case EPWROFF:
	case EINVAL:
	    break;
	default:
	    my_log(LOG_NOTICE,
		   "%s: SIOCGIFEFLAGS failed status, %s",
		   name, strerror(errno));
	    break;
	}
	*ret_eflags = 0;
    }
    return (ret);
}

PRIVATE_EXTERN int
interface_get_eflags(int sockfd, const char * name, uint64_t * ret_eflags)
{
    int		ret = 0;

    if (siocgifeflags(sockfd, name, ret_eflags) != 0) {
	ret = errno;
    }
    return (ret);
}

STATIC int
siocprotoattach(int s, const char * name)
{
    struct ifreq	ifr;

    bzero(&ifr, sizeof(ifr));
    strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
    return (ioctl(s, SIOCPROTOATTACH, &ifr));
}

PRIVATE_EXTERN int
interface_set_mtu(const char * ifname, int mtu)
{
    int ret = 0;
    int s = inet_dgram_socket();

    if (s < 0) {
	ret = errno;
    }
    else {
	if (siocsifmtu(s, ifname, mtu) < 0) {
	    ret = errno;
	    my_log(LOG_ERR, "siocsifmtu(%s, %d) failed, %s (%d)",
		   ifname, mtu, strerror(ret), ret);
	}
	close(s);
    }
    return (ret);

}

PRIVATE_EXTERN int
interface_up_down(const char * ifname, boolean_t up)
{
    int 	ret = 0;
    int 	s = inet_dgram_socket();

    if (s < 0) {
	ret = errno;
    }
    else {
	my_log(LOG_INFO,
	       "interface_up_down(%s, %s)", ifname,
	       up ? "UP" : "DOWN");
	ret = interface_set_flags(s, ifname,
				  up ? IFF_UP : 0,
				  !up ? IFF_UP : 0);
	close(s);
    }
    return (ret);
}

PRIVATE_EXTERN int
inet_attach_interface(const char * ifname, boolean_t set_iff_up)
{
    int ret = 0;
    int s = inet_dgram_socket();

    if (s < 0) {
	ret = errno;
	goto done;
    }

    if (siocprotoattach(s, ifname) < 0) {
	ret = errno;
	if (ret != EEXIST && ret != ENXIO) {
	    my_log(LOG_INFO, "siocprotoattach(%s) failed, %s (%d)", 
		   ifname, strerror(errno), errno);
	}
    }
    my_log(LOG_INFO,
	   "inet_attach_interface(%s)", ifname);
    if (set_iff_up) {
	(void)interface_set_flags(s, ifname, IFF_UP, 0);
    }
    close(s);

 done:
    return (ret);
}

STATIC int
siocprotodetach(int s, const char * name)
{
    struct ifreq	ifr;

    bzero(&ifr, sizeof(ifr));
    strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
    return (ioctl(s, SIOCPROTODETACH, &ifr));
}

PRIVATE_EXTERN int
inet_detach_interface(const char * ifname)
{
    int ret = 0;
    int s = inet_dgram_socket();

    if (s < 0) {
	ret = errno;
	goto done;
    }
    if (siocprotodetach(s, ifname) < 0) {
	ret = errno;
	if (ret != ENXIO) {
	    my_log(LOG_ERR, "siocprotodetach(%s) failed, %s (%d)", 
		   ifname, strerror(errno), errno);
	}
    }
    close(s);

 done:
    return (ret);
}

STATIC int
siocautoaddr(int s, const char * name, int value)
{
    struct ifreq	ifr;

    bzero(&ifr, sizeof(ifr));
    strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
    ifr.ifr_data = (caddr_t)(intptr_t)value;
    return (ioctl(s, SIOCAUTOADDR, &ifr));
}

PRIVATE_EXTERN int
inet_set_autoaddr(const char * ifname, int val)
{
    int 		s = inet_dgram_socket();
    int			ret = 0;

    if (s < 0) {
	ret = errno;
	my_log(LOG_ERR, 
	       "inet_set_autoaddr(%s, %d): socket() failed, %s (%d)",
	       ifname, val, strerror(errno), errno);
    }
    else {
	if (siocautoaddr(s, ifname, val) < 0) {
	    ret = errno;
	    if (ret != ENXIO) {
		my_log(LOG_ERR, "inet_set_autoaddr(%s, %d) failed, %s (%d)", 
		       ifname, val, strerror(errno), errno);
	    }
	}
	close(s);
    }
    return (ret);
}

STATIC void
set_sockaddr_in(struct sockaddr_in * sin_p, struct in_addr addr)
{
    sin_p->sin_len = sizeof(struct sockaddr_in);
    sin_p->sin_family = AF_INET;
    sin_p->sin_addr = addr;
    return;
}

PRIVATE_EXTERN int
inet_difaddr(int s, const char * name, const struct in_addr addr)
{
    struct ifreq	ifr;

    bzero(&ifr, sizeof(ifr));
    strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
    /* ALIGN: ifr.ifr_addr is aligned (in union), cast okay. */
    set_sockaddr_in((struct sockaddr_in *)(void *)&ifr.ifr_addr, addr);
    return (ioctl(s, SIOCDIFADDR, &ifr));
}

PRIVATE_EXTERN int
inet_aifaddr(int s, const char * name, struct in_addr addr,
	     const struct in_addr * mask,
	     const struct in_addr * broadaddr)
{
    struct in_aliasreq	ifra;

    bzero(&ifra, sizeof(ifra));
    strncpy(ifra.ifra_name, name, sizeof(ifra.ifra_name));
    set_sockaddr_in(&ifra.ifra_addr, addr);
    if (mask != NULL) {
	set_sockaddr_in(&ifra.ifra_mask, *mask);
    }
    if (broadaddr != NULL) {
	set_sockaddr_in(&ifra.ifra_broadaddr, *broadaddr);
    }
    return (ioctl(s, SIOCAIFADDR, &ifra));
}

STATIC int
siocgifprotolist(int s, const char * ifname,
		 u_int32_t * ret_list, u_int32_t * ret_list_count)
{
    struct if_protolistreq	ifpl;

    bzero(&ifpl, sizeof(ifpl));
    strlcpy(ifpl.ifpl_name, ifname, sizeof(ifpl.ifpl_name));
    if (ret_list != NULL) {
	ifpl.ifpl_count = *ret_list_count;
	ifpl.ifpl_list = ret_list;
    }
    if (ioctl(s, SIOCGIFPROTOLIST, &ifpl) < 0) {
	return (-1);
    }
    *ret_list_count = ifpl.ifpl_count;
    return (0);
}

STATIC u_int32_t *
protolist_copy(const char * ifname, u_int32_t * ret_count)
{
    u_int32_t *	protolist;
    u_int32_t	protolist_count;
    int		s;

    protolist = NULL;
    protolist_count = 0;
    s = inet_dgram_socket();
    if (s < 0) {
	my_log(LOG_ERR,
	       "protolist_copy: socket failed, %s (%d)",
	       strerror(errno), errno);
	goto failed;
    }
    if (siocgifprotolist(s, ifname, protolist, &protolist_count) != 0) {
	protolist_count = 0;
	my_log(LOG_ERR,
	       "SIOCGIFPROTOLIST failed: %s (%d)",
	       strerror(errno), errno);
	goto failed;
    }
    if (protolist_count != 0) {
	protolist = (u_int32_t *)malloc(protolist_count * sizeof(*protolist));
	if (siocgifprotolist(s, ifname, protolist, &protolist_count) != 0) {
	    protolist_count = 0;
	    my_log(LOG_ERR,
		   "SIOCGIFPROTOLIST failed#2: %s (%d)",
		   strerror(errno), errno);
	    goto failed;
	}
    }

 failed:
    if (s >= 0) {
	close(s);
    }
    if (protolist_count == 0 && protolist != NULL) {
	free(protolist);
	protolist = NULL;
    }
    *ret_count = protolist_count;
    return (protolist);
}

STATIC boolean_t
proto_is_attached(const char * ifname, u_int32_t protocol)
{
    boolean_t		found = FALSE;
    u_int32_t *		protolist;
    u_int32_t		protolist_count;

    protolist = protolist_copy(ifname, &protolist_count);
    if (protolist != NULL) {
	for (u_int32_t i = 0; i < protolist_count; i++) {
	    if (protolist[i] == protocol) {
		found = TRUE;
		break;
	    }
	}
	free(protolist);
    }
    return (found);
}


#include <netinet6/in6_var.h>
#include <netinet6/nd6.h>

STATIC int
count_prefix_bits(void * val, int size)
{
    int		bit;
    int 	byte;
    uint8_t *	name = (uint8_t *)val;
    int		plen = 0;

    /* look for prefix bytes that have all bits set */
    for (byte = 0; byte < size; byte++, plen += 8) {
	if (name[byte] != 0xff) {
	    break;
	}
    }

    /* all of the bits were set */
    if (byte == size) {
	return (plen);
    }

    /* we have the prefix length when we seee the first bit that isn't set */
    for (bit = 7; bit != 0; bit--, plen++) {
	if (!(name[byte] & (1 << bit))) {
	    break;
	}
    }

    /* valididate that no bits are set after the last bit */
    for (; bit != 0; bit--) {
	if (name[byte] & (1 << bit)) {
	    /* not a simple prefix */
	    return (0);
	}
    }
    byte++;
    for (; byte < size; byte++) {
	if (name[byte]) {
	    /* not a simple prefix */
	    return (0);
	}
    }
    return (plen);
}

STATIC void
set_sockaddr_in6(struct sockaddr_in6 * sin6_p, const struct in6_addr * addr);

PRIVATE_EXTERN int
inet6_dgram_socket()
{
    return (socket(AF_INET6, SOCK_DGRAM, 0));
}

STATIC int
siocprotoattach_in6(int s, const char * name)
{
    struct in6_aliasreq		ifra;

    bzero(&ifra, sizeof(ifra));
    strncpy(ifra.ifra_name, name, sizeof(ifra.ifra_name));
    return (ioctl(s, SIOCPROTOATTACH_IN6, &ifra));
}

STATIC int
siocprotodetach_in6(int s, const char * name)
{
    struct in6_ifreq	ifr;

    bzero(&ifr, sizeof(ifr));
    strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
    return (ioctl(s, SIOCPROTODETACH_IN6, &ifr));
}

STATIC int
siocclat46_start(int s, const char * name)
{
    struct in6_ifreq	ifr6;

    bzero(&ifr6, sizeof(ifr6));
    strlcpy(ifr6.ifr_name, name, sizeof(ifr6.ifr_name));
    return (ioctl(s, SIOCCLAT46_START, &ifr6));
}

STATIC int
siocclat46_stop(int s, const char * name)
{
    struct in6_ifreq	ifr6;

    bzero(&ifr6, sizeof(ifr6));
    strlcpy(ifr6.ifr_name, name, sizeof(ifr6.ifr_name));
    return (ioctl(s, SIOCCLAT46_STOP, &ifr6));
}

STATIC int
siocll_start(int s, const char * name, const struct in6_addr * v6_ll)
{
    struct in6_aliasreq		ifra_in6;

    bzero(&ifra_in6, sizeof(ifra_in6));
    strncpy(ifra_in6.ifra_name, name, sizeof(ifra_in6.ifra_name));
    if (v6_ll != NULL) {
	char 		ntopbuf[INET6_ADDRSTRLEN];

	/* our address */
	set_sockaddr_in6(&ifra_in6.ifra_addr, v6_ll);

	inet_ntop(AF_INET6, v6_ll, ntopbuf, sizeof(ntopbuf));
	my_log(LOG_INFO, "ioctl(%s, SIOCLL_START %s)", name, ntopbuf);
    }
    else {
	my_log(LOG_INFO, "ioctl(%s, SIOCLL_START)", name);
    }
    return (ioctl(s, SIOCLL_START, &ifra_in6));
}

STATIC int
ll_start(int s, const char * name, const struct in6_addr * v6_ll,
	 boolean_t use_cga, uint8_t collision_count)
{
    int 		error = 0;

    if (v6_ll != NULL || use_cga == FALSE || !CGAIsEnabled()) {
	/* don't use CGA */
	error = siocll_start(s, name, v6_ll);
    }
    else {
	struct in6_cgareq	req;

	/* use CGA */
	bzero(&req, sizeof(req));
	strncpy(req.cgar_name, name, sizeof(req.cgar_name));
	CGAPrepareSetForInterfaceLinkLocal(name, &req.cgar_cgaprep);
	req.cgar_lifetime.ia6t_vltime = ND6_INFINITE_LIFETIME;
	req.cgar_lifetime.ia6t_pltime = ND6_INFINITE_LIFETIME;
	req.cgar_collision_count = collision_count;
	my_log(LOG_INFO, "ioctl(%s, SIOCLL_CGASTART) collision_count=%d",
	       name, collision_count);
	error = ioctl(s, SIOCLL_CGASTART, &req);
    }
    return (error);
}

STATIC int
siocll_stop(int s, const char * name)
{
    struct in6_ifreq		ifr;

    bzero(&ifr, sizeof(ifr));
    strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
    my_log(LOG_INFO, "ioctl(%s, SIOCLL_STOP)", name);
    return (ioctl(s, SIOCLL_STOP, &ifr));
}

STATIC void
set_sockaddr_in6(struct sockaddr_in6 * sin6_p, const struct in6_addr * addr)
{
    sin6_p->sin6_family = AF_INET6;
    sin6_p->sin6_len = sizeof(struct sockaddr_in6);
    sin6_p->sin6_addr = *addr;
    return;
}

STATIC int
siocgifaflag_in6(int s, const char * ifname, const struct in6_addr * in6_addr,
		 int * ret_flags)
{
    struct in6_ifreq	ifr6;

    bzero((char *)&ifr6, sizeof(ifr6));
    strncpy(ifr6.ifr_name, ifname, sizeof(ifr6.ifr_name));
    set_sockaddr_in6(&ifr6.ifr_addr, in6_addr);
    if (ioctl(s, SIOCGIFAFLAG_IN6, &ifr6) < 0) {
	return (-1);
    }
    *ret_flags = ifr6.ifr_ifru.ifru_flags6;
    return (0);
}

STATIC int
siocgifalifetime_in6(int s, const char * ifname,
		     const struct in6_addr * in6_addr,
		     u_int32_t * ret_valid_lifetime,
		     u_int32_t * ret_preferred_lifetime)
{
    struct in6_ifreq	ifr6;

    bzero((char *)&ifr6, sizeof(ifr6));
    strncpy(ifr6.ifr_name, ifname, sizeof(ifr6.ifr_name));
    set_sockaddr_in6(&ifr6.ifr_addr, in6_addr);
    if (ioctl(s, SIOCGIFALIFETIME_IN6, &ifr6) < 0) {
	return (-1);
    }
    *ret_valid_lifetime = ifr6.ifr_ifru.ifru_lifetime.ia6t_vltime;
    *ret_preferred_lifetime = ifr6.ifr_ifru.ifru_lifetime.ia6t_pltime;
    return (0);
}

STATIC int
siocsifcgaprep_in6(int s, const char * ifname)
{
    struct in6_cgareq	req;

    bzero(&req, sizeof(req));
    strncpy(req.cgar_name, ifname, sizeof(req.cgar_name));
    CGAPrepareSetForInterface(ifname, &req.cgar_cgaprep);
    req.cgar_lifetime.ia6t_vltime = ND6_INFINITE_LIFETIME;
    req.cgar_lifetime.ia6t_pltime = ND6_INFINITE_LIFETIME;
    return (ioctl(s, SIOCSIFCGAPREP_IN6, &req));
}

PRIVATE_EXTERN int
inet6_attach_interface(const char * ifname, boolean_t set_iff_up)
{
    int	ret = 0;
    int s = inet6_dgram_socket();

    if (s < 0) {
	ret = errno;
	my_log(LOG_ERR,
	       "inet6_attach_interface(%s): socket() failed, %s (%d)",
	       ifname, strerror(ret), ret);
	goto done;
    }
    if (siocprotoattach_in6(s, ifname) < 0) {
	ret = errno;
	if (ret != EEXIST && ret != ENXIO) {
	    my_log(LOG_INFO, "siocprotoattach_in6(%s) failed, %s (%d)",
		   ifname, strerror(errno), errno);
	}
    }
    my_log(LOG_INFO,
	   "inet6_attach_interface(%s)", ifname);
    if (set_iff_up) {
	(void)interface_set_flags(s, ifname, IFF_UP, 0);
    }
    close(s);

 done:
    return (ret);
}

PRIVATE_EXTERN int
inet6_detach_interface(const char * ifname)
{
    int ret = 0;
    int s = inet6_dgram_socket();

    if (s < 0) {
	ret = errno;
	my_log(LOG_ERR, 
	       "inet6_detach_interface(%s): socket() failed, %s (%d)",
	       ifname, strerror(ret), ret);
	goto done;
    }
    if (siocprotodetach_in6(s, ifname) < 0) {
	ret = errno;
	if (ret != ENXIO) {
	    my_log(LOG_INFO, "siocprotodetach_in6(%s) failed, %s (%d)",
		   ifname, strerror(errno), errno);
	}
    }
    close(s);
    my_log(LOG_INFO,
	   "inet6_detach_interface(%s)", ifname);

 done:
    return (ret);
}

PRIVATE_EXTERN boolean_t
inet6_is_attached(const char * ifname)
{
    return (proto_is_attached(ifname, PF_INET6));
}

STATIC boolean_t
nd_flags_set_with_socket(int s, const char * if_name, 
			 uint32_t set_flags, uint32_t clear_flags)
{
    uint32_t		new_flags;
    struct in6_ndireq 	nd;

    bzero(&nd, sizeof(nd));
    strncpy(nd.ifname, if_name, sizeof(nd.ifname));
    if (ioctl(s, SIOCGIFINFO_IN6, &nd)) {
	my_log_fl(LOG_ERR, "SIOCGIFINFO_IN6(%s) failed, %s",
		  if_name, strerror(errno));
	return (FALSE);
    }
    new_flags = nd.ndi.flags;
    if (set_flags) {
	new_flags |= set_flags;
    }
    if (clear_flags) {
	new_flags &= ~clear_flags;
    }
    if (new_flags != nd.ndi.flags) {
	nd.ndi.flags = new_flags;
	if (ioctl(s, SIOCSIFINFO_FLAGS, (caddr_t)&nd)) {
	    my_log_fl(LOG_ERR, "SIOCSIFINFO_FLAGS(%s) failed, %s",
		      if_name, strerror(errno));
	    return (FALSE);
	}
    }
    return (TRUE);
}

PRIVATE_EXTERN int
inet6_linklocal_start(const char * ifname,
		      const struct in6_addr * v6_ll,
		      boolean_t perform_nud,
		      boolean_t use_cga,
		      boolean_t enable_dad,
		      uint8_t collision_count)
{
    uint32_t	clear_flags;
    int 	ret = 0;
    int 	s = inet6_dgram_socket();
    uint32_t	set_flags;

    if (s < 0) {
	ret = errno;
	my_log(LOG_ERR, 
	       "inet6_linklocal_start(%s): socket() failed, %s (%d)",
	       ifname, strerror(ret), ret);
	goto done;
    }

    /* set/clear the ND flags */
    set_flags = 0;
    clear_flags = ND6_IFF_IFDISABLED;
    if (use_cga) {
	clear_flags |= ND6_IFF_INSECURE;
    }
    else {
	set_flags |= ND6_IFF_INSECURE;
    }
    if (perform_nud) {
	set_flags |= ND6_IFF_PERFORMNUD;
    }
    else {
	clear_flags |= ND6_IFF_PERFORMNUD;
    }
    if (enable_dad) {
	set_flags |= ND6_IFF_DAD;
    }
    else {
	clear_flags |= ND6_IFF_DAD;
    }
    nd_flags_set_with_socket(s, ifname, set_flags, clear_flags);

    /* start IPv6 link-local */
    if (ll_start(s, ifname, v6_ll, use_cga, collision_count) < 0) {
	ret = errno;
	if (errno != ENXIO) {
	    my_log(LOG_ERR, "siocll_start(%s) failed, %s (%d)",
		   ifname, strerror(errno), errno);
	}
    }
    close(s);
 done:
    return (ret);
}

PRIVATE_EXTERN int
inet6_linklocal_stop(const char * ifname)
{
    int ret = 0;
    int s = inet6_dgram_socket();

    if (s < 0) {
	ret = errno;
	my_log(LOG_ERR, 
	       "inet6_linklocal_stop(%s): socket() failed, %s (%d)",
	       ifname, strerror(ret), ret);
	goto done;
    }
    if (siocll_stop(s, ifname) < 0) {
	ret = errno;
	if (errno != ENXIO) {
	    my_log(LOG_ERR, "siocll_stop(%s) failed, %s (%d)",
		   ifname, strerror(errno), errno);
	}
    }
    close(s);

 done:
    return (ret);
}

STATIC int
siocautoconf_start(int s, const char * if_name)
{
    struct in6_ifreq	ifr;

    bzero(&ifr, sizeof(ifr));
    strncpy(ifr.ifr_name, if_name, sizeof(ifr.ifr_name));
    return (ioctl(s, SIOCAUTOCONF_START, &ifr));
}

STATIC int
siocautoconf_stop(int s, const char * if_name)
{
    struct in6_ifreq	ifr;

    bzero(&ifr, sizeof(ifr));
    strncpy(ifr.ifr_name, if_name, sizeof(ifr.ifr_name));
    return (ioctl(s, SIOCAUTOCONF_STOP, &ifr));
}

PRIVATE_EXTERN int
inet6_rtadv_enable(const char * if_name, boolean_t use_cga)
{
    int			ret = 0;
    int			s = inet6_dgram_socket();

    if (s < 0) {
	ret = errno;
	my_log(LOG_ERR, 
	       "inet6_rtadv_enable(%s): socket() failed, %s (%d)",
	       if_name, strerror(ret), ret);
	goto done;
    }
    /* set the per-interface modifier */
    if (use_cga && CGAIsEnabled()
	&& siocsifcgaprep_in6(s, if_name) < 0) {
	int	saved_errno;

	saved_errno = errno;
	if (saved_errno != ENXIO) {
	    my_log(LOG_ERR, "siocsifcgaprep_in6(%s) failed, %s (%d)",
		   if_name, strerror(saved_errno), saved_errno);
	}
#if ! TARGET_OS_WATCH
	ret = saved_errno;
	goto done;
#endif
    }

    /* enable processing Router Advertisements */
    if (siocautoconf_start(s, if_name) < 0) {
	ret = errno;
	if (errno != ENXIO) {
	    my_log(LOG_ERR, "siocautoconf_start(%s) failed, %s (%d)",
		   if_name, strerror(errno), errno);
	}
    }
    my_log(LOG_INFO,
	   "rtadv_enable(%s)", if_name);
 done:
    if (s >= 0) {
	close(s);
    }
    return (ret);
}

PRIVATE_EXTERN int
inet6_rtadv_disable(const char * if_name)
{
    int			ret = 0;
    int			s = inet6_dgram_socket();

    if (s < 0) {
	ret = errno;
	my_log(LOG_ERR, 
	       "inet6_rtadv_disable(%s): socket() failed, %s (%d)",
	       if_name, strerror(ret), ret);
	goto done;
    }
    if (siocautoconf_stop(s, if_name) < 0) {
	ret = errno;
	if (errno != ENXIO) {
	    my_log(LOG_ERR, "siocautoconf_stop(%s) failed, %s (%d)",
		   if_name, strerror(errno), errno);
	}
    }
    close(s);
    my_log(LOG_INFO,
	   "rtadv_disable(%s)", if_name);
 done:
    return (ret);
}

PRIVATE_EXTERN boolean_t
inet6_has_nat64_prefixlist(const char * if_name)
{
    struct if_nat64req	ifr;
    int			ret;
    int			s = inet6_dgram_socket();

    if (s < 0) {
	ret = errno;
	my_log(LOG_ERR,
	       "inet6_has_nat64_prefixlist(%s): socket() failed, %s (%d)",
	       if_name, strerror(ret), ret);
	return FALSE;
    }
    bzero(&ifr, sizeof(ifr));
    strncpy(ifr.ifnat64_name, if_name, sizeof(ifr.ifnat64_name));
    ret = ioctl(s, SIOCGIFNAT64PREFIX, &ifr);
    close(s);
    return ((ret == 0) && (ifr.ifnat64_prefixes[0].prefix_len > 0));
}

PRIVATE_EXTERN int
inet6_difaddr(int s, const char * name, const struct in6_addr * addr)
{
    struct in6_ifreq	ifr;

    bzero(&ifr, sizeof(ifr));
    strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
    if (addr != NULL) {
	set_sockaddr_in6(&ifr.ifr_ifru.ifru_addr, addr);
    }
    return (ioctl(s, SIOCDIFADDR_IN6, &ifr));
}

/*
 * from netinet6/in6.c 
 */
STATIC void
in6_len2mask(struct in6_addr * mask, int len)
{
    int i;

    bzero(mask, sizeof(*mask));
    for (i = 0; i < len / 8; i++)
	mask->s6_addr[i] = 0xff;
    if (len % 8)
	mask->s6_addr[i] = (0xff00 >> (len % 8)) & 0xff;
}

STATIC void
in6_maskaddr(struct in6_addr * addr, const struct in6_addr * mask)
{
    int i;

    for (i = 0; i < sizeof(addr->s6_addr); i++) {
	addr->s6_addr[i] &= mask->s6_addr[i];
    }
    return;
}

PRIVATE_EXTERN void
in6_netaddr(struct in6_addr * addr, int len)
{
    struct in6_addr	mask;

    in6_len2mask(&mask, len);
    in6_maskaddr(addr, &mask);
    return;
}

STATIC char *
inet6_prefix_list_copy(size_t * ret_buf_len)
{
    char *		buf = NULL;
    size_t 		buf_len;
    int 		mib[] = {
	CTL_NET, PF_INET6, IPPROTO_ICMPV6, ICMPV6CTL_ND6_PRLIST
    };

    *ret_buf_len = 0;
    if (sysctl(mib, sizeof(mib) / sizeof(mib[0]), NULL, &buf_len, NULL, 0)
	< 0) {
	return (NULL);
    }
    buf_len += 1024;
    buf = malloc(buf_len);
    if (sysctl(mib, sizeof(mib) / sizeof(mib[0]), buf, &buf_len, NULL, 0)
	< 0) {
	free(buf);
	buf = NULL;
    }
    else {
	*ret_buf_len = buf_len;
    }
    return (buf);
}

PRIVATE_EXTERN int
inet6_get_prefix_length(const struct in6_addr * addr, int if_index)
{
    char *		buf = NULL;
    size_t 		buf_len;
    struct in6_prefix *	end;
    struct in6_prefix *	next;
    int			prefix_length = 0;
    struct in6_prefix *	scan;

    buf = inet6_prefix_list_copy(&buf_len);
    if (buf == NULL) {
	goto done;
    }

    /* ALIGN: buf is aligned (from malloc), cast ok. */
    end = (struct in6_prefix *)(void *)(buf + buf_len);
    for (scan = (struct in6_prefix *)(void *)buf; scan < end; scan = next) {
	struct sockaddr_in6 *	advrtr;
	struct in6_addr		netaddr;

	advrtr = (struct sockaddr_in6 *)(scan + 1);
	next = (struct in6_prefix *)&advrtr[scan->advrtrs];

	if (if_index != 0 && if_index != scan->if_index) {
	    continue;
	}
	netaddr = *addr;
	in6_netaddr(&netaddr, scan->prefixlen);
	if (IN6_ARE_ADDR_EQUAL(&netaddr, &scan->prefix.sin6_addr)) {
	    prefix_length = scan->prefixlen;
	    break;
	}
    }

 done:
    if (buf != NULL) {
	free(buf);
    }
    return (prefix_length);
}

PRIVATE_EXTERN int
inet6_router_and_prefix_count(int if_index, int * ret_prefix_count)
{
    char *		buf = NULL;
    size_t 		buf_len;
    struct in6_prefix *	end;
    struct in6_prefix *	next;
    int			prefix_count = 0;
    int			router_count = 0;
    struct in6_prefix *	scan;

    buf = inet6_prefix_list_copy(&buf_len);
    if (buf == NULL) {
	goto done;
    }

    /* ALIGN: buf is aligned (from malloc), cast ok. */
    end = (struct in6_prefix *)(void *)(buf + buf_len);
    for (scan = (struct in6_prefix *)(void *)buf; scan < end; scan = next) {
	struct sockaddr_in6 *	advrtr;

	advrtr = (struct sockaddr_in6 *)(scan + 1);
	next = (struct in6_prefix *)&advrtr[scan->advrtrs];
	if (if_index != 0 && if_index != scan->if_index) {
	    continue;
	}
	if (scan->advrtrs == 0) {
	    continue;
	}
	router_count += scan->advrtrs;
	prefix_count++;
    }

 done:
    if (buf != NULL) {
	free(buf);
    }
    *ret_prefix_count = prefix_count;
    return (router_count);
}

PRIVATE_EXTERN int
inet6_aifaddr(int s, const char * name, const struct in6_addr * addr,
	      const struct in6_addr * dstaddr, int prefix_length, 
	      int flags,
	      u_int32_t valid_lifetime,
	      u_int32_t preferred_lifetime)
{
    struct in6_aliasreq	ifra_in6;

    bzero(&ifra_in6, sizeof(ifra_in6));
    strncpy(ifra_in6.ifra_name, name, sizeof(ifra_in6.ifra_name));
    ifra_in6.ifra_lifetime.ia6t_vltime = valid_lifetime;
    ifra_in6.ifra_lifetime.ia6t_pltime = preferred_lifetime;
    ifra_in6.ifra_flags = flags;
    if (addr != NULL) {
	set_sockaddr_in6(&ifra_in6.ifra_addr, addr);
    }
    if (dstaddr != NULL) {
	set_sockaddr_in6(&ifra_in6.ifra_dstaddr, dstaddr);
    }
    if (prefix_length != 0) {
	struct in6_addr		prefixmask;

	in6_len2mask(&prefixmask, prefix_length);
	set_sockaddr_in6(&ifra_in6.ifra_prefixmask, &prefixmask);
    }
    return (ioctl(s, SIOCAIFADDR_IN6, &ifra_in6));
}

STATIC int
inet6_if_ioctl(const char * ifname, unsigned long request)
{
    struct in6_ifreq	ifr;
    int 		ret = 0;
    int			s;

    s = inet6_dgram_socket();
    if (s < 0) {
	ret = errno;
	goto done;
    }
    bzero(&ifr, sizeof(ifr));
    strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));

    if (ioctl(s, request, &ifr) < 0) {
	ret = errno;
    }
 done:
    if (s >=0) {
	close(s);
    }
    return (ret);
}

PRIVATE_EXTERN int
inet6_flush_prefixes(const char * ifname)
{
    my_log(LOG_INFO,
	   "inet6_flush_prefixes(%s)", ifname);
    return (inet6_if_ioctl(ifname, SIOCSPFXFLUSH_IN6));
}

PRIVATE_EXTERN int
inet6_flush_routes(const char * ifname)
{
    my_log(LOG_INFO,
	   "inet6_flush_routes(%s)", ifname);
    return (inet6_if_ioctl(ifname, SIOCSRTRFLUSH_IN6));
}

STATIC boolean_t
inet6_sysctl_get_int(int code, int * ret_value_p)
{
    int 	mib[] = { CTL_NET, PF_INET6, IPPROTO_IPV6, 0 };
    size_t 	size;

    mib[3] = code;
    size = sizeof(*ret_value_p);
    if (sysctl(mib, sizeof(mib)/sizeof(mib[0]), ret_value_p, &size, NULL, 0)
	< 0) {
	my_log(LOG_ERR, "inet6_sysctl_get_int(%d) failed, %s", code,
	       strerror(errno));
	return (FALSE);
    }
    return (TRUE);
}

PRIVATE_EXTERN boolean_t
inet6_forwarding_is_enabled(void)
{
    int		enabled = 0;

    if (inet6_sysctl_get_int(IPV6CTL_FORWARDING, &enabled) == FALSE) {
	return (FALSE);
    }
    return (enabled != 0);
}

PRIVATE_EXTERN int
inet6_ifstat(const char * if_name, struct in6_ifstat * stat)
{
    struct in6_ifreq 	ifr;
    int 		ret;
    int 		s;

    bzero(&ifr, sizeof(ifr));
    s = inet6_dgram_socket();
    if (s < 0) {
	ret = errno;
	my_log(LOG_ERR, "socket(%s) failed, %s", if_name, strerror(errno));
	goto done;
    }
    strncpy(ifr.ifr_name, if_name, sizeof(ifr.ifr_name));
    if (ioctl(s, SIOCGIFSTAT_IN6, (char *)&ifr) < 0) {
	ret = errno;
	my_log(LOG_ERR, "SIOCGIFSTAT_IN6(%s) failed, %s",
	       if_name, strerror(errno));
	goto done;
    }
    ret = 0;

 done:
    *stat = ifr.ifr_ifru.ifru_stat;
    if (s >= 0) {
	close(s);
    }
    return (ret);
}

STATIC int
inet6_clat46_start_stop(const char * if_name, boolean_t start)
{
    int		ret = 0;
    int		s;

    s = inet6_dgram_socket();
    if (s < 0) {
	ret = errno;
	my_log(LOG_ERR, "socket(%s) failed, %s", if_name, strerror(errno));
	goto done;
    }
    if (start) {
	my_log(LOG_INFO, "ioctl(%s, SIOCCLAT46_START)", if_name);
	if (siocclat46_start(s, if_name) < 0) {
	    my_log(LOG_INFO, "ioctl(%s, SIOCCLAT46_START), failed, %s (%d)",
		   if_name, strerror(errno), errno);
	    ret = errno;
	}
    }
    else {
	my_log(LOG_INFO, "ioctl(%s, SIOCCLAT46_STOP)", if_name);
	if (siocclat46_stop(s, if_name) < 0) {
	    my_log(LOG_INFO, "ioctl(%s, SIOCCLAT46_STOP), failed, %s (%d)",
		   if_name, strerror(errno), errno);
	    ret = errno;
	}
    }
    close(s);

 done:
    return (ret);
}

PRIVATE_EXTERN int
inet6_clat46_start(const char * if_name)
{
    return (inet6_clat46_start_stop(if_name, TRUE));
}

PRIVATE_EXTERN int
inet6_clat46_stop(const char * if_name)
{
    return (inet6_clat46_start_stop(if_name, FALSE));
}

/**
 ** inet6_addrlist_*
 **/

STATIC char *
copy_if_info(int if_index, int af, int * ret_len_p)
{
    char *			buf = NULL;
    size_t			buf_len = 0;
    int				mib[6];

    mib[0] = CTL_NET;
    mib[1] = PF_ROUTE;
    mib[2] = 0;
    mib[3] = af;
    mib[4] = NET_RT_IFLIST;
    mib[5] = if_index;

    *ret_len_p = 0;
    if (sysctl(mib, 6, NULL, &buf_len, NULL, 0) < 0) {
	fprintf(stderr, "sysctl() size failed: %s", strerror(errno));
	goto failed;
    }
    buf_len *= 2; /* just in case something changes */
    buf = malloc(buf_len);
    if (sysctl(mib, 6, buf, &buf_len, NULL, 0) < 0) {
	free(buf);
	buf = NULL;
	fprintf(stderr, "sysctl() failed: %s", strerror(errno));
	goto failed;
    }
    *ret_len_p = (int)buf_len;

 failed:
    return (buf);
}

PRIVATE_EXTERN boolean_t
inet6_get_linklocal_address(int if_index, struct in6_addr * ret_addr)
{
    char *			buf = NULL;
    char *			buf_end;
    int				buf_len;
    boolean_t			found = FALSE;
    char *			scan;
    struct rt_msghdr *		rtm;

    bzero(ret_addr, sizeof(*ret_addr));
    buf = copy_if_info(if_index, AF_INET6, &buf_len);
    if (buf == NULL) {
	goto done;
    }
    buf_end = buf + buf_len;
    for (scan = buf; scan < buf_end; scan += rtm->rtm_msglen) {
	struct ifa_msghdr *	ifam;
	struct rt_addrinfo	info;

	/* ALIGN: buf aligned (from calling copy_if_info), scan aligned,
	 * cast ok. */
	rtm = (struct rt_msghdr *)(void *)scan;
	if (rtm->rtm_version != RTM_VERSION) {
	    continue;
	}
	if (rtm->rtm_type == RTM_NEWADDR) {
	    errno_t			error;
	    struct sockaddr_in6 *	sin6_p;

	    ifam = (struct ifa_msghdr *)rtm;
	    info.rti_addrs = ifam->ifam_addrs;
	    error = rt_xaddrs((char *)(ifam + 1),
			      ((char *)ifam) + ifam->ifam_msglen,
			      &info);
	    if (error) {
		fprintf(stderr, "couldn't extract rt_addrinfo %s (%d)\n",
			strerror(error), error);
		goto done;
	    }
	    /* ALIGN: info.rti_info aligned (sockaddr), cast ok. */
	    sin6_p = (struct sockaddr_in6 *)(void *)info.rti_info[RTAX_IFA];
	    if (sin6_p == NULL
		|| sin6_p->sin6_len < sizeof(struct sockaddr_in6)) {
		continue;
	    }
	    if (IN6_IS_ADDR_LINKLOCAL(&sin6_p->sin6_addr)) {
		*ret_addr = sin6_p->sin6_addr;
		ret_addr->s6_addr16[1] = 0; /* mask scope id */
		found = TRUE;
		break;
	    }
	}
    }

 done:
    if (buf != NULL) {
	free(buf);
    }
    return (found);
}

PRIVATE_EXTERN void
inet6_addrlist_copy(inet6_addrlist_t * addr_list_p, int if_index)
{
    int				addr_index = 0;
    char *			buf = NULL;
    char *			buf_end;
    int				buf_len;
    int				count;
    int				error;
    int				i;
    char			ifname[IFNAMSIZ + 1];
    inet6_addrinfo_t *		linklocal = NULL;
    inet6_addrinfo_t *		list = NULL;
    char *			scan;
    struct rt_msghdr *		rtm;
    int				s = -1;

    buf = copy_if_info(if_index, AF_INET6, &buf_len);
    if (buf == NULL) {
	goto done;
    }
    buf_end = buf + buf_len;

    /* figure out how many IPv6 addresses there are */
    count = 0;
    ifname[0] = '\0';
    for (scan = buf; scan < buf_end; scan += rtm->rtm_msglen) {
	struct if_msghdr * 	ifm;
	
	/* ALIGN: buf aligned (from calling copy_if_info), scan aligned,
	 * cast ok. */
	rtm = (struct rt_msghdr *)(void *)scan;
	if (rtm->rtm_version != RTM_VERSION) {
	    continue;
	}
	switch (rtm->rtm_type) {
	case RTM_IFINFO:
	    ifm = (struct if_msghdr *)rtm;
	    if (ifm->ifm_addrs & RTA_IFP) {
		struct sockaddr_dl *	dl_p;
		
		dl_p = (struct sockaddr_dl *)(ifm + 1);
		if (dl_p->sdl_nlen == 0 
		    || dl_p->sdl_nlen >= sizeof(ifname)) {
		    goto done;
		}
		bcopy(dl_p->sdl_data, ifname, dl_p->sdl_nlen);
		ifname[dl_p->sdl_nlen] = '\0';
	    }
	    break;
	case RTM_NEWADDR:
	    count++;
	    break;
	default:
	    break;
	}
    }
    if (ifname[0] == '\0') {
	goto done;
    }
    if (count == 0) {
	goto done;
    }
    if (count > INET6_ADDRLIST_N_STATIC) {
	list = (inet6_addrinfo_t *)malloc(sizeof(*list) * count);
	if (list == NULL) {
	    goto done;
	}
    }
    else {
	list = addr_list_p->list_static;
    }
    for (scan = buf; scan < buf_end; scan += rtm->rtm_msglen) {
	boolean_t		got_address = FALSE;
	struct ifa_msghdr *	ifam;
	struct rt_addrinfo	info;

	rtm = (struct rt_msghdr *)(void *)scan;
	if (rtm->rtm_version != RTM_VERSION) {
	    continue;
	}
	if (rtm->rtm_type == RTM_NEWADDR) {
	    ifam = (struct ifa_msghdr *)rtm;
	    info.rti_addrs = ifam->ifam_addrs;
	    error = rt_xaddrs((char *)(ifam + 1),
			      ((char *)ifam) + ifam->ifam_msglen,
			      &info);
	    if (error) {
		fprintf(stderr, "couldn't extract rt_addrinfo %s (%d)\n",
			strerror(error), error);
		goto done;
	    }
	    for (i = 0; i < RTAX_MAX; i++) {
		struct sockaddr_in6 *	sin6_p;
		
		/* ALIGN: info.rti_info aligned (sockaddr), cast ok. */
		sin6_p = (struct sockaddr_in6 *)(void *)info.rti_info[i];
		if (sin6_p == NULL
		    || sin6_p->sin6_len < sizeof(struct sockaddr_in6)) {
		    continue;
		}
		switch (i) {
		case RTAX_NETMASK:
		    list[addr_index].prefix_length 
			= count_prefix_bits(&sin6_p->sin6_addr,
					    sizeof(sin6_p->sin6_addr));
		    break;
		case RTAX_IFA:
		    list[addr_index].addr = sin6_p->sin6_addr;
		    got_address = TRUE;
		    break;
		default:
		    break;
		}
	    }
	    if (got_address) {
		if (s < 0) {
		    s = inet6_dgram_socket();
		}
		if (s >= 0) {
		    siocgifaflag_in6(s, ifname, 
				     &list[addr_index].addr,
				     &list[addr_index].addr_flags);
		    siocgifalifetime_in6(s, ifname, 
					 &list[addr_index].addr,
					 &list[addr_index].valid_lifetime,
					 &list[addr_index].preferred_lifetime);
		}
		/* Mask the v6 LL scope id */
		if (IN6_IS_ADDR_LINKLOCAL(&list[addr_index].addr)) {
		    list[addr_index].addr.s6_addr16[1] = 0;
		    if (linklocal == NULL) {
			linklocal = &list[addr_index];
		    }
		}
		addr_index++;
	    }
	}
    }
    if (addr_index == 0) {
	if (list != addr_list_p->list_static) {
	    free(list);
	}
	list = NULL;
    }

 done:
    if (s >= 0) {
	close(s);
    }
    if (buf != NULL) {
	free(buf);
    }
    addr_list_p->list = list;
    addr_list_p->count = addr_index;
    addr_list_p->linklocal = linklocal;
    return;
}

STATIC void
lifetime_to_str(u_int32_t t, char * buf, size_t buf_size)
{
    if (t == -1) {
	strlcpy(buf, "infinity", buf_size);
    }
    else {
	snprintf(buf, buf_size, "%u", t);
    }
    return;
}

PRIVATE_EXTERN CFStringRef
inet6_addrlist_copy_description(const inet6_addrlist_t * addr_list_p)
{
    int				i;
    inet6_addrinfo_t *		scan;
    CFMutableStringRef		str;

    str = CFStringCreateMutable(NULL, 0);
    STRING_APPEND(str, "{");
    for (i = 0, scan = addr_list_p->list; i < addr_list_p->count; i++, scan++) {
	char 	ntopbuf[INET6_ADDRSTRLEN];
	char	pltime_str[32];
	char	vltime_str[32];

	lifetime_to_str(scan->valid_lifetime,
			vltime_str, sizeof(vltime_str));
	lifetime_to_str(scan->preferred_lifetime,
			pltime_str, sizeof(pltime_str));
	STRING_APPEND(str, "%s%s/%d flags 0x%04x vltime=%s pltime=%s\n",
		      i == 0 ? "\n" : "",
		      inet_ntop(AF_INET6, &scan->addr,
				ntopbuf, sizeof(ntopbuf)),
		      scan->prefix_length,
		      scan->addr_flags,
		      vltime_str,
		      pltime_str);
    }
    STRING_APPEND(str, "}");
    return (str);
}

PRIVATE_EXTERN void
inet6_addrlist_print(const inet6_addrlist_t * addr_list_p)
{
    CFStringRef		str;

    str = inet6_addrlist_copy_description(addr_list_p);
    SCPrint(TRUE, stdout, CFSTR("%@\n"), str);
    CFRelease(str);
    return;
}

PRIVATE_EXTERN void
inet6_addrlist_free(inet6_addrlist_t * addr_list_p)
{
    if (addr_list_p->list == NULL) {
	return;
    }
    if (addr_list_p->list != addr_list_p->list_static) {
	free(addr_list_p->list);
    }
    inet6_addrlist_init(addr_list_p);
    return;
}

PRIVATE_EXTERN void
inet6_addrlist_init(inet6_addrlist_t * addr_list_p)
{
    addr_list_p->list = NULL;
    addr_list_p->count = 0;
    addr_list_p->linklocal = NULL;
    return;
}

PRIVATE_EXTERN boolean_t
inet6_addrlist_in6_addr_is_ready(const inet6_addrlist_t * addr_list_p,
				 const struct in6_addr * addr)
{
    int			i;
    inet6_addrinfo_t *	scan;

#define NOT_READY	(IN6_IFF_NOTREADY | IN6_IFF_OPTIMISTIC)
    for (i = 0, scan = addr_list_p->list; i < addr_list_p->count; i++, scan++) {
	if (IN6_ARE_ADDR_EQUAL(&scan->addr, addr)) {
	    return ((scan->addr_flags & NOT_READY) == 0);
	}
    }
    return (FALSE);
}

PRIVATE_EXTERN boolean_t
inet6_addrlist_contains_address(const inet6_addrlist_t * addr_list_p,
				const inet6_addrinfo_t * addr)
{
    int			i;
    inet6_addrinfo_t *	scan;

    for (i = 0, scan = addr_list_p->list; i < addr_list_p->count; i++, scan++) {
	if (IN6_ARE_ADDR_EQUAL(&scan->addr, &addr->addr)
	    && (scan->prefix_length == addr->prefix_length)) {
	    return (TRUE);
	}
    }
    return (FALSE);
}

PRIVATE_EXTERN inet6_addrinfo_t *
inet6_addrlist_get_linklocal(const inet6_addrlist_t * addr_list_p)
{
    if (addr_list_p != NULL) {
	return (addr_list_p->linklocal);
    }
    return (NULL);
}

/**
 ** Test Harnesses
 **/

#if TEST_INET6_ADDRLIST || TEST_IPV6_LL
#include <stdio.h>
#include <stdlib.h>
#include "util.h"

PRIVATE_EXTERN Boolean G_IPConfiguration_verbose = 1;

STATIC bool S_cga_enabled;

PRIVATE_EXTERN bool 
CGAIsEnabled(void)
{
    return (S_cga_enabled);
}

PRIVATE_EXTERN void
CGAPrepareSetForInterface(const char * name, struct in6_cga_prepare * cga_prep)
{
    if (S_cga_enabled == FALSE) {
	return;
    }
    cga_prep->cga_security_level = 0;
    fill_with_random(cga_prep->cga_modifier.octets, 
		     sizeof(cga_prep->cga_modifier.octets));
    return;
}

#endif /* TEST_INET6_ADDRLIST || TEST_IPV6_LL */

#if TEST_INET6_ADDRLIST
int
main(int argc, char * argv[])
{
    inet6_addrlist_t 	addresses;
    int			if_index;

    if (argc < 2) {
	fprintf(stderr, "you must specify the interface\n");
	exit(1);
    }
    if_index = if_nametoindex(argv[1]);
    if (if_index == 0) {
	fprintf(stderr, "No such interface '%s'\n", argv[1]);
	exit(2);
    }
    inet6_addrlist_copy(&addresses, if_index);
    inet6_addrlist_print(&addresses);
    inet6_addrlist_free(&addresses);
    exit(0);
    return(0);
}
#endif /* TEST_INET6_ADDRLIST */

#if TEST_IPV6_LL
STATIC void
usage()
{
    fprintf(stderr, "usage: ipv6ll start | stop <ifname> [ <cga> ]\n");
    exit(1);
}

int
main(int argc, char * argv[])
{
    int			is_start = 0;

    if (argc < 3) {
	usage();
    }
    if (strcasecmp(argv[1], "start") == 0) {
	is_start = 1;
    }
    else if (strcasecmp(argv[1], "stop") == 0) {
    }
    else {
	usage();
    }

    if (is_start) {
	S_cga_enabled = (argc > 3);
	if (inet6_linklocal_start(argv[2], TRUE) != 0) {
	    exit(1);
	}
	if (inet6_rtadv_enable(argv[2]) != 0) {
	    exit(1);
	}
    }
    else {
	inet6_rtadv_disable(argv[2]);
	if (inet6_linklocal_stop(argv[2]) != 0) {
	    exit(1);
	}
    }
    exit(0);
    return (0);
    
}

#endif /* TEST_IPV6_LL */

#if TEST_IPV6_LINKLOCAL_ADDRESS
boolean_t G_is_netboot;

int
main(int argc, char * argv[])
{
    struct in6_addr	addr;
    int			if_index;
    char 		ntopbuf[INET6_ADDRSTRLEN];

    if (argc < 2) {
	fprintf(stderr, "you must specify the interface\n");
	exit(1);
    }
    if_index = if_nametoindex(argv[1]);
    if (if_index == 0) {
	fprintf(stderr, "No such interface '%s'\n", argv[1]);
	exit(2);
    }
    if (!inet6_get_linklocal_address(if_index, &addr)) {
	fprintf(stderr, "Interface '%s' has no linklocal address\n",
		argv[1]);
	exit(2);
    }
    printf("%s\n", inet_ntop(AF_INET6, &addr, ntopbuf, sizeof(ntopbuf)));
    if (argc > 2) {
	printf("my pid is %d\n", getpid());
	sleep(60);
    }
    exit(0);
    return(0);
}
#endif /* TEST_IPV6_LINKLOCAL_ADDRESS */

#if TEST_IPV6_ROUTER_PREFIX_COUNT
boolean_t G_is_netboot;

int
main(int argc, char * argv[])
{
    int			if_index;
    int			prefix_count = 0;
    int			router_count = 0;

    if (argc < 2) {
	fprintf(stderr, "you must specify the interface\n");
	exit(1);
    }
    if_index = if_nametoindex(argv[1]);
    if (if_index == 0) {
	fprintf(stderr, "No such interface '%s'\n", argv[1]);
	exit(2);
    }

    router_count = inet6_router_and_prefix_count(if_index, &prefix_count);
    printf("%s prefixes %d routers %d\n", argv[1], prefix_count, router_count);
    if (argc > 2) {
	printf("my pid is %d\n", getpid());
	sleep(60);
    }
    exit(0);
    return(0);
}
#endif /* TEST_IPV6_ROUTER_PREFIX_COUNT */

#if TEST_PROTOLIST
#ifndef PF_BRIDGE
#define	PF_BRIDGE	((uint32_t)0x62726467)	/* 'brdg' */
#endif

#define PF_EAPOL	((uint32_t)0x8021ec)

STATIC const char *
get_protocol_name(u_int32_t proto)
{
    const char *	str = NULL;

    switch (proto) {
    case PF_INET:
	str = "INET";
	break;
    case PF_INET6:
	str = "INET6";
	break;
    case PF_BRIDGE:
	str = "BRIDGE";
	break;
    case PF_VLAN:
	str = "VLAN";
	break;
    case PF_BOND:
	str = "BOND";
	break;
    case PF_EAPOL:
	str = "EAPOL";
	break;
    case PF_NDRV:
	str = "NDRV";
	break;
    default:
	break;
    }
    return (str);
}

STATIC void
show_protocols(u_int32_t * list, u_int32_t count)
{
    printf("Protocol count %d:\n", count);
    for (u_int32_t i = 0; i < count; i++) {
	union {
	    u_int32_t	i;
	    char	c[5];
	} bytes;
	u_int32_t	proto;
	const char *	proto_name;

	proto = list[i];
	proto_name = get_protocol_name(proto);
	if (proto_name == NULL) {
	    bytes.i = htonl(proto);
	    bytes.c[4] = '\0';
	    proto_name = bytes.c;
	    for (u_int32_t j = 0; j < 4; j++) {
		if (!isprint(bytes.c[j])) {
		    bytes.c[j] = '?';
		}
	    }
	}
	printf("%d. %u (0x%x) (%s)\n", i, proto, proto, proto_name);
    }
}

boolean_t G_is_netboot;

int
main(int argc, char * argv[])
{
    const char *	ifname;
    u_int32_t *		protolist;
    u_int32_t		protolist_count;

    if (argc < 2) {
	fprintf(stderr, "you must specify the interface\n");
	exit(1);
    }
    ifname = argv[1];
    protolist = protolist_copy(ifname, &protolist_count);
    if (protolist != NULL) {
	show_protocols(protolist, protolist_count);
	free(protolist);
    }
    else {
	printf("No protocols attached\n");
    }
    printf("IPv6 is %sattached to %s\n",
	   inet6_is_attached(ifname) ? "" : "not ", ifname);
    exit(0);
    return (0);
}

#endif /* TEST_PROTOLIST */
