// pd_ix_shims.h - freebsd kernel and iflib calls ix makes past what the haiku layer offers
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#ifndef PD_IX_SHIMS_H
#define PD_IX_SHIMS_H

#include <sys/sx.h>
#include <sys/sysctl.h>
#include <machine/atomic.h>
#include <net/iflib.h>

// freebsd's atomic_set_32 ors the bits in, the haiku compat one stores them
#undef atomic_set_32
#define atomic_set_32(ptr, val) (void)atomic_or((int32 *)(ptr), (val))
#undef atomic_readandclear_32
#define atomic_readandclear_32(ptr) ((u_int)atomic_get_and_set((int32 *)(ptr), 0))
#ifndef atomic_swap_int
#define atomic_swap_int(ptr, val) ((u_int)atomic_get_and_set((int32 *)(ptr), (val)))
#endif
#ifndef atomic_store_rel_32
#define atomic_store_rel_32(ptr, val) atomic_set((int32 *)(ptr), (val))
#endif
#ifndef atomic_load_acq_int
#define atomic_load_acq_int(ptr) ((u_int)atomic_get((int32 *)(ptr)))
#endif

// the sized sysctl handlers, never run since haiku's sysctl layer keeps no tree
static inline int sysctl_handle_8(SYSCTL_HANDLER_ARGS) { return -1; }
static inline int sysctl_handle_16(SYSCTL_HANDLER_ARGS) { return -1; }
static inline int sysctl_handle_32(SYSCTL_HANDLER_ARGS) { return -1; }
static inline int sysctl_handle_bool(SYSCTL_HANDLER_ARGS) { return -1; }
#define SYSCTL_ADD_U64(ctx, parent, nbr, name, access, ptr, val, descr) \
	SYSCTL_ADD_UQUAD(ctx, parent, nbr, name, access, ptr, descr)

// no wake from d3 through pme here
static inline bool pci_has_pme(device_t dev, int state) { return (false); }
static inline void pci_clear_pme(device_t dev) { }
static inline void pci_enable_pme(device_t dev) { }

// the iflib calls newer than haiku's iflib, mapped onto what it has
static inline bool
iflib_is_running(if_ctx_t ctx)
{
	return ((if_getdrvflags(iflib_get_ifp(ctx)) & IFF_DRV_RUNNING) != 0);
}

static inline void iflib_request_reset_if_up(if_ctx_t ctx) { iflib_request_reset(ctx); }
static inline void iflib_init_failed(if_ctx_t ctx) { }

// libkern bits the haiku headers leave undeclared
long strtol(const char *, char **, int);
void nanotime(struct timespec *);
#define hexdump(ptr, len, hdr, flags) do { } while (0)

#endif
