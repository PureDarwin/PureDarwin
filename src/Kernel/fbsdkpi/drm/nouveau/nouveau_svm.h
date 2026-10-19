// SPDX-License-Identifier: MIT
// shared virtual memory entry points, PD builds nouveau without SVM so they do nothing
#ifndef __NOUVEAU_SVM_H__
#define __NOUVEAU_SVM_H__

#include <linux/errno.h>
#include <linux/types.h>

struct drm_device;
struct drm_file;
struct nouveau_drm;
struct nouveau_svmm;

static inline void nouveau_svm_init(struct nouveau_drm *drm) { }
static inline void nouveau_svm_fini(struct nouveau_drm *drm) { }
static inline void nouveau_svm_suspend(struct nouveau_drm *drm) { }
static inline void nouveau_svm_resume(struct nouveau_drm *drm) { }

static inline int
nouveau_svmm_init(struct drm_device *dev, void *data, struct drm_file *file)
{
	return -ENOSYS;
}

static inline int
nouveau_svmm_bind(struct drm_device *dev, void *data, struct drm_file *file)
{
	return -ENOSYS;
}

static inline void nouveau_svmm_fini(struct nouveau_svmm **psvmm) { }
static inline int nouveau_svmm_join(struct nouveau_svmm *svmm, u64 inst) { return 0; }
static inline void nouveau_svmm_part(struct nouveau_svmm *svmm, u64 inst) { }

#endif
