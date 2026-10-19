/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2010 Bjoern A. Zeeb <bz@FreeBSD.org>
 * Copyright (c) 1980, 1986, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/epoch.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/ethernet.h>
#include <net/if_vlan_var.h>

/* API for driver access to network stack owned ifnet.*/
uint64_t
if_setbaudrate(struct ifnet *ifp, uint64_t baudrate)
{
	uint64_t oldbrate;

	oldbrate = ifp->if_baudrate;
	ifp->if_baudrate = baudrate;
	return (oldbrate);
}

uint64_t
if_getbaudrate(const if_t ifp)
{
	return (ifp->if_baudrate);
}

int
if_setcapabilities(if_t ifp, int capabilities)
{
	ifp->if_capabilities = capabilities;
	return (0);
}

int
if_setcapabilitiesbit(if_t ifp, int setbit, int clearbit)
{
	ifp->if_capabilities &= ~clearbit;
	ifp->if_capabilities |= setbit;
	return (0);
}

int
if_getcapabilities(const if_t ifp)
{
	return (ifp->if_capabilities);
}

int
if_setcapenable(if_t ifp, int capabilities)
{
	ifp->if_capenable = capabilities;
	return (0);
}

int
if_setcapenablebit(if_t ifp, int setcap, int clearcap)
{
	ifp->if_capenable &= ~clearcap;
	ifp->if_capenable |= setcap;
	return (0);
}

#if 0
int
if_setcapabilities2(if_t ifp, int capabilities)
{
	ifp->if_capabilities2 = capabilities;
	return (0);
}

int
if_setcapabilities2bit(if_t ifp, int setbit, int clearbit)
{
	ifp->if_capabilities2 &= ~clearbit;
	ifp->if_capabilities2 |= setbit;
	return (0);
}

int
if_getcapabilities2(const if_t ifp)
{
	return (ifp->if_capabilities2);
}

int
if_setcapenable2(if_t ifp, int capabilities2)
{
	ifp->if_capenable2 = capabilities2;
	return (0);
}

int
if_setcapenable2bit(if_t ifp, int setcap, int clearcap)
{
	ifp->if_capenable2 &= ~clearcap;
	ifp->if_capenable2 |= setcap;
	return (0);
}
#endif

const char *
if_getdname(const if_t ifp)
{
	return (ifp->if_dname);
}

void
if_setdname(if_t ifp, const char *dname)
{
	ifp->if_dname = dname;
}

const char *
if_name(if_t ifp)
{
	return (ifp->if_xname);
}

int
if_setname(if_t ifp, const char *name)
{
	if (strlen(name) > sizeof(ifp->if_xname) - 1)
		return (ENAMETOOLONG);
	strcpy(ifp->if_xname, name);

	return (0);
}

int
if_togglecapenable(if_t ifp, int togglecap)
{
	ifp->if_capenable ^= togglecap;
	return (0);
}

int
if_getcapenable(const if_t ifp)
{
	return (ifp->if_capenable);
}

#if 0
int
if_togglecapenable2(if_t ifp, int togglecap)
{
	ifp->if_capenable2 ^= togglecap;
	return (0);
}

int
if_getcapenable2(const if_t ifp)
{
	return (ifp->if_capenable2);
}
#endif

int
if_getdunit(const if_t ifp)
{
	return (ifp->if_dunit);
}

int
if_getindex(const if_t ifp)
{
	return (ifp->if_index);
}

#if 0
int
if_getidxgen(const if_t ifp)
{
	return (ifp->if_idxgen);
}

const char *
if_getdescr(if_t ifp)
{
	return (ifp->if_description);
}

void
if_setdescr(if_t ifp, char *descrbuf)
{
	sx_xlock(&ifdescr_sx);
	char *odescrbuf = ifp->if_description;
	ifp->if_description = descrbuf;
	sx_xunlock(&ifdescr_sx);

	if_freedescr(odescrbuf);
}

char *
if_allocdescr(size_t sz, int malloc_flag)
{
	malloc_flag &= (M_WAITOK | M_NOWAIT);
	return (malloc(sz, M_IFDESCR, M_ZERO | malloc_flag));
}

void
if_freedescr(char *descrbuf)
{
	free(descrbuf, M_IFDESCR);
}

int
if_getalloctype(const if_t ifp)
{
	return (ifp->if_alloctype);
}

void
if_setlastchange(if_t ifp)
{
	getmicrotime(&ifp->if_lastchange);
}
#endif

/*
 * This is largely undesirable because it ties ifnet to a device, but does
 * provide flexiblity for an embedded product vendor. Should be used with
 * the understanding that it violates the interface boundaries, and should be
 * a last resort only.
 */
