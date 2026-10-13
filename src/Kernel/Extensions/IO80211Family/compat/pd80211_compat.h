/*
 * OpenBSD kernel interfaces that net80211 and its drivers use, built on the
 * xnu kext KPIs. Every net80211 entry point runs under one recursive lock,
 * the way OpenBSD runs it under the kernel lock and splnet.
 */
#ifndef PD80211_COMPAT_H
#define PD80211_COMPAT_H

#include <stdint.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/queue.h>
#include <sys/time.h>
#include <sys/kpi_mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/ioccom.h>
#include <sys/random.h>
#include <kern/clock.h>
#include <libkern/OSByteOrder.h>
#include <libkern/OSAtomic.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include "pd_tree.h"

// the openbsd headers keep their kernel parts under _KERNEL
#define _KERNEL 1

#ifdef __cplusplus
extern "C" {
#endif

// types and attributes

typedef uint64_t	uint64_t_compat;
#ifndef __packed
#define __packed	__attribute__((__packed__))
#endif
#ifndef __aligned
#define __aligned(x)	__attribute__((__aligned__(x)))
#endif
#ifndef __unused
#define __unused	__attribute__((__unused__))
#endif
#ifndef __predict_false
#define __predict_false(x)	__builtin_expect(!!(x), 0)
#define __predict_true(x)	__builtin_expect(!!(x), 1)
#endif
#ifndef nitems
#define nitems(a)	(sizeof(a) / sizeof((a)[0]))
#endif
#ifndef howmany
#define howmany(x, y)	(((x) + ((y) - 1)) / (y))
#endif
#ifndef roundup
#define roundup(x, y)	((((x) + ((y) - 1)) / (y)) * (y))
#endif
#ifndef DEVNAME
#define DEVNAME(sc)	((sc)->sc_dev.dv_xname)
#endif

#define KASSERT(e)	((void)0)
#define CTASSERT(e)	_Static_assert(e, #e)
#define MUTEX_ASSERT_LOCKED(m)	((void)0)
#define NET_ASSERT_LOCKED()	((void)0)
#define KERNEL_ASSERT_LOCKED()	((void)0)
#define splassert(x)	((void)0)
#define DPRINTF(x)	do { } while (0)
#define DPRINTFN(n, x)	do { } while (0)

// byte order

#define htole16(x)	OSSwapHostToLittleInt16(x)
#define htole32(x)	OSSwapHostToLittleInt32(x)
#define htole64(x)	OSSwapHostToLittleInt64(x)
#define letoh16(x)	OSSwapLittleToHostInt16(x)
#define letoh32(x)	OSSwapLittleToHostInt32(x)
#define letoh64(x)	OSSwapLittleToHostInt64(x)
#ifndef htobe16
#define htobe16(x)	OSSwapHostToBigInt16(x)
#define htobe32(x)	OSSwapHostToBigInt32(x)
#define htobe64(x)	OSSwapHostToBigInt64(x)
#endif
#define betoh16(x)	OSSwapBigToHostInt16(x)
#define betoh32(x)	OSSwapBigToHostInt32(x)
#define betoh64(x)	OSSwapBigToHostInt64(x)
#define swap16(x)	OSSwapInt16(x)
#define swap32(x)	OSSwapInt32(x)
#define swap64(x)	OSSwapInt64(x)

// the one lock

void	pd80211_lock(void);
void	pd80211_unlock(void);
int	pd80211_locked(void);
#define IPL_NET		0
#define IPL_SOFTNET	0
#define IPL_USB		0
static inline int splnet(void) { pd80211_lock(); return 0; }
static inline int splsoftnet(void) { pd80211_lock(); return 0; }
static inline int splusb(void) { pd80211_lock(); return 0; }
static inline void splx(int s) { (void)s; pd80211_unlock(); }

struct mutex { int dummy; };
#define mtx_init(m, ipl)	((void)(m))
#define mtx_enter(m)		pd80211_lock()
#define mtx_leave(m)		pd80211_unlock()
struct rwlock { int dummy; };
#define rw_init(l, n)		((void)(l))
#define rw_enter_write(l)	pd80211_lock()
#define rw_exit_write(l)	pd80211_unlock()

// memory

#define M_DEVBUF	2
#define M_TEMP		80
#define M_USBDEV	102
#define M_WAITOK	0x0001
#define M_NOWAIT	0x0002
#define M_ZERO		0x0008
#define M_CANFAIL	0x0004
void	*pd80211_malloc(size_t, int, int);
void	pd80211_free(void *, int, size_t);
void	*pd80211_mallocarray(size_t, size_t, int, int);
#define malloc(s, t, f)		pd80211_malloc((s), (t), (f))
#define mallocarray(n, s, t, f)	pd80211_mallocarray((n), (s), (t), (f))
#define free(p, t, s)		pd80211_free((p), (t), (s))

// time

extern int hz;
int	pd80211_ticks(void);
#define ticks	pd80211_ticks()
void	pd80211_init(void);
void	getmicrouptime(struct timeval *);
#define microuptime_compat(tv)	getmicrouptime(tv)
uint64_t	getnsecuptime(void);
time_t	pd80211_uptime(void);
#define time_uptime	pd80211_uptime()
#define SEC_TO_NSEC(s)	((uint64_t)(s) * 1000000000ULL)
#define MSEC_TO_NSEC(m)	((uint64_t)(m) * 1000000ULL)
#define USEC_TO_NSEC(u)	((uint64_t)(u) * 1000ULL)
#define DELAY(us)	delay(us)
void	delay(int);
int	ratecheck(struct timeval *, const struct timeval *);
#ifndef timevalcmp
#define timevalcmp(tvp, uvp, cmp) \
	(((tvp)->tv_sec == (uvp)->tv_sec) ? \
	    ((tvp)->tv_usec cmp (uvp)->tv_usec) : \
	    ((tvp)->tv_sec cmp (uvp)->tv_sec))
#endif
#ifndef timevalsub
#define timevalsub(tvp, uvp, vvp) do { \
	(vvp)->tv_sec = (tvp)->tv_sec - (uvp)->tv_sec; \
	(vvp)->tv_usec = (tvp)->tv_usec - (uvp)->tv_usec; \
	if ((vvp)->tv_usec < 0) { (vvp)->tv_sec--; (vvp)->tv_usec += 1000000; } \
} while (0)
#endif

