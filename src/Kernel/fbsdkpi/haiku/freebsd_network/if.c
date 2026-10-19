/*
 * Copyright 2009, Colin Günther, coling@gmx.de.
 * Copyright 2007-2009, Axel Dörfler, axeld@pinc-software.de.
 * Copyright 2007, Hugo Santos. All Rights Reserved.
 * Copyright 2004, Marcus Overhagen. All Rights Reserved.
 *
 * Distributed under the terms of the MIT License.
 */


#include "device.h"

#include <stdio.h>
#include <net/if_types.h>
#include <sys/sockio.h>

#include <compat/sys/bus.h>
#include <compat/sys/kernel.h>
#include <compat/sys/taskqueue.h>

#include <compat/net/bpf.h>
#include <compat/net/ethernet.h>
#include <compat/net/if.h>
#include <compat/net/if_arp.h>
#include <compat/net/if_media.h>
#include <compat/net/if_var.h>
#include <compat/net/if_vlan_var.h>
#include <compat/sys/malloc.h>



int ifqmaxlen = IFQ_MAXLEN;

static void	if_input_default(struct ifnet *, struct mbuf *);
static int	if_requestencap_default(struct ifnet *, struct if_encap_req *);



#define IFNET_HOLD (void *)(uintptr_t)(-1)


static void
insert_into_device_name_list(struct ifnet * ifp)
{
	int i;
	for (i = 0; i < MAX_DEVICES; i++) {
		if (gDeviceNameList[i] == NULL) {
			gDeviceNameList[i] = ifp->device_name;
			return;
		}
	}

	panic("too many devices");
}


static void
remove_from_device_name_list(struct ifnet * ifp)
{
	int i;
	for (i = 0; i < MAX_DEVICES; i++) {
		if (ifp->device_name == gDeviceNameList[i]) {
			int last;
			for (last = i + 1; last < MAX_DEVICES; last++) {
				if (gDeviceNameList[last] == NULL)
					break;
			}
			last--;

			if (i == last)
				gDeviceNameList[i] = NULL;
			else {
				// switch positions with the last entry
				gDeviceNameList[i] = gDeviceNameList[last];
				gDeviceNameList[last] = NULL;
			}
			break;
		}
	}
}


static struct ifnet *
ifnet_byindex_locked(u_int idx)
{
	struct ifnet *ifp;

	ifp = gDevices[idx];

	return (ifp);
}


if_t
ifnet_byindex(u_int idx)
{
	struct ifnet *ifp;

	IFNET_RLOCK_NOSLEEP();
	ifp = ifnet_byindex_locked(idx);
	IFNET_RUNLOCK_NOSLEEP();

	return (ifp);
}


static void
ifnet_setbyindex_locked(u_short idx, struct ifnet *ifp)
{
	gDevices[idx] = ifp;
}


static void
ifnet_setbyindex(u_short idx, struct ifnet *ifp)
{
	IFNET_WLOCK();
	ifnet_setbyindex_locked(idx, ifp);
	IFNET_WUNLOCK();
}


static int
ifindex_alloc_locked(u_short *idxp)
{
	u_short index;

	for (index = 0; index < MAX_DEVICES; index++) {
		if (gDevices[index] == NULL) {
			break;
		}
	}

	if (index == MAX_DEVICES)
		return ENOSPC;

	gDeviceCount++;
	*idxp = index;

	return ENOERR;
}


static void
ifindex_free_locked(u_short idx)
{
	gDevices[idx] = NULL;
	gDeviceCount--;
}


