/*
 * Copyright (c) 2024 Apple Inc. All rights reserved.
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

#include <kern/cpu_data.h>
#include <kern/kalloc.h>
#include <kern/locks.h>
#include <kern/mem_acct.h>
#include <kern/percpu.h>

#include <os/atomic_private.h>
#include <os/log.h>
#include <os/ptrtools.h>

#include <sys/mem_acct_private.h>
#include <sys/param.h>
#include <sys/sysctl.h>

#include <net/net_sysctl.h>

struct mem_acct {
	int64_t _Atomic ma_allocated; /* Amount of memory accounted towards this subsystem (ignore temporary per-CPU accounting from below) */
	int32_t *__zpercpu ma_percpu; /* Per-CPU "bounce-buffer" of accounting that will be folded in to `ma_allocated` */
	uint64_t ma_hardlimit; /* hard limit that will not be exceeded */
	uint8_t ma_percent; /* Percent of hard-limit we should start soft-limiting (if != 100 && != 0) */
	uint64_t _Atomic ma_peak;
	char ma_name[MEM_ACCT_NAME_LENGTH]; /* Name of the subsystem using this instance of memory-accounting module */
};

#define MEM_ACCT_PCPU_MAX 1024 * 1024 /* Update global var after 1MB in the per-cpu var */

static struct mem_acct *memacct[MEM_ACCT_MAX];

static uint64_t
mem_acct_softlimit(uint64_t hardlimit, uint8_t percent)
{
	return (hardlimit * percent) / 100;
}

static uint64_t
mem_acct_presoftlimit(uint64_t hardlimit, uint8_t percent)
{
	return (mem_acct_softlimit(hardlimit, percent) * percent) / 100;
}

int
mem_acct_limited(const struct mem_acct *macct)
{
	uint64_t hardlimit;
	int64_t allocated;
	uint8_t percent;

	allocated = os_atomic_load(&macct->ma_allocated, relaxed);
	if (allocated < 0) {
		return 0;
	}

	hardlimit = os_access_once(macct->ma_hardlimit);
	if (hardlimit && allocated > hardlimit) {
		return MEMACCT_HARDLIMIT;
	}

	percent = os_access_once(macct->ma_percent);
	if (percent) {
		if (allocated > mem_acct_softlimit(hardlimit, percent)) {
			return MEMACCT_SOFTLIMIT;
		}

		if (allocated > mem_acct_presoftlimit(hardlimit, percent)) {
			return MEMACCT_PRESOFTLIMIT;
		}
	}

	return 0;
}

void
_mem_acct_add(struct mem_acct *macct, int size)
{
	int *pcpu;

	/*
	 * Yes, the accounting is not 100% accurate with the per-cpu
	 * "bounce-buffer" storing intermediate results. For example, we may
	 * report "hard-limit" even though all the per-cpu counters may bring us
	 * below the limit. But honestly, we don't care... If we hit hard-limit
	 * the system is gonna be in a bad state anyways until we have given
	 * away enough memory.
	 *
	 * The same counts for softlimit, but softlimit still allows us to
	 * account memory and just makes us a bit more aggressive at freeing
	 * stuff.
	 */

	/* Now, add the size to the per-cpu variable */
	disable_preemption();
	pcpu = zpercpu_get(macct->ma_percpu);
	*pcpu += size;

	/* If we added enough to the pcpu variable, fold it into the global variable */
	if (*pcpu > MEM_ACCT_PCPU_MAX || *pcpu < -MEM_ACCT_PCPU_MAX) {
		int limited, newlimited;
		int64_t allocated;

		limited = mem_acct_limited(macct);

		allocated = os_atomic_add(&macct->ma_allocated, *pcpu, relaxed);

		/*
		 * Can be temporarily < 0 if the CPU freeing memory hits
		 * MEM_ACCT_PCPU_MAX first.
		 */
		if (allocated > 0) {
			os_atomic_max(&macct->ma_peak, allocated, relaxed);
		}

		newlimited = mem_acct_limited(macct);
		if (limited != newlimited) {
			os_log(OS_LOG_DEFAULT,
			    "memacct: %s goes from %u to %u for its limit",
			    macct->ma_name, limited, newlimited);
		}

		*pcpu = 0;
	}
	enable_preemption();
}

static LCK_GRP_DECLARE(mem_acct_mtx_grp, "mem_acct");
static LCK_MTX_DECLARE(mem_acct_mtx, &mem_acct_mtx_grp);

