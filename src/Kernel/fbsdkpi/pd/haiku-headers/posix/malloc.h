/*
 * Copyright 2002-2021 Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _MALLOC_H
#define _MALLOC_H


#include <unistd.h>


#ifdef __cplusplus
extern "C" {
#endif


extern void *malloc(size_t numBytes);
extern void *realloc(void *oldPointer, size_t newSize);
extern void *calloc(size_t numElements, size_t size);
extern void free(void *pointer);
extern void *memalign(size_t alignment, size_t numBytes) _ALIGNED_BY_ARG(1);
extern void *valloc(size_t numBytes);

#ifdef _DEFAULT_SOURCE
size_t malloc_usable_size(void *ptr);
#endif


#ifdef __cplusplus
}
#endif

#endif /* _MALLOC_H */
