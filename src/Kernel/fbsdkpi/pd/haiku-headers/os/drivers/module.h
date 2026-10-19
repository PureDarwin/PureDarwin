/*
 * Copyright 2002-2008, Haiku Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _MODULE_H
#define _MODULE_H


#include <OS.h>


/* Every module exports a list of module_info structures.
 * It defines the interface of the module and the name
 * that is used to access the interface.
 */

typedef struct module_info {
	const char	*name;
	uint32		flags;
	status_t	(*std_ops)(int32, ...);
} module_info;

/* module standard operations */
#define	B_MODULE_INIT	1
#define	B_MODULE_UNINIT	2

/* module flags */
#define	B_KEEP_LOADED	0x00000001


/* Use the module_dependency structure to let the
 * kernel automatically load modules yet depend on
 * before B_MODULE_INIT is called.
 */

typedef struct module_dependency {
	const char	*name;
	module_info	**info;
} module_dependency;


#ifdef __cplusplus
extern "C" {
#endif

extern status_t get_module(const char *path, module_info **_info);
extern status_t put_module(const char *path);
extern status_t get_next_loaded_module_name(uint32 *cookie, char *buffer,
	size_t *_bufferSize);
extern void *open_module_list_etc(const char *prefix, const char *suffix);
extern void *open_module_list(const char *prefix);
extern status_t close_module_list(void *cookie);
extern status_t read_next_module_name(void *cookie, char *buffer,
	size_t *_bufferSize);

#ifdef __cplusplus
}
#endif

#endif	/* _MODULE_H */
