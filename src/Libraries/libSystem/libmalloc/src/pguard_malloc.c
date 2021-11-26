/*
 * Copyright (c) 2020 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#include "pguard_malloc.h"

#include <TargetConditionals.h>
#if !TARGET_OS_DRIVERKIT
# include <dlfcn.h>  // dladdr()
#endif
#include <mach/mach_time.h>  // mach_absolute_time()
#include <sys/codesign.h>  // csops()

#include "internal.h"


#pragma mark -
#pragma mark Types and Structures

static const char * const slot_state_labels[] = {
	"unused", "allocated", "freed"
};

typedef enum {
	ss_unused,
	ss_allocated,
	ss_freed
} slot_state_t;

MALLOC_STATIC_ASSERT(ss_unused == 0, "unused encoded with 0");
MALLOC_STATIC_ASSERT(ss_freed < (1 << 2), "enum encodable with 2 bits");

typedef struct {
	slot_state_t state : 2;
	uint32_t metadata : 30; // metadata << slots, so borrowing 2 bits here is okay.
	uint16_t size;
	uint16_t offset;
} slot_t;

MALLOC_STATIC_ASSERT(PAGE_MAX_SIZE <= UINT16_MAX, "16 bits for page offsets");
MALLOC_STATIC_ASSERT(sizeof(slot_t) == 8, "slot_t size");

// typedef struct { ... } stack_trace_t;

#define STACK_TRACE_SIZE (MALLOC_TARGET_64BIT ? 144 : 80)
MALLOC_STATIC_ASSERT(sizeof(stack_trace_t) <= STACK_TRACE_SIZE, "stack_trace_t size");

typedef struct {
	uint32_t slot;
	stack_trace_t alloc_trace;
	stack_trace_t dealloc_trace;
} metadata_t;

#define METADATA_SIZE (MALLOC_TARGET_64BIT ? 296 : 168)
MALLOC_STATIC_ASSERT(sizeof(metadata_t) <= METADATA_SIZE, "metadata_t size");

typedef struct {
	// Malloc zone
	malloc_zone_t malloc_zone;
	malloc_zone_t *wrapped_zone;

	// Configuration
	uint32_t num_slots;
	uint32_t max_allocations;
	uint32_t max_metadata;
	uint32_t sample_counter_range;
	boolean_t signal_handler;
	boolean_t debug_log;
	uint64_t debug_log_throttle_ms;

	// Quarantine
	size_t size;
	vm_address_t begin;
	vm_address_t end;

	// Metadata
	slot_t *slots;
	metadata_t *metadata;
	uint8_t padding[PAGE_MAX_SIZE];

	// Mutable state
	_malloc_lock_s lock;
	uint32_t num_allocations;
	uint32_t num_metadata;
	uint32_t rr_slot_index;

	// Statistics
	size_t size_in_use;
	size_t max_size_in_use;
	uint64_t last_log_time;
} pguard_zone_t;

MALLOC_STATIC_ASSERT(__offsetof(pguard_zone_t, malloc_zone) == 0,
		"pguard_zone_t instances must be usable as regular zones");
MALLOC_STATIC_ASSERT(__offsetof(pguard_zone_t, padding) < PAGE_MAX_SIZE,
		"First page is mapped read-only");
MALLOC_STATIC_ASSERT(__offsetof(pguard_zone_t, lock) >= PAGE_MAX_SIZE,
		"Mutable state is on separate page");
MALLOC_STATIC_ASSERT(sizeof(pguard_zone_t) < (2 * PAGE_MAX_SIZE),
		"Zone fits on 2 pages");


#pragma mark -
#pragma mark Decider Functions

// The "decider" functions are performance critical.  They should be inlinable and must not lock.

MALLOC_ALWAYS_INLINE
static inline boolean_t
is_full(pguard_zone_t *zone)
{
	return zone->num_allocations == zone->max_allocations;
}

static uint32_t rand_uniform(uint32_t upper_bound);

MALLOC_ALWAYS_INLINE
static inline boolean_t
should_sample_counter(uint32_t counter_range)
{
	MALLOC_STATIC_ASSERT(sizeof(void *) >= sizeof(uint32_t), "Pointer is used as 32bit counter");
	uint32_t counter = (uint32_t)_os_tsd_get_direct(__TSD_MALLOC_PGUARD_SAMPLE_COUNTER);
	// 0 -> regenerate counter; 1 -> sample allocation
	if (counter == 0) {
		counter = rand_uniform(counter_range);
	} else {
		counter--;
	}
	_os_tsd_set_direct(__TSD_MALLOC_PGUARD_SAMPLE_COUNTER, (void *)(uintptr_t)counter);
	return counter == 0;
}

MALLOC_ALWAYS_INLINE
static inline boolean_t
should_sample(pguard_zone_t *zone, size_t size)
{
	boolean_t good_size = (size <= PAGE_SIZE);
	boolean_t not_full = !is_full(zone); // Optimization: racy check; we check again in allocate() for correctness.
	return good_size && not_full && should_sample_counter(zone->sample_counter_range);
}

MALLOC_ALWAYS_INLINE
static inline boolean_t
is_guarded(pguard_zone_t *zone, vm_address_t addr)
{
	return zone->begin <= addr && addr < zone->end;
}


#pragma mark -
#pragma mark Slot <-> Address Mapping

static size_t
quarantine_size(uint32_t num_slots)
{
	return (2 * num_slots + 1) * PAGE_SIZE;
}

static vm_address_t
page_addr(pguard_zone_t *zone, uint32_t slot)
{
	MALLOC_ASSERT(slot < zone->num_slots);
	uint32_t page = 1 + 2 * slot;
	vm_offset_t offset = page * PAGE_SIZE;
	return zone->begin + offset;
}

static vm_address_t
block_addr(pguard_zone_t *zone, uint32_t slot) {
	vm_address_t page = page_addr(zone, slot);
	uint16_t offset = zone->slots[slot].offset;
	return page + offset;
}

static uint32_t
page_idx(pguard_zone_t *zone, vm_address_t addr)
{
	MALLOC_ASSERT(is_guarded(zone, addr));
	vm_offset_t offset = addr - zone->begin;
	return (uint32_t)(offset / PAGE_SIZE);
}

static boolean_t
is_guard_page(pguard_zone_t *zone, vm_address_t addr)
{
	return page_idx(zone, addr) % 2 == 0;
}


#pragma mark -
#pragma mark Slot Lookup

static uint32_t
nearest_slot(pguard_zone_t *zone, vm_address_t addr)
{
	if (addr < (zone->begin + PAGE_SIZE)) {
		return 0;
	}
	if (addr >= (zone->end - PAGE_SIZE)) {
		return zone->num_slots - 1;
	}

	uint32_t page = page_idx(zone, addr);
	uint32_t slot = (page - 1) / 2;
	boolean_t guard_page = is_guard_page(zone, addr);
	boolean_t right_half = ((addr % PAGE_SIZE) >= (PAGE_SIZE / 2));

	if (guard_page && right_half) {
		slot++; // Round up.
	}
	return slot;
}

typedef enum {
	b_block_addr,			// Canonical block address.
	b_valid,					// Address within block.
	b_oob_slot,				// Outside block, but within slot.
	b_oob_guard_page	// Guard page.
} bounds_status_t;

typedef struct {
	uint32_t slot;
	bounds_status_t bounds : 31;
	boolean_t live_block_addr : 1; // Canonical block address for live allocation.
} slot_lookup_t;

MALLOC_STATIC_ASSERT(sizeof(slot_lookup_t) == 8, "slot_lookup_t size");

static slot_lookup_t
lookup_slot(pguard_zone_t *zone, vm_address_t addr)
{
	MALLOC_ASSERT(is_guarded(zone, addr));
	MALLOC_ASSERT(zone->begin % PAGE_SIZE == 0);

	uint32_t slot = nearest_slot(zone, addr);
	uint16_t offset = (addr % PAGE_SIZE);
	uint16_t begin = zone->slots[slot].offset;
	uint16_t end = begin + zone->slots[slot].size;

	bounds_status_t bounds;
	if (is_guard_page(zone, addr)) {
		bounds = b_oob_guard_page;
	} else if (offset == begin) {
		bounds = b_block_addr;
	} else if (begin < offset && offset < end) {
		bounds = b_valid;
	} else {
		bounds = b_oob_slot;
	}

	boolean_t live_slot = (zone->slots[slot].state == ss_allocated);
	return (slot_lookup_t){
		.slot = slot,
		.bounds = bounds,
		.live_block_addr = (live_slot && bounds == b_block_addr)
	};
}


#pragma mark -
#pragma mark Allocator Helpers

// Darwin ABI requires 16 byte alignment.
static const size_t k_min_alignment = 16;

static size_t
aligned_size(size_t size)
{
	if (size == 0) {
		return k_min_alignment;
	}
	const size_t mask = (k_min_alignment - 1);
	return (size + mask) & ~mask;
}

// Current implementation: round-robin; delays reuse until at least (num_slots - max_allocations).
// Possible alternatives: LRU, random.
static uint32_t
choose_available_slot(pguard_zone_t *zone)
{
	uint32_t slot = zone->rr_slot_index;
	while (zone->slots[slot].state == ss_allocated) {
		slot = (slot + 1) % zone->num_slots;
	}
	// Delay reuse if immediately freed.
	zone->rr_slot_index = (slot + 1) % zone->num_slots;
	return slot;
}

static uint32_t
choose_metadata(pguard_zone_t *zone)
{
	if (zone->num_metadata < zone->max_metadata) {
		return zone->num_metadata++;
	}

	while (true) {
		uint32_t index = rand_uniform(zone->max_metadata);
		uint32_t s = zone->metadata[index].slot;
		if (zone->slots[s].state == ss_freed) {
			return index;
		}
	}
}

static boolean_t
is_power_of_2(size_t n) {
	return __builtin_popcountl(n) == 1;
}

static uint16_t
choose_offset_on_page(size_t size, size_t alignment, uint16_t page_size) {
	MALLOC_ASSERT(size <= page_size);
	MALLOC_ASSERT(alignment <= page_size && is_power_of_2(alignment));
	MALLOC_ASSERT(is_power_of_2(page_size));
	boolean_t left_align = rand_uniform(2);
	if (left_align) {
		return 0;
	}
	size_t mask = ~(alignment - 1);
	return (page_size - size) & mask;
}


#pragma mark -
#pragma mark Allocator Functions

MALLOC_ALWAYS_INLINE
static inline void capture_trace(stack_trace_t *trace);

static void mark_inaccessible(vm_address_t page);
static void mark_read_write(vm_address_t page);
static void log_zone_state(pguard_zone_t *zone, const char *type, vm_address_t addr);

// Note: the functions below require locking.

static size_t
lookup_size(pguard_zone_t *zone, vm_address_t addr)
{
	slot_lookup_t res = lookup_slot(zone, addr);
	if (!res.live_block_addr) {
		return 0;
	}
	return zone->slots[res.slot].size;
}

static vm_address_t
allocate(pguard_zone_t *zone, size_t size, size_t alignment)
{
	MALLOC_ASSERT(size <= PAGE_SIZE);
	MALLOC_ASSERT(k_min_alignment <= alignment && alignment <= PAGE_SIZE);
	MALLOC_ASSERT(is_power_of_2(alignment));

	if (is_full(zone)) {
		return (vm_address_t)NULL;
	}

	size = aligned_size(size);
	uint32_t slot = choose_available_slot(zone);
	uint32_t metadata = choose_metadata(zone);
	uint16_t offset = choose_offset_on_page(size, alignment, PAGE_SIZE);

	zone->slots[slot] = (slot_t){
		.state = ss_allocated,
		.metadata = metadata,
		.size = size,
		.offset = offset
	};
	zone->metadata[metadata].slot = slot;
	capture_trace(&zone->metadata[metadata].alloc_trace);

	vm_address_t page = page_addr(zone, slot);
	mark_read_write(page);

	zone->num_allocations++;
	zone->size_in_use += size;
	zone->max_size_in_use = MAX(zone->size_in_use, zone->max_size_in_use);

	vm_address_t addr = page + offset;
	log_zone_state(zone, "allocated", addr);

	return addr;
}

static void
deallocate(pguard_zone_t *zone, vm_address_t addr)
{
	slot_lookup_t res = lookup_slot(zone, addr);
	if (!res.live_block_addr) {
		// TODO(yln): error report; TODO(yln): distinguish between most likely cause
		// corrupted pointer (unused, *) or (*, !block_ptr) and double free (freed, block_ptr)
		MALLOC_REPORT_FATAL_ERROR(addr, "PGuard: invalid pointer passed to free");
	}

	uint32_t slot = res.slot;
	uint32_t metadata = zone->slots[slot].metadata;

	zone->slots[slot].state = ss_freed;
	capture_trace(&zone->metadata[metadata].dealloc_trace);

	vm_address_t page = page_addr(zone, slot);
	mark_inaccessible(page);

	zone->num_allocations--;
	zone->size_in_use -= zone->slots[slot].size;

	log_zone_state(zone, "freed", addr);
}

#define DELEGATE(function, args...) \
	zone->wrapped_zone->function(zone->wrapped_zone, args)

static vm_address_t
reallocate(pguard_zone_t *zone, vm_address_t addr, size_t new_size, boolean_t sample)
{
	boolean_t guarded = is_guarded(zone, addr);
	// Note: should_sample() is stateful.
	MALLOC_ASSERT(guarded || sample);

	size_t size;
	if (guarded) {
		size = lookup_size(zone, addr);
	} else {
		size = DELEGATE(size, (void *)addr);
	}
	if (!size) {
		// TODO(yln): error report
		MALLOC_REPORT_FATAL_ERROR(addr, "PGuard: invalid pointer passed to realloc");
	}

	vm_address_t new_addr;
	if (sample && !is_full(zone)) {
		new_addr = allocate(zone, new_size, k_min_alignment);
		MALLOC_ASSERT(new_addr);
	} else {
		new_addr = (vm_address_t)DELEGATE(malloc, new_size);
		if (!new_addr) {
			return (vm_address_t)NULL;
		}
	}
	memcpy((void *)new_addr, (void *)addr, MIN(size, new_size));

	if (guarded) {
		deallocate(zone, addr);
	} else {
		DELEGATE(free, (void *)addr);
	}
	return new_addr;
}


#pragma mark -
#pragma mark Lock Helpers

static void init_lock(pguard_zone_t *zone) { _malloc_lock_init(&zone->lock); }
static void lock(pguard_zone_t *zone) { _malloc_lock_lock(&zone->lock); }
static void unlock(pguard_zone_t *zone) { _malloc_lock_unlock(&zone->lock); }
static boolean_t trylock(pguard_zone_t *zone) { return _malloc_lock_trylock(&zone->lock); }


#pragma mark -
#pragma mark Zone Functions

#define DELEGATE_UNSAMPLED(size, function, args...) \
	if (os_likely(!should_sample(zone, size))) \
		return DELEGATE(function, args)

#define DELEGATE_UNGUARDED(ptr, function, args...) \
	if (os_likely(!is_guarded(zone, (vm_address_t)ptr))) \
		return DELEGATE(function, args)

#define SAMPLED_ALLOCATE(size, alignment, function, args...) \
	DELEGATE_UNSAMPLED(size, function, args); \
	lock(zone); \
	void *ptr = (void *)allocate(zone, size, alignment); \
	unlock(zone); \
	if (!ptr) return DELEGATE(function, args)

#define GUARDED_DEALLOCATE(ptr, function, args...) \
	DELEGATE_UNGUARDED(ptr, function, args); \
	lock(zone); \
	deallocate(zone, (vm_address_t)ptr); \
	unlock(zone)


static size_t
pguard_size(pguard_zone_t *zone, const void *ptr)
{
	DELEGATE_UNGUARDED(ptr, size, ptr);
	lock(zone);
	size_t size = lookup_size(zone, (vm_address_t)ptr);
	unlock(zone);
	return size;
}

static void *
pguard_malloc(pguard_zone_t *zone, size_t size)
{
	SAMPLED_ALLOCATE(size, k_min_alignment, malloc, size);
	return ptr;
}

static void *
pguard_calloc(pguard_zone_t *zone, size_t num_items, size_t size)
{
	size_t total_size;
	if (os_unlikely(os_mul_overflow(num_items, size, &total_size))) {
		return DELEGATE(calloc, num_items, size);
	}
	SAMPLED_ALLOCATE(total_size, k_min_alignment, calloc, num_items, size);
	memset(ptr, 0, total_size);
	return ptr;
}

static void *
pguard_valloc(pguard_zone_t *zone, size_t size)
{
	SAMPLED_ALLOCATE(size, /*alignment=*/PAGE_SIZE, valloc, size);
	return ptr;
}