struct mem_acct *
mem_acct_register(const char *__null_terminated name,
    uint64_t hardlimit, uint8_t percent)
{
	struct mem_acct *acct = NULL;
	int i, index = -1;

	if (percent > 100) {
		os_log(OS_LOG_DEFAULT,
		    "memacct: percentage for softlimit is out-of-bounds\n");
		return NULL;
	}

	lck_mtx_lock(&mem_acct_mtx);

	/* Find an empty slot in the accounting array and check for name uniqueness */
	for (i = 0; i < MEM_ACCT_MAX; i++) {
		if (memacct[i] == NULL) {
			if (index == -1) {
				index = i;
			}

			continue;
		}

		if (strlcmp(memacct[i]->ma_name, name, MEM_ACCT_NAME_LENGTH - 1) == 0) {
			os_log(OS_LOG_DEFAULT,
			    "memacct: subsystem %s already exists", name);
			goto exit;
		}
	}

	if (index == -1) {
		os_log(OS_LOG_DEFAULT, "memacct: No space for additional subsystem");
		goto exit;
	}

	memacct[index] = kalloc_type(struct mem_acct, Z_WAITOK_ZERO_NOFAIL);

	acct = memacct[index];

	strlcpy(acct->ma_name, name, MEM_ACCT_NAME_LENGTH);
	acct->ma_hardlimit = hardlimit;
	if (percent >= 100) {
		os_log(OS_LOG_DEFAULT,
		    "memacct: percent is > 100");

		memacct[index] = NULL;
		kfree_type(struct mem_acct, acct);
		acct = NULL;

		goto exit;
	}
	acct->ma_percent = percent;
	acct->ma_percpu = zalloc_percpu_permanent_type(int32_t);

exit:
	lck_mtx_unlock(&mem_acct_mtx);

	return acct;
}

/*
 *	Memory Accounting sysctl handlers
 */

struct walkarg {
	int     w_op, w_sub;
	struct sysctl_req *w_req;
};

/* sysctls on a per-subsystem basis */
static int sysctl_subsystem_peak(struct walkarg *w);
static int sysctl_subsystem_soft_limit(struct walkarg *w);
static int sysctl_subsystem_hard_limit(struct walkarg *w);
static int sysctl_subsystem_allocated(struct walkarg *w);
static int sysctl_all_subsystem_statistics(struct walkarg *w);

/* sysctls for all active subsystems */
static int sysctl_all_statistics(struct sysctl_req *);
static int sysctl_mem_acct_subsystems(struct sysctl_req *);

/* Handler function for all Memory Accounting sysctls */
static int sysctl_mem_acct SYSCTL_HANDLER_ARGS;

/* Helper functions */
static void memacct_copy_stats(struct memacct_statistics *s, struct mem_acct *a);

SYSCTL_NODE(_kern, OID_AUTO, memacct,
    CTLFLAG_RW | CTLFLAG_LOCKED, sysctl_mem_acct, "Memory Accounting");

static int
sysctl_mem_acct SYSCTL_HANDLER_ARGS
{
#pragma unused(oidp)
	DECLARE_SYSCTL_HANDLER_ARG_ARRAY(int, 2, name, namelen);
	int error = EINVAL;
	struct walkarg w;

	/* Verify the specified subsystem index is valid */
	if (name[1] >= MEM_ACCT_MAX || name[1] < 0) {
		return EINVAL;
	}

	bzero(&w, sizeof(w));
	w.w_req = req;
	w.w_op = name[0];
	w.w_sub = name[1];

	switch (w.w_op) {
	case MEM_ACCT_PEAK:
		error = sysctl_subsystem_peak(&w);
		break;
	case MEM_ACCT_SOFT_LIMIT:
		error = sysctl_subsystem_soft_limit(&w);
		break;
	case MEM_ACCT_HARD_LIMIT:
		error = sysctl_subsystem_hard_limit(&w);
		break;
	case MEM_ACCT_ALLOCATED:
		error = sysctl_subsystem_allocated(&w);
		break;
	case MEM_ACCT_SUBSYSTEMS:
		error = sysctl_mem_acct_subsystems(req);
		break;
	case MEM_ACCT_ALL_SUBSYSTEM_STATISTICS:
		error = sysctl_all_subsystem_statistics(&w);
		break;
	case MEM_ACCT_ALL_STATISTICS:
		error = sysctl_all_statistics(req);
		break;
	}

	return error;
}

static int
sysctl_subsystem_peak(struct walkarg *w)
{
	int error;
	uint64_t value;
	int changed = 0;
	struct mem_acct *acct = memacct[w->w_sub];

	if (acct == NULL) {
		return ENOENT;
	}

	value = os_atomic_load(&acct->ma_peak, relaxed);
	error = sysctl_io_number(w->w_req, value, sizeof(value), &value, &changed);
	if (error || !changed) {
		return error;
	}

	os_atomic_store(&acct->ma_peak, value, relaxed);
	return 0;
}

static int
sysctl_subsystem_soft_limit(struct walkarg *w)
{
	int error;
	uint64_t hardlimit, value;
	int changed = 0;
	struct mem_acct *acct = memacct[w->w_sub];

	if (acct == NULL) {
		return ENOENT;
	}

	hardlimit = os_atomic_load(&acct->ma_hardlimit, relaxed);
	if (acct->ma_percent) {
		value = mem_acct_softlimit(hardlimit, acct->ma_percent);
	} else {
		value = hardlimit;
	}
	error = sysctl_io_number(w->w_req, value, sizeof(value), &value, &changed);
	if (error || !changed) {
		return error;
	}

	return EPERM;
}

