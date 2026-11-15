// generic.h - the general DRM answers for drm-kmod (Linux 6.14 DRM), by hand for PDDRMnvidia
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#ifndef PD_NVDRM_CONFTEST_GENERIC_H
#define PD_NVDRM_CONFTEST_GENERIC_H
#define NV_DRM_AVAILABLE
#define NV_DRM_ALPHA_BLENDING_AVAILABLE
#define NV_DRM_FBDEV_TTM_AVAILABLE
#define NV_IS_EXPORT_SYMBOL_PRESENT_drm_gem_prime_fd_to_handle 1
#define NV_IS_EXPORT_SYMBOL_PRESENT_drm_gem_prime_handle_to_fd 1
#define NV_IS_EXPORT_SYMBOL_PRESENT_timer_delete_sync 1
// no GPL-only export split here, every linuxkpi symbol is open to the driver
#define NV_IS_EXPORT_SYMBOL_GPL_refcount_inc 0
#define NV_IS_EXPORT_SYMBOL_GPL_refcount_dec_and_test 0
#define NV_IS_EXPORT_SYMBOL_GPL___vma_start_write 0
#endif