int
if_setdev(if_t ifp, void *dev)
{
	return (0);
}

int
if_setdrvflagbits(if_t ifp, int set_flags, int clear_flags)
{
	ifp->if_drv_flags &= ~clear_flags;
	ifp->if_drv_flags |= set_flags;

	return (0);
}

int
if_getdrvflags(const if_t ifp)
{
	return (ifp->if_drv_flags);
}

int
if_setdrvflags(if_t ifp, int flags)
{
	ifp->if_drv_flags = flags;
	return (0);
}

int
if_setflags(if_t ifp, int flags)
{
	ifp->if_flags = flags;
	return (0);
}

int
if_setflagbits(if_t ifp, int set, int clear)
{
	ifp->if_flags &= ~clear;
	ifp->if_flags |= set;
	return (0);
}

int
if_getflags(const if_t ifp)
{
	return (ifp->if_flags);
}

int
if_clearhwassist(if_t ifp)
{
	ifp->if_hwassist = 0;
	return (0);
}

int
if_sethwassistbits(if_t ifp, int toset, int toclear)
{
	ifp->if_hwassist &= ~toclear;
	ifp->if_hwassist |= toset;

	return (0);
}

int
if_sethwassist(if_t ifp, int hwassist_bit)
{
	ifp->if_hwassist = hwassist_bit;
	return (0);
}

int
if_gethwassist(const if_t ifp)
{
	return (ifp->if_hwassist);
}

int
if_togglehwassist(if_t ifp, int toggle_bits)
{
	ifp->if_hwassist ^= toggle_bits;
	return (0);
}

int
if_setmtu(if_t ifp, int mtu)
{
	ifp->if_mtu = mtu;
	return (0);
}

#if 0
void
if_notifymtu(if_t ifp)
{
#ifdef INET6
	nd6_setmtu(ifp);
#endif
	rt_updatemtu(ifp);
}
#endif

int
if_getmtu(const if_t ifp)
{
	return (ifp->if_mtu);
}

#if 0
int
if_getmtu_family(const if_t ifp, int family)
{
	struct domain *dp;

	SLIST_FOREACH(dp, &domains, dom_next) {
		if (dp->dom_family == family && dp->dom_ifmtu != NULL)
			return (dp->dom_ifmtu(ifp));
	}

	return (ifp->if_mtu);
}
#endif

/*
 * Methods for drivers to access interface unicast and multicast
 * link level addresses.  Driver shall not know 'struct ifaddr' neither
 * 'struct ifmultiaddr'.
 */
u_int
if_lladdr_count(if_t ifp)
{
	struct epoch_tracker et;
	struct ifaddr *ifa;
	u_int count;

	count = 0;
	NET_EPOCH_ENTER(et);
	TAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link)
		if (ifa->ifa_addr->sa_family == AF_LINK)
			count++;
	NET_EPOCH_EXIT(et);

	return (count);
}

#if 0
int
if_foreach(if_foreach_cb_t cb, void *cb_arg)
{
	if_t ifp;
	int error;

	NET_EPOCH_ASSERT();
	MPASS(cb);

	error = 0;
	TAILQ_FOREACH(ifp, &V_ifnet, if_link) {
		error = cb(ifp, cb_arg);
		if (error != 0)
			break;
	}

	return (error);
}

/*
 * Iterates over the list of interfaces, permitting callback function @cb to sleep.
 * Stops iteration if @cb returns non-zero error code.
 * Returns the last error code from @cb.
 * @match_cb: optional match callback limiting the iteration to only matched interfaces
 * @match_arg: argument to pass to @match_cb
 * @cb: iteration callback
 * @cb_arg: argument to pass to @cb
 */
int
if_foreach_sleep(if_foreach_match_t match_cb, void *match_arg, if_foreach_cb_t cb,
	void *cb_arg)
{
	int match_count = 0, array_size = 16; /* 128 bytes for malloc */
	struct ifnet **match_array = NULL;
	int error = 0;

	MPASS(cb);

	while (true) {
		struct ifnet **new_array;
		int new_size = array_size;
		struct epoch_tracker et;
		struct ifnet *ifp;

		while (new_size < match_count)
			new_size *= 2;
		new_array = malloc(new_size * sizeof(void *), M_TEMP, M_WAITOK);
		if (match_array != NULL)
			memcpy(new_array, match_array, array_size * sizeof(void *));
		free(match_array, M_TEMP);
		match_array = new_array;
		array_size = new_size;

		match_count = 0;
		NET_EPOCH_ENTER(et);
		CK_STAILQ_FOREACH(ifp, &V_ifnet, if_link) {
			if (match_cb != NULL && !match_cb(ifp, match_arg))
				continue;
			if (match_count < array_size) {
				if (if_try_ref(ifp))
					match_array[match_count++] = ifp;
			} else
				match_count++;
		}
		NET_EPOCH_EXIT(et);

		if (match_count > array_size) {
			for (int i = 0; i < array_size; i++)
				if_rele(match_array[i]);
			continue;
		} else {
			for (int i = 0; i < match_count; i++) {
				if (error == 0)
					error = cb(match_array[i], cb_arg);
				if_rele(match_array[i]);
			}
			free(match_array, M_TEMP);
			break;
		}
	}

	return (error);
}