int
if_alloc_inplace(struct ifnet *ifp, u_char type)
{
	char semName[64];
	u_short index;

	snprintf(semName, sizeof(semName), "%s receive", gDriverName);

	ifp->receive_sem = create_sem(0, semName);
	if (ifp->receive_sem < B_OK)
		return ifp->receive_sem;

	ifp->link_state_sem = -1;
	ifp->open_count = 0;
	ifp->flags = 0;
	ifp->if_type = type;
	ifq_init(&ifp->receive_queue, semName);

	// Search for the first free device slot, and use that one
	IFNET_WLOCK();
	if (ifindex_alloc_locked(&index) != ENOERR) {
		IFNET_WUNLOCK();
		panic("too many devices");
		goto err2;
	}
	ifnet_setbyindex_locked(index, IFNET_HOLD);
	IFNET_WUNLOCK();

	ifp->if_index = index;
	ifnet_setbyindex(ifp->if_index, ifp);

	IF_ADDR_LOCK_INIT(ifp);
	return 0;

err2:
	delete_sem(ifp->receive_sem);

	return -1;
}


struct ifnet *
if_alloc(u_char type)
{
	struct ifnet *ifp = _kernel_malloc(sizeof(struct ifnet), M_ZERO);
	if (ifp == NULL)
		return NULL;

	if (if_alloc_inplace(ifp, type) != 0) {
		_kernel_free(ifp);
		return NULL;
	}

	return ifp;
}


void
if_free_inplace(struct ifnet *ifp)
{
	// IEEE80211 devices won't be in this list,
	// so don't try to remove them.
	if (ifp->if_type == IFT_ETHER)
		remove_from_device_name_list(ifp);

	IFNET_WLOCK();
	ifindex_free_locked(ifp->if_index);
	IFNET_WUNLOCK();

	IF_ADDR_LOCK_DESTROY(ifp);

	delete_sem(ifp->receive_sem);
	ifq_uninit(&ifp->receive_queue);
}


void
if_free(struct ifnet *ifp)
{
	if_free_inplace(ifp);

	_kernel_free(ifp);
}


void
if_initname(struct ifnet *ifp, const char *name, int unit)
{
	dprintf("if_initname(%p, %s, %d)\n", ifp, name, unit);

	if (name == NULL || name[0] == '\0')
		panic("interface goes unnamed");

	ifp->if_dname = name;
	ifp->if_dunit = unit;

	strlcpy(ifp->if_xname, name, sizeof(ifp->if_xname));

	snprintf(ifp->device_name, sizeof(ifp->device_name), "net/%s/%i",
		gDriverName, ifp->if_index);

	driver_printf("%s: /dev/%s\n", gDriverName, ifp->device_name);
	insert_into_device_name_list(ifp);

	ifp->root_device = find_root_device(unit);
}


void
ifq_init(struct ifqueue *ifq, const char *name)
{
	ifq->ifq_head = NULL;
	ifq->ifq_tail = NULL;
	ifq->ifq_len = 0;
	ifq->ifq_maxlen = IFQ_MAXLEN;
	ifq->ifq_drops = 0;

	mtx_init(&ifq->ifq_mtx, name, NULL, MTX_DEF);
}


void
ifq_uninit(struct ifqueue *ifq)
{
	mtx_destroy(&ifq->ifq_mtx);
}


static int
if_transmit_default(struct ifnet *ifp, struct mbuf *m)
{
	int error;

	IFQ_HANDOFF(ifp, m, error);
	return (error);
}


static void
if_input_default(struct ifnet *ifp __unused, struct mbuf *m)
{

	m_freem(m);
}


/*
 * Flush an interface queue.
 */
void
if_qflush(struct ifnet *ifp)
{
	struct mbuf *m, *n;
	struct ifaltq *ifq;

	ifq = &ifp->if_snd;
	IFQ_LOCK(ifq);
#ifdef ALTQ
	if (ALTQ_IS_ENABLED(ifq))
		ALTQ_PURGE(ifq);
#endif
	n = ifq->ifq_head;
	while ((m = n) != NULL) {
		n = m->m_nextpkt;
		m_freem(m);
	}
	ifq->ifq_head = 0;
	ifq->ifq_tail = 0;
	ifq->ifq_len = 0;
	IFQ_UNLOCK(ifq);
}


