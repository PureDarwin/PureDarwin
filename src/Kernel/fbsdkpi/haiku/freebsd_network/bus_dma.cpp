/*
 * Copyright 2019-2022, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Augustin Cavalier <waddlesplash>
 */

extern "C" {
#include <sys/malloc.h>
#include <sys/bus.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/mbuf.h>

#include <machine/bus.h>
#include <vm/vm_extern.h>

phys_addr_t vm_page_max_address();
	// declared in <vm/vm_page.h> which we can't include here.
}

#include "device.h"
#include "pd_fbsd_os.h"
#include "pd_haiku_kernel.h"


// #pragma mark - structures


struct bus_dma_tag {
	bus_dma_tag_t	parent;
	int32			ref_count;
	int32			map_count;

	int				flags;
#define BUS_DMA_COULD_BOUNCE	BUS_DMA_BUS1

	phys_size_t		alignment;
	phys_addr_t		boundary;
	phys_addr_t		lowaddr;
	phys_addr_t		highaddr;

	phys_size_t		maxsize;
	uint32			maxsegments;
	phys_size_t		maxsegsz;

	// the IOPCIDevice, and whether an iommu remaps its dma
	void*			pd_device;
	bool			pd_has_mapper;
};

struct bus_dmamap {
	bus_dma_tag_t		dmat;

	bus_dma_segment_t*	segments;
	int					nsegs;

	void*		bounce_buffer;
	bus_size_t	bounce_buffer_size;

	// one mapping per loaded segment while an iommu is in use
	void**		pd_cookies;
	int			pd_ncookies;

	enum {
		BUFFER_NONE = 0,
		BUFFER_PROHIBITED,

		BUFFER_TYPE_SIMPLE,
		BUFFER_TYPE_MBUF,
	} buffer_type;
	union {
		struct {
			void*				buffer;
			bus_size_t			buffer_length;
		};
		struct mbuf*		mbuf;
	};
};


// #pragma mark - functions


extern "C" void
busdma_lock_mutex(void* arg, bus_dma_lock_op_t op)
{
	struct mtx* dmtx = (struct mtx*)arg;
	switch (op) {
	case BUS_DMA_LOCK:
		mtx_lock(dmtx);
	break;
	case BUS_DMA_UNLOCK:
		mtx_unlock(dmtx);
	break;
	default:
		panic("busdma_lock_mutex: unknown operation 0x%x", op);
	}
}


extern "C" int
bus_dma_tag_create(bus_dma_tag_t parent, bus_size_t alignment, bus_addr_t boundary,
	bus_addr_t lowaddr, bus_addr_t highaddr, bus_dma_filter_t* filter,
	void* filterarg, bus_size_t maxsize, int nsegments, bus_size_t maxsegsz,
	int flags, bus_dma_lock_t* lockfunc, void* lockfuncarg, bus_dma_tag_t* dmat)
{
	if (maxsegsz == 0)
		return EINVAL;
	if (filter != NULL) {
		panic("bus_dma_tag_create: error: filters not supported!");
		return EOPNOTSUPP;
	}

	bus_dma_tag_t newtag = (bus_dma_tag_t)kernel_malloc(sizeof(*newtag),
		M_DEVBUF, M_ZERO | M_NOWAIT);
	if (newtag == NULL)
		return ENOMEM;

	if (boundary != 0 && boundary < maxsegsz)
		maxsegsz = boundary;

	newtag->alignment = alignment;
	newtag->boundary = boundary;
	newtag->lowaddr = lowaddr;
	newtag->highaddr = highaddr;
	newtag->maxsize = maxsize;
	newtag->maxsegments = nsegments;
	newtag->maxsegsz = maxsegsz;
	newtag->flags = flags;
	newtag->ref_count = 1;
	newtag->map_count = 0;

	// lockfunc is only needed if callbacks will be invoked asynchronously.

	if (parent != NULL) {
		newtag->parent = parent;
		atomic_add(&parent->ref_count, 1);
		newtag->pd_device = parent->pd_device;
		newtag->pd_has_mapper = parent->pd_has_mapper;

		newtag->lowaddr = MIN(parent->lowaddr, newtag->lowaddr);
		newtag->highaddr = MAX(parent->highaddr, newtag->highaddr);
		newtag->alignment = MAX(parent->alignment, newtag->alignment);

		if (newtag->boundary == 0) {
			newtag->boundary = parent->boundary;
		} else if (parent->boundary != 0) {
			newtag->boundary = MIN(parent->boundary, newtag->boundary);
		}
	}

	if (newtag->lowaddr < vm_page_max_address())
		newtag->flags |= BUS_DMA_COULD_BOUNCE;
	if (newtag->alignment > 1)
		newtag->flags |= BUS_DMA_COULD_BOUNCE;

	*dmat = newtag;
	return 0;
}


