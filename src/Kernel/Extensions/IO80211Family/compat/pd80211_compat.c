/*
 * Runtime for the OpenBSD interfaces in pd80211_compat.h: the lock, the
 * timeout and task thread, mbuf helpers and queues, and the ifnet glue that
 * hands frames to the owning IO80211Controller.
 */
#include "pd80211_compat.h"
#include <IOKit/IOLib.h>
#include <IOKit/IOLocks.h>
#include <kern/thread.h>
#include <kern/locks.h>

int hz = 100;

static IORecursiveLock *pd_lock;
static IOLock *pd_qlock;
static TAILQ_HEAD(, timeout) pd_timeouts = TAILQ_HEAD_INITIALIZER(pd_timeouts);
static TAILQ_HEAD(, task) pd_tasks = TAILQ_HEAD_INITIALIZER(pd_tasks);
static int pd_qwake;
static int pd_thread_started;
static mbuf_tag_id_t pd_cookie_tag;

struct taskq { int dummy; };
static struct taskq pd_systq;
struct taskq *const systq = &pd_systq;
struct taskq *const systqmp = &pd_systq;

const uint8_t etherbroadcastaddr[ETHER_ADDR_LEN] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
const uint8_t etheranyaddr[ETHER_ADDR_LEN] = { 0 };

static void pd_worker(void *, wait_result_t);

// set up once by IO80211Family before any controller attaches
void
pd80211_init(void)
{
	thread_t t;

	if (pd_lock)
		return;
	pd_lock = IORecursiveLockAlloc();
	pd_qlock = IOLockAlloc();
	mbuf_tag_id_find("org.puredarwin.80211.node", &pd_cookie_tag);
	if (kernel_thread_start(pd_worker, NULL, &t) == KERN_SUCCESS) {
		pd_thread_started = 1;
		thread_deallocate(t);
	}
}

void
pd80211_lock(void)
{
	IORecursiveLockLock(pd_lock);
}

void
pd80211_unlock(void)
{
	IORecursiveLockUnlock(pd_lock);
}

int
pd80211_locked(void)
{
	return IORecursiveLockHaveLock(pd_lock);
}

// memory keeps its size in front so free does not need it

void *
pd80211_malloc(size_t size, int type, int flags)
{
	size_t *p = IOMalloc(size + 16);

	(void)type;
	if (p == NULL)
		return NULL;
	p[0] = size + 16;
	if (flags & M_ZERO)
		bzero(p + 2, size);
	return p + 2;
}

void *
pd80211_mallocarray(size_t n, size_t size, int type, int flags)
{
	if (size && n > SIZE_MAX / size)
		return NULL;
	return pd80211_malloc(n * size, type, flags);
}

void
pd80211_free(void *ptr, int type, size_t size)
{
	size_t *p = ptr;

	(void)type;
	(void)size;
	if (p == NULL)
		return;
	p -= 2;
	IOFree(p, p[0]);
}

// time

uint64_t
getnsecuptime(void)
{
	uint64_t abs, ns;

	clock_get_uptime(&abs);
	absolutetime_to_nanoseconds(abs, &ns);
	return ns;
}

void
getmicrouptime(struct timeval *tv)
{
	uint64_t ns = getnsecuptime();

	tv->tv_sec = (time_t)(ns / 1000000000ULL);
	tv->tv_usec = (suseconds_t)((ns % 1000000000ULL) / 1000);
}

time_t
pd80211_uptime(void)
{
	return (time_t)(getnsecuptime() / 1000000000ULL);
}

int
pd80211_ticks(void)
{
	return (int)(getnsecuptime() / (1000000000ULL / (uint64_t)hz));
}

int
ratecheck(struct timeval *last, const struct timeval *interval)
{
	struct timeval now, delta;

	getmicrouptime(&now);
	timevalsub(&now, last, &delta);
	if ((last->tv_sec == 0 && last->tv_usec == 0) || !timevalcmp(&delta, interval, <)) {
		*last = now;
		return 1;
	}
	return 0;
}

// the worker thread runs due timeouts and queued tasks under the lock