/*
 * Uses just 1 pointer of the 4 available in the public struct.
 */
if_t
if_iter_start(struct if_iter *iter)
{
	if_t ifp;

	NET_EPOCH_ASSERT();

	bzero(iter, sizeof(*iter));
	ifp = CK_STAILQ_FIRST(&V_ifnet);
	if (ifp != NULL)
		iter->context[0] = CK_STAILQ_NEXT(ifp, if_link);
	else
		iter->context[0] = NULL;
	return (ifp);
}

if_t
if_iter_next(struct if_iter *iter)
{
	if_t cur_ifp = iter->context[0];

	if (cur_ifp != NULL)
		iter->context[0] = CK_STAILQ_NEXT(cur_ifp, if_link);
	return (cur_ifp);
}

void
if_iter_finish(struct if_iter *iter)
{
	/* Nothing to do here for now. */
}
#endif

u_int
if_foreach_lladdr(if_t ifp, iflladdr_cb_t cb, void *cb_arg)
{
	struct epoch_tracker et;
	struct ifaddr *ifa;
	u_int count;

	MPASS(cb);

	count = 0;
	NET_EPOCH_ENTER(et);
	TAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
		if (ifa->ifa_addr->sa_family != AF_LINK)
			continue;
		count += (*cb)(cb_arg, (struct sockaddr_dl *)ifa->ifa_addr,
			count);
	}
	NET_EPOCH_EXIT(et);

	return (count);
}

u_int
if_llmaddr_count(if_t ifp)
{
	struct epoch_tracker et;
	struct ifmultiaddr *ifma;
	int count;

	count = 0;
	NET_EPOCH_ENTER(et);
	TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link)
		if (ifma->ifma_addr->sa_family == AF_LINK)
			count++;
	NET_EPOCH_EXIT(et);

	return (count);
}

bool
if_maddr_empty(if_t ifp)
{

	return (TAILQ_EMPTY(&ifp->if_multiaddrs));
}

u_int
if_foreach_llmaddr(if_t ifp, iflladdr_cb_t cb, void *cb_arg)
{
	struct epoch_tracker et;
	struct ifmultiaddr *ifma;
	u_int count;

	MPASS(cb);

	count = 0;
	NET_EPOCH_ENTER(et);
	TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		count += (*cb)(cb_arg, (struct sockaddr_dl *)ifma->ifma_addr,
			count);
	}
	NET_EPOCH_EXIT(et);

	return (count);
}

#if 0
u_int
if_foreach_addr_type(if_t ifp, int type, if_addr_cb_t cb, void *cb_arg)
{
	struct epoch_tracker et;
	struct ifaddr *ifa;
	u_int count;

	MPASS(cb);

	count = 0;
	NET_EPOCH_ENTER(et);
	CK_STAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
		if (ifa->ifa_addr->sa_family != type)
			continue;
		count += (*cb)(cb_arg, ifa, count);
	}
	NET_EPOCH_EXIT(et);

	return (count);
}

struct ifaddr *
ifa_iter_start(if_t ifp, struct ifa_iter *iter)
{
	struct ifaddr *ifa;

	NET_EPOCH_ASSERT();

	bzero(iter, sizeof(*iter));
	ifa = CK_STAILQ_FIRST(&ifp->if_addrhead);
	if (ifa != NULL)
		iter->context[0] = CK_STAILQ_NEXT(ifa, ifa_link);
	else
		iter->context[0] = NULL;
	return (ifa);
}

struct ifaddr *
ifa_iter_next(struct ifa_iter *iter)
{
	struct ifaddr *ifa = iter->context[0];

	if (ifa != NULL)
		iter->context[0] = CK_STAILQ_NEXT(ifa, ifa_link);
	return (ifa);
}

void
ifa_iter_finish(struct ifa_iter *iter)
{
	/* Nothing to do here for now. */
}
#endif