// timeouts and tasks run on one thread under the lock

struct timeout {
	TAILQ_ENTRY(timeout) to_list;
	void		(*to_func)(void *);
	void		*to_arg;
	uint64_t	to_deadline;
	int		to_flags;
};
#define TIMEOUT_ONQUEUE		0x01
#define TIMEOUT_INITIALIZED	0x04
void	timeout_set(struct timeout *, void (*)(void *), void *);
#define timeout_set_proc(to, f, a)	timeout_set((to), (f), (a))
int	timeout_add(struct timeout *, int);
int	timeout_add_sec(struct timeout *, int);
int	timeout_add_msec(struct timeout *, int);
int	timeout_add_usec(struct timeout *, int);
int	timeout_del(struct timeout *);
int	timeout_del_barrier(struct timeout *);
#define timeout_pending(to)	((to)->to_flags & TIMEOUT_ONQUEUE)
#define timeout_initialized(to)	((to)->to_flags & TIMEOUT_INITIALIZED)

struct task {
	TAILQ_ENTRY(task) t_entry;
	void		(*t_func)(void *);
	void		*t_arg;
	unsigned int	t_flags;
};
#define TASK_ONQUEUE	1
struct taskq;
extern struct taskq *const systq;
extern struct taskq *const systqmp;
#define net_tq(i)	systq
void	task_set(struct task *, void (*)(void *), void *);
int	task_add(struct taskq *, struct task *);
int	task_del(struct taskq *, struct task *);
#define task_pending(t)	((t)->t_flags & TASK_ONQUEUE)
#define TASK_INITIALIZER(f, a)	{ { NULL, NULL }, (f), (a), 0 }

// sleeping on a channel, used by usb sync helpers
int	tsleep_nsec(const volatile void *, int, const char *, uint64_t);
// wakeup is xnu's, it wakes the event tsleep_nsec sleeps on
#define INFSLP	UINT64_MAX
#define PCATCH	0x100

// random

uint32_t	arc4random(void);
uint32_t	arc4random_uniform(uint32_t);
void	arc4random_buf(void *, size_t);

