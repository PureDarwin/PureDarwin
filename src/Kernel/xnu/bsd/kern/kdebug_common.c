/*
 * Copyright (c) 2000-2021 Apple Inc. All rights reserved.
 *
 * @Apple_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */

#include <sys/kdebug_common.h>
#include <vm/vm_kern_xnu.h>

LCK_GRP_DECLARE(kdebug_lck_grp, "kdebug");
int kdbg_debug = 0;

extern struct kd_control kd_control_trace, kd_control_triage;

int
kdebug_storage_lock(struct kd_control *kd_ctrl_page)
{
	int intrs_en = ml_set_interrupts_enabled(false);
	lck_spin_lock_grp(&kd_ctrl_page->kdc_storage_lock, &kdebug_lck_grp);
	return intrs_en;
}

void
kdebug_storage_unlock(struct kd_control *kd_ctrl_page, int intrs_en)
{
	lck_spin_unlock(&kd_ctrl_page->kdc_storage_lock);
	ml_set_interrupts_enabled(intrs_en);
}

// Turn on boot tracing and set the number of events.
static TUNABLE(unsigned int, new_nkdbufs, "trace", 0);
// Enable wrapping during boot tracing.
TUNABLE(unsigned int, trace_wrap, "trace_wrap", 0);
// The filter description to apply to boot tracing.
static TUNABLE_STR(trace_typefilter, 256, "trace_typefilter", "");

// Turn on wake tracing and set the number of events.
TUNABLE(unsigned int, wake_nkdbufs, "trace_wake", 0);
// Write trace events to a file in the event of a panic.
TUNABLE(unsigned int, write_trace_on_panic, "trace_panic", 0);

// Obsolete leak logging system.
TUNABLE(int, log_leaks, "-l", 0);

__startup_func
void
kdebug_startup(void)
{
	lck_spin_init(&kd_control_trace.kdc_storage_lock, &kdebug_lck_grp, LCK_ATTR_NULL);
	lck_spin_init(&kd_control_triage.kdc_storage_lock, &kdebug_lck_grp, LCK_ATTR_NULL);
	kdebug_init(new_nkdbufs, trace_typefilter,
	    (trace_wrap ? KDOPT_WRAPPING : 0) | KDOPT_ATBOOT);
	create_buffers_triage();
}

uint32_t
kdbg_cpu_count(void)
{
#if defined(__x86_64__)
	return ml_early_cpu_max_number() + 1;
#else // defined(__x86_64__)
	return ml_get_cpu_count();
#endif // !defined(__x86_64__)
}

/*
 * Both kdebug_timestamp and kdebug_using_continuous_time are known
 * to kexts. And going forward we always want to use mach_continuous_time().
 * So we keep these 2 routines as-is to keep the TRACE mode use outside
 * the kernel intact. TRIAGE mode will explicitly only use mach_continuous_time()
 * for its timestamp.
 */
bool
kdebug_using_continuous_time(void)
{
	return kd_control_trace.kdc_flags & KDBG_CONTINUOUS_TIME;
}

uint64_t
kdebug_timestamp(void)
{
	if (kdebug_using_continuous_time()) {
		return mach_continuous_time();
	} else {
		return mach_absolute_time();
	}
}

