/* 	DPC module API
 *  Copyright 2007-2008, Haiku Inc. All Rights Reserved.
 *  Distributed under the terms of the MIT License
 */
 
#ifndef _DPC_MODULE_H_
#define _DPC_MODULE_H_

#include <OS.h>
#include <module.h>

#ifdef __cplusplus
extern "C" {
#endif

#define B_DPC_MODULE_NAME "generic/dpc/v1"

typedef void (*dpc_func) (void *arg);

typedef struct {
	module_info	info;

	status_t	(*new_dpc_queue)(void **queue, const char *name, int32 priority);
	status_t	(*delete_dpc_queue)(void *queue);
	status_t	(*queue_dpc)(void *queue, dpc_func func, void *arg);
} dpc_module_info;


#ifdef __cplusplus
}
#endif

#endif		// _DPC_MODULE_H_
