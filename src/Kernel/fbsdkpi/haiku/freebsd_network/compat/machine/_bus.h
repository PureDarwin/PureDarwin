/*
 * Copyright 2022, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _FBSD_COMPAT_MACHINE__BUS_H_
#define _FBSD_COMPAT_MACHINE__BUS_H_


#include <SupportDefs.h>


typedef phys_addr_t bus_addr_t;
typedef size_t bus_size_t;
typedef addr_t bus_space_handle_t;


typedef int bus_space_tag_t;
enum {
	BUS_SPACE_TAG_INVALID = 0,

	BUS_SPACE_TAG_IO,
	BUS_SPACE_TAG_MEM,
	BUS_SPACE_TAG_IRQ,
	BUS_SPACE_TAG_MSI,
};


#endif /* _FBSD_COMPAT_MACHINE__BUS_H_ */