int
create_buffers(
	struct kd_control *kd_ctrl_page,
	struct kd_buffer *kd_data_page,
	vm_tag_t tag)
{
	unsigned int i;
	unsigned int p_buffer_size;
	unsigned int f_buffer_size;
	unsigned int f_buffers;
	int error = 0;
	int ncpus, count_storage_units = 0;

	struct kd_bufinfo *kdbip = NULL;
	struct kd_region *kd_bufs = NULL;
	int kdb_storage_count = kd_data_page->kdb_storage_count;

	ncpus = kd_ctrl_page->alloc_cpus;

	kdbip = kalloc_type_tag(struct kd_bufinfo, ncpus, Z_WAITOK | Z_ZERO, tag);
	if (kdbip == NULL) {
		error = ENOSPC;
		goto out;
	}
	kd_data_page->kdb_info = kdbip;

	f_buffers = kdb_storage_count / N_STORAGE_UNITS_PER_BUFFER;
	kd_data_page->kdb_region_count = f_buffers;

	f_buffer_size = N_STORAGE_UNITS_PER_BUFFER * sizeof(struct kd_storage);
	p_buffer_size = (kdb_storage_count % N_STORAGE_UNITS_PER_BUFFER) * sizeof(struct kd_storage);

	if (p_buffer_size) {
		kd_data_page->kdb_region_count++;
	}

	if (kd_ctrl_page->kdebug_kdcopybuf_size > 0 && kd_data_page->kdcopybuf == NULL) {
		if (kmem_alloc(kernel_map, (vm_offset_t *)&kd_data_page->kdcopybuf,
		    (vm_size_t) kd_ctrl_page->kdebug_kdcopybuf_size,
		    KMA_DATA | KMA_ZERO, tag) != KERN_SUCCESS) {
			error = ENOSPC;
			goto out;
		}
	}

	kd_bufs = kalloc_type_tag(struct kd_region, kd_data_page->kdb_region_count,
	    Z_WAITOK | Z_ZERO, tag);
	if (kd_bufs == NULL) {
		error = ENOSPC;
		goto out;
	}
	kd_data_page->kd_bufs = kd_bufs;

	for (i = 0; i < f_buffers; i++) {
		if (kmem_alloc(kernel_map, (vm_offset_t *)&kd_bufs[i].kdr_addr,
		    (vm_size_t)f_buffer_size, KMA_DATA | KMA_ZERO, tag) != KERN_SUCCESS) {
			error = ENOSPC;
			goto out;
		}

		kd_bufs[i].kdr_size = f_buffer_size;
	}
	if (p_buffer_size) {
		if (kmem_alloc(kernel_map, (vm_offset_t *)&kd_bufs[i].kdr_addr,
		    (vm_size_t)p_buffer_size, KMA_DATA | KMA_ZERO, tag) != KERN_SUCCESS) {
			error = ENOSPC;
			goto out;
		}

		kd_bufs[i].kdr_size = p_buffer_size;
	}

	count_storage_units = 0;
	for (i = 0; i < kd_data_page->kdb_region_count; i++) {
		struct kd_storage *kds;
		uint16_t n_elements;
		static_assert(N_STORAGE_UNITS_PER_BUFFER <= UINT16_MAX);
		assert(kd_bufs[i].kdr_size <= N_STORAGE_UNITS_PER_BUFFER *
		    sizeof(struct kd_storage));

		n_elements = kd_bufs[i].kdr_size / sizeof(struct kd_storage);
		kds = kd_bufs[i].kdr_addr;

		for (uint16_t n = 0; n < n_elements; n++) {
			kds[n].kds_next.buffer_index = kd_ctrl_page->kds_free_list.buffer_index;
			kds[n].kds_next.offset = kd_ctrl_page->kds_free_list.offset;

			kd_ctrl_page->kds_free_list.buffer_index = i;
			kd_ctrl_page->kds_free_list.offset = n;
		}
		count_storage_units += n_elements;
	}

	kd_data_page->kdb_storage_count = count_storage_units;

	for (i = 0; i < ncpus; i++) {
		kdbip[i].kd_list_head.raw = KDS_PTR_NULL;
		kdbip[i].kd_list_tail.raw = KDS_PTR_NULL;
		kdbip[i].kd_lostevents = false;
		kdbip[i].num_bufs = 0;
	}

	kd_ctrl_page->kdc_flags |= KDBG_BUFINIT;

	kd_ctrl_page->kdc_storage_used = 0;
out:
	if (error) {
		delete_buffers(kd_ctrl_page, kd_data_page);
	}

	return error;
}

void
delete_buffers(struct kd_control *kd_ctrl_page,
    struct kd_buffer *kd_data_page)
{
	unsigned int i;
	int kdb_region_count = kd_data_page->kdb_region_count;

	struct kd_bufinfo *kdbip = kd_data_page->kdb_info;
	struct kd_region *kd_bufs = kd_data_page->kd_bufs;

	if (kd_bufs) {
		for (i = 0; i < kdb_region_count; i++) {
			if (kd_bufs[i].kdr_addr) {
				kmem_free(kernel_map, (vm_offset_t)kd_bufs[i].kdr_addr, (vm_size_t)kd_bufs[i].kdr_size);
			}
		}
		kfree_type(struct kd_region, kdb_region_count, kd_bufs);

		kd_data_page->kd_bufs = NULL;
		kd_data_page->kdb_region_count = 0;
	}
	if (kd_data_page->kdcopybuf) {
		kmem_free(kernel_map, (vm_offset_t)kd_data_page->kdcopybuf, kd_ctrl_page->kdebug_kdcopybuf_size);

		kd_data_page->kdcopybuf = NULL;
	}
	kd_ctrl_page->kds_free_list.raw = KDS_PTR_NULL;

	if (kdbip) {
		kfree_type(struct kd_bufinfo, kd_ctrl_page->alloc_cpus, kdbip);
		kd_data_page->kdb_info = NULL;
	}
	kd_ctrl_page->kdc_coprocs = NULL;
	kd_ctrl_page->kdebug_cpus = 0;
	kd_ctrl_page->alloc_cpus = 0;
	kd_ctrl_page->kdc_flags &= ~KDBG_BUFINIT;
}

static void
_register_out_of_space(struct kd_control *kd_ctrl_page)
{
	kd_ctrl_page->kdc_emit = KDEMIT_DISABLE;
	kdebug_enable = 0;
	kd_ctrl_page->enabled = 0;
	commpage_update_kdebug_state();
}

