/*
 * Copyright 2009, Colin Günther, coling@gmx.de.
 * Copyright 2007, Hugo Santos. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _FBSD_COMPAT_SYS_MALLOC_H_
#define _FBSD_COMPAT_SYS_MALLOC_H_


#include <malloc.h>

#include <vm/vm.h>

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/_mutex.h>


/*
 * flags to malloc.
 */
#define M_NOWAIT		0x0001
#define M_WAITOK		0x0002
#define M_ZERO			0x0100

#define	M_MAGIC			877983977	/* time when first defined :-) */

#define M_DEVBUF		0


struct malloc_type {
};


void *_kernel_malloc(size_t size, int flags);
void _kernel_free(void *ptr);

void *_kernel_contigmalloc(const char *file, int line, size_t size, int flags,
	vm_paddr_t low, vm_paddr_t high, unsigned long alignment,
	unsigned long boundary);
void _kernel_contigfree(void *addr, unsigned long size);

#define kernel_malloc(size, base, flags) \
	_kernel_malloc(size, flags)

#define kernel_free(ptr, tag) \
	_kernel_free(ptr)

#define kernel_contigmalloc(size, type, flags, low, high, alignment, boundary) \
	_kernel_contigmalloc(__FILE__, __LINE__, size, flags, low, high, \
		alignment, boundary)

#define kernel_contigfree(addr, size, base) \
	_kernel_contigfree(addr, size)

#ifdef FBSD_DRIVER
#	define malloc(size, tag, flags)	kernel_malloc(size, tag, flags)
#	ifdef PD_LKPI_DRM
// drm has members named free, so the two-argument form is a function here
static inline void pd_fbsd_free(void *pointer, ...) { _kernel_free(pointer); }
#	define free pd_fbsd_free
#	else
#	define free(pointer, tag)		kernel_free(pointer, tag)
#	endif
#	define contigmalloc(size, type, flags, low, high, alignment, boundary) \
		_kernel_contigmalloc(__FILE__, __LINE__, size, flags, low, high, \
			alignment, boundary)
#	define contigfree(addr, size, base) \
		_kernel_contigfree(addr, size)
#endif

void *_kernel_realloc(void *ptr, size_t size, int flags);
size_t _kernel_malloc_usable_size(const void *ptr);

#ifdef PD_LINUXKPI
// the kernel-style realloc and friends linuxkpi calls
#define realloc(ptr, size, tag, flags) _kernel_realloc(ptr, size, flags)
#define reallocf(ptr, size, tag, flags) _kernel_realloc(ptr, size, flags)
#define malloc_usable_size(ptr) _kernel_malloc_usable_size(ptr)
#define mallocarray(n, size, tag, flags) kernel_malloc((n) * (size), tag, flags)
#define malloc_domainset(size, tag, ds, flags) kernel_malloc(size, tag, flags)
#define malloc_aligned(size, align, tag, flags) _kernel_malloc(size, flags)
#define zfree(ptr, tag) kernel_free(ptr, tag)
#endif

#define	MALLOC_DEFINE(type, shortdesc, longdesc)	int type[1]

#define	MALLOC_DECLARE(type) \
		extern int type[1]

#endif	/* _FBSD_COMPAT_SYS_MALLOC_H_ */
