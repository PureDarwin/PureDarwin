/*
 * Copyright 2003-2022, Axel Dörfler, axeld@pinc-software.de.
 * Copyright 2004-2022, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _KERNEL_UTIL_KERNEL_CPP_H
#define _KERNEL_UTIL_KERNEL_CPP_H

#ifdef __cplusplus

#include <new>
#include <stdlib.h>

#if _KERNEL_MODE || _LOADER_MODE

using std::nothrow;

// We need new() versions we can use when also linking against libgcc.
// std::nothrow can't be used since it's defined in both libgcc and
// kernel_cpp.cpp.
typedef struct {} mynothrow_t;
extern const mynothrow_t mynothrow;

#if __cplusplus >= 201103L
#define _THROW(x)
#define _NOEXCEPT noexcept
#else
#define _THROW(x) throw (x)
#define _NOEXCEPT throw ()
#endif
extern void* operator new(size_t size) _THROW(std::bad_alloc);
extern void* operator new[](size_t size) _THROW(std::bad_alloc);
extern void* operator new(size_t size, const std::nothrow_t &) _NOEXCEPT;
extern void* operator new[](size_t size, const std::nothrow_t &) _NOEXCEPT;
extern void* operator new(size_t size, const mynothrow_t &) _NOEXCEPT;
extern void* operator new[](size_t size, const mynothrow_t &) _NOEXCEPT;
extern void operator delete(void *ptr) _NOEXCEPT;
extern void operator delete[](void *ptr) _NOEXCEPT;
extern void operator delete(void *ptr, const std::nothrow_t &) _NOEXCEPT;

#if __cplusplus >= 201402L
extern void operator delete(void* ptr, std::size_t) _NOEXCEPT;
extern void operator delete[](void* ptr, std::size_t) _NOEXCEPT;
#endif // __cplusplus >= 201402L

#endif	// #if _KERNEL_MODE

#endif	// __cplusplus

#endif	/* _KERNEL_UTIL_KERNEL_CPP_H */