static void
pd_kick(void)
{
	IOLockLock(pd_qlock);
	pd_qwake = 1;
	IOLockWakeup(pd_qlock, &pd_qwake, false);
	IOLockUnlock(pd_qlock);
}

static void
pd_worker(void *arg, wait_result_t wr)
{
	(void)arg;
	(void)wr;
	for (;;) {
		struct timeout *to, *due = NULL;
		struct task *t = NULL;
		uint64_t now = getnsecuptime(), next = UINT64_MAX;

		IOLockLock(pd_qlock);
		TAILQ_FOREACH(to, &pd_timeouts, to_list) {
			if (to->to_deadline <= now) {
				due = to;
				break;
			}
			if (to->to_deadline < next)
				next = to->to_deadline;
		}
		if (due) {
			TAILQ_REMOVE(&pd_timeouts, due, to_list);
			due->to_flags &= ~TIMEOUT_ONQUEUE;
		} else if ((t = TAILQ_FIRST(&pd_tasks)) != NULL) {
			TAILQ_REMOVE(&pd_tasks, t, t_entry);
			t->t_flags &= ~TASK_ONQUEUE;
		}
		if (!due && !t) {
			if (!pd_qwake) {
				if (next == UINT64_MAX) {
					IOLockSleep(pd_qlock, &pd_qwake, THREAD_UNINT);
				} else {
					uint64_t deadline;

					nanoseconds_to_absolutetime(next - now, &deadline);
					deadline += mach_absolute_time();
					IOLockSleepDeadline(pd_qlock, &pd_qwake, deadline, THREAD_UNINT);
				}
			}
			pd_qwake = 0;
			IOLockUnlock(pd_qlock);
			continue;
		}
		IOLockUnlock(pd_qlock);

		pd80211_lock();
		if (due)
			due->to_func(due->to_arg);
		else
			t->t_func(t->t_arg);
		pd80211_unlock();
	}
}

void
timeout_set(struct timeout *to, void (*fn)(void *), void *arg)
{
	bzero(to, sizeof(*to));
	to->to_func = fn;
	to->to_arg = arg;
	to->to_flags = TIMEOUT_INITIALIZED;
}

static int
timeout_add_nsec(struct timeout *to, uint64_t nsec)
{
	int was;

	IOLockLock(pd_qlock);
	was = to->to_flags & TIMEOUT_ONQUEUE;
	if (was)
		TAILQ_REMOVE(&pd_timeouts, to, to_list);
	to->to_deadline = getnsecuptime() + nsec;
	to->to_flags |= TIMEOUT_ONQUEUE | TIMEOUT_INITIALIZED;
	TAILQ_INSERT_TAIL(&pd_timeouts, to, to_list);
	IOLockUnlock(pd_qlock);
	pd_kick();
	return !was;
}

int
timeout_add(struct timeout *to, int t)
{
	return timeout_add_nsec(to, (uint64_t)(t > 0 ? t : 1) * (1000000000ULL / (uint64_t)hz));
}

int
timeout_add_sec(struct timeout *to, int s)
{
	return timeout_add_nsec(to, SEC_TO_NSEC(s));
}

int
timeout_add_msec(struct timeout *to, int ms)
{
	return timeout_add_nsec(to, MSEC_TO_NSEC(ms));
}

int
timeout_add_usec(struct timeout *to, int us)
{
	return timeout_add_nsec(to, USEC_TO_NSEC(us));
}

int
timeout_del(struct timeout *to)
{
	int was;

	IOLockLock(pd_qlock);
	was = to->to_flags & TIMEOUT_ONQUEUE;
	if (was) {
		TAILQ_REMOVE(&pd_timeouts, to, to_list);
		to->to_flags &= ~TIMEOUT_ONQUEUE;
	}
	IOLockUnlock(pd_qlock);
	return was != 0;
}

int
timeout_del_barrier(struct timeout *to)
{
	return timeout_del(to);
}

void
task_set(struct task *t, void (*fn)(void *), void *arg)
{
	bzero(t, sizeof(*t));
	t->t_func = fn;
	t->t_arg = arg;
}