extern "C" int
bus_dma_tag_destroy(bus_dma_tag_t dmat)
{
	if (dmat == NULL)
		return 0;
	if (dmat->map_count != 0)
		return EBUSY;

	while (dmat != NULL) {
		bus_dma_tag_t parent;

		parent = dmat->parent;
		atomic_add(&dmat->ref_count, -1);
		if (dmat->ref_count == 0) {
			kernel_free(dmat, M_DEVBUF);

			// Last reference released, so release our reference on our parent.
			dmat = parent;
		} else
			dmat = NULL;
	}
	return 0;
}


extern "C" int
bus_dmamap_create(bus_dma_tag_t dmat, int flags, bus_dmamap_t* mapp)
{
	*mapp = (bus_dmamap_t)calloc(sizeof(**mapp), 1);
	if (*mapp == NULL)
		return ENOMEM;

	(*mapp)->dmat = dmat;
	(*mapp)->nsegs = 0;
	(*mapp)->segments = (bus_dma_segment_t *)calloc(dmat->maxsegments,
		sizeof(bus_dma_segment_t));
	if ((*mapp)->segments == NULL) {
		free((*mapp));
		*mapp = NULL;
		return ENOMEM;
	}
	if (dmat->pd_device != NULL) {
		(*mapp)->pd_cookies = (void**)calloc(dmat->maxsegments, sizeof(void*));
		if ((*mapp)->pd_cookies == NULL) {
			free((*mapp)->segments);
			free((*mapp));
			*mapp = NULL;
			return ENOMEM;
		}
	}

	atomic_add(&dmat->map_count, 1);
	return 0;
}


extern "C" int
bus_dmamap_destroy(bus_dma_tag_t dmat, bus_dmamap_t map)
{
	if (map == NULL)
		return 0;
	if (map->buffer_type > bus_dmamap::BUFFER_PROHIBITED)
		return EBUSY;

	atomic_add(&map->dmat->map_count, -1);
	kernel_contigfree(map->bounce_buffer, map->bounce_buffer_size, M_DEVBUF);
	free(map->pd_cookies);
	free(map->segments);
	free(map);
	return 0;
}