void
if_attach(struct ifnet *ifp)
{
	unsigned socksize, ifasize;
	int namelen, masklen;
	struct sockaddr_dl *sdl;
	struct ifaddr *ifa;

	TAILQ_INIT(&ifp->if_addrhead);
	TAILQ_INIT(&ifp->if_prefixhead);
	TAILQ_INIT(&ifp->if_multiaddrs);

	IF_ADDR_LOCK_INIT(ifp);

	ifp->if_lladdr.sdl_family = AF_LINK;

	ifq_init((struct ifqueue *) &ifp->if_snd, ifp->if_xname);

	if (ifp->if_transmit == NULL) {
		ifp->if_transmit = if_transmit_default;
		ifp->if_qflush = if_qflush;
	}
	if (ifp->if_input == NULL)
		ifp->if_input = if_input_default;

	if (ifp->if_requestencap == NULL)
		ifp->if_requestencap = if_requestencap_default;

	/*
	 * Create a Link Level name for this device.
	 */
	namelen = strlen(ifp->if_xname);
	/*
	 * Always save enough space for any possiable name so we
	 * can do a rename in place later.
	 */
	masklen = offsetof(struct sockaddr_dl, sdl_data[0]) + IFNAMSIZ;
	socksize = masklen + ifp->if_addrlen;
	if (socksize < sizeof(*sdl))
		socksize = sizeof(*sdl);
	socksize = roundup2(socksize, sizeof(long));
	ifasize = sizeof(*ifa) + 2 * socksize;
	ifa = ifa_alloc(ifasize, M_WAITOK);
	sdl = (struct sockaddr_dl *)(ifa + 1);
	sdl->sdl_len = socksize;
	sdl->sdl_family = AF_LINK;
	bcopy(ifp->if_xname, sdl->sdl_data, namelen);
	sdl->sdl_nlen = namelen;
	sdl->sdl_index = ifp->if_index;
	sdl->sdl_type = ifp->if_type;
	ifp->if_addr = ifa;
	ifa->ifa_ifp = ifp;
	//ifa->ifa_rtrequest = link_rtrequest;
	ifa->ifa_addr = (struct sockaddr *)sdl;
	sdl = (struct sockaddr_dl *)(socksize + (caddr_t)sdl);
	ifa->ifa_netmask = (struct sockaddr *)sdl;
	sdl->sdl_len = masklen;
	while (namelen != 0)
		sdl->sdl_data[--namelen] = 0xff;
	dprintf("if_attach %p\n", ifa->ifa_addr);
}


void
if_detach(struct ifnet *ifp)
{
	if (HAIKU_DRIVER_REQUIRES(FBSD_SWI_TASKQUEUE))
		taskqueue_drain(taskqueue_swi, &ifp->if_linktask);

	IF_ADDR_LOCK_DESTROY(ifp);
	ifq_uninit((struct ifqueue *) &ifp->if_snd);
}


void
if_start(struct ifnet *ifp)
{
	ifp->if_start(ifp);
}


int
if_printf(struct ifnet *ifp, const char *format, ...)
{
	char buf[256];
	va_list vl;
	va_start(vl, format);
	vsnprintf(buf, sizeof(buf), format, vl);
	va_end(vl);

	dprintf("[%s] %s", ifp->device_name, buf);
	return 0;
}


/*
 * Compat function for handling basic encapsulation requests.
 * Not converted stacks (FDDI, IB, ..) supports traditional
 * output model: ARP (and other similar L2 protocols) are handled
 * inside output routine, arpresolve/nd6_resolve() returns MAC
 * address instead of full prepend.
 *
 * This function creates calculated header==MAC for IPv4/IPv6 and
 * returns EAFNOSUPPORT (which is then handled in ARP code) for other
 * address families.
 */