// logging

#define LOG_ERR		3
#define LOG_WARNING	4
#define LOG_NOTICE	5
#define LOG_INFO	6
#define LOG_DEBUG	7
#define log(level, ...)	printf(__VA_ARGS__)
#define addlog(...)	printf(__VA_ARGS__)

// mbufs, through the xnu kpi

#define M_PKTHDR	MBUF_PKTHDR
#define M_EXT		MBUF_EXT
#define M_EOR		MBUF_EOR
#define M_BCAST		MBUF_BCAST
#define M_MCAST		MBUF_MCAST
#define M_DONTWAIT	MBUF_DONTWAIT
#define M_WAIT		MBUF_WAITOK
#define MT_DATA		MBUF_TYPE_DATA
#define MT_HEADER	MBUF_TYPE_HEADER
#define M_VLANTAG	0
#define MCLBYTES	2048
#define MHLEN		((int)mbuf_get_mhlen())
#define MLEN		((int)mbuf_get_mlen())
#define MINCLSIZE	((int)mbuf_get_minclsize())
#define MAXMCLBYTES	(64 * 1024)

#define mtod(m, t)	((t)mbuf_data(m))
#define M_LEN(m)	((int)mbuf_len(m))
#define M_PKTLEN(m)	((int)mbuf_pkthdr_len(m))
#define M_NEXT(m)	mbuf_next(m)
#define M_FLAGS(m)	mbuf_flags(m)
#define M_SETLEN(m, l)	mbuf_setlen((m), (size_t)(l))
#define M_SETPKTLEN(m, l)	mbuf_pkthdr_setlen((m), (size_t)(l))
#define M_SETNEXT(m, n)	mbuf_setnext((m), (n))
#define M_SETFLAG(m, f)	mbuf_setflags_mask((m), (f), (f))
#define M_CLRFLAG(m, f)	mbuf_setflags_mask((m), 0, (f))
#define M_EXTSIZE(m)	((int)mbuf_maxlen(m))
// leave room at the front of an empty mbuf
#define M_RESERVE(m, n)	mbuf_setdata((m), (char *)mbuf_data(m) + (n), 0)

struct mbuf	*m_gethdr(int, int);
struct mbuf	*m_get(int, int);
struct mbuf	*m_clget(struct mbuf *, int, u_int);
#define MGETHDR(m, how, type)	((m) = m_gethdr((how), (type)))
#define MGET(m, how, type)	((m) = m_get((how), (type)))
#define MCLGET(m, how)		((void)m_clget((m), (how), MCLBYTES))
#define MCLGETL(m, how, l)	m_clget((m), (how), (l))
void	m_freem(struct mbuf *);
struct mbuf	*m_free(struct mbuf *);
void	m_adj(struct mbuf *, int);
void	m_copydata(struct mbuf *, int, int, void *);
int	m_copyback(struct mbuf *, int, int, const void *, int);
struct mbuf	*m_pullup(struct mbuf *, int);
struct mbuf	*m_prepend(struct mbuf *, int, int);
#define M_PREPEND(m, plen, how)	((m) = m_prepend((m), (plen), (how)))
struct mbuf	*m_dup_pkt(struct mbuf *, unsigned int, int);
int	m_dup_pkthdr(struct mbuf *, struct mbuf *, int);
void	m_cat(struct mbuf *, struct mbuf *);
struct mbuf	*m_split(struct mbuf *, int, int);
int	m_trailingspace(struct mbuf *);
int	m_leadingspace(struct mbuf *);
void	m_align(struct mbuf *, int);
struct mbuf	*m_makespace(struct mbuf *, int, int, int *);
#define M_TRAILINGSPACE(m)	m_trailingspace(m)
#define M_LEADINGSPACE(m)	m_leadingspace(m)
#define M_ALIGN(m, l)		m_align((m), (l))
#define MH_ALIGN(m, l)		m_align((m), (l))
// the node pointer that ph_cookie carries on openbsd
void	*m_cookie(struct mbuf *);
void	m_setcookie(struct mbuf *, void *);
int	m_hasdata_writable(struct mbuf *);

