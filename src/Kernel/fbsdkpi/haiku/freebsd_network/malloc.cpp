/*
 * Copyright 2010, Ingo Weinhold, ingo_weinhold@gmx.de.
 * Copyright 2019, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */

extern "C" {
#include <compat/sys/malloc.h>
}

#include <stdio.h>
#include <string.h>

#include <util/BitUtils.h>

#include <kernel/heap.h>
#include <kernel/vm/vm.h>


void*
_kernel_malloc(size_t size, int flags)
{
	// According to the FreeBSD kernel malloc man page the allocator is expected
	// to return power of two aligned addresses for allocations up to one page
	// size. While it also states that this shouldn't be relied upon, some drivers
	// may depend on it.
	void *ptr
		= memalign_etc(size >= PAGESIZE ? PAGESIZE : next_power_of_2(size), size,
			(flags & M_NOWAIT) ? HEAP_DONT_WAIT_FOR_MEMORY : 0);
	if (ptr == NULL)
		return NULL;

	if (flags & M_ZERO)
		memset(ptr, 0, size);

	return ptr;
}


void
_kernel_free(void *ptr)
{
	// FreeBSD's free also takes contigmalloc memory, which starts on a page
	if (ptr != NULL && ((uintptr_t)ptr & (B_PAGE_SIZE - 1)) == 0) {
		area_id area = area_for(ptr);
		area_info info;
		if (area >= 0 && get_area_info(area, &info) == B_OK && info.address == ptr) {
			delete_area(area);
			return;
		}
	}
	free(ptr);
}


void *
_kernel_realloc(void *ptr, size_t size, int flags)
{
	if (ptr == NULL)
		return _kernel_malloc(size, flags);

	void *fresh = _kernel_malloc(size, flags & ~M_ZERO);
	if (fresh == NULL)
		return NULL;

	size_t old = _kernel_malloc_usable_size(ptr);
	memcpy(fresh, ptr, old < size ? old : size);
	free(ptr);
	return fresh;
}


extern "C" size_t malloc_usable_size(void *ptr);

size_t
_kernel_malloc_usable_size(const void *ptr)
{
	return malloc_usable_size((void *)ptr);
}


void *
_kernel_contigmalloc(const char *file, int line, size_t size, int flags,
	vm_paddr_t low, vm_paddr_t high, unsigned long alignment,
	unsigned long boundary)
{
	const bool zero = (flags & M_ZERO) != 0, dontWait = (flags & M_NOWAIT) != 0;

	size = ROUNDUP(size, B_PAGE_SIZE);

	uint32 creationFlags = (zero ? 0 : CREATE_AREA_DONT_CLEAR)
		| (dontWait ? CREATE_AREA_DONT_WAIT : 0);

	char name[B_OS_NAME_LENGTH];
	const char* baseName = strrchr(file, '/');
	baseName = baseName != NULL ? baseName + 1 : file;
	snprintf(name, sizeof(name), "contig:%s:%d", baseName, line);

	virtual_address_restrictions virtualRestrictions = {};

	physical_address_restrictions physicalRestrictions = {};
	physicalRestrictions.low_address = low;
	physicalRestrictions.high_address = high;
	physicalRestrictions.alignment = alignment;
	physicalRestrictions.boundary = boundary;

	void* address;
	area_id area = create_area_etc(B_SYSTEM_TEAM, name, size, B_CONTIGUOUS,
		B_KERNEL_READ_AREA | B_KERNEL_WRITE_AREA, creationFlags, 0,
		&virtualRestrictions, &physicalRestrictions, &address);
	if (area < 0)
		return NULL;

	return address;
}


void
_kernel_contigfree(void *addr, size_t size)
{
	if (addr == NULL)
		return;

	delete_area(area_for(addr));
}


vm_paddr_t
pmap_kextract(vm_offset_t virtualAddress)
{
	physical_entry entry;
	status_t status = get_memory_map((void *)virtualAddress, 1, &entry, 1);
	if (status < B_OK) {
		panic("fbsd compat: get_memory_map failed for %p, error %08" B_PRIx32
			"\n", (void *)virtualAddress, status);
	}

	return (vm_paddr_t)entry.address;
}
