/*
 * Copyright 2010-2012 Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef MALLOC_DEBUG_H
#define MALLOC_DEBUG_H


#include <OS.h>

#ifdef __cplusplus
extern "C" {
#endif

status_t heap_debug_start_wall_checking(int msInterval);
status_t heap_debug_stop_wall_checking();

void heap_debug_set_memory_reuse(bool enabled);
void heap_debug_set_paranoid_validation(bool enabled);
void heap_debug_set_debugger_calls(bool enabled);
void heap_debug_set_default_alignment(size_t defaultAlignment);
void heap_debug_validate_heaps();
void heap_debug_validate_walls();

void heap_debug_dump_allocations(bool statsOnly, thread_id thread);
void heap_debug_dump_heaps(bool dumpAreas, bool dumpBins);

void *heap_debug_malloc_with_guard_page(size_t size);

status_t heap_debug_get_allocation_info(void *address, size_t *size,
	thread_id *thread);

status_t heap_debug_dump_allocations_on_exit(bool enabled);
status_t heap_debug_set_stack_trace_depth(size_t stackTraceDepth);

#ifdef __cplusplus
}
#endif

#endif /* MALLOC_DEBUG_H */