static void
pguard_free(pguard_zone_t *zone, void *ptr)
{
	GUARDED_DEALLOCATE(ptr, free, ptr);
}

static void *
pguard_realloc(pguard_zone_t *zone, void *ptr, size_t new_size)
{
	if (os_unlikely(!ptr)) {
		return pguard_malloc(zone, new_size);
	}
	boolean_t sample = should_sample(zone, new_size);
	if (os_likely(!sample)) {
		DELEGATE_UNGUARDED(ptr, realloc, ptr, new_size);
	}
	lock(zone);
	void *new_ptr = (void *)reallocate(zone, (vm_address_t)ptr, new_size, sample);
	unlock(zone);
	return new_ptr;
}

static void my_vm_deallocate(vm_address_t addr, size_t size);
static void
pguard_destroy(pguard_zone_t *zone)
{
	DELEGATE(free, zone->metadata);
	DELEGATE(free, zone->slots);
	my_vm_deallocate(zone->begin, zone->size);
	my_vm_deallocate((vm_address_t)zone, sizeof(pguard_zone_t));
}

static unsigned
pguard_batch_malloc(pguard_zone_t *zone, size_t size, void **results, unsigned count)
{
	if (os_unlikely(count == 0)) {
		return 0;
	}
	DELEGATE_UNSAMPLED(size, batch_malloc, size, results, count);

	uint32_t sample_count = 1; // Sample at least one allocation.
	for (uint32_t i = 1; i < count; i++) {
		if (should_sample_counter(zone->sample_counter_range)) {
			sample_count++;
		}
	}
	// TODO(yln): Express the above with only one call to rand_uniform(). "n choose k"?

	for (uint32_t i = 0; i < sample_count; i++) {
		lock(zone);
		void *ptr = (void *)allocate(zone, size, k_min_alignment);
		unlock(zone);
		if (!ptr) {
			sample_count = i;
			break; // Zone full.
		}
		results[i] = ptr;
	}

	void **remaining_results = results + sample_count;
	uint32_t remaining_count = count - sample_count;
	remaining_count = DELEGATE(batch_malloc, size, remaining_results, remaining_count) ;

	// TODO(yln): sampled allocations will always be in the beginning of the results
	// array.  We could shuffle it: https://en.wikipedia.org/wiki/Fisher–Yates_shuffle
	return sample_count + remaining_count;
}

