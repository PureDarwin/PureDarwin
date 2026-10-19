/*
 * Copyright 2022, Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _FBSD_COMPAT_SYS_BUS_H_
#define _FBSD_COMPAT_SYS_BUS_H_


#include <sys/haiku-module.h>

#include <sys/_bus_dma.h>
#include <sys/_bus_macros.h>

#include <sys/queue.h>


#define	FILTER_STRAY			B_UNHANDLED_INTERRUPT
#define	FILTER_HANDLED			B_HANDLED_INTERRUPT
#define	FILTER_SCHEDULE_THREAD	B_INVOKE_SCHEDULER

/* Note that we reversed the original order, so whenever actual (negative)
   numbers are used in a driver, we have to change it. */
#define BUS_PROBE_SPECIFIC		0
#define BUS_PROBE_LOW_PRIORITY	10
#define BUS_PROBE_DEFAULT		20
#define BUS_PROBE_GENERIC		100

#define __BUS_ACCESSOR(varp, var, ivarp, ivar, type)						\
																			\
static __inline type varp ## _get_ ## var(device_t dev)                 \
{                                                                       \
	return 0;														\
}                                                                       \
																			\
static __inline void varp ## _set_ ## var(device_t dev, type t)				\
{																			\
}


struct resource;

struct resource_spec {
	int	type;
	int	rid;
	int	flags;
};

enum intr_type {
	INTR_TYPE_NET	= 4,
	INTR_MPSAFE		= 512,
};


int bus_generic_detach(device_t dev);
int bus_generic_suspend(device_t dev);
int bus_generic_resume(device_t dev);
void bus_generic_shutdown(device_t dev);

typedef int (driver_filter_t)(void *arg);
typedef void driver_intr_t(void *);


int resource_int_value(const char *name, int unit, const char *resname,
	int *result);
int resource_disabled(const char *name, int unit);

struct resource *bus_alloc_resource(device_t dev, int type, int *rid,
	unsigned long start, unsigned long end, unsigned long count, uint32 flags);
int bus_release_resource(device_t dev, int type, int rid, struct resource *r);
int bus_alloc_resources(device_t dev, struct resource_spec *resourceSpec,
	struct resource **resources);
void bus_release_resources(device_t dev,
	const struct resource_spec *resourceSpec, struct resource **resources);

int	bus_child_present(device_t child);
void	bus_enumerate_hinted_children(device_t bus);

static inline struct resource *
bus_alloc_resource_any(device_t dev, int type, int *rid, uint32 flags)
{
	return bus_alloc_resource(dev, type, rid, 0, ~0, 1, flags);
}

static inline struct resource *
bus_alloc_resource_anywhere(device_t dev, int type, int *rid,
    unsigned long count, uint32 flags)
{
	return (bus_alloc_resource(dev, type, rid, 0, ~0, count, flags));
}

bus_dma_tag_t bus_get_dma_tag(device_t dev);

int bus_setup_intr(device_t dev, struct resource *r, int flags,
	driver_filter_t* filter, driver_intr_t handler, void *arg, void **_cookie);
int bus_teardown_intr(device_t dev, struct resource *r, void *cookie);
int bus_bind_intr(device_t dev, struct resource *r, int cpu);
int bus_describe_intr(device_t dev, struct resource *irq, void *cookie,
	const char* fmt, ...);

const char *device_get_name(device_t dev);
const char *device_get_nameunit(device_t dev);
int device_get_unit(device_t dev);
void *device_get_softc(device_t dev);
void device_set_softc(device_t dev, void *softc);
int device_printf(device_t dev, const char *, ...) __printflike(2, 3);
void device_set_desc(device_t dev, const char *desc);
void device_set_desc_copy(device_t dev, const char *desc);
const char *device_get_desc(device_t dev);
device_t device_get_parent(device_t dev);
u_int32_t device_get_flags(device_t dev);
devclass_t device_get_devclass(device_t dev);
int device_get_children(device_t dev, device_t **devlistp, int *devcountp);

void device_set_ivars(device_t dev, void *);
void *device_get_ivars(device_t dev);

device_t device_add_child(device_t dev, const char* name, int unit);
int device_delete_child(device_t dev, device_t child);
int device_is_attached(device_t dev);
int device_attach(device_t dev);
int device_detach(device_t dev);
int bus_print_child_header(device_t dev, device_t child);
int bus_print_child_footer(device_t dev, device_t child);
int bus_generic_print_child(device_t dev, device_t child);
void bus_generic_driver_added(device_t dev, driver_t *driver);
int bus_generic_attach(device_t dev);
int device_set_driver(device_t dev, driver_t *driver);
int device_is_alive(device_t dev);


static inline struct sysctl_ctx_list *
device_get_sysctl_ctx(device_t dev)
{
	return NULL;
}


static inline void *
device_get_sysctl_tree(device_t dev)
{
	return NULL;
}

devclass_t devclass_find(const char *classname);
device_t devclass_get_device(devclass_t dc, int unit);
int devclass_get_maxunit(devclass_t dc);

#endif	/* _FBSD_COMPAT_SYS_BUS_H_ */