bool
kdebug_storage_alloc(
	struct kd_control *kd_ctrl_page,
	struct kd_buffer *kd_data_page,
	int cpu)
{
	union kds_ptr kdsp;
	struct kd_storage *kdsp_actual, *kdsp_next_actual;
	struct kd_bufinfo *kdbip, *kdbp, *kdbp_vict, *kdbp_try;
	uint64_t oldest_ts, ts;
	bool retval = true;
	struct kd_region *kd_bufs;

	int intrs_en = kdebug_storage_lock(kd_ctrl_page);

	kdbp = &kd_data_page->kdb_info[cpu];
	kd_bufs = kd_data_page->kd_bufs;
	kdbip = kd_data_page->kdb_info;

	/* If someone beat us to the allocate, return success */
	if (kdbp->kd_list_tail.raw != KDS_PTR_NULL) {
		kdsp_actual = POINTER_FROM_KDS_PTR(kd_bufs, kdbp->kd_list_tail);

		if (kdsp_actual->kds_bufindx < kd_ctrl_page->kdebug_events_per_storage_unit) {
			goto out;
		}
	}

	if ((kdsp = kd_ctrl_page->kds_free_list).raw != KDS_PTR_NULL) {
		/*
		 * If there's a free page, grab it from the free list.
		 */
		kdsp_actual = POINTER_FROM_KDS_PTR(kd_bufs, kdsp);
		kd_ctrl_page->kds_free_list = kdsp_actual->kds_next;

		kd_ctrl_page->kdc_storage_used++;
	} else {
		/*
		 * Otherwise, we're going to lose events and repurpose the oldest
		 * storage unit we can find.
		 */
		if (kd_ctrl_page->kdc_live_flags & KDBG_NOWRAP) {
			_register_out_of_space(kd_ctrl_page);
			kd_ctrl_page->kdc_live_flags |= KDBG_WRAPPED;
			kdbp->kd_lostevents = true;
			retval = false;
			goto out;
		}
		kdbp_vict = NULL;
		oldest_ts = UINT64_MAX;

		for (kdbp_try = &kdbip[0]; kdbp_try < &kdbip[kd_ctrl_page->kdebug_cpus]; kdbp_try++) {
			if (kdbp_try->kd_list_head.raw == KDS_PTR_NULL) {
				/*
				 * no storage unit to steal
				 */
				continue;
			}

			kdsp_actual = POINTER_FROM_KDS_PTR(kd_bufs, kdbp_try->kd_list_head);

			if (kdsp_actual->kds_bufcnt < kd_ctrl_page->kdebug_events_per_storage_unit) {
				/*
				 * make sure we don't steal the storage unit
				 * being actively recorded to...  need to
				 * move on because we don't want an out-of-order
				 * set of events showing up later
				 */
				continue;
			}

			/*
			 * When wrapping, steal the storage unit with the
			 * earliest timestamp on its last event, instead of the
			 * earliest timestamp on the first event.  This allows a
			 * storage unit with more recent events to be preserved,
			 * even if the storage unit contains events that are
			 * older than those found in other CPUs.
			 */
			ts = kdbg_get_timestamp(&kdsp_actual->kds_records[kd_ctrl_page->kdebug_events_per_storage_unit - 1]);
			if (ts < oldest_ts) {
				oldest_ts = ts;
				kdbp_vict = kdbp_try;
			}
		}
		if (kdbp_vict == NULL && kd_ctrl_page->mode == KDEBUG_MODE_TRACE) {
			_register_out_of_space(kd_ctrl_page);
			retval = false;
			goto out;
		}
		kdsp = kdbp_vict->kd_list_head;
		kdsp_actual = POINTER_FROM_KDS_PTR(kd_bufs, kdsp);
		kdbp_vict->kd_list_head = kdsp_actual->kds_next;

		if (kdbp_vict->kd_list_head.raw != KDS_PTR_NULL) {
			kdsp_next_actual = POINTER_FROM_KDS_PTR(kd_bufs, kdbp_vict->kd_list_head);
			kdsp_next_actual->kds_lostevents = true;
		} else {
			kdbp_vict->kd_lostevents = true;
		}

		if (kd_ctrl_page->kdc_oldest_time < oldest_ts) {
			kd_ctrl_page->kdc_oldest_time = oldest_ts;
		}
		kd_ctrl_page->kdc_live_flags |= KDBG_WRAPPED;
	}

	if (kd_ctrl_page->mode == KDEBUG_MODE_TRACE) {
		kdsp_actual->kds_timestamp = kdebug_timestamp();
	} else {
		kdsp_actual->kds_timestamp = mach_continuous_time();
	}

	kdsp_actual->kds_next.raw = KDS_PTR_NULL;
	kdsp_actual->kds_bufcnt   = 0;
	kdsp_actual->kds_readlast = 0;

	kdsp_actual->kds_lostevents = kdbp->kd_lostevents;
	kdbp->kd_lostevents = false;
	kdsp_actual->kds_bufindx = 0;

	if (kdbp->kd_list_head.raw == KDS_PTR_NULL) {
		kdbp->kd_list_head = kdsp;
	} else {
		POINTER_FROM_KDS_PTR(kd_bufs, kdbp->kd_list_tail)->kds_next = kdsp;
	}
	kdbp->kd_list_tail = kdsp;
out:
	kdebug_storage_unlock(kd_ctrl_page, intrs_en);

	return retval;
}