static int
_allocate_dmamem(bus_dma_tag_t dmat, phys_size_t size, void** vaddr, int flags)
{
	int mflags;
	if (flags & BUS_DMA_NOWAIT)
		mflags = M_NOWAIT;
	else
		mflags = M_WAITOK;

	if (flags & BUS_DMA_ZERO)
		mflags |= M_ZERO;

	// FreeBSD uses standard malloc() for the case where size <= PAGE_SIZE,
	// but we want to keep DMA'd memory a bit more separate, so we always use
	// contigmalloc.

	// The range specified by lowaddr, highaddr is an *exclusion* range,
	// not an inclusion range. So we want to at least start with the low end,
	// if possible. (The most common exclusion range is 32-bit only, and
	// ones other than that are very rare, so typically this will succeed.)
	if (dmat->lowaddr > B_PAGE_SIZE) {
		*vaddr = kernel_contigmalloc(size, M_DEVBUF, mflags,
			0, dmat->lowaddr,
			dmat->alignment ? dmat->alignment : 1ul, dmat->boundary);
		if (*vaddr == NULL)
			dprintf("bus_dmamem_alloc: failed to allocate with lowaddr "
				"0x%" B_PRIxPHYSADDR "\n", dmat->lowaddr);
	}
	if (*vaddr == NULL && dmat->highaddr < BUS_SPACE_MAXADDR) {
		*vaddr = kernel_contigmalloc(size, M_DEVBUF, mflags,
			dmat->highaddr, BUS_SPACE_MAXADDR,
			dmat->alignment ? dmat->alignment : 1ul, dmat->boundary);
	}

	if (*vaddr == NULL) {
		dprintf("bus_dmamem_alloc: failed to allocate for tag (size %d, "
			"low 0x%" B_PRIxPHYSADDR ", high 0x%" B_PRIxPHYSADDR ", "
			"boundary 0x%" B_PRIxPHYSADDR ")\n",
			(int)size, dmat->lowaddr, dmat->highaddr, dmat->boundary);
		return ENOMEM;
	} else if (vtophys(*vaddr) & (dmat->alignment - 1)) {
		dprintf("bus_dmamem_alloc: failed to align memory: wanted %#x, got %#x\n",
			dmat->alignment, vtophys(vaddr));
		bus_dmamem_free_tagless(*vaddr, size);
		return ENOMEM;
	}

	return 0;
}


extern "C" int
bus_dmamem_alloc(bus_dma_tag_t dmat, void** vaddr, int flags,
	bus_dmamap_t* mapp)
{
	// FreeBSD does not permit the "mapp" argument to be NULL, but we do
	// (primarily for the OpenBSD shims.)
	if (mapp != NULL) {
		bus_dmamap_create(dmat, flags, mapp);

		// Drivers assume dmamem will never be bounced, so ensure that.
		(*mapp)->buffer_type = bus_dmamap::BUFFER_PROHIBITED;
	}

	int status = _allocate_dmamem(dmat, dmat->maxsize, vaddr, flags);
	if (status != 0 && mapp != NULL)
		bus_dmamap_destroy(dmat, *mapp);
	return status;
}


extern "C" void
bus_dmamem_free_tagless(void* vaddr, size_t size)
{
	kernel_contigfree(vaddr, size, M_DEVBUF);
}


extern "C" void
bus_dmamem_free(bus_dma_tag_t dmat, void* vaddr, bus_dmamap_t map)
{
	bus_dmamem_free_tagless(vaddr, dmat->maxsize);
	bus_dmamap_destroy(dmat, map);
}


static int
_prepare_bounce_buffer(bus_dmamap_t map, bus_size_t reqsize, int flags)
{
	if (map->buffer_type == bus_dmamap::BUFFER_PROHIBITED) {
		panic("cannot bounce, direct DMA only!");
		return B_NOT_ALLOWED;
	}
	if (map->buffer_type != bus_dmamap::BUFFER_NONE) {
		panic("bounce buffer already in use! (type %d)", map->buffer_type);
		return EBUSY;
	}

	if (map->bounce_buffer_size >= reqsize)
		return 0;

	if (map->bounce_buffer != NULL) {
		kernel_contigfree(map->bounce_buffer, map->bounce_buffer_size, 0);
		map->bounce_buffer = NULL;
		map->bounce_buffer_size = 0;
	}

	// The contiguous allocator will round up anyway, so we might as well
	// do it first so that we know how large our buffer really is.
	reqsize = roundup(reqsize, B_PAGE_SIZE);

	int error = _allocate_dmamem(map->dmat, reqsize, &map->bounce_buffer, flags);
	if (error != 0)
		return error;
	map->bounce_buffer_size = reqsize;

	return 0;
}


static bool
_validate_address(bus_dma_tag_t dmat, bus_addr_t paddr, bool validate_alignment = true)
{
	if (paddr > dmat->lowaddr && paddr <= dmat->highaddr)
		return false;
	if (validate_alignment && !vm_addr_align_ok(paddr, dmat->alignment))
		return false;

	return true;
}


