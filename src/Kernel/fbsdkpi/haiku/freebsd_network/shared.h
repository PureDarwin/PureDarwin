/*
 * Copyright 2009, Colin Günther, coling@gmx.de. All Rights Reserved.
 * Copyright 2007, Axel Dörfler, axeld@pinc-software.de. All Rights Reserved.
 * Copyright 2007, Hugo Santos. All Rights Reserved.
 * Copyright 2004, Marcus Overhagen. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef SHARED_H_
#define SHARED_H_


#include <sys/bus.h>


#define MAX_DEVICES	8


struct ifnet;

struct device {
	struct device	*parent;
	struct device	*root;

	driver_t		*driver;
	struct list		children;

	uint32			flags;

	char			device_name[128];
	int				unit;
	char			nameunit[64];
	const char		*description;
	void			*softc;
	void			*ivars;

	struct {
		void* (*device_register)(device_t dev);
		int (*device_probe)(device_t dev);
		int (*device_attach)(device_t dev);
		int (*device_detach)(device_t dev);
		int (*device_suspend)(device_t dev);
		int (*device_resume)(device_t dev);
		void (*device_shutdown)(device_t dev);

		int (*miibus_readreg)(device_t, int, int);
		int (*miibus_writereg)(device_t, int, int, int);
		void (*miibus_statchg)(device_t);
		void (*miibus_linkchg)(device_t);
		void (*miibus_mediainit)(device_t);

		int (*bus_child_location_str)(device_t dev, device_t child,
			char *buf, size_t buflen);
		int (*bus_child_pnpinfo_str)(device_t dev, device_t child,
			char *buf, size_t buflen);
		void (*bus_hinted_child)(device_t dev, const char *name, int unit);
		int (*bus_print_child)(device_t dev, device_t child);
		int (*bus_read_ivar)(device_t dev, device_t child, int which,
		    uintptr_t *result);
		bus_dma_tag_t (*bus_get_dma_tag)(device_t dev);
	} methods;

	struct list_link link;
};


extern const char *gDeviceNameList[];
extern struct ifnet *gDevices[];
extern int32 gDeviceCount;

#endif /* SHARED_H_ */
