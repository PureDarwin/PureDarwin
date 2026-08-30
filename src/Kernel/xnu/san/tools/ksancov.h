/*
 * Copyright (c) 2019 Apple Inc. All rights reserved.
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
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */

#ifndef _KSANCOV_H_
#define _KSANCOV_H_

#include <stdint.h>
#include <stdatomic.h>
#include <sys/ioccom.h>
#include <sys/ioctl.h>
#include <strings.h>
#include <assert.h>
#include <unistd.h>

#define KSANCOV_DEVNODE "ksancov"
#define KSANCOV_PATH "/dev/" KSANCOV_DEVNODE

/* Set mode */
#define KSANCOV_IOC_TRACE            _IOW('K', 1, size_t) /* number of pcs */
#define KSANCOV_IOC_COUNTERS         _IO('K', 2)
#define KSANCOV_IOC_STKSIZE          _IOW('K', 3, size_t) /* number of pcs */

/* Establish a shared mapping of the coverage buffer. */
#define KSANCOV_IOC_MAP              _IOWR('K', 8, struct ksancov_buf_desc)

/* Establish a shared mapping of the edge address buffer. */
#define KSANCOV_IOC_MAP_EDGEMAP      _IOWR('K', 9, struct ksancov_buf_desc)

/* Log the current thread */
#define KSANCOV_IOC_START            _IOW('K', 10, uintptr_t)
#define KSANCOV_IOC_NEDGES           _IOR('K', 50, size_t)
#define KSANCOV_IOC_TESTPANIC        _IOW('K', 20, uint64_t)

/* Operations related to on-demand instrumentation */
#define KSANCOV_IOC_ON_DEMAND        _IOWR('K', 60, struct ksancov_on_demand_msg)

/* Set comparison log mode */
#define KSANCOV_IOC_CMPS_TRACE       _IOW('K', 70, size_t) /* number of cmps */
#define KSANCOV_IOC_CMPS_TRACE_FUNC  _IOW('K', 71, size_t) /* number of cmps */

/* Establish a shared mapping of the comparisons buffer. */
#define KSANCOV_IOC_CMPS_MAP         _IOWR('K', 90, struct ksancov_buf_desc)

/* Testcases buffer */
#define KSANCOV_IOC_TESTCASES       _IOW('K', 100, size_t) /* number of testcases */
#define KSANCOV_IOC_TESTCASES_MAP   _IOWR('K', 101, struct ksancov_buf_desc)
#define KSANCOV_IOC_TESTCASES_LOG   _IO('K', 102)

/*
 * shared kernel-user mapping
 */

#define KSANCOV_MAX_EDGES         (1 << 24)
#define KSANCOV_MAX_HITS          UINT8_MAX
#define KSANCOV_TRACE_MAGIC       (uint32_t)0x5AD17F5BU
#define KSANCOV_COUNTERS_MAGIC    (uint32_t)0x5AD27F6BU
#define KSANCOV_EDGEMAP_MAGIC     (uint32_t)0x5AD37F7BU
#define KSANCOV_STKSIZE_MAGIC     (uint32_t)0x5AD47F8BU
#define KSANCOV_CMPS_TRACE_MAGIC  (uint32_t)0x5AD47F9BU

/*
 * ioctl
 */

struct ksancov_buf_desc {
	uintptr_t ptr;  /* ptr to shared buffer [out] */
	size_t sz;      /* size of shared buffer [out] */
};

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

/* PC tracing only records PCs. */
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

/* Comparison type values */
enum {
	KCOV_CMP_CONST = 1,
	KCOV_CMP_SIZE1 = 0,
	KCOV_CMP_SIZE2 = 2,
	KCOV_CMP_SIZE4 = 4,
	KCOV_CMP_SIZE8 = 6,

	KCOV_CMP_FUNC_MEMCMP = 32,
	KCOV_CMP_FUNC_STRCMP = 34,
	KCOV_CMP_FUNC_STRNCMP = 36,
	KCOV_CMP_FUNC_STRBUFCMP = 38,
};

#define KCOV_CMP_SIZE_MASK 0xfffffff6
#define KCOV_CMP_IS_FUNC(type) (type >= KCOV_CMP_FUNC_MEMCMP)

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
 * On-demand related functionalities
 */
typedef enum {
	KS_OD_GET_GATE = 1,
	KS_OD_SET_GATE = 2,
	KS_OD_GET_RANGE = 3,
} ksancov_on_demand_operation_t;

struct ksancov_on_demand_msg {
	char bundle[/*KMOD_MAX_NAME*/ 64];
	ksancov_on_demand_operation_t operation;
	union {
		uint64_t gate;
		struct {
			uint32_t start;
			uint32_t stop;
		} range;
	};
};

/*
 * ksancov userspace API
 *
 * Usage:
 * 1) open the ksancov device
 * 2) set the coverage mode
 * 3) map the coverage buffer
 * 4) start the trace on a thread
 * 5) flip the enable bit
 */

static inline int
ksancov_open(void)
{
	return open(KSANCOV_PATH, 0);
}

