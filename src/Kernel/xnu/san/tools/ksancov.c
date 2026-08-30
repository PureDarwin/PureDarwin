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

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <errno.h>

#include <fcntl.h>
#include <sys/ioctl.h>

#include <getopt.h>
#include <unistd.h>

#include "ksancov.h"

static void
usage(void)
{
	fprintf(stderr,
	    "usage: ./ksancov [OPTIONS]\n\n"
	    "  -t | --trace        use trace (PC log) mode [default]\n"
	    "  -s | --stksize      use trace (PC log) with stack size mode\n"
	    "  -c | --counters     use edge counter mode\n"
	    "  -p | --cmptrace     use trace (CMP log) mode\n"
	    "  -n | --entries <n>  override max entries in trace log\n"
	    "  -x | --exec <path>  instrument execution of binary at <path>\n"
	    "  -b | --bundle <b>   bundle for on-demand tracing\n");
	exit(1);
}

/*
 * Structure holds all data required for coverage collection.
 */
typedef struct ksancov_state {
	ksancov_mode_t       ks_mode;
	ksancov_edgemap_t    *ks_edgemap;
	union {
		ksancov_header_t       *ks_header;
		ksancov_trace_t        *ks_trace;
		ksancov_counters_t     *ks_counters;
	};
	ksancov_cmps_mode_t  ks_cmps_mode;
	union {
		ksancov_header_t       *ks_cmps_header;
		ksancov_trace_t        *ks_cmps_trace;
	};
} ksancov_state_t;

/*
 * Configures ksancov device for selected coverage mode.
 */
static int
ksancov_set_mode(int fd, ksancov_mode_t mode, int max_entries)
{
	int ret = 0;

	switch (mode) {
	case KS_MODE_TRACE:
		ret = ksancov_mode_trace(fd, max_entries);
		break;
	case KS_MODE_STKSIZE:
		ret = ksancov_mode_stksize(fd, max_entries);
		break;
	case KS_MODE_COUNTERS:
		ret = ksancov_mode_counters(fd);
		break;
	default:
		perror("ksancov unsupported mode\n");
		return ENOTSUP;
	}

	return ret;
}

/*
 * Configures ksancov device for selected comparison mode.
 */
static int
ksancov_cmps_set_mode(int fd, ksancov_cmps_mode_t mode, int max_entries)
{
	int ret = 0;

	switch (mode) {
	case KS_CMPS_MODE_TRACE:
		ret = ksancov_cmps_mode_trace(fd, max_entries, false);
		break;
	case KS_CMPS_MODE_TRACE_FUNC:
		ret = ksancov_cmps_mode_trace(fd, max_entries, true);
		break;
	default:
		perror("ksancov unsupported cmps mode\n");
		return ENOTSUP;
	}

	return ret;
}

/*
 * Initialize coverage state from provided options. Shared mappings with kernel are established
 * here.
 */
static int
ksancov_init_state(int fd, ksancov_mode_t mode, ksancov_cmps_mode_t cmps_mode, int max_entries, ksancov_state_t *state)
{
	uintptr_t addr;
	size_t sz;
	int ret = 0;

	/* Map edge map into process address space. */
	ret = ksancov_map_edgemap(fd, &addr, NULL);
	if (ret) {
		perror("ksancov map counters\n");
		return ret;
	}
	state->ks_edgemap = (void *)addr;
	fprintf(stderr, "nedges (edgemap) = %u\n", state->ks_edgemap->ke_nedges);

	/* Setup selected tracing mode. */
	ret = ksancov_set_mode(fd, mode, max_entries);
	if (ret) {
		perror("ksancov set mode\n");
		return ret;
	}

	/* Map buffer for selected mode into process address space. */
	ret = ksancov_map(fd, &addr, &sz);
	if (ret) {
		perror("ksancov map");
		return ret;
	}
	fprintf(stderr, "mapped to 0x%lx + %lu\n", addr, sz);

	/* Finalize state members. */
	state->ks_mode = mode;
	state->ks_header = (void *)addr;

	if (mode == KS_MODE_COUNTERS) {
		fprintf(stderr, "nedges (counters) = %u\n", state->ks_counters->kc_nedges);
	} else {
		fprintf(stderr, "maxpcs = %lu\n", ksancov_trace_max_ent(state->ks_trace));
	}

	if (cmps_mode == KS_CMPS_MODE_NONE) {
		state->ks_cmps_mode = cmps_mode;
		state->ks_cmps_header = NULL;
		return ret;
	}

	/* Setup selected comparison tracing mode. */
	ret = ksancov_cmps_set_mode(fd, cmps_mode, max_entries);
	if (ret) {
		perror("ksancov cmps set mode\n");
		return ret;
	}

	/* Map buffer for selected mode into process address space. */
	ret = ksancov_cmps_map(fd, &addr, &sz);
	if (ret) {
		perror("ksancov cmps map");
		return ret;
	}
	fprintf(stderr, "cmps mapped to 0x%lx + %lu\n", addr, sz);

	/* Finalize state members. */
	state->ks_cmps_mode = cmps_mode;
	state->ks_cmps_header = (void *)addr;

	fprintf(stderr, "maxcmps = %lu\n", ksancov_trace_max_ent(state->ks_cmps_trace));

	return ret;
}