int
task_add(struct taskq *tq, struct task *t)
{
	(void)tq;
	IOLockLock(pd_qlock);
	if (t->t_flags & TASK_ONQUEUE) {
		IOLockUnlock(pd_qlock);
		return 0;
	}
	t->t_flags |= TASK_ONQUEUE;
	TAILQ_INSERT_TAIL(&pd_tasks, t, t_entry);
	IOLockUnlock(pd_qlock);
	pd_kick();
	return 1;
}

int
task_del(struct taskq *tq, struct task *t)
{
	int was;

	(void)tq;
	IOLockLock(pd_qlock);
	was = t->t_flags & TASK_ONQUEUE;
	if (was) {
		TAILQ_REMOVE(&pd_tasks, t, t_entry);
		t->t_flags &= ~TASK_ONQUEUE;
	}
	IOLockUnlock(pd_qlock);
	return was != 0;
}

// sleeping drops the lock the way tsleep drops the kernel lock

int
tsleep_nsec(const volatile void *ident, int pri, const char *wmesg, uint64_t nsecs)
{
	int r;

	(void)pri;
	(void)wmesg;
	if (nsecs == INFSLP) {
		r = IORecursiveLockSleep(pd_lock, (void *)(uintptr_t)ident, THREAD_UNINT);
	} else {
		uint64_t deadline;

		nanoseconds_to_absolutetime(nsecs, &deadline);
		deadline += mach_absolute_time();
		r = IORecursiveLockSleepDeadline(pd_lock, (void *)(uintptr_t)ident, deadline, THREAD_UNINT);
	}
	return r == THREAD_TIMED_OUT ? EWOULDBLOCK : 0;
}


time_t
getuptime(void)
{
	return pd80211_uptime();
}

uint32_t
ether_crc32_le_update(uint32_t crc, const uint8_t *buf, size_t len)
{
	while (len--) {
		crc ^= *buf++;
		for (int bit = 0; bit < 8; bit++)
			crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320U : 0);
	}
	return crc;
}

uint32_t
ether_crc32_le(const uint8_t *buf, size_t len)
{
	return ether_crc32_le_update(0xffffffffU, buf, len);
}

int
ether_output(struct ifnet *ifp, struct mbuf *m, struct sockaddr *dst, struct rtentry *rt)
{
	(void)dst;
	(void)rt;
	return if_enqueue(ifp, m);
}

// random

void
arc4random_buf(void *buf, size_t len)
{
	read_random(buf, (u_int)len);
}

uint32_t
arc4random(void)
{
	uint32_t v;

	read_random(&v, sizeof(v));
	return v;
}

uint32_t
arc4random_uniform(uint32_t bound)
{
	uint32_t r, min;

	if (bound < 2)
		return 0;
	min = -bound % bound;
	do {
		r = arc4random();
	} while (r < min);
	return r % bound;
}

// ethernet

char *
ether_sprintf(const uint8_t *ap)
{
	static char bufs[4][18];
	static int i;
	char *b = bufs[i++ & 3];

	snprintf(b, 18, "%02x:%02x:%02x:%02x:%02x:%02x", ap[0], ap[1], ap[2], ap[3], ap[4], ap[5]);
	return b;
}

int
pd80211_timingsafe_bcmp(const void *b1, const void *b2, size_t n)
{
	const unsigned char *p1 = b1, *p2 = b2;
	int ret = 0;

	for (; n > 0; n--)
		ret |= *p1++ ^ *p2++;
	return ret != 0;
}

void
pd80211_explicit_bzero(void *p, size_t n)
{
	volatile unsigned char *v = p;

	while (n--)
		*v++ = 0;
}

// mbufs

struct mbuf *
m_gethdr(int how, int type)
{
	mbuf_t m;

	if (mbuf_gethdr(how, type, &m) != 0)
		return NULL;
	return m;
}

struct mbuf *
m_get(int how, int type)
{
	mbuf_t m;

	if (mbuf_get(how, type, &m) != 0)
		return NULL;
	return m;
}

// attach a cluster, an mbuf without one is left as is for the caller to check M_EXT
struct mbuf *
m_clget(struct mbuf *m, int how, u_int len)
{
	mbuf_t mm = m;

	if (m == NULL) {
		if (mbuf_allocpacket(how, len, NULL, &mm) != 0)
			return NULL;
		return mm;
	}
	if (len <= MCLBYTES)
		mbuf_mclget(how, mbuf_type(m), &mm);
	else
		mbuf_getcluster(how, mbuf_type(m), len, &mm);
	return mm;
}

void
m_freem(struct mbuf *m)
{
	if (m)
		mbuf_freem(m);
}

struct mbuf *
m_free(struct mbuf *m)
{
	return m ? mbuf_free(m) : NULL;
}

void
m_adj(struct mbuf *m, int len)
{
	mbuf_adj(m, len);
}

void
m_copydata(struct mbuf *m, int off, int len, void *out)
{
	mbuf_copydata(m, (size_t)off, (size_t)len, out);
}

int
m_copyback(struct mbuf *m, int off, int len, const void *data, int how)
{
	return mbuf_copyback(m, (size_t)off, (size_t)len, data, how);
}

struct mbuf *
m_pullup(struct mbuf *m, int len)
{
	mbuf_t mm = m;

	if (mbuf_pullup(&mm, (size_t)len) != 0)
		return NULL;
	return mm;
}

struct mbuf *
m_prepend(struct mbuf *m, int len, int how)
{
	mbuf_t mm = m;

	if (mbuf_prepend(&mm, (size_t)len, how) != 0)
		return NULL;
	return mm;
}

struct mbuf *
m_dup_pkt(struct mbuf *m, unsigned int adj, int how)
{
	mbuf_t n;

	(void)adj;
	if (mbuf_dup(m, how, &n) != 0)
		return NULL;
	return n;
}

int
m_dup_pkthdr(struct mbuf *to, struct mbuf *from, int how)
{
	(void)how;
	return mbuf_copy_pkthdr(to, from);
}

void
m_cat(struct mbuf *m, struct mbuf *n)
{
	while (mbuf_next(m))
		m = mbuf_next(m);
	mbuf_setnext(m, n);
}

struct mbuf *
m_split(struct mbuf *m, int len, int how)
{
	mbuf_t n;

	if (mbuf_split(m, (size_t)len, how, &n) != 0)
		return NULL;
	return n;
}

int
m_trailingspace(struct mbuf *m)
{
	return (int)mbuf_trailingspace(m);
}

int
m_leadingspace(struct mbuf *m)
{
	return (int)mbuf_leadingspace(m);
}

void
m_align(struct mbuf *m, int len)
{
	size_t space = mbuf_maxlen(m);
	char *start = (char *)mbuf_datastart(m);

	mbuf_setdata(m, start + ((space - (size_t)len) & ~(sizeof(long) - 1)), 0);
}

void *
m_cookie(struct mbuf *m)
{
	size_t len;
	void **p;

	if (mbuf_tag_find(m, pd_cookie_tag, 1, &len, (void **)&p) != 0 || len != sizeof(void *))
		return NULL;
	return *p;
}

void
m_setcookie(struct mbuf *m, void *cookie)
{
	void **p;
	size_t len;

	if (mbuf_tag_find(m, pd_cookie_tag, 1, &len, (void **)&p) == 0 && len == sizeof(void *)) {
		*p = cookie;
		return;
	}
	if (mbuf_tag_allocate(m, pd_cookie_tag, 1, sizeof(void *), MBUF_DONTWAIT, (void **)&p) == 0)
		*p = cookie;
}

// packet lists and queues

void
ml_init(struct mbuf_list *ml)
{
	ml->ml_head = ml->ml_tail = NULL;
	ml->ml_len = 0;
}

void
ml_enqueue(struct mbuf_list *ml, struct mbuf *m)
{
	mbuf_setnextpkt(m, NULL);
	if (ml->ml_tail == NULL)
		ml->ml_head = m;
	else
		mbuf_setnextpkt(ml->ml_tail, m);
	ml->ml_tail = m;
	ml->ml_len++;
}

