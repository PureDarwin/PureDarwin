/*
 * Copyright 2009, Colin Günther, coling@gmx.de.
 * Copyright 2007, Hugo Santos. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _FBSD_COMPAT_SYS_SYSTM_H_
#define _FBSD_COMPAT_SYS_SYSTM_H_


#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include <machine/atomic.h>
#include <machine/cpufunc.h>

#include <sys/callout.h>
#include <sys/cdefs.h>
#include <sys/queue.h>

#include <sys/libkern.h>

__BEGIN_DECLS


#ifdef PD_LKPI_DRM
// drm code prints through PureDarwin's printf, which knows linux's %p extensions
int pd_drm_printf(const char *format, ...);
#define printf(...) pd_drm_printf(__VA_ARGS__)
#else
#define printf freebsd_printf
int printf(const char *format, ...) __printflike(1, 2);
#endif


#define ovbcopy(f, t, l) bcopy((f), (t), (l))

#if KDEBUG_LEVEL_2
#define INVARIANTS
#endif

#if KDEBUG_LEVEL_1
#define bootverbose 1
#else
#define bootverbose 0
#endif

#ifdef INVARIANTS
#define KASSERT_FREEBSD(cond,msg) do {	\
	if (!(cond))				\
		panic msg;				\
} while (0)
#define KASSERT(cond,msg) KASSERT_FREEBSD(cond,msg)
#else
#define KASSERT_FREEBSD(exp,msg) do {} while (0)
#define KASSERT(cond,msg) KASSERT_FREEBSD(cond,msg)
#endif

#define DELAY(n) \
	do {				\
		if (n < 1000)	\
			spin(n);	\
		else			\
			snooze(n);	\
	} while (0)

#define	cold 0

void wakeup(void *);
void wakeup_one(void *);

#ifndef CTASSERT	/* Allow lint to override */
#define	CTASSERT(x)	_Static_assert(x, "compile-time assertion failed")
#endif


static inline int
copyin(const void * __restrict udaddr, void * __restrict kaddr,
	size_t len)
{
	return user_memcpy(kaddr, udaddr, len);
}


static inline int
copyout(const void * __restrict kaddr, void * __restrict udaddr,
	size_t len)
{
	return user_memcpy(udaddr, kaddr, len);
}


static inline void log(int level, const char *fmt, ...) { }


int snprintf(char *, size_t, const char *, ...) __printflike(3, 4);
extern int sprintf(char *buf, const char *, ...);

extern int driver_vprintf(const char *format, va_list vl);
#ifdef PD_LKPI_DRM
extern int pd_drm_vprintf(const char *format, va_list vl);
#define vprintf(fmt, vl) pd_drm_vprintf(fmt, vl)
#else
#define vprintf(fmt, vl) driver_vprintf(fmt, vl)
#endif

extern int vsnprintf(char *, size_t, const char *, __va_list)
	__printflike(3, 0);

int msleep(void *, struct mtx *, int, const char *, int);
int _pause(const char *, int);
#ifdef PD_LINUXKPI
// object-like, so struct members named pause still work
static inline int pd_fbsd_pause(const char *waitMessage, int timeout) { return _pause(waitMessage, timeout); }
#define pause pd_fbsd_pause
#else
#define pause(waitMessage, timeout) _pause((waitMessage), (timeout))
#endif
#define msleep_spin(chan, mtx, wmesg, timo) \
	msleep(chan, mtx, PZERO, wmesg, timo)
#define mtx_sleep msleep

void critical_enter(void);
void critical_exit(void);

struct unrhdr;
struct unrhdr *new_unrhdr(int low, int high, struct mtx *mutex);
void delete_unrhdr(struct unrhdr *);
int alloc_unr(struct unrhdr *);
void free_unr(struct unrhdr *, u_int);

extern char *getenv(const char *name);
extern void    freeenv(char *env);
extern char *kern_getenv(const char *name);

__END_DECLS

#endif	/* _FBSD_COMPAT_SYS_SYSTM_H_ */