static int
ksancov_print_state(ksancov_state_t *state)
{
	if (state->ks_mode == KS_MODE_COUNTERS) {
		for (size_t i = 0; i < state->ks_counters->kc_nedges; i++) {
			size_t hits = state->ks_counters->kc_hits[i];
			if (hits) {
				fprintf(stderr, "0x%lx: %lu hits [idx %lu]\n",
				    ksancov_edge_addr(state->ks_edgemap, i), hits, i);
			}
		}
	} else {
		size_t head = ksancov_trace_head(state->ks_trace);
		fprintf(stderr, "head = %lu\n", head);

		for (uint32_t i = 0; i < head; i++) {
			if (state->ks_mode == KS_MODE_TRACE) {
				fprintf(stderr, "0x%lx\n", ksancov_trace_entry(state->ks_trace, i));
			} else {
				fprintf(stderr, "0x%lx [size %u]\n", ksancov_stksize_pc(state->ks_trace, i),
				    ksancov_stksize_size(state->ks_trace, i));
			}
		}
	}

	if (state->ks_cmps_mode == KS_CMPS_MODE_TRACE || state->ks_cmps_mode == KS_CMPS_MODE_TRACE_FUNC) {
		static const char *type_map[KCOV_CMP_SIZE8 + 1] = {
			"8 bits", NULL, "16 bits", NULL, "32 bits",
			NULL, "64 bits"
		};

		size_t head = ksancov_trace_head(state->ks_cmps_trace);
		fprintf(stderr, "cmps head = %lu\n", head);

		for (uint32_t i = 0; i < head;) {
			ksancov_cmps_trace_ent_t *entry = ksancov_cmps_trace_entry(state->ks_cmps_trace, i);
			if (KCOV_CMP_IS_FUNC(entry->type)) {
				size_t space = ksancov_cmps_trace_func_space(entry->len1_func, entry->len2_func);
				i += space / sizeof(ksancov_cmps_trace_ent_t);
				fprintf(stderr, "0x%llx [func %u %u] '%s' '%s'\n", entry->pc, entry->len1_func, entry->len2_func,
				    ksancov_cmps_trace_func_arg1(entry),
				    ksancov_cmps_trace_func_arg2(entry));
			} else {
				uint64_t type = entry->type & KCOV_CMP_SIZE_MASK;
				fprintf(stderr, "0x%llx [%s] 0x%llx 0x%llx\n", entry->pc, type_map[type], entry->args[0], entry->args[1]);
				++i;
			}
		}
	}

	return 0;
}

static int
ksancov_on_demand_set_enabled(int fd, const char *bundle, bool enabled)
{
	int ret = 0;
	const uint64_t gate = enabled ? 1 : 0;
	if (bundle) {
		fprintf(stderr, "setting on-demand gate for '%s': %llu\n", bundle, gate);
		ret = ksancov_on_demand_set_gate(fd, bundle, gate);
		if (ret) {
			perror("ksancov on demand");
		}
	}
	return ret;
}

