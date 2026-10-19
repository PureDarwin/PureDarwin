// SPDX-License-Identifier: MIT
// sync_file interface, a fence wrapped in a file descriptor
// written from what drm_syncobj.c, drm_atomic_uapi.c and amdgpu_cs.c call
#ifndef _PD_LINUX_SYNC_FILE_H_
#define _PD_LINUX_SYNC_FILE_H_

#include <linux/types.h>
#include <linux/dma-fence.h>
// fs.h renames struct file, users and this struct must agree on the member name
#include <linux/fs.h>

struct file;

struct sync_file {
	struct file *file;
	struct dma_fence *fence;
};

struct sync_file *sync_file_create(struct dma_fence *fence);
struct dma_fence *sync_file_get_fence(int fd);

#endif
