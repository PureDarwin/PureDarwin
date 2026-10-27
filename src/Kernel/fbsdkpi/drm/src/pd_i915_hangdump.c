// SPDX-License-Identifier: MIT
// prints i915's gpu error state to the kernel log, PD has no sysfs error node to read it from

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/string.h>

#include "i915_drv.h"
#include "i915_gpu_error.h"
#include "gt/intel_gt.h"
#include "intel_runtime_pm.h"

long pd_drm_tunable_long(const char *path, long def);
void pd_i915_hangdump(struct i915_gpu_coredump *error);
void pd_i915_hangdump_test(struct drm_device *dev);

// whole dump, one text line, and an ascii85 batch line
#define PD_HANGDUMP_MAX		(64 * 1024)
#define PD_HANGDUMP_LINE	2048
#define PD_HANGDUMP_BATCH	(24 * 1024)
#define PD_HANGDUMP_CHUNK	512
#define PD_HANGDUMP_BUF		(16 * 1024)

static atomic_t pd_hangdump_printed;

static bool pd_hangdump_testing;

struct pd_hangdump {
	size_t out;
	unsigned int n;
	bool batch_next;
};

static void
pd_hangdump_emit(struct pd_hangdump *d, const char *s, size_t len, bool cont)
{
	char tmp[PD_HANGDUMP_CHUNK + 1];

	while (len > 0 && d->out < PD_HANGDUMP_MAX) {
		size_t n = min3(len, (size_t)PD_HANGDUMP_CHUNK, PD_HANGDUMP_MAX - d->out);

		memcpy(tmp, s, n);
		tmp[n] = '\0';
		printf("i915 hang %u%c %s\n", d->n, cont ? '+' : ':', tmp);
		d->out += n;
		s += n;
		len -= n;
		cont = true;
		cond_resched();
	}
}

// one complete line of len bytes, s holds its first kept bytes, long ones are cut to their cap
static void
pd_hangdump_line(struct pd_hangdump *d, char *s, size_t kept, size_t len)
{
	size_t cap = d->batch_next ? PD_HANGDUMP_BATCH : PD_HANGDUMP_LINE;

	s[kept] = '\0';
	d->batch_next = strstr(s, " --- batch = ") != NULL;
	pd_hangdump_emit(d, s, min(kept, cap), false);
	if (len > cap && d->out < PD_HANGDUMP_MAX)
		printf("i915 hang %u+ [%zu more bytes left out]\n", d->n, len - cap);
}

static void
pd_hangdump_print(struct i915_gpu_coredump *error, const char *why)
{
	struct pd_hangdump d = { .n = pd_hangdump_testing ? 0 :
	    (unsigned int)atomic_read(&pd_hangdump_printed) };
	size_t cap = PD_HANGDUMP_BATCH + 1, acc_len = 0, line_len = 0;
	char *buf, *acc;
	loff_t off = 0;
	ssize_t got = 0;

	buf = kvmalloc(PD_HANGDUMP_BUF, GFP_KERNEL);
	acc = kvmalloc(cap, GFP_KERNEL);
	if (buf == NULL || acc == NULL) {
		printf("i915 hang %u: no memory for the dump\n", d.n);
		goto out;
	}
	printf("i915 hang %u: gpu error state (%s), first %u KB, long lines cut\n", d.n, why,
	    PD_HANGDUMP_MAX / 1024);

	// lines can span reads, acc gathers one and keeps only its first cap bytes
	while (d.out < PD_HANGDUMP_MAX &&
	    (got = i915_gpu_coredump_copy_to_buffer(error, buf, off, PD_HANGDUMP_BUF)) > 0) {
		off += got;
		for (ssize_t i = 0; i < got; i++) {
			if (buf[i] == '\n') {
				pd_hangdump_line(&d, acc, acc_len, line_len);
				acc_len = line_len = 0;
				continue;
			}
			if (acc_len < cap - 1)
				acc[acc_len++] = buf[i];
			line_len++;
		}
	}
	if (acc_len > 0)
		pd_hangdump_line(&d, acc, acc_len, line_len);
	if (got < 0)
		printf("i915 hang %u: reading the error state failed, %zd\n", d.n, got);
	printf("i915 hang %u: end, %zu of %lld bytes printed\n", d.n, d.out, (long long)off);
out:
	kvfree(acc);
	kvfree(buf);
}

// i915_error_state_store calls this for every capture it is handed
void
pd_i915_hangdump(struct i915_gpu_coredump *error)
{
	long limit = pd_drm_tunable_long("pdi915_hangdump", 1);

	if (pd_hangdump_testing && !IS_ERR_OR_NULL(error))
		pd_hangdump_print(error, "test capture at attach");
	else if (!IS_ERR_OR_NULL(error) && !error->simulated &&
	    atomic_inc_return(&pd_hangdump_printed) <= limit)
		pd_hangdump_print(error, "gpu hang");
}

// pdi915_hangdump_test=1 captures the idle gt once at attach to show the dump works
void
pd_i915_hangdump_test(struct drm_device *dev)
{
	struct drm_i915_private *i915 = to_i915(dev);
	intel_wakeref_t wakeref;

	if (pd_drm_tunable_long("pdi915_hangdump_test", 0) == 0)
		return;
	pd_hangdump_testing = true;
	with_intel_runtime_pm(&i915->runtime_pm, wakeref)
		i915_capture_error_state(to_gt(i915), ALL_ENGINES, CORE_DUMP_FLAG_NONE);
	pd_hangdump_testing = false;
	// frees the slot so a real hang is kept as the first error
	i915_reset_error_state(i915);
}
