// SPDX-License-Identifier: MIT
// freebsd kernel and iflib calls newer iflib drivers make past what the haiku layer offers,
// included from a driver's main header after its kernel includes
#ifndef PD_IFLIB_SHIMS_H
#define PD_IFLIB_SHIMS_H

#include <sys/sx.h>
#include <sys/sysctl.h>
#include <machine/atomic.h>
#include <net/iflib.h>

#include "pd_fbsd_glue.h"

// freebsd's atomic_set_32 ors the bits in, the haiku compat one stores them
#undef atomic_set_32
#define atomic_set_32(ptr, val) (void)atomic_or((int32 *)(ptr), (val))
#undef atomic_readandclear_32
#define atomic_readandclear_32(ptr) ((u_int)atomic_get_and_set((int32 *)(ptr), 0))
#ifndef atomic_swap_int
#define atomic_swap_int(ptr, val) ((u_int)atomic_get_and_set((int32 *)(ptr), (val)))
#endif

// the haiku atomics are full barriers, so acquire and release map onto the plain ones
#ifndef atomic_cmpset_acq_32
#define atomic_cmpset_acq_32(ptr, old, new) atomic_cmpset_32(ptr, old, new)
#endif
#ifndef atomic_store_rel_32
#define atomic_store_rel_32(ptr, val) (void)atomic_set((int32 *)(ptr), (val))
#endif
#ifndef atomic_load_acq_int
#define atomic_load_acq_int(ptr) ((u_int)atomic_get((int32 *)(ptr)))
#endif
#ifndef atomic_load_acq_64
#define atomic_load_acq_64(ptr) ((uint64_t)atomic_get64((int64 *)(ptr)))
#endif
#ifndef atomic_store_rel_64
#define atomic_store_rel_64(ptr, val) (void)atomic_set64((int64 *)(ptr), (val))
#endif

// the sized sysctl handlers, never run since haiku's sysctl layer keeps no tree
static inline int sysctl_handle_8(SYSCTL_HANDLER_ARGS) { return -1; }
static inline int sysctl_handle_16(SYSCTL_HANDLER_ARGS) { return -1; }
static inline int sysctl_handle_32(SYSCTL_HANDLER_ARGS) { return -1; }
static inline int sysctl_handle_bool(SYSCTL_HANDLER_ARGS) { return -1; }
#ifndef SYSCTL_ADD_U64
#define SYSCTL_ADD_U64(ctx, parent, nbr, name, access, ptr, val, descr) \
	SYSCTL_ADD_UQUAD(ctx, parent, nbr, name, access, ptr, descr)
#endif

// the iflib calls newer than haiku's iflib, mapped onto what it has
static inline bool
iflib_is_running(if_ctx_t ctx)
{
	return ((if_getdrvflags(iflib_get_ifp(ctx)) & IFF_DRV_RUNNING) != 0);
}

static inline void iflib_request_reset_if_up(if_ctx_t ctx) { iflib_request_reset(ctx); }
static inline void iflib_init_failed(if_ctx_t ctx) { }

// net/rss_config.h hash types, no kernel rss so a random key and the tcp and ip hashes
#ifndef RSS_HASHTYPE_RSS_IPV4
#define RSS_HASHTYPE_RSS_IPV4           (1 << 1)
#define RSS_HASHTYPE_RSS_TCP_IPV4       (1 << 2)
#define RSS_HASHTYPE_RSS_IPV6           (1 << 3)
#define RSS_HASHTYPE_RSS_TCP_IPV6       (1 << 4)
#define RSS_HASHTYPE_RSS_IPV6_EX        (1 << 5)
#define RSS_HASHTYPE_RSS_TCP_IPV6_EX    (1 << 6)
#define RSS_HASHTYPE_RSS_UDP_IPV4       (1 << 7)
#define RSS_HASHTYPE_RSS_UDP_IPV6       (1 << 9)
#define RSS_HASHTYPE_RSS_UDP_IPV6_EX    (1 << 10)
#define rss_getkey(key) arc4rand((key), 40, 0)
#define rss_gethashconfig() (RSS_HASHTYPE_RSS_IPV4 | RSS_HASHTYPE_RSS_TCP_IPV4 | \
    RSS_HASHTYPE_RSS_IPV6 | RSS_HASHTYPE_RSS_TCP_IPV6)
#endif

// libkern bits the haiku headers leave undeclared
long strtol(const char *, char **, int);
void nanotime(struct timespec *);
#ifndef hexdump
#define hexdump(ptr, len, hdr, flags) do { } while (0)
#endif

#endif