int
if_setsoftc(if_t ifp, void *softc)
{
	ifp->if_softc = softc;
	return (0);
}

void *
if_getsoftc(const if_t ifp)
{
	return (ifp->if_softc);
}

void
if_setrcvif(struct mbuf *m, if_t ifp)
{
#if 0
	MPASS((m->m_pkthdr.csum_flags & CSUM_SND_TAG) == 0);
#endif
	m->m_pkthdr.rcvif = (struct ifnet *)ifp;
}

void
if_setvtag(struct mbuf *m, uint16_t tag)
{
	m->m_pkthdr.ether_vtag = tag;
}

uint16_t
if_getvtag(struct mbuf *m)
{
	return (m->m_pkthdr.ether_vtag);
}

int
if_sendq_empty(if_t ifp)
{
	return (IFQ_DRV_IS_EMPTY(&ifp->if_snd));
}

struct ifaddr *
if_getifaddr(const if_t ifp)
{
	return (ifp->if_addr);
}

int
if_getamcount(const if_t ifp)
{
	return (ifp->if_amcount);
}

int
if_setsendqready(if_t ifp)
{
	IFQ_SET_READY(&ifp->if_snd);
	return (0);
}

int
if_setsendqlen(if_t ifp, int tx_desc_count)
{
	IFQ_SET_MAXLEN(&ifp->if_snd, tx_desc_count);
	ifp->if_snd.ifq_drv_maxlen = tx_desc_count;
	return (0);
}

#if 0
void
if_setnetmapadapter(if_t ifp, struct netmap_adapter *na)
{
	ifp->if_netmap = na;
}

struct netmap_adapter *
if_getnetmapadapter(if_t ifp)
{
	return (ifp->if_netmap);
}
#endif

int
if_vlantrunkinuse(if_t ifp)
{
	return (ifp->if_vlantrunk != NULL);
}

void
if_init(if_t ifp, void *ctx)
{
	(*ifp->if_init)(ctx);
}

void
if_input(if_t ifp, struct mbuf* sendmp)
{
	(*ifp->if_input)(ifp, sendmp);
}

int
if_transmit(if_t ifp, struct mbuf *m)
{
	return ((*ifp->if_transmit)(ifp, m));
}

int
if_resolvemulti(if_t ifp, struct sockaddr **srcs, struct sockaddr *dst)
{
	if (ifp->if_resolvemulti == NULL)
		return (EOPNOTSUPP);

	return (ifp->if_resolvemulti(ifp, srcs, dst));
}

int
if_ioctl(if_t ifp, u_long cmd, void *data)
{
	if (ifp->if_ioctl == NULL)
		return (EOPNOTSUPP);

	return (ifp->if_ioctl(ifp, cmd, (caddr_t)data));
}

struct mbuf *
if_dequeue(if_t ifp)
{
	struct mbuf *m;

	IFQ_DRV_DEQUEUE(&ifp->if_snd, m);
	return (m);
}

int
if_sendq_prepend(if_t ifp, struct mbuf *m)
{
	IFQ_DRV_PREPEND(&ifp->if_snd, m);
	return (0);
}

int
if_setifheaderlen(if_t ifp, int len)
{
	ifp->if_hdrlen = len;
	return (0);
}

caddr_t
if_getlladdr(const if_t ifp)
{
	return (IF_LLADDR(ifp));
}

void *
if_gethandle(u_char type)
{
	return (if_alloc(type));
}

void
if_vlancap(if_t ifp)
{
	VLAN_CAPABILITIES(ifp);
}

int
if_sethwtsomax(if_t ifp, u_int if_hw_tsomax)
{
	ifp->if_hw_tsomax = if_hw_tsomax;
		return (0);
}

int
if_sethwtsomaxsegcount(if_t ifp, u_int if_hw_tsomaxsegcount)
{
	ifp->if_hw_tsomaxsegcount = if_hw_tsomaxsegcount;
		return (0);
}

int
if_sethwtsomaxsegsize(if_t ifp, u_int if_hw_tsomaxsegsize)
{
	ifp->if_hw_tsomaxsegsize = if_hw_tsomaxsegsize;
		return (0);
}

u_int
if_gethwtsomax(const if_t ifp)
{
	return (ifp->if_hw_tsomax);
}

u_int
if_gethwtsomaxsegcount(const if_t ifp)
{
	return (ifp->if_hw_tsomaxsegcount);
}

u_int
if_gethwtsomaxsegsize(const if_t ifp)
{
	return (ifp->if_hw_tsomaxsegsize);
}

