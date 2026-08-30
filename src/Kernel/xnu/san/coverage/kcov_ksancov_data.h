/*
 * Copyright (c) 2021 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */
#ifndef _KCOV_KSANCOV_DATA_H_
#define _KCOV_KSANCOV_DATA_H_

#if KERNEL_PRIVATE

#if CONFIG_KSANCOV

/*
 * Supported coverage modes.
 */
typedef enum {
	KS_MODE_NONE,
	KS_MODE_TRACE,
	KS_MODE_COUNTERS,
	KS_MODE_STKSIZE,
	KS_MODE_MAX
} ksancov_mode_t;

/*
 * A header that is always present in every ksancov mode shared memory structure.
 */
typedef struct ksancov_header {
	uint32_t         kh_magic;
	_Atomic uint32_t kh_enabled;
} ksancov_header_t;

/*
 * TRACE mode data structure.
 */

/*
 * All trace based tools share this structure.
 */
typedef struct ksancov_trace {
	ksancov_header_t kt_hdr;         /* header (must be always first) */
	uint32_t         kt_maxent;      /* Maximum entries in this shared buffer. */
	_Atomic uint32_t kt_head;        /* Pointer to the first unused element. */
	uint64_t         kt_entries[];   /* Trace entries in this buffer. */
} ksancov_trace_t;

/* PC tracing only records PCs */
typedef uintptr_t ksancov_trace_pc_ent_t;

/* STKSIZE tracing records PCs and stack size. */
typedef struct ksancov_trace_stksize_entry {
	uintptr_t pc;                      /* PC */
	uint32_t  stksize;                 /* associated stack size */
} ksancov_trace_stksize_ent_t;

/*
 * COUNTERS mode data structure.
 */
typedef struct ksancov_counters {
	ksancov_header_t kc_hdr;
	uint32_t         kc_nedges;       /* total number of edges */
	uint8_t          kc_hits[];       /* hits on each edge (8bit saturating) */
} ksancov_counters_t;

/*
 * Edge to PC mapping.
 */
typedef struct ksancov_edgemap {
	uint32_t  ke_magic;
	uint32_t  ke_nedges;
	uintptr_t ke_addrs[];             /* address of each edge relative to 'offset' */
} ksancov_edgemap_t;

/*
 * Supported comparison logging modes.
 */
typedef enum {
	KS_CMPS_MODE_NONE,
	KS_CMPS_MODE_TRACE,
	KS_CMPS_MODE_TRACE_FUNC,
	KS_CMPS_MODE_MAX
} ksancov_cmps_mode_t;

#define KSANCOV_CMPS_TRACE_FUNC_MAX_BYTES 512

/* CMPS TRACE mode tracks comparison values */
typedef struct __attribute__((__packed__)) ksancov_cmps_trace_entry {
	uint64_t pc;
	uint32_t type;
	uint16_t len1_func;
	uint16_t len2_func;
	union {
		uint64_t args[2];              /* cmp instruction arguments */
		uint8_t args_func[0];          /* cmp function arguments (variadic) */
	};
} ksancov_cmps_trace_ent_t;

/* Calculate the total space that a ksancov_cmps_trace_ent_t tracing a function takes */
static inline size_t
ksancov_cmps_trace_func_space(size_t len1_func, size_t len2_func)
{
	static_assert(sizeof(ksancov_cmps_trace_ent_t) == sizeof(uint64_t) * 3 + sizeof(uint32_t) + sizeof(uint16_t) * 2, "ksancov_cmps_trace_ent_t invalid size");

	size_t size = sizeof(uint64_t) + sizeof(uint32_t) + sizeof(uint16_t) * 2; // header
	size += len1_func + len2_func;
	size_t rem = size % sizeof(ksancov_cmps_trace_ent_t);
	if (rem == 0) {
		return size;
	}
	return size + sizeof(ksancov_cmps_trace_ent_t) - rem;
}

static inline uint8_t *
ksancov_cmps_trace_func_arg1(ksancov_cmps_trace_ent_t *entry)
{
	return entry->args_func;
}

static inline uint8_t *
ksancov_cmps_trace_func_arg2(ksancov_cmps_trace_ent_t *entry)
{
	uint8_t* func_args = entry->args_func;
	return &func_args[entry->len1_func];
}

#define KSANCOV_SERIALIZED_TESTCASE_BYTES 16777216 // 16MiB
#define KSANCOV_SERIALIZED_TESTCASES_MAX_COUNT 100

typedef struct ksancov_serialized_testcase {
	uint32_t size;
	uint8_t  buffer[KSANCOV_SERIALIZED_TESTCASE_BYTES];
} ksancov_serialized_testcase_t;

/*
 * Store the latest executed testcases in kernel to dump on panic.
 */
typedef struct ksancov_serialized_testcases {
	uint32_t head;         /* current head of the circular buffer */
	uint32_t inner_index;  /* current inner index in the head testcase (e.g. current call being executed) */
	ksancov_serialized_testcase_t list[];  /* testcases circular buffer */
} ksancov_serialized_testcases_t;

/*
 * Represents state of a ksancov device when userspace asks for coverage data recording.
 */

struct ksancov_dev {
	ksancov_mode_t mode;

	union {
		ksancov_header_t       *hdr;
		ksancov_trace_t        *trace;
		ksancov_counters_t     *counters;
	};
	size_t sz;     /* size of allocated trace/counters buffer */

	size_t maxpcs;

	ksancov_cmps_mode_t cmps_mode;

	union {
		ksancov_header_t       *cmps_hdr;
		ksancov_trace_t        *cmps_trace;
	};
	size_t cmps_sz;     /* size of allocated cmps trace buffer */

	ksancov_serialized_testcases_t* testcases;
	uint32_t testcases_count;     /* number of testcases in the buffer (less or equal than max count) */

	thread_t thread;
	dev_t dev;
	lck_mtx_t lock;
};
typedef struct ksancov_dev * ksancov_dev_t;


#endif /* CONFIG_KSANCOV */

#endif /* KERNEL_PRIVATE */

#endif /* _KCOV_KSANCOV_DATA_H_ */
