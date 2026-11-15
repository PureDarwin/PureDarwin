// pd_nvdrm_pre.h - included ahead of each nvidia-drm source: what its FreeBSD build expects linuxkpi to have in scope
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#ifndef PD_NVDRM_PRE_H
#define PD_NVDRM_PRE_H

// the sx lock bits nvidia-drm's own rwsem_is_locked reads
#include <sys/param.h>
#include <sys/lock.h>
#include <sys/sx.h>
// PD's sx keeps only the exclusive owner in sx_lock, with FreeBSD's values the check reads that
#ifndef SX_LOCK_SHARED
#define SX_LOCK_SHARED 0x01
#endif
#ifndef SX_LOCK_FLAGMASK
#define SX_LOCK_FLAGMASK 0x1f
#endif
// fput, for the dma-buf import
#include <linux/file.h>
// nvidia-drm's registration goes through pd_nvidia_fb.c, which gives the device to the display client
#define drm_dev_register pd_nvdrm_dev_register

#endif
