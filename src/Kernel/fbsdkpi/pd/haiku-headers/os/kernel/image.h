/*
 * Copyright 2007-2019, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _IMAGE_H
#define	_IMAGE_H


#include <OS.h>
#include <sys/param.h>


typedef	int32 image_id;

typedef enum {
	B_APP_IMAGE			= 1,
	B_LIBRARY_IMAGE,
	B_ADD_ON_IMAGE,
	B_SYSTEM_IMAGE
} image_type;

typedef struct {
	image_id	id;
	image_type	type;
	int32		sequence;
	int32		init_order;
	void		(*init_routine)();
	void		(*term_routine)();
	dev_t		device;
	ino_t		node;
	char		name[MAXPATHLEN];
	void		*text;
	void		*data;
	int32		text_size;
	int32		data_size;

	/* Haiku R1 extensions */
	int32		api_version;	/* the Haiku API version used by the image */
	int32		abi;			/* the Haiku ABI used by the image */
} image_info;


#ifdef __cplusplus
extern "C" {
#endif


/* flags for clear_caches() */
#define B_FLUSH_DCACHE				0x0001	/* data cache */
#define B_FLUSH_ICACHE				0x0004	/* instruction cache */
#define B_INVALIDATE_DCACHE			0x0002
#define B_INVALIDATE_ICACHE			0x0008


/* symbol types */
#define B_SYMBOL_TYPE_DATA			0x1
#define B_SYMBOL_TYPE_TEXT			0x2
#define B_SYMBOL_TYPE_ANY			0x5


/* initialization/termination functions of shared objects */
#define B_INIT_BEFORE_FUNCTION_NAME	"initialize_before"
#define B_INIT_AFTER_FUNCTION_NAME	"initialize_after"
#define B_TERM_BEFORE_FUNCTION_NAME	"terminate_before"
#define B_TERM_AFTER_FUNCTION_NAME	"terminate_after"

void initialize_before(image_id self);
void initialize_after(image_id self);
void terminate_before(image_id self);
void terminate_after(image_id self);


#define B_APP_IMAGE_SYMBOL		((void*)(addr_t)0)
	/* value that can be used instead of a pointer to a symbol in the program
	   image. */
#define B_CURRENT_IMAGE_SYMBOL	((void*)__func__)
	/* pointer to a symbol in the callers image */


thread_id load_image(int32 argc, const char **argv, const char **environ);
image_id load_add_on(const char *path);
status_t unload_add_on(image_id image);
status_t get_image_symbol(image_id image, const char *name, int32 symbolType,
				void **_symbolLocation);
status_t get_nth_image_symbol(image_id image, int32 n, char *nameBuffer,
				int32 *_nameLength, int32 *_symbolType, void **_symbolLocation);
void clear_caches(void *address, size_t length, uint32 flags);

#define get_image_info(image, info) \
				_get_image_info((image), (info), sizeof(*(info)))
#define get_next_image_info(team, cookie, info) \
				_get_next_image_info((team), (cookie), (info), sizeof(*(info)))

/* private, use the macros above */
status_t _get_image_info(image_id image, image_info *info, size_t size);
status_t _get_next_image_info(team_id team, int32 *cookie, image_info *info,
				size_t size);


#ifdef __cplusplus
}
#endif


#endif	/* _IMAGE_H */
