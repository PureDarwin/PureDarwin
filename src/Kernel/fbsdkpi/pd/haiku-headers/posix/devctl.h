/*
 * Copyright 2024 Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _DEVCTL_H_
#define _DEVCTL_H_

#include <sys/types.h>


#ifdef __cplusplus
extern "C" {
#endif

int posix_devctl(int fd, int cmd, void* __restrict argument, size_t size, int* __restrict result);

#ifdef __cplusplus
}
#endif


#endif // _DEVCTL_H_