struct mbuf_list {
	struct mbuf	*ml_head;
	struct mbuf	*ml_tail;
	u_int		ml_len;
};
#define MBUF_LIST_INITIALIZER()	{ NULL, NULL, 0 }
void	ml_init(struct mbuf_list *);
void	ml_enqueue(struct mbuf_list *, struct mbuf *);
struct mbuf	*ml_dequeue(struct mbuf_list *);
void	ml_purge(struct mbuf_list *);
#define ml_empty(ml)	((ml)->ml_len == 0)
#define ml_len(ml)	((ml)->ml_len)
#define MBUF_LIST_FOREACH(ml, m) \
	for ((m) = (ml)->ml_head; (m) != NULL; (m) = mbuf_nextpkt(m))

struct mbuf_queue {
	struct mbuf_list	mq_list;
	u_int			mq_maxlen;
	u_int			mq_drops;
};
#define MBUF_QUEUE_INITIALIZER(m, i)	{ MBUF_LIST_INITIALIZER(), (m), 0 }
void	mq_init(struct mbuf_queue *, u_int, int);
int	mq_enqueue(struct mbuf_queue *, struct mbuf *);
struct mbuf	*mq_dequeue(struct mbuf_queue *);
u_int	mq_purge(struct mbuf_queue *);
#define mq_len(mq)	((mq)->mq_list.ml_len)
#define mq_empty(mq)	(mq_len(mq) == 0)
#define mq_full(mq)	(mq_len(mq) >= (mq)->mq_maxlen)
#define mq_drops(mq)	((mq)->mq_drops)

// mbuf tags are not used by the stack here
struct m_tag;
#define m_tag_find(m, t, p)	((struct m_tag *)NULL)

// ethernet and llc

#define ETHER_ADDR_LEN	6
#define ETHER_TYPE_LEN	2
#define ETHER_CRC_LEN	4
#define ETHER_HDR_LEN	(ETHER_ADDR_LEN * 2 + ETHER_TYPE_LEN)
#define ETHER_MIN_LEN	64
#define ETHER_MAX_LEN	1518
#define ETHERMTU	(ETHER_MAX_LEN - ETHER_HDR_LEN - ETHER_CRC_LEN)
#define ETHER_ALIGN	2
#define ETHERTYPE_IP	0x0800
#define ETHERTYPE_ARP	0x0806
#define ETHERTYPE_IPV6	0x86DD
#define ETHERTYPE_PAE	0x888E
#define ETHERTYPE_EAPOL	ETHERTYPE_PAE
#define ETHERTYPE_VLAN	0x8100
#define ETHER_IS_MULTICAST(a)	(*(a) & 0x01)
#define ETHER_IS_BROADCAST(a) \
	(((a)[0] & (a)[1] & (a)[2] & (a)[3] & (a)[4] & (a)[5]) == 0xff)
#define ETHER_IS_EQ(a1, a2)	(memcmp((a1), (a2), ETHER_ADDR_LEN) == 0)

struct ether_header {
	uint8_t		ether_dhost[ETHER_ADDR_LEN];
	uint8_t		ether_shost[ETHER_ADDR_LEN];
	uint16_t	ether_type;
} __packed;

extern const uint8_t etherbroadcastaddr[ETHER_ADDR_LEN];
extern const uint8_t etheranyaddr[ETHER_ADDR_LEN];
char	*ether_sprintf(const uint8_t *);

struct llc {
	uint8_t llc_dsap;
	uint8_t llc_ssap;
	union {
		struct {
			uint8_t control;
			uint8_t format_id;
			uint8_t class_id;
			uint8_t window_x2;
		} __packed type_u;
		struct {
			uint8_t control;
			uint8_t org_code[3];
			uint16_t ether_type;
		} __packed type_snap;
	} llc_un;
} __packed;
#define llc_control		llc_un.type_u.control
#define llc_snap		llc_un.type_snap
#define LLC_SNAPFRAMELEN	8
#define LLC_UI			0x3
#define LLC_SNAP_LSAP		0xaa

// interface media, from openbsd net/if_media.h
#include "pd_if_media.h"

// interfaces: the parts of struct ifnet net80211 touches

