// IOSurfaceGPU.h - IOSurface pixel memory from the GPU's host-visible memory, see IOSurfaceGPU.c
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#ifndef IOSURFACE_GPU_H
#define IOSURFACE_GPU_H

#include <stdint.h>

struct IOSurfaceGPUMemory {
    uint64_t memory;
    void *base;
    uint64_t size;
};

int IOSurfaceGPUAvailable(void);
// allocates and maps exportable memory, the token lets any process import it
int IOSurfaceGPUAllocate(uint64_t size, struct IOSurfaceGPUMemory *m, uint64_t *token);
int IOSurfaceGPUImport(uint64_t token, uint64_t size, struct IOSurfaceGPUMemory *m);
// a dma-buf fd for the token, for GL_EXT_memory_object_fd
int IOSurfaceGPUMakeFD(uint64_t token, uint64_t size);
void IOSurfaceGPUFree(struct IOSurfaceGPUMemory *m);

#endif
