// SPDX-License-Identifier: MIT
// haiku's kernel heap extensions
#ifndef _PD_HAIKU_KERNEL_HEAP_H
#define _PD_HAIKU_KERNEL_HEAP_H
#include <OS.h>
#define HEAP_DONT_WAIT_FOR_MEMORY	0x01
#define HEAP_DONT_LOCK_KERNEL_SPACE	0x02
#define HEAP_PRIORITY_VIP			0x04
#ifdef __cplusplus
extern "C" {
#endif
void *memalign_etc(size_t alignment, size_t size, uint32 flags);
void *malloc_etc(size_t size, uint32 flags);
void free_etc(void *address, uint32 flags);
#ifdef __cplusplus
}
#endif
#endif