struct ifnet;
struct ifqueue {
	struct mbuf_list	ifq_list;
	u_int			ifq_maxlen;
	int			ifq_oactive;
	struct ifnet		*ifq_if;
};
#define IFQ_MAXLEN	256
int	ifq_enqueue(struct ifqueue *, struct mbuf *);
struct mbuf	*ifq_dequeue(struct ifqueue *);
void	ifq_purge(struct ifqueue *);
#define ifq_set_oactive(q)	((q)->ifq_oactive = 1)
#define ifq_clr_oactive(q)	((q)->ifq_oactive = 0)
#define ifq_is_oactive(q)	((q)->ifq_oactive)
#define ifq_empty(q)		ml_empty(&(q)->ifq_list)
#define ifq_len(q)		ml_len(&(q)->ifq_list)
#define ifq_set_maxlen(q, n)	((q)->ifq_maxlen = (n))

#define IFNAMSIZ_COMPAT	16
#ifndef IFNAMSIZ
#define IFNAMSIZ	IFNAMSIZ_COMPAT
#endif
#define IFF_UP		0x1
#define IFF_BROADCAST	0x2
#define IFF_DEBUG	0x4
#define IFF_RUNNING	0x40
#define IFF_PROMISC	0x100
#define IFF_ALLMULTI	0x200
#define IFF_SIMPLEX	0x800
#define IFF_MULTICAST	0x8000
#define IFCAP_VLAN_MTU	0x10
#define LINK_STATE_UNKNOWN	0
#define LINK_STATE_INVALID	1
#define LINK_STATE_DOWN		2
#define LINK_STATE_UP		4
#define LINK_STATE_IS_UP(s)	((s) >= LINK_STATE_UP)
#define IFT_IEEE80211	0x47
#define IFT_ETHER	0x6

struct sockaddr_dl_compat {
	uint8_t		sdl_len;
	uint8_t		sdl_family;
	uint16_t	sdl_index;
	uint8_t		sdl_type;
	uint8_t		sdl_nlen;
	uint8_t		sdl_alen;
	uint8_t		sdl_slen;
	char		sdl_data[24];
};
#define LLADDR(s)	((caddr_t)((s)->sdl_data + (s)->sdl_nlen))

struct ifreq_compat;
struct rtentry;
struct ifnet {
	char		if_xname[IFNAMSIZ_COMPAT];
	void		*if_softc;
	int		if_flags;
	int		if_xflags;
	int		if_timer;
	u_int		if_mtu;
	u_int		if_hdrlen;
	int		if_capabilities;
	int		if_link_state;
	uint64_t	if_baudrate;
	uint8_t		if_type;
	uint8_t		if_priority;
	uint64_t	if_ipackets, if_ierrors, if_opackets, if_oerrors;
	uint64_t	if_ibytes, if_obytes, if_imcasts, if_omcasts;
	void		*if_bpf;
	struct ifqueue	if_snd;
	struct sockaddr_dl_compat *if_sadl;
	struct sockaddr_dl_compat if_sadl_store;
	void		(*if_start)(struct ifnet *);
	void		(*if_watchdog)(struct ifnet *);
	int		(*if_ioctl)(struct ifnet *, u_long, caddr_t);
	int		(*if_output)(struct ifnet *, struct mbuf *, struct sockaddr *, struct rtentry *);
	// the owning IO80211Controller takes received frames and link changes
	void		*if_pdowner;
	void		(*if_pdinput)(struct ifnet *, struct mbuf *);
	void		(*if_pdlinkchange)(struct ifnet *);
	// if_slowtimo's once a second if_timer countdown, which drives if_watchdog
	struct timeout	if_slowtimo;
};
struct arpcom {
	struct ifnet	ac_if;
	uint8_t		ac_enaddr[ETHER_ADDR_LEN];
};

void	if_attach(struct ifnet *);
void	if_detach(struct ifnet *);
void	if_deactivate(struct ifnet *);
void	if_start(struct ifnet *);
void	if_input(struct ifnet *, struct mbuf_list *);
int	if_enqueue(struct ifnet *, struct mbuf *);
void	if_link_state_change(struct ifnet *);
void	if_addgroup(struct ifnet *, const char *);
void	ether_ifattach(struct ifnet *);
void	ether_ifdetach(struct ifnet *);
#define if_counters_alloc(ifp)	((void)0)
#define if_attach_queues(ifp, n)	((void)0)
#define if_ih_insert(ifp, f, c)	((void)0)
#define if_ih_remove(ifp, f, c)	((void)0)