static void
pguard_batch_free(pguard_zone_t *zone, void **to_be_freed, unsigned count)
{
	for (uint32_t i = 0; i < count; i++) {
		vm_address_t addr = (vm_address_t)to_be_freed[i];
		if (os_unlikely(is_guarded(zone, addr))) {
			lock(zone);
			deallocate(zone, addr);
			unlock(zone);
			to_be_freed[i] = NULL;
		}
	}
	return DELEGATE(batch_free, to_be_freed, count);
}

static void *
pguard_memalign(pguard_zone_t *zone, size_t alignment, size_t size)
{
	// Delegate for (alignment > page size) and invalid alignment sizes.
	if (alignment > PAGE_SIZE || !is_power_of_2(alignment) || alignment < sizeof(void *)) {
		return DELEGATE(memalign, alignment, size);
	}
	size_t adj_alignment = MAX(alignment, k_min_alignment);
	SAMPLED_ALLOCATE(size, adj_alignment, memalign, alignment, size);
	return ptr;
}

static void
pguard_free_definite_size(pguard_zone_t *zone, void *ptr, size_t size)
{
	GUARDED_DEALLOCATE(ptr, free_definite_size, ptr, size);
}

static size_t
pguard_pressure_relief(pguard_zone_t *zone, size_t goal)
{
	// We consume a constant amount of memory, so just delegate.
	return DELEGATE(pressure_relief, goal);
}

static boolean_t
pguard_claimed_address(pguard_zone_t *zone, void *ptr)
{
	DELEGATE_UNGUARDED(ptr, claimed_address, ptr);
	return TRUE;
}


#pragma mark -
#pragma mark Introspection Functions

typedef enum { rt_zone_only, rt_slots, rt_slots_and_metadata } read_type_t;

#define READ(remote_address, size, local_memory) \
{ \
	kern_return_t kr = reader(task, (vm_address_t)remote_address, size, (void **)local_memory); \
	if (kr != KERN_SUCCESS) return kr; \
}

static kern_return_t
read_zone(task_t task, vm_address_t zone_address, memory_reader_t reader, pguard_zone_t *zone, read_type_t read_type)
{
	pguard_zone_t *zone_ptr;
	READ(zone_address, sizeof(pguard_zone_t), &zone_ptr);
	*zone = *zone_ptr;  // Copy to writable memory
	// Leaks zone_ptr if called from CrashReporter (crash_reporter_memory_reader_t allocates new buffers)

	if (read_type >= rt_slots) {
		READ(zone->slots, zone->num_slots * sizeof(slot_t), &zone->slots);
	}
	if (read_type >= rt_slots_and_metadata) {
		READ(zone->metadata, zone->max_metadata * sizeof(metadata_t), &zone->metadata);
	}
	return KERN_SUCCESS;
}

#define READ_ZONE(zone, read_type) \
	pguard_zone_t zone_copy; \
	{ \
		kern_return_t kr = read_zone(task, zone_address, reader, &zone_copy, read_type); \
		if (kr != KERN_SUCCESS) return kr; \
	} \
	pguard_zone_t *zone = &zone_copy;

#define RECORD(remote_address, size_, type) \
{ \
	vm_range_t range = { .address = remote_address, .size = size_ }; \
	recorder(task, context, type, &range, /*count=*/1); \
}

static kern_return_t
pguard_enumerator(task_t task, void *context, unsigned type_mask,
		vm_address_t zone_address, memory_reader_t reader,
		vm_range_recorder_t recorder)
{
	MALLOC_ASSERT(reader);
	MALLOC_ASSERT(recorder);

	boolean_t record_allocs = (type_mask & MALLOC_PTR_IN_USE_RANGE_TYPE);
	boolean_t record_regions = (type_mask & MALLOC_PTR_REGION_RANGE_TYPE);
	if (!record_allocs && !record_regions) {
		return KERN_SUCCESS;
	}

	READ_ZONE(zone, rt_slots);

	for (uint32_t i = 0; i < zone->num_slots; i++) {
		if (zone->slots[i].state != ss_allocated) {
			continue;
		}
		// TODO(yln): we could do these in bulk.  Currently, it shouldn't matter
		// since the number of active slots (bounded by max_allocations) is small.
		// If we optimize our allocator (to prevent wasting a page per allocation)
		// and this allows us to significantly grow the number of allocations, then
		// we should change the code here to record in chunks.
		if (record_regions) {
			vm_address_t page = page_addr(zone, i);
			RECORD(page, PAGE_SIZE, MALLOC_PTR_REGION_RANGE_TYPE);
		}
		if (record_allocs) {
			vm_address_t alloc = block_addr(zone, i);
			RECORD(alloc, zone->slots[i].size, MALLOC_PTR_IN_USE_RANGE_TYPE);
		}
	}
	return KERN_SUCCESS;
}

static void
pguard_statistics(pguard_zone_t *zone, malloc_statistics_t *stats)
{
	*stats = (malloc_statistics_t){
		.blocks_in_use = zone->num_allocations,
		.size_in_use = zone->size_in_use,
		.max_size_in_use = zone->max_size_in_use,
		.size_allocated = zone->num_allocations * PAGE_SIZE
	};
}

static kern_return_t
pguard_statistics_task(task_t task, vm_address_t zone_address, memory_reader_t reader, malloc_statistics_t *stats)
{
	READ_ZONE(zone, rt_zone_only);
	pguard_statistics(zone, stats);
	return KERN_SUCCESS;
}

static void
print_zone(pguard_zone_t *zone, boolean_t verbose, print_task_printer_t printer) {
	malloc_statistics_t stats;
	pguard_statistics(zone, &stats);
	printer("PGuard zone: slots: %u, slots in use: %u, size in use: %llu, max size in use: %llu, allocated size: %llu\n",
					zone->num_slots, stats.blocks_in_use, stats.size_in_use, stats.max_size_in_use, stats.size_allocated);
	printer("Quarantine: size: %llu, address range: [%p - %p]\n", zone->size, zone->begin, zone->end);

	printer("Slots (#, state, offset, size, block address):\n");
	for (uint32_t i = 0; i < zone->num_slots; i++) {
		slot_state_t state = zone->slots[i].state;
		if (state != ss_allocated && !verbose) {
			continue;
		}
		const char *label = slot_state_labels[state];
		uint16_t offset = zone->slots[i].offset;
		uint16_t size = zone->slots[i].size;
		vm_address_t block = block_addr(zone, i);
		printer("%4u, %9s, %4u, %4u, %p\n", i, label, offset, size, block);
	}
}


static void
pguard_print(pguard_zone_t *zone, boolean_t verbose)
{
	print_zone(zone, verbose, malloc_report_simple);
}

static void
pguard_print_task(task_t task, unsigned level, vm_address_t zone_address, memory_reader_t reader, print_task_printer_t printer)
{
	pguard_zone_t zone;
	kern_return_t kr = read_zone(task, zone_address, reader, &zone, rt_slots);
	if (kr != KERN_SUCCESS) {
		printer("Failed to read PGuard zone at %p\n", zone_address);
		return;
	}

	boolean_t verbose = (level >= MALLOC_VERBOSE_PRINT_LEVEL);
	print_zone(&zone, verbose, printer);
}

static void
pguard_log(pguard_zone_t *zone, void *address)
{
	// Unsupported.
}

static size_t
pguard_good_size(pguard_zone_t *zone, size_t size)
{
	return DELEGATE(introspect->good_size, size);
}

static boolean_t
pguard_check(pguard_zone_t *zone)
{
	return TRUE; // Zone is always in a consistent state.
}

static void
pguard_force_lock(pguard_zone_t *zone)
{
	lock(zone);
}

static void
pguard_force_unlock(pguard_zone_t *zone)
{
	unlock(zone);
}

static void
pguard_reinit_lock(pguard_zone_t *zone)
{
	init_lock(zone);
}

static boolean_t
pguard_zone_locked(pguard_zone_t *zone)
{
	boolean_t lock_taken = trylock(zone);
	if (lock_taken) {
		unlock(zone);
	}
	return !lock_taken;
}


#pragma mark -
#pragma mark Zone Templates

// Suppress warning: incompatible function pointer types
#define FN_PTR(fn) (void *)(&fn)

static const malloc_introspection_t introspection_template = {
	// Block and region enumeration
	.enumerator = FN_PTR(pguard_enumerator),

	// Statistics
	.statistics = FN_PTR(pguard_statistics),
	.task_statistics = FN_PTR(pguard_statistics_task),

	// Logging
	.print = FN_PTR(pguard_print),
	.print_task = FN_PTR(pguard_print_task),
	.log = FN_PTR(pguard_log),

	// Queries
	.good_size = FN_PTR(pguard_good_size),
	.check = FN_PTR(pguard_check),

	// Locking
	.force_lock = FN_PTR(pguard_force_lock),
	.force_unlock = FN_PTR(pguard_force_unlock),
	.reinit_lock = FN_PTR(pguard_reinit_lock),
	.zone_locked = FN_PTR(pguard_zone_locked),

	// Discharge checking
	.enable_discharge_checking = NULL,
	.disable_discharge_checking = NULL,
	.discharge = NULL,
#ifdef __BLOCKS__
	.enumerate_discharged_pointers = NULL,
#else
	.enumerate_unavailable_without_blocks = NULL,
#endif
};

static const malloc_zone_t malloc_zone_template = {
	// Reserved for CFAllocator
	.reserved1 = NULL,
	.reserved2 = NULL,

	// Standard operations
	.size = FN_PTR(pguard_size),
	.malloc = FN_PTR(pguard_malloc),
	.calloc = FN_PTR(pguard_calloc),
	.valloc = FN_PTR(pguard_valloc),
	.free = FN_PTR(pguard_free),
	.realloc = FN_PTR(pguard_realloc),
	.destroy = FN_PTR(pguard_destroy),

	// Batch operations
	.batch_malloc = FN_PTR(pguard_batch_malloc),
	.batch_free = FN_PTR(pguard_batch_free),

	// Introspection
	.zone_name = NULL, // Do not initialize with static string; set_zone_name() frees this pointer.
	.version = 12,
	.introspect = (malloc_introspection_t *)&introspection_template, // Effectively const.

	// Specialized operations
	.memalign = FN_PTR(pguard_memalign),
	.free_definite_size = FN_PTR(pguard_free_definite_size),
	.pressure_relief = FN_PTR(pguard_pressure_relief),
	.claimed_address = FN_PTR(pguard_claimed_address)
};


#pragma mark -
#pragma mark Configuration Options

static const char *
env_var(const char *name)
{
	const char **env = (const char **)*_NSGetEnviron();
	return _simple_getenv(env, name);
}

static uint32_t
env_uint(const char *name, uint32_t default_value) {
	const char *value = env_var(name);
	if (!value) return default_value;
	return (uint32_t)strtoul(value, NULL, 0);
}

static boolean_t
env_bool(const char *name) {
	const char *value = env_var(name);
	if (!value) return FALSE;
	return value[0] == '1';
}

#if CONFIG_FEATUREFLAGS_SIMPLE
# define FEATURE_FLAG(feature, default) os_feature_enabled_simple(libmalloc, feature, default)
#else
# define FEATURE_FLAG(feature, default) (default)
#endif


#pragma mark -
#pragma mark Zone Configuration

static bool
is_platform_binary(void)
{
	uint32_t flags = 0;
	int err = csops(getpid(), CS_OPS_STATUS, &flags, sizeof(flags));
	if (err) {
		return false;
	}
	return (flags & CS_PLATFORM_BINARY);
}

static bool
should_activate(bool internal_build)
{
	uint32_t activation_rate = (internal_build ? 250 : 1000);
	return rand_uniform(activation_rate) == 0;
}

bool
pguard_enabled(bool internal_build)
{
	if (env_var("MallocProbGuard")) {
		return env_bool("MallocProbGuard");
	}
#if TARGET_OS_OSX || TARGET_OS_IOS
	if (FEATURE_FLAG(ProbGuard, true) && (internal_build || is_platform_binary())) {
		bool activate = TARGET_OS_OSX ?
				should_activate(internal_build) :
				env_bool("MallocProbGuardViaLaunchd");
		if (activate) {
			return true;
		}
	}
#endif  // macOS || iOS
	if (FEATURE_FLAG(ProbGuardAllProcesses, false)) {
		return true;
	}
	return false;
}

static uint32_t
choose_memory_budget_in_kb(void)
{
	return (TARGET_OS_OSX ? 8 : 2) * 1024;
}

// TODO(yln): uniform sampling is likely not optimal here, since we will tend to
// sample around the average of our range, which is probably more frequent than
// what we want.  We probably want the average to be less frequent, but still be
// able to reach the "very frequent" end of our range occassionally.  Consider
// using a geometric (or other weighted distribution) here.
static uint32_t
choose_sample_rate(void)
{
	uint32_t min = 500, max = 10000;
	return rand_uniform(max - min) + min;
}

static const double k_slot_multiplier = 10.0;
static const double k_metadata_multiplier = 3.0;
static uint32_t
compute_max_allocations(size_t memory_budget_in_kb)
{
	size_t memory_budget = memory_budget_in_kb * 1024;
	size_t fixed_overhead = round_page(sizeof(pguard_zone_t));
	size_t vm_map_entry_size = 80; // struct vm_map_entry in <vm/vm_map.h>
	size_t per_allocation_overhead =
			PAGE_SIZE +
			k_slot_multiplier * 2 * vm_map_entry_size + // TODO(yln): Implement mark_inaccessible to fill holes so we can drop the k_slot_multiplier here. +27% more protected allocations!
			// 2 * vm_map_entry_size + // Allocations split the VM region
			k_slot_multiplier * sizeof(slot_t) +
			k_metadata_multiplier * sizeof(metadata_t);

	uint32_t max_allocations = (uint32_t)((memory_budget - fixed_overhead) / per_allocation_overhead);
	if (memory_budget < fixed_overhead || max_allocations == 0) {
		MALLOC_REPORT_FATAL_ERROR(0, "PGuard: memory budget too small");
	}
	return max_allocations;
}

static void
configure_zone(pguard_zone_t *zone) {
	uint32_t memory_budget_in_kb = env_uint("MallocPGuardMemoryBudgetInKB", choose_memory_budget_in_kb());
	zone->max_allocations = env_uint("MallocPGuardAllocations", compute_max_allocations(memory_budget_in_kb));
	zone->num_slots = env_uint("MallocPGuardSlots", k_slot_multiplier * zone->max_allocations);
	zone->max_metadata = env_uint("MallocPGuardMetadata", k_metadata_multiplier * zone->max_allocations);
	uint32_t sample_rate = env_uint("MallocPGuardSampleRate", choose_sample_rate());
	if (sample_rate == 0) {
		MALLOC_REPORT_FATAL_ERROR(0, "PGuard: sample rate cannot be 0");
	}
	// Approximate a (1 / sample_rate) chance for sampling; 1 means "always sample".
	zone->sample_counter_range = (sample_rate != 1) ? (2 * sample_rate) : 1;
	zone->signal_handler = env_bool("MallocPGuardSignalHandler");
	zone->debug_log = env_bool("MallocPGuardDebugLog");
	zone->debug_log_throttle_ms = env_uint("MallocPGuardDebugLogThrottleInMillis", 1000);

	if (zone->debug_log) {
		malloc_report(ASL_LEVEL_INFO,
				"PGuard: configuration: %u allocations, %u slots, %u metadata, 1/%u sample rate\n",
				zone->max_allocations, zone->num_slots, zone->max_metadata, sample_rate);
	}
}


#pragma mark -
#pragma mark Zone Creation

#define VM_PROT_READ_WRITE (VM_PROT_READ | VM_PROT_WRITE)

static vm_address_t my_vm_map(size_t size, vm_prot_t protection, int tag);
static void my_vm_deallocate(vm_address_t addr, size_t size);
static void my_vm_protect(vm_address_t addr, size_t size, vm_prot_t protection);

static void
setup_zone(pguard_zone_t *zone, malloc_zone_t *wrapped_zone) {
	// Malloc zone
	zone->malloc_zone = malloc_zone_template;
	zone->wrapped_zone = wrapped_zone;

	// Configuration
	configure_zone(zone);

	// Quarantine
	zone->size = quarantine_size(zone->num_slots);
	zone->begin = my_vm_map(zone->size, VM_PROT_NONE, VM_MEMORY_MALLOC_PGUARD); // TODO(yln): place at "unusually high" address to minimize chance that a randomly corrupted pointers fall into the guarded range.
	zone->end = zone->begin + zone->size;

	// Metadata
	zone->slots = DELEGATE(malloc, zone->num_slots * sizeof(slot_t));
	zone->metadata = DELEGATE(malloc, zone->max_metadata * sizeof(metadata_t));
	MALLOC_ASSERT(zone->slots && zone->metadata);

	// Mutable state
	init_lock(zone);
}

static void install_signal_handler(void *unused);
malloc_zone_t *
pguard_create_zone(malloc_zone_t *wrapped_zone)
{
	pguard_zone_t *zone = (pguard_zone_t *)my_vm_map(sizeof(pguard_zone_t), VM_PROT_READ_WRITE, VM_MEMORY_MALLOC);
	setup_zone(zone, wrapped_zone);
	my_vm_protect((vm_address_t)zone, PAGE_MAX_SIZE, VM_PROT_READ);

	if (zone->signal_handler) {
		static os_once_t once_pred;
		os_once(&once_pred, NULL, &install_signal_handler);
	}

	return (malloc_zone_t *)zone;
}

#pragma mark -
#pragma mark Logging

static uint64_t
to_millis(uint64_t mach_ticks)
{
	mach_timebase_info_data_t timebase;
	mach_timebase_info(&timebase);
	const uint64_t nanos_per_ms = 1e6;
	return (mach_ticks * timebase.numer / timebase.denom) / nanos_per_ms;
}

static boolean_t
should_log(pguard_zone_t *zone)
{
	if (!zone->debug_log) {
		return FALSE;
	}
	uint64_t now = mach_absolute_time();
	uint64_t delta_ms = to_millis(now - zone->last_log_time);
	boolean_t log = (delta_ms >= zone->debug_log_throttle_ms);
	if (log) {
		zone->last_log_time = now;
	}
	return log;
}

static void
log_zone_state(pguard_zone_t *zone, const char *type, vm_address_t addr)
{
	if (!should_log(zone)) {
		return;
	}
	malloc_report(ASL_LEVEL_INFO, "PGuard: %9s 0x%lx, fill state: %3u/%u\n",
			type, addr,	zone->num_allocations, zone->max_allocations);
}


#pragma mark -
#pragma mark Fault Diagnosis

static void
fill_in_report(pguard_zone_t *zone, uint32_t slot, pguard_report_t *report)
{
	slot_t *s = &zone->slots[slot];
	metadata_t *m = &zone->metadata[s->metadata];

	report->nearest_allocation = block_addr(zone, slot);
	report->allocation_size = s->size;
	report->allocation_state = slot_state_labels[s->state];
	report->num_traces = 0;

	if (m->slot == slot) {
		report->num_traces++;
		memcpy(&report->alloc_trace, &m->alloc_trace, sizeof(stack_trace_t));
		if (s->state == ss_freed) {
			report->num_traces++;
			memcpy(&report->dealloc_trace, &m->dealloc_trace, sizeof(stack_trace_t));
		}
	}
}

static void
diagnose_page_fault(pguard_zone_t *zone, vm_address_t fault_address, pguard_report_t *report)
{
	slot_lookup_t res = lookup_slot(zone, fault_address);
	slot_state_t ss = zone->slots[res.slot].state;

	// We got here because of a page fault.
	MALLOC_ASSERT(ss != ss_allocated || res.bounds == b_oob_guard_page);

	// Note that all of the following error conditions may also be caused by:
	//  *) Randomly corrupted pointer
	//  *) Long-range OOB (access stride > (page size / 2))
	// We will always misdiagnose some of these errors no matter how we slice it.

	// TODO(yln): extract "nearest allocation helper"
	switch (ss) {
		case ss_unused:
			// Nearest slot was never used.
			// TODO(yln): if bounds == oob_guard_page; we could try to look at the slot on the other side of the guard page.
			report->error_type = "long-range OOB";
			report->confidence = "low";
			break;
		case ss_allocated:
			// Most likely an OOB access from an active allocation onto a guard page.
			MALLOC_ASSERT(res.bounds == b_oob_guard_page);
			report->error_type = "out-of-bounds";
			report->confidence = "high";
			break;
		case ss_freed:
			if (res.bounds == b_block_addr || res.bounds == b_valid) {
				report->error_type = "use-after-free";
				report->confidence = "high";
			} else {
				MALLOC_ASSERT(res.bounds == b_oob_slot || res.bounds == b_oob_guard_page);
				// This could be a combination of OOB and UAF, or one of the generic errors
				// outlined above.
				// TODO(yln): still try to diagnose something here
				report->error_type = "OOB + UAF";
				report->confidence = "low";
			}
			break;
		default:
			__builtin_unreachable();
	}

	report->fault_address = fault_address;
	fill_in_report(zone, res.slot, report);
}


#pragma mark -
#pragma mark Error Printing

static const uint32_t k_buf_len = 1024;
static void
get_symbol_and_module_name(vm_address_t addr, char buf[k_buf_len])
{
#if !TARGET_OS_DRIVERKIT
	Dl_info info;
	int success = dladdr((void *)addr, &info);
	MALLOC_ASSERT(success);
	snprintf(buf, k_buf_len, "%s  (%s)", info.dli_sname, info.dli_fname);
#else
	strcpy(buf, "?");
#endif
}

static void
print_trace(stack_trace_t *trace, const char *label)
{
	malloc_report(ASL_LEVEL_ERR, "%s trace (thread %llu):\n", label, trace->thread_id);
	for (uint32_t i = 0; i < trace->num_frames; i++) {
		char sym_name[k_buf_len];
		get_symbol_and_module_name(trace->frames[i], sym_name);
		malloc_report(ASL_LEVEL_ERR, "  #%u %s\n", i, sym_name);
	}
	malloc_report(ASL_LEVEL_ERR, "\n", label);
}

static void
print_report(pguard_report_t *report)
{
	malloc_report(ASL_LEVEL_ERR, "PGuard: invalid access at 0x%lx\n",
			report->fault_address);
	malloc_report(ASL_LEVEL_ERR, "Error type: %s (%s confidence)\n",
			report->error_type, report->confidence);
	malloc_report(ASL_LEVEL_ERR, "Nearest allocation: 0x%lx, size: %lu, state: %s\n",
			report->nearest_allocation, report->allocation_size, report->allocation_state);

	if (report->num_traces >= 1) {
		print_trace(&report->alloc_trace, "Allocation");
		if (report->num_traces >= 2) {
			print_trace(&report->dealloc_trace, "Deallocation");
		}
	} else {
		malloc_report(ASL_LEVEL_ERR, "Allocation stack traces not available.  "
			"Try increasing `MallocPGuardMetadata` and rerun.\n");
	}
}


#pragma mark -
#pragma mark Crash Reporter API

static crash_reporter_memory_reader_t g_crm_reader;
static kern_return_t
memory_reader_adapter(task_t task, vm_address_t address, vm_size_t size, void **local_memory)
{
	*local_memory = g_crm_reader(task, address, size);
	return *local_memory ? KERN_SUCCESS : KERN_FAILURE;
}

kern_return_t
pgm_diagnose_fault_from_crash_reporter(vm_address_t fault_address, pgm_report_t *report,
		task_t task, vm_address_t zone_address, crash_reporter_memory_reader_t crm_reader)
{
	g_crm_reader = crm_reader;

	memory_reader_t *reader = memory_reader_adapter;
	READ_ZONE(zone, rt_slots_and_metadata);

	diagnose_page_fault(zone, fault_address, report);
	free(zone->metadata);
	free(zone->slots);
	// zone lives on the stack

	return KERN_SUCCESS;
}


#pragma mark -
#pragma mark Signal Handler

extern malloc_zone_t **malloc_zones;
static void
report_error_from_signal_handler(vm_address_t fault_address)
{
	// TODO(yln): maybe look up by name, once the zone naming has been figured out.
	pguard_zone_t *zone = (pguard_zone_t *)malloc_zones[0];
	MALLOC_ASSERT(zone->malloc_zone.size == FN_PTR(pguard_size));

	if (!is_guarded(zone, fault_address)) {
		return;
	}

	pguard_report_t report;
	{
		trylock(zone); // Best-effort locking to avoid deadlock.
		diagnose_page_fault(zone, fault_address, &report);
		unlock(zone);
	}
	print_report(&report);

	MALLOC_REPORT_FATAL_ERROR(fault_address, "PGuard: invalid access detected");
}

static struct sigaction prev_sigaction;
static void
signal_handler(int sig, siginfo_t *info, void *ucontext)
{
	MALLOC_ASSERT(sig == SIGBUS);
	report_error_from_signal_handler((vm_address_t)info->si_addr);

	// Delegate to previous handler.
	if (prev_sigaction.sa_flags & SA_SIGINFO) {
		prev_sigaction.sa_sigaction(sig, info, ucontext);
	} else if (prev_sigaction.sa_handler == SIG_IGN ||
						 prev_sigaction.sa_handler == SIG_DFL) {
		// If the previous handler was the default handler, or was ignoring this
		// signal, install the default handler and re-raise the signal in order to
		// get a core dump and terminate this process.
		signal(SIGBUS, SIG_DFL);
		raise(SIGBUS);
	} else {
		prev_sigaction.sa_handler(sig);
	}
}

static void
install_signal_handler(void *unused)
{
	struct sigaction act = {
		.sa_sigaction = &signal_handler,
		.sa_flags = SA_SIGINFO
	};
	int res = sigaction(SIGBUS, &act, &prev_sigaction);
	MALLOC_ASSERT(res == 0);
}


#pragma mark -
#pragma mark Mockable Helpers

#ifndef PGUARD_MOCK_RANDOM
static uint32_t
rand_uniform(uint32_t upper_bound)
{
	MALLOC_ASSERT(upper_bound > 0);
	return arc4random_uniform(upper_bound);
}
#endif

#ifndef PGUARD_MOCK_CAPTURE_TRACE
MALLOC_ALWAYS_INLINE
static inline void
capture_trace(stack_trace_t *trace)
{
	// Frame 0 is thread_stack_pcs() itself; last frame usually is a garbage value.
	const uint32_t dropped_frames = 2;
	const uint32_t max_frames = k_pguard_trace_max_frames + dropped_frames;
	vm_address_t frames[max_frames];
	uint32_t num_frames;
	thread_stack_pcs(frames, max_frames, &num_frames);
	num_frames = (num_frames > dropped_frames) ? (num_frames - dropped_frames) : 0;

	trace->thread_id = _pthread_threadid_self_np_direct();
	trace->num_frames = num_frames;
	memcpy(trace->frames, &frames[1], num_frames * sizeof(vm_address_t));
}
#endif

#ifndef PGUARD_MOCK_PAGE_ACCESS
static void
mark_inaccessible(vm_address_t page)
{
	int res = madvise((void *)page, PAGE_SIZE, CONFIG_MADVISE_STYLE);
	MALLOC_ASSERT(res == 0);
	my_vm_protect(page, PAGE_SIZE, VM_PROT_NONE);
}

static void
mark_read_write(vm_address_t page)
{
	// It is faster to just unprotect the page without calling madvise() first.
	my_vm_protect(page, PAGE_SIZE, VM_PROT_READ_WRITE);
}
#endif


#pragma mark -
#pragma mark Mach VM Helpers

static vm_address_t
my_vm_map(size_t size, vm_prot_t protection, int tag)
{
	vm_map_t target = mach_task_self();
	mach_vm_address_t address = 0;
	mach_vm_size_t size_rounded = round_page(size);
	mach_vm_offset_t mask = 0x0;
	int flags = VM_FLAGS_ANYWHERE | VM_MAKE_TAG(tag);
	mem_entry_name_port_t object = MEMORY_OBJECT_NULL;
	memory_object_offset_t offset = 0;
	boolean_t copy = FALSE;
	vm_prot_t cur_protection = protection;
	vm_prot_t max_protection = VM_PROT_READ | VM_PROT_WRITE;
	vm_inherit_t inheritance = VM_INHERIT_DEFAULT;

	kern_return_t kr = mach_vm_map(target, &address, size_rounded, mask, flags,
		object, offset, copy, cur_protection, max_protection, inheritance);
	MALLOC_ASSERT(kr == KERN_SUCCESS);
	return address;
}

static void
my_vm_deallocate(vm_address_t addr, size_t size)
{
	vm_map_t target = mach_task_self();
	mach_vm_address_t address = (mach_vm_address_t)addr;
	mach_vm_size_t size_rounded = round_page(size);
	kern_return_t kr = mach_vm_deallocate(target, address, size_rounded);
	MALLOC_ASSERT(kr == KERN_SUCCESS);
}

static void
my_vm_protect(vm_address_t addr, size_t size, vm_prot_t protection) {
	vm_map_t target = mach_task_self();
	mach_vm_address_t address = (mach_vm_address_t)addr;
	mach_vm_size_t size_rounded = round_page(size);
	boolean_t set_maximum = FALSE;
	kern_return_t kr = mach_vm_protect(target, address, size_rounded, set_maximum, protection);
	MALLOC_ASSERT(kr == KERN_SUCCESS);
}
