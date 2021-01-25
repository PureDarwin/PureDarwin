/*
 * Copyright (c) 2020 Apple Inc. All rights reserved.
 *
 * @APPLE_APACHE_LICENSE_HEADER_START@
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * @APPLE_APACHE_LICENSE_HEADER_END@
 */

/*
 * IMPORTANT: This header file describes INTERNAL interfaces to libdispatch
 * which are subject to change in future releases of Mac OS X. Any applications
 * relying on these interfaces WILL break.
 */

#ifndef __OS_WORKGROUP_INTERNAL__
#define __OS_WORKGROUP_INTERNAL__

#include <stdint.h>
#include <sys/work_interval.h>
#include <sys/types.h>
#include <os/lock.h>

void _os_workgroup_xref_dispose(os_workgroup_t wg);
void _os_workgroup_dispose(os_workgroup_t wg);
void _os_workgroup_interval_xref_dispose(os_workgroup_interval_t wgi);
void _os_workgroup_interval_dispose(os_workgroup_interval_t wgi);
void _os_workgroup_debug(os_workgroup_t wg, char *buf, size_t size);

extern pthread_key_t _os_workgroup_key;
void _os_workgroup_tsd_cleanup(void *ctxt);

/*
 * os_workgroup_type_t is an internal representation that is a superset of types
 * for various types of workgroups. Currently it only includes
 * os_workgroup_interval_type_t and the types specified below
 *
 * Making the workgroup type uint16_t means that we have a total of 64k types
 * which is plenty
 */
typedef uint16_t os_workgroup_type_t;
#define OS_WORKGROUP_TYPE_DEFAULT		0x0
#define OS_WORKGROUP_TYPE_PARALLEL		0x40

/* To be set when the caller provided workgroup attribute has been expanded
 * and resolved. */
#define _OS_WORKGROUP_ATTR_RESOLVED_INIT 0x782618DA
struct os_workgroup_attr_s {
	uint32_t sig;
	uint32_t wg_attr_flags;
	os_workgroup_type_t wg_type;
	uint16_t empty;
	uint32_t reserved[13];
};

#define _OS_WORKGROUP_JOIN_TOKEN_SIG_INIT 0x4D5F5A58
struct os_workgroup_join_token_s {
	uint32_t sig;
	mach_port_t thread;
	os_workgroup_t old_wg;
	os_workgroup_t new_wg;
	uint64_t reserved[2];
};

struct os_workgroup_interval_data_s {
	uint32_t sig;
	uint32_t reserved[14];
};

/* This is lazily allocated if the arena is used by clients */
typedef struct os_workgroup_arena_s {
	void *client_arena;
	os_workgroup_working_arena_destructor_t destructor;
	uint32_t max_workers; /* Client specified max size */
	uint32_t next_worker_index;
	mach_port_t arena_indices[0]; /* Dyanmic depending on max_workers */
} *os_workgroup_arena_t;

#define OS_WORKGROUP_OWNER (1 << 0)
#define OS_WORKGROUP_CANCELED (1 << 1)
#define OS_WORKGROUP_LABEL_NEEDS_FREE (1 << 2)
#define OS_WORKGROUP_INTERVAL_STARTED (1 << 3)


/* Note that os_workgroup_type_t doesn't have to be in the wg_atomic_flags, we
 * just put it there to pack the struct.
 *
 * We have to put the arena related state in an atomic because the
 * joined_cnt is modified in a real time context as part of os_workgroup_join
 * and os_workgroup_leave(). We cannot have a lock and so it needs to all be
 * part of a single _os_workgroup_atomic_flags sized atomic state */

#if !defined(__LP64__) || (__LP64_ && !defined(__arm64__))
// For 32 bit watches (armv7), we can only do DCAS up to 64 bits so the union
// type is for uint64_t.
//
// 16 bits for tracking the type
// 16 bits for max number of threads which have joined a workgroup (64k is plenty)
// 32 bits for arena pointer
// -----
// 64 bits
typedef uint64_t _os_workgroup_atomic_flags;