static inline int
ksancov_map(int fd, uintptr_t *buf, size_t *sz)
{
	int ret;
	struct ksancov_buf_desc mc = {0};

	assert(buf != NULL);

	ret = ioctl(fd, KSANCOV_IOC_MAP, &mc);
	if (ret == -1) {
		return errno;
	}

	*buf = mc.ptr;
	if (sz) {
		*sz = mc.sz;
	}

	ksancov_header_t *hdr = (ksancov_header_t *)mc.ptr;
	assert(hdr->kh_magic == KSANCOV_TRACE_MAGIC ||
	    hdr->kh_magic == KSANCOV_COUNTERS_MAGIC ||
	    hdr->kh_magic == KSANCOV_STKSIZE_MAGIC);

	return 0;
}

static inline int
ksancov_map_edgemap(int fd, uintptr_t *buf, size_t *sz)
{
	int ret;
	struct ksancov_buf_desc mc = {0};

	assert(buf != NULL);

	ret = ioctl(fd, KSANCOV_IOC_MAP_EDGEMAP, &mc);
	if (ret == -1) {
		return errno;
	}

	*buf = mc.ptr;
	if (sz) {
		*sz = mc.sz;
	}

	ksancov_edgemap_t *emap = (ksancov_edgemap_t *)mc.ptr;
	assert(emap->ke_magic == KSANCOV_EDGEMAP_MAGIC);

	return 0;
}

static inline size_t
ksancov_nedges(int fd)
{
	size_t nedges;
	int ret = ioctl(fd, KSANCOV_IOC_NEDGES, &nedges);
	if (ret == -1) {
		return SIZE_MAX;
	}
	return nedges;
}

static inline int
ksancov_mode_trace(int fd, size_t entries)
{
	int ret;
	ret = ioctl(fd, KSANCOV_IOC_TRACE, &entries);
	if (ret == -1) {
		return errno;
	}
	return 0;
}

static inline int
ksancov_mode_stksize(int fd, size_t entries)
{
	int ret;
	ret = ioctl(fd, KSANCOV_IOC_STKSIZE, &entries);
	if (ret == -1) {
		return errno;
	}
	return 0;
}

static inline int
ksancov_mode_counters(int fd)
{
	int ret;
	ret = ioctl(fd, KSANCOV_IOC_COUNTERS);
	if (ret == -1) {
		return errno;
	}
	return 0;
}

static inline int
ksancov_thread_self(int fd)
{
	int ret;
	uintptr_t th = 0;
	ret = ioctl(fd, KSANCOV_IOC_START, &th);
	if (ret == -1) {
		return errno;
	}
	return 0;
}

static inline int
ksancov_start(void *buf)
{
	ksancov_header_t *hdr = (ksancov_header_t *)buf;
	atomic_store_explicit(&hdr->kh_enabled, 1, memory_order_relaxed);
	return 0;
}

static inline int
ksancov_stop(void *buf)
{
	ksancov_header_t *hdr = (ksancov_header_t *)buf;
	atomic_store_explicit(&hdr->kh_enabled, 0, memory_order_relaxed);
	return 0;
}

static inline int
ksancov_reset(void *buf)
{
	ksancov_header_t *hdr = (ksancov_header_t *)buf;
	if (hdr->kh_magic == KSANCOV_TRACE_MAGIC || hdr->kh_magic == KSANCOV_STKSIZE_MAGIC || hdr->kh_magic == KSANCOV_CMPS_TRACE_MAGIC) {
		ksancov_trace_t *trace = (ksancov_trace_t *)buf;
		atomic_store_explicit(&trace->kt_head, 0, memory_order_relaxed);
	} else if (hdr->kh_magic == KSANCOV_COUNTERS_MAGIC) {
		ksancov_counters_t *counters = (ksancov_counters_t *)buf;
		bzero(counters->kc_hits, counters->kc_nedges);
	} else {
		return EINVAL;
	}
	return 0;
}

static inline uintptr_t
ksancov_edge_addr(ksancov_edgemap_t *kemap, size_t idx)
{
	assert(kemap);
	if (idx >= kemap->ke_nedges) {
		return 0;
	}
	return kemap->ke_addrs[idx];
}

static inline size_t
ksancov_trace_max_ent(ksancov_trace_t *trace)
{
	assert(trace);
	return trace->kt_maxent;
}

static inline size_t
ksancov_trace_head(ksancov_trace_t *trace)
{
	assert(trace);
	size_t maxent = trace->kt_maxent;
	size_t head = atomic_load_explicit(&trace->kt_head, memory_order_acquire);
	return head < maxent ? head : maxent;
}

static inline uintptr_t
ksancov_trace_entry(ksancov_trace_t *trace, size_t i)
{
	assert(trace);
	assert(trace->kt_hdr.kh_magic == KSANCOV_TRACE_MAGIC);
	if (i >= trace->kt_head) {
		return 0;
	}

	ksancov_trace_pc_ent_t *entries = (ksancov_trace_pc_ent_t *)trace->kt_entries;
	return entries[i];
}

