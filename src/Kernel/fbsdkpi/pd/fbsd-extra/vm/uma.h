// SPDX-License-Identifier: MIT
// FreeBSD's vm/uma.h zone api
#pragma once
#include_next <vm/uma.h>

// uma(9) zones as plain malloc with the zone's ctor and dtor, pd_lkpi_uma.c
typedef struct uma_zone *uma_zone_t;
typedef int (*uma_ctor)(void *mem, int size, void *arg, int flags);
typedef void (*uma_dtor)(void *mem, int size, void *arg);
typedef int (*uma_init)(void *mem, int size, int flags);
typedef void (*uma_fini)(void *mem, int size);
typedef int (*uma_import)(void *arg, void **store, int count, int domain, int flags);
typedef void (*uma_release)(void *arg, void **store, int count);

#define UMA_ALIGN_PTR (sizeof(void *) - 1)
#define UMA_ALIGN_LONG (sizeof(long) - 1)
#define UMA_ALIGN_CACHE 63
#define UMA_ZONE_ZINIT 0x0002
#define UMA_ZONE_NOFREE 0x0020
#define UMA_ZONE_CONTIG 0x0080

uma_zone_t uma_zcreate(const char *name, size_t size, uma_ctor ctor, uma_dtor dtor,
    uma_init uminit, uma_fini fini, int align, uint32_t flags);
uma_zone_t uma_zcache_create(const char *name, int size, uma_ctor ctor, uma_dtor dtor,
    uma_init zinit, uma_fini zfini, uma_import zimport, uma_release zrelease,
    void *arg, int flags);
void uma_zdestroy(uma_zone_t zone);
void *uma_zalloc_arg(uma_zone_t zone, void *arg, int flags);
void uma_zfree_arg(uma_zone_t zone, void *item, void *arg);
static inline void *uma_zalloc(uma_zone_t zone, int flags) { return uma_zalloc_arg(zone, NULL, flags); }
static inline void uma_zfree(uma_zone_t zone, void *item) { uma_zfree_arg(zone, item, NULL); }
static inline void uma_zone_reserve(uma_zone_t zone, int items) { }
static inline void uma_prealloc(uma_zone_t zone, int items) { }