static int
sysctl_subsystem_hard_limit(struct walkarg *w)
{
	int error;
	uint64_t value;
	int changed = 0;
	struct mem_acct *acct = memacct[w->w_sub];

	if (acct == NULL) {
		return ENOENT;
	}

	value = os_atomic_load(&acct->ma_hardlimit, relaxed);
	error = sysctl_io_number(w->w_req, value, sizeof(value), &value, &changed);
	if (error || !changed) {
		return error;
	}

	acct->ma_hardlimit = value;
	return 0;
}

static int
sysctl_subsystem_allocated(struct walkarg *w)
{
	int64_t value;
	struct mem_acct *acct = memacct[w->w_sub];

	lck_mtx_lock(&mem_acct_mtx);

	if (acct == NULL) {
		return ENOENT;
	}

	value = os_atomic_load(&acct->ma_allocated, relaxed);
	zpercpu_foreach(v, acct->ma_percpu) {
		value += *v;
	}

	lck_mtx_unlock(&mem_acct_mtx);

	return sysctl_io_number(w->w_req, value, sizeof(value), NULL, NULL);
}

static int
sysctl_all_subsystem_statistics(struct walkarg *w)
{
	/* Returns a single memacct_statistics struct for the specified subsystem */
	struct memacct_statistics stats = {};
	struct mem_acct *acct = memacct[w->w_sub];

	lck_mtx_lock(&mem_acct_mtx);

	if (acct == NULL) {
		return ENOENT;
	}

	memacct_copy_stats(&stats, acct);

	lck_mtx_unlock(&mem_acct_mtx);

	return sysctl_io_opaque(w->w_req, &stats, sizeof(stats), NULL);
}

static int
sysctl_all_statistics(struct sysctl_req *req)
{
	/* Returns an array of memacct_statistics structs for all active subsystems */
	int i, error;
	int count = 0;

	lck_mtx_lock(&mem_acct_mtx);

	for (i = 0; i < MEM_ACCT_MAX; i++) {
		if (memacct[i] == NULL) {
			break;
		}
		count++;
	}

	struct memacct_statistics *memstats = kalloc_data(sizeof(struct memacct_statistics) * count, Z_WAITOK_ZERO_NOFAIL);

	for (i = 0; i < count; i++) {
		struct mem_acct *acct;
		struct memacct_statistics *stats;

		acct = memacct[i];
		stats = &memstats[i];

		memacct_copy_stats(stats, acct);
	}

	lck_mtx_unlock(&mem_acct_mtx);

	error = sysctl_io_opaque(req, memstats, sizeof(struct memacct_statistics) * count, NULL);
	if (error) {
		kfree_data(memstats, sizeof(struct memacct_statistics) * count);
		return error;
	}

	kfree_data(memstats, sizeof(struct memacct_statistics) * count);
	return 0;
}

static int
sysctl_mem_acct_subsystems(struct sysctl_req *req)
{
	/* Returns an array names for all active subsystems */
	int i, j, error;
	int count = 0;
	int totalCharCount = 0;

	lck_mtx_lock(&mem_acct_mtx);

	for (i = 0; i < MEM_ACCT_MAX; i++) {
		if (memacct[i] == NULL) {
			break;
		}
		count++;
	}

	char *names = kalloc_data(count * MEM_ACCT_NAME_LENGTH, Z_WAITOK_ZERO_NOFAIL);

	for (i = 0; i < count; i++) {
		struct mem_acct *acct = memacct[i];
		char acct_name[MEM_ACCT_NAME_LENGTH];

		strbufcpy(acct_name, acct->ma_name);

		for (j = 0; j < MEM_ACCT_NAME_LENGTH; j++) {
			names[totalCharCount++] = acct_name[j];
		}
	}

	lck_mtx_unlock(&mem_acct_mtx);

	error = sysctl_io_opaque(req, names, sizeof(char) * count * MEM_ACCT_NAME_LENGTH, NULL);
	if (error) {
		kfree_data(names, sizeof(char) * count * MEM_ACCT_NAME_LENGTH);
		return error;
	}

	kfree_data(names, sizeof(char) * count * MEM_ACCT_NAME_LENGTH);
	return 0;
}

static void
memacct_copy_stats(struct memacct_statistics *s, struct mem_acct *a)
{
	s->peak = os_atomic_load(&a->ma_peak, relaxed);
	s->allocated = os_atomic_load(&a->ma_allocated, relaxed);
	zpercpu_foreach(v, a->ma_percpu) {
		s->allocated += *v;
	}
	if (a->ma_percent) {
		s->softlimit = mem_acct_softlimit(a->ma_hardlimit, a->ma_percent);
	} else {
		s->softlimit = a->ma_hardlimit;
	}
	s->hardlimit = a->ma_hardlimit;
	strbufcpy(s->ma_name, a->ma_name);
}