static int
_bus_load_buffer(bus_dma_tag_t dmat, void* buf, bus_size_t buflen,
	int flags, bus_addr_t& last_phys_addr, bus_dma_segment_t* segs,
	int& seg, bool first)
{
	vm_offset_t virtual_addr = (vm_offset_t)buf;
	const bus_addr_t boundary_mask = ~(dmat->boundary - 1);

	while (buflen > 0) {
		const bus_addr_t phys_addr = pmap_kextract(virtual_addr);

		bus_size_t segment_size = PAGESIZE - (phys_addr & PAGE_MASK);
		if (segment_size > buflen)
			segment_size = buflen;
		if (segment_size > dmat->maxsegsz)
			segment_size = dmat->maxsegsz;

		if (dmat->boundary > 0) {
			// Make sure we don't cross a boundary.
			bus_addr_t boundary_addr = (phys_addr + dmat->boundary) & boundary_mask;
			if (segment_size > (boundary_addr - phys_addr))
				segment_size = (boundary_addr - phys_addr);
		}

		// If possible, coalesce into the previous segment.
		if (!first && phys_addr == last_phys_addr
				&& (segs[seg].ds_len + segment_size) <= dmat->maxsegsz
				&& (dmat->boundary == 0
					|| (segs[seg].ds_addr & boundary_mask)
						== (phys_addr & boundary_mask))) {
			if (!_validate_address(dmat, phys_addr, false))
				return ERANGE;

			segs[seg].ds_len += segment_size;
		} else {
			if (first)
				first = false;
			else if (++seg >= dmat->maxsegments)
				break;

			if (!_validate_address(dmat, phys_addr))
				return ERANGE;

			segs[seg].ds_addr = phys_addr;
			segs[seg].ds_len = segment_size;
		}

		last_phys_addr = phys_addr + segment_size;
		virtual_addr += segment_size;
		buflen -= segment_size;
	}

	return (buflen != 0 ? EFBIG : 0);
}


extern "C" int
pd_bus_dma_tag_has_mapper(bus_dma_tag_t tag)
{
	return tag != NULL && tag->pd_has_mapper;
}


extern "C" void
pd_bus_dma_tag_set_device(bus_dma_tag_t tag, void* pcidev)
{
	tag->pd_device = pcidev;
	tag->pd_has_mapper = pdos_dma_has_mapper(pcidev) != 0;
}


static void
_pd_unmap_segments(bus_dmamap_t map)
{
	for (int i = 0; i < map->pd_ncookies; i++)
		pdos_dma_unmap(map->pd_cookies[i]);
	map->pd_ncookies = 0;
}


// rewrites physical segments to bus addresses through the device's iommu
static int
_pd_map_segments(bus_dma_tag_t dmat, bus_dmamap_t map, bus_dma_segment_t* segs,
	int nsegs)
{
	// haiku's own bounce buffers already respect the address limits
	if (!dmat->pd_has_mapper || map == NULL)
		return 0;

	_pd_unmap_segments(map);
	for (int i = 0; i < nsegs; i++) {
		uint64_t busaddr;
		void* cookie = pdos_dma_map_phys(dmat->pd_device, segs[i].ds_addr,
			segs[i].ds_len, 64, &busaddr);
		if (cookie == NULL) {
			_pd_unmap_segments(map);
			return ENOMEM;
		}
		map->pd_cookies[map->pd_ncookies++] = cookie;
		segs[i].ds_addr = busaddr;
	}
	return 0;
}


// how many address bits a tag's exclusion range leaves the device
static int
_pd_address_bits(bus_dma_tag_t dmat)
{
	if (dmat->lowaddr >= BUS_SPACE_MAXADDR)
		return 64;
	int bits = 0;
	while (bits < 64 && (dmat->lowaddr >> bits) != 0)
		bits++;
	return bits;
}


// true when a physical range needs no mapping, the linuxkpi fast path
extern "C" bool
bus_dma_id_mapped(bus_dma_tag_t dmat, vm_paddr_t buf, bus_size_t buflen)
{
	if (dmat == NULL)
		return true;
	return !dmat->pd_has_mapper && buf + buflen - 1 <= dmat->lowaddr;
}


// one physically contiguous range, mapped or bounced by the seam as needed
extern "C" int
_bus_dmamap_load_phys(bus_dma_tag_t dmat, bus_dmamap_t map, vm_paddr_t buf,
	bus_size_t buflen, int flags, bus_dma_segment_t* segs, int* segp)
{
	if (dmat->pd_device == NULL || map->pd_cookies == NULL)
		return EINVAL;

	_pd_unmap_segments(map);
	uint64_t busaddr;
	void* cookie = pdos_dma_map_phys(dmat->pd_device, buf, buflen,
		_pd_address_bits(dmat), &busaddr);
	if (cookie == NULL)
		return ENOMEM;

	map->pd_cookies[map->pd_ncookies++] = cookie;
	segs[0].ds_addr = busaddr;
	segs[0].ds_len = buflen;
	*segp = 0;
	return 0;
}


extern "C" int
bus_dmamap_load(bus_dma_tag_t dmat, bus_dmamap_t map, void *buf,
	bus_size_t buflen, bus_dmamap_callback_t *callback,
	void *callback_arg, int flags)
{
	bus_addr_t lastaddr = 0;
	int error, seg = 0;

	if (buflen > dmat->maxsize)
		return EINVAL;

	error = _bus_load_buffer(dmat, buf, buflen, flags,
		lastaddr, map->segments, seg, true);

	if (error != 0) {
		// Try again using a bounce buffer.
		error = _prepare_bounce_buffer(map, buflen, flags);
		if (error != 0)
			return error;

		map->buffer_type = bus_dmamap::BUFFER_TYPE_SIMPLE;
		map->buffer = buf;
		map->buffer_length = buflen;

		seg = lastaddr = 0;
		error = _bus_load_buffer(dmat, map->bounce_buffer, buflen, flags,
			lastaddr, map->segments, seg, true);
	}

	if (error == 0)
		error = _pd_map_segments(dmat, map, map->segments, seg + 1);

	if (error)
		(*callback)(callback_arg, map->segments, 0, error);
	else
		(*callback)(callback_arg, map->segments, seg + 1, 0);

	// ENOMEM is returned; all other errors are only sent to the callback.
	if (error == ENOMEM)
		return error;
	return 0;
}


extern "C" int
bus_dmamap_load_mbuf_sg(bus_dma_tag_t dmat, bus_dmamap_t map, struct mbuf* mb,
	bus_dma_segment_t* segs, int* _nsegs, int flags)
{
	M_ASSERTPKTHDR(mb);

	if (mb->m_pkthdr.len > dmat->maxsize)
		return EINVAL;

	int seg = 0, error = 0;
	bool first = true;
	bus_addr_t lastaddr = 0;
	flags |= BUS_DMA_NOWAIT;

	for (struct mbuf* m = mb; m != NULL && error == 0; m = m->m_next) {
		if (m->m_len <= 0)
			continue;

		error = _bus_load_buffer(dmat, m->m_data, m->m_len,
			flags, lastaddr, segs, seg, first);
		first = false;
	}

	if (error != 0) {
		// Try again using a bounce buffer.
		error = _prepare_bounce_buffer(map, mb->m_pkthdr.len, flags);
		if (error != 0)
			return error;

		map->buffer_type = bus_dmamap::BUFFER_TYPE_MBUF;
		map->mbuf = mb;

		seg = lastaddr = 0;
		error = _bus_load_buffer(dmat, map->bounce_buffer, mb->m_pkthdr.len, flags,
			lastaddr, segs, seg, true);
	}

	if (error == 0)
		error = _pd_map_segments(dmat, map, segs, seg + 1);

	*_nsegs = seg + 1;
	return error;
}


extern "C" int
bus_dmamap_load_mbuf(bus_dma_tag_t dmat, bus_dmamap_t map, struct mbuf* mb,
	bus_dmamap_callback2_t* callback, void* callback_arg, int flags)
{
	int nsegs, error;
	error = bus_dmamap_load_mbuf_sg(dmat, map, mb, map->segments, &nsegs, flags);

	if (error) {
		(*callback)(callback_arg, map->segments, 0, 0, error);
	} else {
		(*callback)(callback_arg, map->segments, nsegs, mb->m_pkthdr.len,
			error);
	}
	return error;
}


extern "C" void
bus_dmamap_unload(bus_dma_tag_t dmat, bus_dmamap_t map)
{
	if (map == NULL)
		return;

	_pd_unmap_segments(map);
	if (map->buffer_type != bus_dmamap::BUFFER_PROHIBITED)
		map->buffer_type = bus_dmamap::BUFFER_NONE;
	map->buffer = NULL;
}


extern "C" void
bus_dmamap_sync(bus_dma_tag_t dmat, bus_dmamap_t map, bus_dmasync_op_t op)
{
	if (map == NULL)
		return;

	bus_size_t length = 0;
	switch (map->buffer_type) {
		case bus_dmamap::BUFFER_NONE:
		case bus_dmamap::BUFFER_PROHIBITED:
			// Nothing to do.
			return;

		case bus_dmamap::BUFFER_TYPE_SIMPLE:
			length = map->buffer_length;
			break;

		case bus_dmamap::BUFFER_TYPE_MBUF:
			length = map->mbuf->m_pkthdr.len;
			break;

		default:
			panic("unknown buffer type");
	}

	bus_dmamap_sync_etc(dmat, map, 0, length, op);
}


extern "C" void
bus_dmamap_sync_etc(bus_dma_tag_t dmat, bus_dmamap_t map,
	bus_addr_t offset, bus_size_t length, bus_dmasync_op_t op)
{
	if (map == NULL)
		return;

	if ((op & (BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD)) != 0) {
		for (int i = 0; i < map->pd_ncookies; i++)
			pdos_dma_sync(map->pd_cookies[i], 1);
	}

	if ((op & BUS_DMASYNC_PREWRITE) != 0) {
		// "Pre-write": after CPU writes, before device reads.
		switch (map->buffer_type) {
			case bus_dmamap::BUFFER_NONE:
			case bus_dmamap::BUFFER_PROHIBITED:
				// Nothing to do.
				break;

			case bus_dmamap::BUFFER_TYPE_SIMPLE:
				KASSERT((offset + length) <= map->buffer_length, ("mis-sized sync"));
				memcpy((caddr_t)map->bounce_buffer + offset,
					(caddr_t)map->buffer + offset, length);
				break;

			case bus_dmamap::BUFFER_TYPE_MBUF:
				m_copydata(map->mbuf, offset, length,
					(caddr_t)map->bounce_buffer + offset);
				break;

			default:
				panic("unknown buffer type");
		}

		memory_write_barrier();
	}

	if ((op & BUS_DMASYNC_POSTREAD) != 0) {
		// "Post-read": after device writes, before CPU reads.
		memory_read_barrier();
		for (int i = 0; i < map->pd_ncookies; i++)
			pdos_dma_sync(map->pd_cookies[i], 0);

		switch (map->buffer_type) {
			case bus_dmamap::BUFFER_NONE:
			case bus_dmamap::BUFFER_PROHIBITED:
				// Nothing to do.
				break;

			case bus_dmamap::BUFFER_TYPE_SIMPLE:
				KASSERT((offset + length) <= map->buffer_length, ("mis-sized sync"));
				memcpy((caddr_t)map->buffer + offset,
					(caddr_t)map->bounce_buffer + offset, length);
				break;

			case bus_dmamap::BUFFER_TYPE_MBUF:
				m_copyback(map->mbuf, offset, length,
					(caddr_t)map->bounce_buffer + offset);
				break;

			default:
				panic("unknown buffer type");
		}
	}
}