static int
if_requestencap_default(struct ifnet *ifp, struct if_encap_req *req)
{

	if (req->rtype != IFENCAP_LL)
		return (EOPNOTSUPP);

	if (req->bufsize < req->lladdr_len)
		return (ENOMEM);

	switch (req->family) {
	case AF_INET:
	case AF_INET6:
		break;
	default:
		return (EAFNOSUPPORT);
	}

	/* Copy lladdr to storage as is */
	memmove(req->buf, req->lladdr, req->lladdr_len);
	req->bufsize = req->lladdr_len;
	req->lladdr_off = 0;

	return (0);
}


#ifdef __PUREDARWIN__
void pd_netif_input(struct ifnet *ifp, struct mbuf *m);
void pd_netif_link(struct ifnet *ifp, int linkState);
#endif


void
if_link_state_change(struct ifnet *ifp, int linkState)
{
	if (ifp->if_link_state == linkState)
		return;

	ifp->if_link_state = linkState;
#ifdef __PUREDARWIN__
	pd_netif_link(ifp, linkState);
#endif
	release_sem_etc(ifp->link_state_sem, 1, B_DO_NOT_RESCHEDULE);
}

static struct ifmultiaddr *
if_findmulti(struct ifnet *ifp, struct sockaddr *_address)
{
	struct sockaddr_dl *address = (struct sockaddr_dl *) _address;
	struct ifmultiaddr *ifma;

	TAILQ_FOREACH (ifma, &ifp->if_multiaddrs, ifma_link) {
		if (memcmp(LLADDR(address),
			LLADDR((struct sockaddr_dl *)ifma->ifma_addr), ETHER_ADDR_LEN) == 0)
			return ifma;
	}

	return NULL;
}


/*
 * if_freemulti: free ifmultiaddr structure and possibly attached related
 * addresses.  The caller is responsible for implementing reference
 * counting, notifying the driver, handling routing messages, and releasing
 * any dependent link layer state.
 */
static void
if_freemulti(struct ifmultiaddr *ifma)
{

	KASSERT(ifma->ifma_refcount == 0, ("if_freemulti: refcount %d",
	    ifma->ifma_refcount));
	KASSERT(ifma->ifma_protospec == NULL,
	    ("if_freemulti: protospec not NULL"));

	if (ifma->ifma_lladdr != NULL)
		free(ifma->ifma_lladdr);

	// Haiku note: We use a field in the ifmultiaddr struct (ifma_addr_storage)
	// to store the address and let ifma_addr point to that. We therefore do not
	// free it here, as it will be freed as part of freeing the if_multiaddr.
	//free(ifma->ifma_addr);

	free(ifma);
}


static struct ifmultiaddr *
_if_addmulti(struct ifnet *ifp, struct sockaddr *address)
{
	struct ifmultiaddr *addr = if_findmulti(ifp, address);

	if (addr != NULL) {
		addr->ifma_refcount++;
		return addr;
	}

	addr = (struct ifmultiaddr *) malloc(sizeof(struct ifmultiaddr));
	if (addr == NULL)
		return NULL;

	addr->ifma_lladdr = NULL;
	addr->ifma_ifp = ifp;
	addr->ifma_protospec = NULL;

	memcpy(&addr->ifma_addr_storage, address, sizeof(struct sockaddr_dl));
	addr->ifma_addr = (struct sockaddr *) &addr->ifma_addr_storage;

	addr->ifma_refcount = 1;

	TAILQ_INSERT_HEAD(&ifp->if_multiaddrs, addr, ifma_link);

	return addr;
}


int
if_addmulti(struct ifnet *ifp, struct sockaddr *address,
	struct ifmultiaddr **out)
{
	struct ifmultiaddr *result;
	int refcount = 0;

	IF_ADDR_LOCK(ifp);
	result = _if_addmulti(ifp, address);
	if (result)
		refcount = result->ifma_refcount;
	IF_ADDR_UNLOCK(ifp);

	if (result == NULL)
		return ENOBUFS;

	if (refcount == 1 && ifp->if_ioctl != NULL)
		ifp->if_ioctl(ifp, SIOCADDMULTI, NULL);

	if (out)
		(*out) = result;

	return 0;
}