struct mbuf *
ml_dequeue(struct mbuf_list *ml)
{
	struct mbuf *m = ml->ml_head;

	if (m == NULL)
		return NULL;
	ml->ml_head = mbuf_nextpkt(m);
	if (ml->ml_head == NULL)
		ml->ml_tail = NULL;
	mbuf_setnextpkt(m, NULL);
	ml->ml_len--;
	return m;
}

void
ml_purge(struct mbuf_list *ml)
{
	struct mbuf *m;

	while ((m = ml_dequeue(ml)) != NULL)
		m_freem(m);
}

void
mq_init(struct mbuf_queue *mq, u_int maxlen, int ipl)
{
	(void)ipl;
	ml_init(&mq->mq_list);
	mq->mq_maxlen = maxlen;
	mq->mq_drops = 0;
}

int
mq_enqueue(struct mbuf_queue *mq, struct mbuf *m)
{
	pd80211_lock();
	if (mq_full(mq)) {
		mq->mq_drops++;
		pd80211_unlock();
		m_freem(m);
		return ENOBUFS;
	}
	ml_enqueue(&mq->mq_list, m);
	pd80211_unlock();
	return 0;
}

struct mbuf *
mq_dequeue(struct mbuf_queue *mq)
{
	struct mbuf *m;

	pd80211_lock();
	m = ml_dequeue(&mq->mq_list);
	pd80211_unlock();
	return m;
}

u_int
mq_purge(struct mbuf_queue *mq)
{
	u_int n;

	pd80211_lock();
	n = mq->mq_list.ml_len;
	ml_purge(&mq->mq_list);
	pd80211_unlock();
	return n;
}

int
ifq_enqueue(struct ifqueue *q, struct mbuf *m)
{
	if (q->ifq_maxlen && ml_len(&q->ifq_list) >= q->ifq_maxlen) {
		m_freem(m);
		return ENOBUFS;
	}
	ml_enqueue(&q->ifq_list, m);
	return 0;
}

struct mbuf *
ifq_dequeue(struct ifqueue *q)
{
	return ml_dequeue(&q->ifq_list);
}

void
ifq_purge(struct ifqueue *q)
{
	ml_purge(&q->ifq_list);
}

// interfaces

void
if_attach(struct ifnet *ifp)
{
	ifp->if_snd.ifq_if = ifp;
	if (ifp->if_snd.ifq_maxlen == 0)
		ifp->if_snd.ifq_maxlen = IFQ_MAXLEN;
}

void
ether_ifattach(struct ifnet *ifp)
{
	struct arpcom *ac = (struct arpcom *)ifp;

	ifp->if_sadl = &ifp->if_sadl_store;
	ifp->if_sadl->sdl_alen = ETHER_ADDR_LEN;
	ifp->if_sadl->sdl_nlen = 0;
	memcpy(LLADDR(ifp->if_sadl), ac->ac_enaddr, ETHER_ADDR_LEN);
	if (ifp->if_mtu == 0)
		ifp->if_mtu = ETHERMTU;
	ifp->if_hdrlen = ETHER_HDR_LEN;
}

void
ether_ifdetach(struct ifnet *ifp)
{
	ifq_purge(&ifp->if_snd);
}

void
if_detach(struct ifnet *ifp)
{
	ifq_purge(&ifp->if_snd);
}

void
if_deactivate(struct ifnet *ifp)
{
	(void)ifp;
}

void
if_addgroup(struct ifnet *ifp, const char *name)
{
	(void)ifp;
	(void)name;
}

void
if_start(struct ifnet *ifp)
{
	if (ifp->if_start)
		ifp->if_start(ifp);
}

int
if_enqueue(struct ifnet *ifp, struct mbuf *m)
{
	int error = ifq_enqueue(&ifp->if_snd, m);

	if (error == 0)
		if_start(ifp);
	return error;
}

void
if_input(struct ifnet *ifp, struct mbuf_list *ml)
{
	struct mbuf *m;

	while ((m = ml_dequeue(ml)) != NULL) {
		if (ifp->if_pdinput)
			ifp->if_pdinput(ifp, m);
		else
			m_freem(m);
	}
}

void
if_link_state_change(struct ifnet *ifp)
{
	if (ifp->if_pdlinkchange)
		ifp->if_pdlinkchange(ifp);
}
