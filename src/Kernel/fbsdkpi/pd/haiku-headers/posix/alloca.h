/*
 * Copyright 2003-2012 Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef	_ALLOCA_H
#define	_ALLOCA_H


#include <sys/types.h>


#undef	__alloca
#undef	alloca

#ifdef __cplusplus
extern "C" {
#endif

extern void * __alloca (size_t __size);
extern void * alloca (size_t __size);

#ifdef __cplusplus
}
#endif

#define	__alloca(size)	__builtin_alloca (size)
#define alloca(size)	__alloca (size)

#endif	/* _ALLOCA_H */