static int
if_delmulti_locked(struct ifnet *ifp, struct ifmultiaddr *ifma, int detaching)
{
	struct ifmultiaddr *ll_ifma;

	if (ifp != NULL && ifma->ifma_ifp != NULL) {
		KASSERT(ifma->ifma_ifp == ifp,
		    ("%s: inconsistent ifp %p", __func__, ifp));
		IF_ADDR_LOCK_ASSERT(ifp);
	}

	ifp = ifma->ifma_ifp;

	/*
	 * If the ifnet is detaching, null out references to ifnet,
	 * so that upper protocol layers will notice, and not attempt
	 * to obtain locks for an ifnet which no longer exists. The
	 * routing socket announcement must happen before the ifnet
	 * instance is detached from the system.
	 */
	if (detaching) {
#ifdef DIAGNOSTIC
		printf("%s: detaching ifnet instance %p\n", __func__, ifp);
#endif
		/*
		 * ifp may already be nulled out if we are being reentered
		 * to delete the ll_ifma.
		 */
		if (ifp != NULL) {
#ifndef __HAIKU__
			rt_newmaddrmsg(RTM_DELMADDR, ifma);
#endif
			ifma->ifma_ifp = NULL;
		}
	}

	if (--ifma->ifma_refcount > 0)
		return 0;

#ifndef __HAIKU__
	/*
	 * If this ifma is a network-layer ifma, a link-layer ifma may
	 * have been associated with it. Release it first if so.
	 */
	ll_ifma = ifma->ifma_llifma;
	if (ll_ifma != NULL) {
		KASSERT(ifma->ifma_lladdr != NULL,
		    ("%s: llifma w/o lladdr", __func__));
		if (detaching)
			ll_ifma->ifma_ifp = NULL;	/* XXX */
		if (--ll_ifma->ifma_refcount == 0) {
			if (ifp != NULL) {
				TAILQ_REMOVE(&ifp->if_multiaddrs, ll_ifma,
				    ifma_link);
			}
			if_freemulti(ll_ifma);
		}
	}
#endif

	if (ifp != NULL)
		TAILQ_REMOVE(&ifp->if_multiaddrs, ifma, ifma_link);

	if_freemulti(ifma);

	/*
	 * The last reference to this instance of struct ifmultiaddr
	 * was released; the hardware should be notified of this change.
	 */
	return 1;
}


/*
 * Delete all multicast group membership for an interface.
 * Should be used to quickly flush all multicast filters.
 */
void
if_delallmulti(struct ifnet *ifp)
{
	struct ifmultiaddr *ifma;
	struct ifmultiaddr *next;

	IF_ADDR_LOCK(ifp);
	TAILQ_FOREACH_SAFE(ifma, &ifp->if_multiaddrs, ifma_link, next)
		if_delmulti_locked(ifp, ifma, 0);
	IF_ADDR_UNLOCK(ifp);
}


static void
if_delete_multiaddr(struct ifnet *ifp, struct ifmultiaddr *ifma)
{
	TAILQ_REMOVE(&ifp->if_multiaddrs, ifma, ifma_link);
	free(ifma);
}


int
if_delmulti(struct ifnet *ifp, struct sockaddr *sa)
{
	struct ifmultiaddr *ifma;
	int lastref;
#if 0 /* def INVARIANTS */
	struct ifnet *oifp;

	IFNET_RLOCK_NOSLEEP();
	TAILQ_FOREACH(oifp, &V_ifnet, if_link)
		if (ifp == oifp)
			break;
	if (ifp != oifp)
		ifp = NULL;
	IFNET_RUNLOCK_NOSLEEP();

	KASSERT(ifp != NULL, ("%s: ifnet went away", __func__));
#endif
	if (ifp == NULL)
		return (ENOENT);

	IF_ADDR_LOCK(ifp);
	lastref = 0;
	ifma = if_findmulti(ifp, sa);
	if (ifma != NULL)
		lastref = if_delmulti_locked(ifp, ifma, 0);
	IF_ADDR_UNLOCK(ifp);

	if (ifma == NULL)
		return (ENOENT);

	if (lastref && ifp->if_ioctl != NULL) {
		(void)(*ifp->if_ioctl)(ifp, SIOCDELMULTI, 0);
	}

	return (0);
}