void
if_setinitfn(if_t ifp, if_init_fn_t init_fn)
{
	ifp->if_init = init_fn;
}

#if 0
void
if_setinputfn(if_t ifp, if_input_fn_t input_fn)
{
	ifp->if_input = input_fn;
}

if_input_fn_t
if_getinputfn(if_t ifp)
{
	return (ifp->if_input);
}
#endif

void
if_setioctlfn(if_t ifp, if_ioctl_fn_t ioctl_fn)
{
	ifp->if_ioctl = ioctl_fn;
}

#if 0
void
if_setoutputfn(if_t ifp, if_output_fn_t output_fn)
{
	ifp->if_output = output_fn;
}
#endif

void
if_setstartfn(if_t ifp, if_start_fn_t start_fn)
{
	ifp->if_start = start_fn;
}

if_start_fn_t
if_getstartfn(if_t ifp)
{
	return (ifp->if_start);
}

void
if_settransmitfn(if_t ifp, if_transmit_fn_t start_fn)
{
	ifp->if_transmit = start_fn;
}

if_transmit_fn_t
if_gettransmitfn(if_t ifp)
{
	return (ifp->if_transmit);
}

void
if_setqflushfn(if_t ifp, if_qflush_fn_t flush_fn)
{
	ifp->if_qflush = flush_fn;
}

#if 0
void
if_setsndtagallocfn(if_t ifp, if_snd_tag_alloc_t alloc_fn)
{
	ifp->if_snd_tag_alloc = alloc_fn;
}

int
if_snd_tag_alloc(if_t ifp, union if_snd_tag_alloc_params *params,
	struct m_snd_tag **mstp)
{
	if (ifp->if_snd_tag_alloc == NULL)
		return (EOPNOTSUPP);
	return (ifp->if_snd_tag_alloc(ifp, params, mstp));
}
#endif

void
if_setgetcounterfn(if_t ifp, if_get_counter_t fn)
{
	ifp->if_get_counter = fn;
}

#if 0
void
if_setreassignfn(if_t ifp, if_reassign_fn_t fn)
{
	ifp->if_reassign = fn;
}

void
if_setratelimitqueryfn(if_t ifp, if_ratelimit_query_t fn)
{
	ifp->if_ratelimit_query = fn;
}

void
if_setdebugnet_methods(if_t ifp, struct debugnet_methods *m)
{
	ifp->if_debugnet_methods = m;
}
#endif

struct label *
if_getmaclabel(if_t ifp)
{
	return (ifp->if_label);
}

void
if_setmaclabel(if_t ifp, struct label *label)
{
	ifp->if_label = label;
}

int
if_gettype(if_t ifp)
{
	return (ifp->if_type);
}

#if 0
void *
if_getllsoftc(if_t ifp)
{
	return (ifp->if_llsoftc);
}

void
if_setllsoftc(if_t ifp, void *llsoftc)
{
	ifp->if_llsoftc = llsoftc;
}
#endif

int
if_getlinkstate(if_t ifp)
{
	return (ifp->if_link_state);
}

const uint8_t *
if_getbroadcastaddr(if_t ifp)
{
	return (ifp->if_broadcastaddr);
}

void
if_setbroadcastaddr(if_t ifp, const uint8_t *addr)
{
	ifp->if_broadcastaddr = addr;
}

#if 0
int
if_getnumadomain(if_t ifp)
{
	return (ifp->if_numa_domain);
}
#endif

uint64_t
if_getcounter(if_t ifp, ift_counter counter)
{
	return (ifp->if_get_counter(ifp, counter));
}

bool
if_altq_is_enabled(if_t ifp)
{
	return (ALTQ_IS_ENABLED(&ifp->if_snd));
}

#if 0
struct vnet *
if_getvnet(if_t ifp)
{
	return (ifp->if_vnet);
}
#endif

void *
if_getafdata(if_t ifp, int af)
{
	return (ifp->if_afdata[af]);
}

#if 0
u_int
if_getfib(if_t ifp)
{
	return (ifp->if_fib);
}
#endif

uint8_t
if_getaddrlen(if_t ifp)
{
	return (ifp->if_addrlen);
}

struct bpf_if *
if_getbpf(if_t ifp)
{
	return (ifp->if_bpf);
}

struct ifvlantrunk *
if_getvlantrunk(if_t ifp)
{
	return (ifp->if_vlantrunk);
}

#if 0
uint8_t
if_getpcp(if_t ifp)
{
	return (ifp->if_pcp);
}
#endif

void *
if_getl2com(if_t ifp)
{
	return (ifp->if_l2com);
}