// bpf is not wired up
#define DLT_EN10MB		1
#define DLT_IEEE802_11		105
#define DLT_IEEE802_11_RADIO	127
#define BPF_DIRECTION_IN	1
#define BPF_DIRECTION_OUT	2
#define bpfattach(p, ifp, dlt, len)	((void)0)
#define bpf_mtap(b, m, d)	((void)0)
#define bpf_mtap_hdr(b, h, l, m, d)	((void)0)

// bits and pieces of the openbsd kernel
#define ISSET(t, f)	((t) & (f))
#define SET(t, f)	((t) |= (f))
#define CLR(t, f)	((t) &= ~(f))
#define ALIGNED_POINTER(p, t)	1
#define IF_Kbps(x)	((uint64_t)(x) * 1000ULL)
#define IF_Mbps(x)	(IF_Kbps(x) * 1000ULL)
#define IF_Gbps(x)	(IF_Mbps(x) * 1000ULL)
#define IF_WIRELESS_DEFAULT_PRIORITY	4
#define IFF_LINK0	0x1000
#define abs(x)		__builtin_abs(x)
time_t	getuptime(void);
uint32_t	ether_crc32_le_update(uint32_t, const uint8_t *, size_t);
uint32_t	ether_crc32_le(const uint8_t *, size_t);

#ifndef IPTOS_DSCP_CS1
#define IPTOS_DSCP_CS0	0x00
#define IPTOS_DSCP_CS1	0x20
#define IPTOS_DSCP_AF11	0x28
#define IPTOS_DSCP_AF12	0x30
#define IPTOS_DSCP_AF13	0x38
#define IPTOS_DSCP_CS2	0x40
#define IPTOS_DSCP_AF21	0x48
#define IPTOS_DSCP_AF22	0x50
#define IPTOS_DSCP_AF23	0x58
#define IPTOS_DSCP_CS3	0x60
#define IPTOS_DSCP_AF31	0x68
#define IPTOS_DSCP_AF32	0x70
#define IPTOS_DSCP_AF33	0x78
#define IPTOS_DSCP_CS4	0x80
#define IPTOS_DSCP_AF41	0x88
#define IPTOS_DSCP_AF42	0x90
#define IPTOS_DSCP_AF43	0x98
#define IPTOS_DSCP_CS5	0xa0
#define IPTOS_DSCP_VA	0xb0
#define IPTOS_DSCP_EF	0xb8
#define IPTOS_DSCP_CS6	0xc0
#define IPTOS_DSCP_CS7	0xe0
#endif

// socket ioctls the drivers see, routed here by the controller rather than a socket
struct ifreq {
	char	ifr_name[16];
	union {
		struct sockaddr	ifru_addr;
		short		ifru_flags;
		int		ifru_mtu;
		caddr_t		ifru_data;
	} ifr_ifru;
};
#define ifr_data	ifr_ifru.ifru_data
#define ifr_flags	ifr_ifru.ifru_flags
#define ifr_mtu		ifr_ifru.ifru_mtu

// raw frame injection and routing messages are not wired up
struct rtentry;
struct m_tag { int m_tag_dummy; };
#define PACKET_TAG_DLT	0x0100
int	ether_output(struct ifnet *, struct mbuf *, struct sockaddr *, struct rtentry *);
struct if_ieee80211_data {
	uint8_t		ifie_channel;
	uint8_t		ifie_nwid_len;
	uint32_t	ifie_flags;
	uint32_t	ifie_xflags;
	uint8_t		ifie_nwid[32];
	uint8_t		ifie_addr[6];
};
#define rtm_80211info(ifp, ifie)	((void)0)

// small libc pieces
#define timingsafe_bcmp(a, b, l)	pd80211_timingsafe_bcmp((a), (b), (l))
int	pd80211_timingsafe_bcmp(const void *, const void *, size_t);
#define explicit_bzero(p, l)	pd80211_explicit_bzero((p), (l))
void	pd80211_explicit_bzero(void *, size_t);
#define panic(...)	panic(__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif
