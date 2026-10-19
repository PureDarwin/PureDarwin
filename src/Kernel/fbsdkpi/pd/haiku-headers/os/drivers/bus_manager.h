/*
 * Copyright 2009, Haiku Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _BUS_MANAGER_H
#define _BUS_MANAGER_H


#include <module.h>


typedef struct bus_manager_info bus_manager_info;

struct bus_manager_info {
	module_info		minfo;
	status_t		(*rescan)();
};


#endif	/* _BUS_MANAGER_H */
