// pdnv_kms.h - the modeset core: its entry points in the linked object, /dev/nvidia-modeset, the GPU helpers
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#ifndef PDNV_KMS_H
#define PDNV_KMS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// filled from the linked objects: the core's own from nv-modeset-kernel.o, the RM's op from nv-kernel.o
struct pdnvkms_calls {
	void *nvKmsModuleLoad;                  // NvBool (void)
	void *nvKmsOpen;                        // void *(NvU32, enum NvKmsClientType, nvkms_per_open_handle_t *)
	void *nvKmsClose;                       // void (void *)
	void *nvKmsIoctl;                       // NvBool (void *, NvU32, NvU64, size_t)
	void *nvKmsKapiHandleEventQueueChange;  // void (struct NvKmsKapiDevice *)
	void *rm_kernel_rmapi_op;               // void (nvidia_stack_t *, void *)
};
extern struct pdnvkms_calls pdnvkms;

// pdnvkms.c
int pdnvkms_init(void);
void *pdnvkms_open(void);
void pdnvkms_close(void *popen);
int pdnvkms_ioctl(void *popen, uint32_t cmd, uint64_t address, uint32_t size);
int pdnvkms_select(void *popen, void *wql, void *proc);

// the core's imports, every nvkms_* the port defines, X(name)
#define PDNVKMS_PORTED(X) \
	X(nvkms_test_fail_alloc_core_channel) X(nvkms_conceal_vrr_caps) X(nvkms_output_rounding_fix) \
	X(nvkms_disable_hdmi_frl) X(nvkms_disable_vrr_memclk_switch) X(nvkms_hdmi_deepcolor) \
	X(nvkms_vblank_sem_control) X(nvkms_opportunistic_display_sync) X(nvkms_debug_force_color_space) \
	X(nvkms_enable_overlay_layers) X(nvkms_debug_logging) X(nvkms_kernel_supports_syncpts) X(nvkms_syncpt_op) \
	X(nvkms_alloc) X(nvkms_free) X(nvkms_memset) X(nvkms_memcpy) X(nvkms_memmove) X(nvkms_memcmp) \
	X(nvkms_strlen) X(nvkms_strcmp) X(nvkms_usleep) X(nvkms_get_usec) X(nvkms_yield) X(nvkms_dump_stack) \
	X(nvkms_copyin) X(nvkms_copyout) X(nvkms_vsnprintf) X(nvkms_snprintf) X(nvkms_log) X(nvkms_call_rm) \
	X(nvkms_open_gpu) X(nvkms_close_gpu) X(nvkms_enumerate_gpus) X(nvkms_allow_write_combining) \
	X(nvkms_fd_is_nvidia_chardev) X(nvkms_alloc_ref_ptr) X(nvkms_free_ref_ptr) X(nvkms_inc_ref) \
	X(nvkms_dec_ref) X(nvkms_alloc_timer) X(nvkms_alloc_timer_with_ref_ptr) X(nvkms_free_timer) \
	X(nvkms_sema_alloc) X(nvkms_sema_free) X(nvkms_sema_down) X(nvkms_sema_up) X(nvkms_register_backlight) \
	X(nvkms_unregister_backlight) X(nvkms_event_queue_changed) X(nvkms_open_from_kapi) \
	X(nvkms_close_from_kapi) X(nvkms_ioctl_from_kapi) X(nvkms_ioctl_from_kapi_try_pmlock) \
	X(nvkms_get_per_open_data)

struct pdelf_target;
int pdnvkms_resolve(void *ctx, const char *name, struct pdelf_target *t);

// pdnv_dev.c, for the core
int pdnv_dev_gpu_present(uint32_t gpu_id);
uint32_t pdnv_dev_enumerate_gpus(void *gpu_info);     // nv_gpu_info_t[NV_MAX_GPUS]
int pdnv_dev_fd_is_nvidia(int fd);
void *pdnv_dev_kms_per_open(int fd);                  // the nvkms_per_open behind a /dev/nvidia-modeset fd
int pdnv_dev_kms_node(void);                          // /dev/nvidia-modeset, after pdnvkms_init

#ifdef __cplusplus
}
#endif

#endif
