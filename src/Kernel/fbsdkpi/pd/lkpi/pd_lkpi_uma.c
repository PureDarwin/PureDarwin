// SPDX-License-Identifier: MIT
// uma(9) zones as individual heap allocations, enough for linuxkpi's caches

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <vm/uma.h>

struct uma_zone {
	const char *name;
	size_t size;
	size_t align;
	uma_ctor ctor;
	uma_dtor dtor;
	uma_init init;
	uma_fini fini;
	uint32_t flags;
	void *arg;
};

uma_zone_t
uma_zcreate(const char *name, size_t size, uma_ctor ctor, uma_dtor dtor,
	uma_init uminit, uma_fini fini, int align, uint32_t flags)
{
	uma_zone_t zone = malloc(sizeof(*zone), M_DEVBUF, M_WAITOK | M_ZERO);
	if (zone == NULL)
		return NULL;

	zone->name = name;
	zone->size = size;
	zone->align = (size_t)align + 1;
	zone->ctor = ctor;
	zone->dtor = dtor;
	zone->init = uminit;
	zone->fini = fini;
	zone->flags = flags;
	return zone;
}

uma_zone_t
uma_zcache_create(const char *name, int size, uma_ctor ctor, uma_dtor dtor,
	uma_init zinit, uma_fini zfini, uma_import zimport, uma_release zrelease,
	void *arg, int flags)
{
	// a cache zone imports items from its arg, here items come from the heap
	uma_zone_t zone = uma_zcreate(name, size > 0 ? size : 0, ctor, dtor, zinit,
		zfini, UMA_ALIGN_PTR, flags);
	if (zone != NULL)
		zone->arg = arg;
	return zone;
}

void
uma_zdestroy(uma_zone_t zone)
{
	free(zone, M_DEVBUF);
}

void *
uma_zalloc_arg(uma_zone_t zone, void *arg, int flags)
{
	// _kernel_malloc aligns small sizes to their power of two, enough for cache lines
	size_t size = zone->size > zone->align ? zone->size : zone->align;
	void *item = malloc(size, M_DEVBUF, flags | ((zone->flags & UMA_ZONE_ZINIT) ? M_ZERO : 0));
	if (item == NULL)
		return NULL;

	if (zone->init != NULL && zone->init(item, zone->size, flags) != 0) {
		free(item, M_DEVBUF);
		return NULL;
	}

	if (zone->ctor != NULL && zone->ctor(item, zone->size, arg, flags) != 0) {
		if (zone->fini != NULL)
			zone->fini(item, zone->size);
		free(item, M_DEVBUF);
		return NULL;
	}

	if ((flags & M_ZERO) != 0 && zone->ctor == NULL)
		memset(item, 0, zone->size);
	return item;
}

void
uma_zfree_arg(uma_zone_t zone, void *item, void *arg)
{
	if (item == NULL)
		return;

	if (zone->dtor != NULL)
		zone->dtor(item, zone->size, arg);
	if (zone->fini != NULL)
		zone->fini(item, zone->size);
	free(item, M_DEVBUF);
}
