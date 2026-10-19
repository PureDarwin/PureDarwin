/*
 * Copyright 2007-2018, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _FBSD_COMPAT_SYS_KERNEL_H_
#define _FBSD_COMPAT_SYS_KERNEL_H_


#include <stddef.h>

#include <sys/haiku-module.h>

#include <sys/linker_set.h>
#include <sys/queue.h>


/* The rate at which FreeBSD can generate callouts (kind of timeout mechanism).
 * For FreeBSD 8 this is typically 1000 times per second (100 for ARM).
 * This value is defined in a file called subr_param.c
 *
 * While Haiku can have a much higher granularity, it is not a good idea to have
 * this since FreeBSD tries to do certain tasks based on ticks, for instance
 * autonegotiation and wlan scanning.
 * Suffixing LL prevents integer overflows during calculations.
 * as it defines a long long constant. */
#ifdef PD_LKPI_DRM
// drm names struct members and parameters hz, so it gets the variable freebsd has
extern int hz;
#else
#define hz	1000LL
#endif

int32_t _get_ticks();
#define ticks (_get_ticks())


/* sysinit */
// subsystems order sysinits before their order within one subsystem
enum sysinit_sub_id {
	SI_SUB_DUMMY = 0x0000000,
	SI_SUB_KMEM = 0x1800000,
	SI_SUB_LOCK = 0x1B00000,
	SI_SUB_EVENTHANDLER = 0x1C00000,
	SI_SUB_KLD = 0x2000000,
	SI_SUB_CPU = 0x2100000,
	SI_SUB_TASKQ = 0x2880000,
	SI_SUB_SMP = 0x2900000,
	SI_SUB_INIT_IF = 0x3000000,
	SI_SUB_DRIVERS = 0x3100000,
	SI_SUB_CONFIGURE = 0x3800000,
	SI_SUB_ROOT_CONF = 0x4000000,
	SI_SUB_CLOCKS = 0x4800000,
	SI_SUB_PSEUDO = 0x7000000,
	SI_SUB_EXEC = 0x7400000,
	SI_SUB_PROTO_IFATTACHDOMAIN = 0x8808000,
	SI_SUB_VNET_DONE = 0xdc00000,
	SI_SUB_LAST = 0xfffffff
};

enum sysinit_elem_order {
	SI_ORDER_FIRST = 0x0000000,
	SI_ORDER_SECOND = 0x0000001,
	SI_ORDER_THIRD = 0x0000002,
	SI_ORDER_FOURTH = 0x0000003,
	SI_ORDER_MIDDLE = 0x1000000,
	SI_ORDER_ANY = 0xfffffff,	/* last */
	SI_ORDER_LAST = 0xfffffff
};

typedef void (*system_init_func_t)(void *);

struct sysinit {
	const char* name;
	unsigned int subsystem;
	unsigned int order;
	system_init_func_t func;
	const void* arg;
};

#define SYSINIT(uniquifier, _subsystem, _order, _func, ident) \
static struct sysinit sysinit_##uniquifier = { \
	.name 		= #uniquifier,		\
	.subsystem	= _subsystem,		\
	.order 		= _order,			\
	.func		= _func,			\
	.arg		= ident,			\
};									\
DATA_SET(__freebsd_sysinit, sysinit_##uniquifier)

#define SYSUNINIT(uniquifier, _subsystem, _order, _func, ident) \
static struct sysinit sysuninit_##uniquifier = { \
	.name 		= #uniquifier,		\
	.subsystem	= _subsystem,		\
	.order 		= _order,			\
	.func		= _func,			\
	.arg		= ident,			\
};									\
DATA_SET(__freebsd_sysuninit, sysuninit_##uniquifier)


/* confighooks */
typedef void (*ich_func_t)(void *_arg);

struct intr_config_hook {
	TAILQ_ENTRY(intr_config_hook) ich_links;
	ich_func_t	ich_func;
	void		*ich_arg;
};

int config_intrhook_establish(struct intr_config_hook *hook);
void config_intrhook_disestablish(struct intr_config_hook *hook);


/* misc. */
#define TUNABLE_INT(path, var)
#define TUNABLE_INT_FETCH(path, var)


#endif // _FBSD_COMPAT_SYS_KERNEL_H_