typedef uint16_t os_joined_cnt_t;
#define OS_WORKGROUP_JOINED_COUNT_SHIFT 48
#define OS_WORKGROUP_JOINED_COUNT_MASK (((uint64_t) 0xffff) << OS_WORKGROUP_JOINED_COUNT_SHIFT)
#define OS_WORKGROUP_ARENA_MASK 0xffffffffull

#define OS_WORKGROUP_HEADER_INTERNAL \
	DISPATCH_UNION_LE(_os_workgroup_atomic_flags volatile wg_atomic_flags, \
		os_workgroup_arena_t wg_arena, \
		os_workgroup_type_t wg_type, \
		os_joined_cnt_t joined_cnt \
	)
#else
// For all 64 bit systems (including arm64_32), we can do DCAS (or quad width
// CAS for arm64_32) so 128 bit union type works
//
// 16 bits for tracking the type
// 16 bits for empty
// 32 bits for max number of threads which have joined a workgroup
// 64 bits for arena pointer
// -----
// 128 bits
typedef __uint128_t _os_workgroup_atomic_flags;

typedef uint32_t os_joined_cnt_t;
#define OS_WORKGROUP_JOINED_COUNT_SHIFT 96
#define OS_WORKGROUP_JOINED_COUNT_MASK (((__uint128_t) 0xffffffff) << OS_WORKGROUP_JOINED_COUNT_SHIFT)
#define OS_WORKGROUP_ARENA_MASK 0xffffffffffffffffull

#define OS_WORKGROUP_HEADER_INTERNAL \
	DISPATCH_UNION_LE(_os_workgroup_atomic_flags volatile wg_atomic_flags, \
		os_workgroup_arena_t wg_arena, \
		os_workgroup_type_t wg_type, \
		const uint16_t empty, \
		os_joined_cnt_t joined_cnt \
	)
#endif

static inline os_joined_cnt_t
_wg_joined_cnt(_os_workgroup_atomic_flags wgaf)
{
	return (os_joined_cnt_t) (((wgaf & OS_WORKGROUP_JOINED_COUNT_MASK)) >> OS_WORKGROUP_JOINED_COUNT_SHIFT);
}

static inline os_workgroup_arena_t
_wg_arena(_os_workgroup_atomic_flags wgaf)
{
	return (os_workgroup_arena_t) (wgaf & OS_WORKGROUP_ARENA_MASK);
}

#define OS_WORKGROUP_HEADER \
	struct _os_object_s _as_os_obj[0]; \
	OS_OBJECT_STRUCT_HEADER(workgroup); \
	const char *name; \
	uint64_t volatile wg_state; \
	union { \
		work_interval_t wi; \
		mach_port_t port; \
	}; \
	OS_WORKGROUP_HEADER_INTERNAL;

struct os_workgroup_s {
	OS_WORKGROUP_HEADER
};

struct os_workgroup_interval_s {
	struct os_workgroup_s _as_wg[0];
	OS_WORKGROUP_HEADER
	os_clockid_t clock;
	/* Needed to serialize updates to wii when there are multiple racey calls to
	 * os_workgroup_interval_update */
	os_unfair_lock wii_lock;
	work_interval_instance_t wii;
};

struct os_workgroup_parallel_s {
	OS_WORKGROUP_HEADER
};

_Static_assert(sizeof(struct os_workgroup_attr_s) == sizeof(struct os_workgroup_attr_opaque_s),
		"Incorrect size of workgroup attribute structure");
_Static_assert(sizeof(struct os_workgroup_join_token_s) == sizeof(struct os_workgroup_join_token_opaque_s),
		"Incorrect size of workgroup join token structure");
_Static_assert(sizeof(struct os_workgroup_interval_data_s) == sizeof(struct os_workgroup_interval_data_opaque_s),
		"Incorrect size of workgroup interval data structure");

#endif /* __OS_WORKGROUP_INTERNAL__ */