int
main(int argc, char *argv[])
{
	ksancov_mode_t ksan_mode = KS_MODE_NONE;
	ksancov_cmps_mode_t ksan_cmps_mode = KS_CMPS_MODE_NONE;
	ksancov_state_t ksan_state = {0};

	int ret;
	size_t max_entries = 64UL * 1024;
	char *path = NULL;
	char *od_bundle = NULL;

	static struct option opts[] = {
		{ "entries", required_argument, NULL, 'n' },
		{ "exec", required_argument, NULL, 'x' },

		{ "trace", no_argument, NULL, 't' },
		{ "counters", no_argument, NULL, 'c' },
		{ "stksize", no_argument, NULL, 's' },
		{ "cmptrace", no_argument, NULL, 'p' },

		{ "bundle", required_argument, NULL, 'b' },

		{ NULL, 0, NULL, 0 }
	};

	int ch;
	while ((ch = getopt_long(argc, argv, "tsn:x:cpb:", opts, NULL)) != -1) {
		switch (ch) {
		case 'n':
			max_entries = strtoul(optarg, NULL, 0);
			break;
		case 'x':
			path = optarg;
			break;
		case 't':
			ksan_mode = KS_MODE_TRACE;
			break;
		case 'c':
			ksan_mode = KS_MODE_COUNTERS;
			break;
		case 's':
			ksan_mode = KS_MODE_STKSIZE;
			break;
		case 'p':
			ksan_cmps_mode = KS_CMPS_MODE_TRACE_FUNC;
			break;
		case 'b':
			od_bundle = optarg;
			break;
		default:
			usage();
		}
	}

	int fd = ksancov_open();
	if (fd < 0) {
		perror("ksancov_open");
		return errno;
	}
	fprintf(stderr, "opened ksancov on fd %i\n", fd);

	/* Initialize ksancov state. */
	ret = ksancov_init_state(fd, ksan_mode, ksan_cmps_mode, max_entries, &ksan_state);
	if (ret) {
		perror("ksancov init\n");
		return ret;
	}

	/* Execute binary (when provided) with enabled coverage collection. Run getppid() otherwise. */
	if (path) {
		int pid = fork();
		if (pid == 0) {
			/* child */

			ret = ksancov_thread_self(fd);
			if (ret) {
				perror("ksancov thread");
				return ret;
			}

			ksancov_on_demand_set_enabled(fd, od_bundle, true);
			ksancov_reset(ksan_state.ks_header);
			ksancov_start(ksan_state.ks_header);
			if (ksan_state.ks_cmps_header) {
				ksancov_reset(ksan_state.ks_cmps_header);
				ksancov_start(ksan_state.ks_cmps_header);
			}
			ret = execl(path, path, 0);
			perror("execl");
			ksancov_on_demand_set_enabled(fd, od_bundle, false);

			exit(1);
		} else {
			/* parent */
			waitpid(pid, NULL, 0);
			ksancov_stop(ksan_state.ks_header);
			if (ksan_state.ks_cmps_header) {
				ksancov_stop(ksan_state.ks_cmps_header);
			}
			ksancov_on_demand_set_enabled(fd, od_bundle, false);
		}
	} else {
		ret = ksancov_thread_self(fd);
		if (ret) {
			perror("ksancov thread");
			return ret;
		}

		ksancov_on_demand_set_enabled(fd, od_bundle, true);
		ksancov_reset(ksan_state.ks_header);
		ksancov_start(ksan_state.ks_header);
		if (ksan_state.ks_cmps_header) {
			ksancov_reset(ksan_state.ks_cmps_header);
			ksancov_start(ksan_state.ks_cmps_header);
		}
		int ppid = getppid();
		ksancov_stop(ksan_state.ks_header);
		if (ksan_state.ks_cmps_header) {
			ksancov_stop(ksan_state.ks_cmps_header);
		}
		ksancov_on_demand_set_enabled(fd, od_bundle, false);
		fprintf(stderr, "ppid = %i\n", ppid);
	}

	/* Print report and cleanup. */
	ksancov_print_state(&ksan_state);
	ret = close(fd);
	fprintf(stderr, "close = %i\n", ret);

	return 0;
}
