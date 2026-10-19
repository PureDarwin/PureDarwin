/*
 * Copyright 2008, Axel Dörfler, axeld@pinc-software.de.
 * Distributed under the terms of the MIT License.
 */
#ifndef _KERNEL_UTIL_ATOMIC_H
#define _KERNEL_UTIL_ATOMIC_H


#include <limits.h>

#include <SupportDefs.h>

#include <debug.h>


#ifdef __cplusplus

template<typename PointerType> PointerType*
atomic_pointer_test_and_set(PointerType** _pointer, const PointerType* set,
	const PointerType* test)
{
#if LONG_MAX == INT_MAX
	return (PointerType*)atomic_test_and_set((int32*)_pointer, (int32)set,
		(int32)test);
#else
	return (PointerType*)atomic_test_and_set64((int64*)_pointer, (int64)set,
		(int64)test);
#endif
}


template<typename PointerType> PointerType*
atomic_pointer_get_and_set(PointerType** _pointer, const PointerType* set)
{
#if LONG_MAX == INT_MAX
	return (PointerType*)atomic_get_and_set((int32*)_pointer, (int32)set);
#else
	return (PointerType*)atomic_get_and_set64((int64*)_pointer, (int64)set);
#endif
}


template<typename PointerType> void
atomic_pointer_set(PointerType** _pointer, const PointerType* set)
{
	ASSERT((addr_t(_pointer) & (sizeof(PointerType*) - 1)) == 0);
#if LONG_MAX == INT_MAX
	atomic_set((int32*)_pointer, (int32)set);
#else
	atomic_set64((int64*)_pointer, (int64)set);
#endif
}


template<typename PointerType> PointerType*
atomic_pointer_get(PointerType* const* _pointer)
{
	ASSERT((addr_t(_pointer) & (sizeof(PointerType*) - 1)) == 0);
#if LONG_MAX == INT_MAX
	return (PointerType*)atomic_get((int32*)_pointer);
#else
	return (PointerType*)atomic_get64((int64*)_pointer);
#endif
}

#endif	// __cplusplus

#endif	/* _KERNEL_UTIL_ATOMIC_H */