static inline uintptr_t
ksancov_stksize_pc(ksancov_trace_t *trace, size_t i)
{
	assert(trace);
	assert(trace->kt_hdr.kh_magic == KSANCOV_STKSIZE_MAGIC);
	if (i >= trace->kt_head) {
		return 0;
	}

	ksancov_trace_stksize_ent_t *entries = (ksancov_trace_stksize_ent_t *)trace->kt_entries;
	return entries[i].pc;
}

static inline uint32_t
ksancov_stksize_size(ksancov_trace_t *trace, size_t i)
{
	assert(trace);
	assert(trace->kt_hdr.kh_magic == KSANCOV_STKSIZE_MAGIC);
	if (i >= trace->kt_head) {
		return 0;
	}

	ksancov_trace_stksize_ent_t *entries = (ksancov_trace_stksize_ent_t *)trace->kt_entries;
	return entries[i].stksize;
}

static inline int
ksancov_cmps_mode_trace(int fd, size_t entries, bool trace_functions)
{
	int ret;
	if (trace_functions) {
		ret = ioctl(fd, KSANCOV_IOC_CMPS_TRACE_FUNC, &entries);
	} else {
		ret = ioctl(fd, KSANCOV_IOC_CMPS_TRACE, &entries);
	}
	if (ret == -1) {
		return errno;
	}
	return 0;
}

static inline int
ksancov_cmps_map(int fd, uintptr_t *buf, size_t *sz)
{
	int ret;
	struct ksancov_buf_desc mc = {0};

	assert(buf != NULL);

	ret = ioctl(fd, KSANCOV_IOC_CMPS_MAP, &mc);
	if (ret == -1) {
		return errno;
	}

	*buf = mc.ptr;
	if (sz) {
		*sz = mc.sz;
	}

	ksancov_header_t *hdr = (ksancov_header_t *)mc.ptr;
	assert(hdr->kh_magic == KSANCOV_CMPS_TRACE_MAGIC);

	return 0;
}

static inline ksancov_cmps_trace_ent_t*
ksancov_cmps_trace_entry(ksancov_trace_t *trace, size_t i)
{
	assert(trace);
	assert(trace->kt_hdr.kh_magic == KSANCOV_CMPS_TRACE_MAGIC);
	if (i >= trace->kt_head) {
		return NULL;
	}

	ksancov_cmps_trace_ent_t *entries = (ksancov_cmps_trace_ent_t *)trace->kt_entries;
	return &entries[i];
}

static inline int
ksancov_testcases(int fd, size_t num_testcases)
{
	int ret = ioctl(fd, KSANCOV_IOC_TESTCASES, &num_testcases);
	if (ret == -1) {
		return errno;
	}
	return 0;
}

static inline int
ksancov_testcases_map(int fd, uintptr_t *buf, size_t *sz)
{
	int ret;
	struct ksancov_buf_desc mc = {0};

	assert(buf != NULL);

	ret = ioctl(fd, KSANCOV_IOC_TESTCASES_MAP, &mc);
	if (ret == -1) {
		return errno;
	}

	*buf = mc.ptr;
	if (sz) {
		*sz = mc.sz;
	}

	return 0;
}

static inline int
ksancov_testcases_log(int fd)
{
	int ret;
	ret = ioctl(fd, KSANCOV_IOC_TESTCASES_LOG);
	if (ret == -1) {
		return errno;
	}
	return 0;
}

/*
 * On-demand control API
 */

static inline int
_ksancov_on_demand_operation(int fd, const char *bundle, ksancov_on_demand_operation_t op, struct ksancov_on_demand_msg *msg)
{
	int ret;

	msg->operation = op;
	strlcpy(msg->bundle, bundle, sizeof(msg->bundle));

	ret = ioctl(fd, KSANCOV_IOC_ON_DEMAND, msg);
	if (ret == -1) {
		return errno;
	}

	return ret;
}

/*
 * Retrieve the value of the gate for a given module bundle ID.
 */
static inline int
ksancov_on_demand_get_gate(int fd, const char *bundle, uint64_t *gate)
{
	assert(gate);

	struct ksancov_on_demand_msg msg;
	int ret = _ksancov_on_demand_operation(fd, bundle, KS_OD_GET_GATE, &msg);
	if (ret == 0) {
		*gate = msg.gate;
	}
	return ret;
}

/*
 * Set the value of the gate for a given module bundle ID.
 *
 * Any non-zero value enables the invocation of the sanitizer coverage callbacks
 * inserted in the specified module.
 */
static inline int
ksancov_on_demand_set_gate(int fd, const char *bundle, uint64_t value)
{
	struct ksancov_on_demand_msg msg = {};
	msg.gate = value;
	return _ksancov_on_demand_operation(fd, bundle, KS_OD_SET_GATE, &msg);
}

/*
 * Get the guards range for a specified module.
 */
static inline int
ksancov_on_demand_get_range(int fd, const char *bundle, uint32_t *start, uint32_t *stop)
{
	assert(start && stop);

	struct ksancov_on_demand_msg msg = {};
	int ret = _ksancov_on_demand_operation(fd, bundle, KS_OD_GET_RANGE, &msg);
	if (ret == 0) {
		*start = msg.range.start;
		*stop = msg.range.stop;
	}
	return ret;
}

#endif /* _KSANCOV_H_ */