/*
 * Return counter values from counter(9)s stored in ifnet.
 */
uint64_t
if_get_counter_default(struct ifnet *ifp, ift_counter cnt)
{

	KASSERT(cnt < IFCOUNTERS, ("%s: invalid cnt %d", __func__, cnt));

	switch (cnt) {
		case IFCOUNTER_IPACKETS:
			return atomic_get64((int64 *)&ifp->if_ipackets);
		case IFCOUNTER_IERRORS:
			return atomic_get64((int64 *)&ifp->if_ierrors);
		case IFCOUNTER_OPACKETS:
			return atomic_get64((int64 *)&ifp->if_opackets);
		case IFCOUNTER_OERRORS:
			return atomic_get64((int64 *)&ifp->if_oerrors);
		case IFCOUNTER_COLLISIONS:
			return atomic_get64((int64 *)&ifp->if_collisions);
		case IFCOUNTER_IBYTES:
			return atomic_get64((int64 *)&ifp->if_ibytes);
		case IFCOUNTER_OBYTES:
			return atomic_get64((int64 *)&ifp->if_obytes);
		case IFCOUNTER_IMCASTS:
			return atomic_get64((int64 *)&ifp->if_imcasts);
		case IFCOUNTER_OMCASTS:
			return atomic_get64((int64 *)&ifp->if_omcasts);
		case IFCOUNTER_IQDROPS:
			return atomic_get64((int64 *)&ifp->if_iqdrops);
		case IFCOUNTER_OQDROPS:
			return atomic_get64((int64 *)&ifp->if_oqdrops);
		case IFCOUNTER_NOPROTO:
			return atomic_get64((int64 *)&ifp->if_noproto);
		case IFCOUNTERS:
			KASSERT(cnt < IFCOUNTERS, ("%s: invalid cnt %d", __func__, cnt));
	}
	return 0;
}

void
if_addr_rlock(struct ifnet *ifp)
{
	IF_ADDR_LOCK(ifp);
}


void
if_addr_runlock(struct ifnet *ifp)
{
	IF_ADDR_UNLOCK(ifp);
}


void
if_maddr_rlock(struct ifnet *ifp)
{
	IF_ADDR_LOCK(ifp);
}


void
if_maddr_runlock(struct ifnet *ifp)
{
	IF_ADDR_UNLOCK(ifp);
}


int
ether_output(struct ifnet *ifp, struct mbuf *m, struct sockaddr *dst,
	struct route *ro)
{
	return ifp->if_transmit(ifp, m);
}


static void
ether_input(struct ifnet *ifp, struct mbuf *m)
{
	int32 count = 0;

#ifdef __PUREDARWIN__
	// received frames go straight to the iokit controller
	pd_netif_input(ifp, m);
	return;
#endif

	IF_LOCK(&ifp->receive_queue);
	while (m != NULL) {
		struct mbuf *mn = m->m_nextpkt;
		m->m_nextpkt = NULL;

		_IF_ENQUEUE(&ifp->receive_queue, m);
		count++;

		m = mn;
	}
	IF_UNLOCK(&ifp->receive_queue);

	release_sem_etc(ifp->receive_sem, count, B_DO_NOT_RESCHEDULE);
}


