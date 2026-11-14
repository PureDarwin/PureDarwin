/*
 * SPDX-FileCopyrightText: Copyright (c) 2015-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-FileCopyrightText: Copyright (c) 2026 The PureDarwin Project
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

// pdnvkms.c - the modeset core's OS layer, changed by the PureDarwin Project from NVIDIA's
// kernel/nvidia-modeset/nvidia-modeset-linux.c and kernel/nvidia/nv-modeset-interface.c (580.196.07) for xnu:
// the core and the resource manager are called through pointers into the linked objects, its work runs on one
// thread as nv_kthread_q does, timers are thread calls, /dev/nvidia-modeset is pdnv_dev.c's node 254
#include <IOKit/IOLib.h>
#include <kern/thread.h>
#include <kern/thread_call.h>
#include <libkern/OSAtomic.h>
#include <mach/mach_time.h>
#include <sys/proc.h>
#include <sys/select.h>
#include <string.h>

#include <nvtypes.h>
#include <nvstatus.h>
#include <nv-gpu-info.h>
#include <nvidia-modeset-os-interface.h>
#include <nvkms.h>

#include "pdnv_fmt.h"
#include "pdnv_kms.h"

int pdnv_stack_alloc(void **sp);
void pdnv_stack_free(void *sp);

struct pdnvkms_calls pdnvkms;

// one lock over the core, as nvkms_lock; a sleeping lock, nvKms* may block
static IOLock *sLock;

// module parameters at the Linux defaults
static const NvBool sOutputRoundingFix = NV_TRUE, sHdmiDeepcolor = NV_TRUE, sVblankSemControl = NV_TRUE,
    sOpportunisticDisplaySync = NV_TRUE, sEnableOverlayLayers = NV_TRUE;

NvBool nvkms_test_fail_alloc_core_channel(enum FailAllocCoreChannelMethod method) { (void)method; return NV_FALSE; }
NvBool nvkms_conceal_vrr_caps(void) { return NV_FALSE; }
NvBool nvkms_output_rounding_fix(void) { return sOutputRoundingFix; }
NvBool nvkms_disable_hdmi_frl(void) { return NV_FALSE; }
NvBool nvkms_disable_vrr_memclk_switch(void) { return NV_FALSE; }
NvBool nvkms_hdmi_deepcolor(void) { return sHdmiDeepcolor; }
NvBool nvkms_vblank_sem_control(void) { return sVblankSemControl; }
NvBool nvkms_opportunistic_display_sync(void) { return sOpportunisticDisplaySync; }
enum NvKmsDebugForceColorSpace nvkms_debug_force_color_space(void) { return NVKMS_DEBUG_FORCE_COLOR_SPACE_NONE; }
NvBool nvkms_enable_overlay_layers(void) { return sEnableOverlayLayers; }
NvBool nvkms_debug_logging(void) { return NV_FALSE; }

// no Tegra host1x here
NvBool nvkms_kernel_supports_syncpts(void) { return NV_FALSE; }

NvBool
nvkms_syncpt_op(enum NvKmsSyncPtOp op, NvKmsSyncPtOpParams *params)
{
	(void)op;
	(void)params;
	return NV_FALSE;
}

// memory and strings

// nvkms_free is handed the size back, as IOFree wants it
void *
nvkms_alloc(size_t size, NvBool zero)
{
	if (size == 0)
		return NULL;
	return zero ? IOMallocZero(size) : IOMalloc(size);
}

void
nvkms_free(void *ptr, size_t size)
{
	if (ptr != NULL)
		IOFree(ptr, size);
}

void *nvkms_memset(void *ptr, NvU8 c, size_t size) { return memset(ptr, c, size); }
void *nvkms_memcpy(void *dest, const void *src, size_t n) { return memcpy(dest, src, n); }
void *nvkms_memmove(void *dest, const void *src, size_t n) { return memmove(dest, src, n); }
int nvkms_memcmp(const void *s1, const void *s2, size_t n) { return memcmp(s1, s2, n); }
size_t nvkms_strlen(const char *s) { return strlen(s); }
int nvkms_strcmp(const char *s1, const char *s2) { return strcmp(s1, s2); }

// time

void
nvkms_usleep(NvU64 usec)
{
	if (usec < 1000)
		IODelay((unsigned)usec);
	else
		IOSleep((unsigned)((usec + 999) / 1000));
}

NvU64
nvkms_get_usec(void)
{
	uint64_t ns;

	absolutetime_to_nanoseconds(mach_absolute_time(), &ns);
	return ns / 1000;
}

void
nvkms_yield(void)
{
	IOSleep(1);
}

void
nvkms_dump_stack(void)
{
}

// user memory, 0 or -EFAULT as copy_from_user is used there

int
nvkms_copyin(void *kptr, NvU64 uaddr, size_t n)
{
	return copyin((user_addr_t)uaddr, kptr, n) == 0 ? 0 : -14;
}

int
nvkms_copyout(NvU64 uaddr, const void *kptr, size_t n)
{
	return copyout(kptr, (user_addr_t)uaddr, n) == 0 ? 0 : -14;
}

// printing, to the Linux kernel's format rules

int
nvkms_vsnprintf(char *str, size_t size, const char *format, va_list ap)
{
	return pdnv_vsnprintf(str, size, format, ap);
}

int
nvkms_snprintf(char *str, size_t size, const char *format, ...)
{
	va_list ap;
	int n;

	va_start(ap, format);
	n = pdnv_vsnprintf(str, size, format, ap);
	va_end(ap);
	return n;
}

void
nvkms_log(const int level, const char *gpuPrefix, const char *msg)
{
	const char *prefix = level == NVKMS_LOG_LEVEL_ERROR ? "ERROR: " : level == NVKMS_LOG_LEVEL_WARN ? "WARNING: " : "";

	IOLog("nvidia-modeset: %s%s%s\n", prefix, gpuPrefix != NULL ? gpuPrefix : "", msg);
}

// the work thread, as nv_kthread_q: items run in order on one thread, in process context

struct pdnvkms_work {
	struct pdnvkms_work *next;
	void (*fn)(void *);
	void *arg;
};

static IOLock *sWorkLock;
static struct pdnvkms_work *sWorkHead, *sWorkTail;
static int sWorkBusy;

static void
work_thread(void *arg, wait_result_t wr)
{
	(void)arg;
	(void)wr;
	IOLockLock(sWorkLock);
	for (;;) {
		struct pdnvkms_work *w;

		while (sWorkHead == NULL) {
			sWorkBusy = 0;
			IOLockWakeup(sWorkLock, &sWorkBusy, false);
			IOLockSleep(sWorkLock, &sWorkHead, THREAD_UNINT);
		}
		w = sWorkHead;
		sWorkHead = w->next;
		if (sWorkHead == NULL)
			sWorkTail = NULL;
		sWorkBusy = 1;
		IOLockUnlock(sWorkLock);
		w->fn(w->arg);
		IOFree(w, sizeof(*w));
		IOLockLock(sWorkLock);
	}
}

static int
queue_work(void (*fn)(void *), void *arg)
{
	struct pdnvkms_work *w = IOMalloc(sizeof(*w));

	if (w == NULL)
		return -1;
	w->next = NULL;
	w->fn = fn;
	w->arg = arg;
	IOLockLock(sWorkLock);
	if (sWorkTail != NULL)
		sWorkTail->next = w;
	else
		sWorkHead = w;
	sWorkTail = w;
	IOLockWakeup(sWorkLock, &sWorkHead, true);
	IOLockUnlock(sWorkLock);
	return 0;
}

// the RM, through the bridge nv-modeset-interface.c gives on Linux

typedef void (*rm_op_fn)(void *sp, void *ops);

void
nvkms_call_rm(void *ops)
{
	void *sp = NULL;

	if (pdnvkms.rm_kernel_rmapi_op == NULL || pdnv_stack_alloc(&sp) != 0)
		return;
	((rm_op_fn)pdnvkms.rm_kernel_rmapi_op)(sp, ops);
	pdnv_stack_free(sp);
}

// the GPUs are started at probe and stay up, opening one only checks it is there

NvBool
nvkms_open_gpu(NvU32 gpuId)
{
	return pdnv_dev_gpu_present(gpuId) ? NV_TRUE : NV_FALSE;
}

void
nvkms_close_gpu(NvU32 gpuId)
{
	(void)gpuId;
}

NvU32
nvkms_enumerate_gpus(nv_gpu_info_t *gpu_info)
{
	return pdnv_dev_enumerate_gpus(gpu_info);
}

// x86 with PAT, as NV_ALLOW_WRITE_COMBINING answers for the framebuffer
NvBool
nvkms_allow_write_combining(void)
{
	return NV_TRUE;
}

NvBool
nvkms_fd_is_nvidia_chardev(int fd)
{
	return pdnv_dev_fd_is_nvidia(fd) ? NV_TRUE : NV_FALSE;
}

// ref_ptr: a counted handle whose object may go before the handle does

struct nvkms_ref_ptr {
	volatile SInt32 refcnt;
	void *ptr;
};

struct nvkms_ref_ptr *
nvkms_alloc_ref_ptr(void *ptr)
{
	struct nvkms_ref_ptr *ref_ptr = nvkms_alloc(sizeof(*ref_ptr), NV_FALSE);

	if (ref_ptr != NULL) {
		ref_ptr->refcnt = 1;
		ref_ptr->ptr = ptr;
	}
	return ref_ptr;
}

void *
nvkms_dec_ref(struct nvkms_ref_ptr *ref_ptr)
{
	void *ptr = ref_ptr->ptr;

	if (OSDecrementAtomic(&ref_ptr->refcnt) == 1)
		nvkms_free(ref_ptr, sizeof(*ref_ptr));
	return ptr;
}

void
nvkms_free_ref_ptr(struct nvkms_ref_ptr *ref_ptr)
{
	if (ref_ptr != NULL) {
		ref_ptr->ptr = NULL;
		nvkms_dec_ref(ref_ptr);
	}
}

void
nvkms_inc_ref(struct nvkms_ref_ptr *ref_ptr)
{
	OSIncrementAtomic(&ref_ptr->refcnt);
}

// timers: a thread call for the delay, the callback itself on the work thread under the core's lock

struct nvkms_timer_t {
	thread_call_t call;
	NvBool cancel;
	NvBool complete;
	NvBool isRefPtr;
	nvkms_timer_proc_t *proc;
	void *dataPtr;
	NvU32 dataU32;
};

static void
timer_work(void *arg)
{
	struct nvkms_timer_t *timer = arg;
	void *dataPtr;

	if (timer->call != NULL) {
		thread_call_free(timer->call);
		timer->call = NULL;
	}
	IOLockLock(sLock);
	if (timer->isRefPtr) {
		// the object went: the timer counts as canceled
		dataPtr = nvkms_dec_ref(timer->dataPtr);
		if (dataPtr == NULL)
			timer->cancel = NV_TRUE;
	} else {
		dataPtr = timer->dataPtr;
	}
	if (!timer->cancel) {
		timer->proc(dataPtr, timer->dataU32);
		timer->complete = NV_TRUE;
	}
	if (timer->isRefPtr || timer->cancel)
		IOFree(timer, sizeof(*timer));
	IOLockUnlock(sLock);
}

static void
timer_fire(thread_call_param_t p0, thread_call_param_t p1)
{
	(void)p1;
	queue_work(timer_work, p0);
}

static void
timer_start(struct nvkms_timer_t *timer, NvU64 usec)
{
	uint64_t deadline;

	if (usec == 0) {
		queue_work(timer_work, timer);
		return;
	}
	timer->call = thread_call_allocate(timer_fire, timer);
	clock_interval_to_deadline((uint32_t)(usec > UINT32_MAX ? UINT32_MAX : usec), NSEC_PER_USEC, &deadline);
	thread_call_enter_delayed(timer->call, deadline);
}

nvkms_timer_handle_t *
nvkms_alloc_timer(nvkms_timer_proc_t *proc, void *dataPtr, NvU32 dataU32, NvU64 usec)
{
	struct nvkms_timer_t *timer = IOMallocZero(sizeof(*timer));

	if (timer != NULL) {
		timer->proc = proc;
		timer->dataPtr = dataPtr;
		timer->dataU32 = dataU32;
		timer_start(timer, usec);
	}
	return timer;
}

// from an interrupt bottom half on Linux, a plain allocation is fine on the work loop here
NvBool
nvkms_alloc_timer_with_ref_ptr(nvkms_timer_proc_t *proc, struct nvkms_ref_ptr *ref_ptr, NvU32 dataU32, NvU64 usec)
{
	struct nvkms_timer_t *timer = IOMallocZero(sizeof(*timer));

	if (timer == NULL)
		return NV_FALSE;
	nvkms_inc_ref(ref_ptr);
	timer->isRefPtr = NV_TRUE;
	timer->proc = proc;
	timer->dataPtr = ref_ptr;
	timer->dataU32 = dataU32;
	timer_start(timer, usec);
	return NV_TRUE;
}

// called under the core's lock, as on Linux: a pending timer is canceled and frees itself when it runs
void
nvkms_free_timer(nvkms_timer_handle_t *handle)
{
	struct nvkms_timer_t *timer = handle;

	if (timer == NULL)
		return;
	if (timer->complete) {
		IOFree(timer, sizeof(*timer));
		return;
	}
	timer->cancel = NV_TRUE;
}

// semaphores, count 1: up may come from another thread than down

struct nvkms_sema_t {
	IOLock *lock;
	int count;
};

nvkms_sema_handle_t *
nvkms_sema_alloc(void)
{
	struct nvkms_sema_t *sema = IOMallocZero(sizeof(*sema));

	if (sema != NULL) {
		sema->lock = IOLockAlloc();
		sema->count = 1;
	}
	return sema;
}

void
nvkms_sema_free(nvkms_sema_handle_t *sema)
{
	if (sema != NULL) {
		IOLockFree(sema->lock);
		IOFree(sema, sizeof(*sema));
	}
}

void
nvkms_sema_down(nvkms_sema_handle_t *sema)
{
	IOLockLock(sema->lock);
	while (sema->count == 0)
		IOLockSleep(sema->lock, &sema->count, THREAD_UNINT);
	sema->count--;
	IOLockUnlock(sema->lock);
}

void
nvkms_sema_up(nvkms_sema_handle_t *sema)
{
	IOLockLock(sema->lock);
	sema->count++;
	IOLockWakeup(sema->lock, &sema->count, true);
	IOLockUnlock(sema->lock);
}

// no backlight devices here

struct nvkms_backlight_device *
nvkms_register_backlight(NvU32 gpu_id, NvU32 display_id, void *drv_priv, NvU32 current_brightness)
{
	(void)gpu_id;
	(void)display_id;
	(void)drv_priv;
	(void)current_brightness;
	return NULL;
}

void
nvkms_unregister_backlight(struct nvkms_backlight_device *nvkms_bd)
{
	(void)nvkms_bd;
}

// clients: a per-open record for each /dev/nvidia-modeset descriptor and each kernel client

struct nvkms_per_open {
	void *data;
	enum NvKmsClientType type;
	struct NvKmsKapiDevice *device;
	volatile SInt32 events_available;
	IOLock *sel_lock;               // selrecord and selwakeup under it, the wakeup on a thread call
	thread_call_t wake_call;
	struct selinfo sel;
};

static void
kms_wake(thread_call_param_t p0, thread_call_param_t p1)
{
	struct nvkms_per_open *popen = p0;

	(void)p1;
	IOLockLock(popen->sel_lock);
	selwakeup(&popen->sel);
	IOLockUnlock(popen->sel_lock);
}

typedef void *(*kms_open_fn)(NvU32, enum NvKmsClientType, nvkms_per_open_handle_t *);
typedef void (*kms_close_fn)(void *);
typedef NvBool (*kms_ioctl_fn)(void *, NvU32, NvU64, size_t);
typedef void (*kms_kapi_event_fn)(struct NvKmsKapiDevice *);

static struct nvkms_per_open *
open_common(enum NvKmsClientType type, struct NvKmsKapiDevice *device, NvU32 pid)
{
	struct nvkms_per_open *popen = IOMallocZero(sizeof(*popen));

	if (popen == NULL)
		return NULL;
	popen->type = type;
	popen->device = device;
	popen->sel_lock = IOLockAlloc();
	popen->wake_call = thread_call_allocate(kms_wake, popen);
	IOLockLock(sLock);
	popen->data = ((kms_open_fn)pdnvkms.nvKmsOpen)(pid, type, popen);
	IOLockUnlock(sLock);
	if (popen->data == NULL) {
		thread_call_free(popen->wake_call);
		IOLockFree(popen->sel_lock);
		IOFree(popen, sizeof(*popen));
		return NULL;
	}
	return popen;
}

static void
close_common(struct nvkms_per_open *popen)
{
	IOLockLock(sLock);
	((kms_close_fn)pdnvkms.nvKmsClose)(popen->data);
	popen->data = NULL;
	IOLockUnlock(sLock);
	thread_call_cancel_wait(popen->wake_call);
	thread_call_free(popen->wake_call);
	IOLockLock(popen->sel_lock);
	selthreadclear(&popen->sel);
	IOLockUnlock(popen->sel_lock);
	IOLockFree(popen->sel_lock);
	IOFree(popen, sizeof(*popen));
}

static int
ioctl_common(struct nvkms_per_open *popen, NvU32 cmd, NvU64 address, size_t size)
{
	NvBool ret = NV_FALSE;

	IOLockLock(sLock);
	if (popen->data != NULL)
		ret = ((kms_ioctl_fn)pdnvkms.nvKmsIoctl)(popen->data, cmd, address, size);
	IOLockUnlock(sLock);
	return ret ? 0 : -1;
}

static void
kapi_event_work(void *arg)
{
	struct nvkms_per_open *popen = arg;

	if (pdnvkms.nvKmsKapiHandleEventQueueChange != NULL)
		((kms_kapi_event_fn)pdnvkms.nvKmsKapiHandleEventQueueChange)(popen->device);
}

void
nvkms_event_queue_changed(nvkms_per_open_handle_t *pOpenKernel, NvBool eventsAvailable)
{
	struct nvkms_per_open *popen = pOpenKernel;

	if (popen->type == NVKMS_CLIENT_USER_SPACE) {
		__atomic_store_n(&popen->events_available, eventsAvailable ? 1 : 0, __ATOMIC_RELEASE);
		thread_call_enter(popen->wake_call);
	} else if (eventsAvailable) {
		queue_work(kapi_event_work, popen);
	}
}

struct nvkms_per_open *
nvkms_open_from_kapi(struct NvKmsKapiDevice *device)
{
	return open_common(NVKMS_CLIENT_KERNEL_SPACE, device, (NvU32)proc_selfpid());
}

void
nvkms_close_from_kapi(struct nvkms_per_open *popen)
{
	close_common(popen);
}

NvBool
nvkms_ioctl_from_kapi(struct nvkms_per_open *popen, NvU32 cmd, void *params_address, const size_t params_size)
{
	return ioctl_common(popen, cmd, (NvU64)(uintptr_t)params_address, params_size) == 0;
}

// no suspend here, the power-management lock never refuses
NvBool
nvkms_ioctl_from_kapi_try_pmlock(struct nvkms_per_open *popen, NvU32 cmd, void *params_address,
    const size_t params_size)
{
	return nvkms_ioctl_from_kapi(popen, cmd, params_address, params_size);
}

void *
nvkms_get_per_open_data(int fd)
{
	struct nvkms_per_open *popen = pdnv_dev_kms_per_open(fd);

	return popen != NULL ? popen->data : NULL;
}

// /dev/nvidia-modeset, through pdnv_dev.c

void *
pdnvkms_open(void)
{
	return open_common(NVKMS_CLIENT_USER_SPACE, NULL, (NvU32)proc_selfpid());
}

void
pdnvkms_close(void *p)
{
	if (p != NULL)
		close_common(p);
}

// NVKMS_IOCTL_CMD with its NvKmsIoctlParams, already copied in
int
pdnvkms_ioctl(void *p, NvU32 cmd, NvU64 address, NvU32 size)
{
	struct nvkms_per_open *popen = p;

	if (popen == NULL || popen->data == NULL)
		return -1;
	return ioctl_common(popen, cmd, address, size);
}

// readable while the core has events for this client
int
pdnvkms_select(void *p, void *wql, void *proc)
{
	struct nvkms_per_open *popen = p;

	if (popen == NULL || popen->data == NULL)
		return 0;
	int ready;

	IOLockLock(popen->sel_lock);
	selrecord(proc, &popen->sel, wql);
	ready = __atomic_load_n(&popen->events_available, __ATOMIC_ACQUIRE) != 0;
	IOLockUnlock(popen->sel_lock);
	return ready;
}

// module load, as nvkms_init: the work thread, then the core under its lock
int
pdnvkms_init(void)
{
	thread_t t;
	NvBool ok;

	if (pdnvkms.nvKmsModuleLoad == NULL || pdnvkms.nvKmsOpen == NULL || pdnvkms.nvKmsClose == NULL ||
	    pdnvkms.nvKmsIoctl == NULL || pdnvkms.rm_kernel_rmapi_op == NULL)
		return -1;
	sLock = IOLockAlloc();
	sWorkLock = IOLockAlloc();
	if (kernel_thread_start(work_thread, NULL, &t) != KERN_SUCCESS)
		return -1;
	thread_deallocate(t);

	IOLockLock(sLock);
	ok = ((NvBool (*)(void))pdnvkms.nvKmsModuleLoad)();
	IOLockUnlock(sLock);
	return ok ? 0 : -1;
}