void
ether_ifattach(struct ifnet *ifp, const uint8_t *lla)
{
	struct ifaddr *ifa;
	struct sockaddr_dl *sdl;

	ifp->if_addrlen = ETHER_ADDR_LEN;
	ifp->if_hdrlen = ETHER_HDR_LEN;
	if_attach(ifp);
	ifp->if_mtu = ETHERMTU;
	ifp->if_output = ether_output;
	ifp->if_input = ether_input;
	ifp->if_resolvemulti = NULL; // done in the stack
	ifp->if_get_counter = NULL;
	ifp->if_broadcastaddr = etherbroadcastaddr;

	ifa = ifp->if_addr;
	sdl = (struct sockaddr_dl *)ifa->ifa_addr;
	sdl->sdl_type = IFT_ETHER;
	sdl->sdl_alen = ifp->if_addrlen;
	bcopy(lla, LLADDR(sdl), ifp->if_addrlen);
}


void
ether_ifdetach(struct ifnet *ifp)
{
	if_detach(ifp);
}


int
ether_ioctl(struct ifnet *ifp, u_long command, caddr_t data)
{
	struct ifreq *ifr = (struct ifreq *) data;

	//dprintf("ether_ioctl: received %d\n", command);

	switch (command) {
		case SIOCSIFMTU:
			if (ifr->ifr_mtu < ETHERMIN || ifr->ifr_mtu > ETHERMTU)
				return EINVAL;
			ifp->if_mtu = ifr->ifr_mtu;
			break;

		default:
			return EINVAL;
	}

	return 0;
}


/*
 * Initialization, destruction and refcounting functions for ifaddrs.
 */
struct ifaddr *
ifa_alloc(size_t size, int flags)
{
	struct ifaddr *ifa;

	KASSERT(size >= sizeof(struct ifaddr),
	    ("%s: invalid size %zu", __func__, size));

	ifa = _kernel_malloc(size, M_ZERO | flags);
	if (ifa == NULL)
		return (NULL);

	//refcount_init(&ifa->ifa_refcnt, 1);

	return (ifa);

fail:
	/* free(NULL) is okay */
	free(ifa);

	return (NULL);
}

void
if_inc_counter(struct ifnet *ifp, ift_counter cnt, int64_t inc)
{
	switch (cnt) {
		case IFCOUNTER_IPACKETS:
			atomic_add64((int64 *)&ifp->if_ipackets, inc);
			break;
		case IFCOUNTER_IERRORS:
			atomic_add64((int64 *)&ifp->if_ierrors, inc);
			break;
		case IFCOUNTER_OPACKETS:
			atomic_add64((int64 *)&ifp->if_opackets, inc);
			break;
		case IFCOUNTER_OERRORS:
			atomic_add64((int64 *)&ifp->if_oerrors, inc);
			break;
		case IFCOUNTER_COLLISIONS:
			atomic_add64((int64 *)&ifp->if_collisions, inc);
			break;
		case IFCOUNTER_IBYTES:
			atomic_add64((int64 *)&ifp->if_ibytes, inc);
			break;
		case IFCOUNTER_OBYTES:
			atomic_add64((int64 *)&ifp->if_obytes, inc);
			break;
		case IFCOUNTER_IMCASTS:
			atomic_add64((int64 *)&ifp->if_imcasts, inc);
			break;
		case IFCOUNTER_OMCASTS:
			atomic_add64((int64 *)&ifp->if_omcasts, inc);
			break;
		case IFCOUNTER_IQDROPS:
			atomic_add64((int64 *)&ifp->if_iqdrops, inc);
			break;
		case IFCOUNTER_OQDROPS:
			atomic_add64((int64 *)&ifp->if_oqdrops, inc);
			break;
		case IFCOUNTER_NOPROTO:
			atomic_add64((int64 *)&ifp->if_noproto, inc);
			break;
		case IFCOUNTERS:
			KASSERT(cnt < IFCOUNTERS, ("%s: invalid cnt %d", __func__, cnt));
	}
}

void
if_bpfmtap(if_t ifh, struct mbuf *m)
{
	struct ifnet *ifp = (struct ifnet *)ifh;

	BPF_MTAP(ifp, m);
}

void
if_etherbpfmtap(if_t ifh, struct mbuf *m)
{
	struct ifnet *ifp = (struct ifnet *)ifh;

	ETHER_BPF_MTAP(ifp, m);
}
