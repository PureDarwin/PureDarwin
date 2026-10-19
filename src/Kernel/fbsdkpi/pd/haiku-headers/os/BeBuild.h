/*
 * Copyright 2007-2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _BE_BUILD_H
#define _BE_BUILD_H


#include <config/HaikuConfig.h>


#define _UNUSED(argument) argument
#define _PACKED __attribute__((packed))
#define _PRINTFLIKE(_format_, _args_) \
	__attribute__((format(__printf__, _format_, _args_)))

#if __GNUC__ >= 4
# define _ALIGNED_BY_ARG(_no_) __attribute__((alloc_align(_no_)))
# define _EXPORT __attribute__((visibility("default")))
# define _DEPRECATED __attribute__((__deprecated__))
#else
# define _ALIGNED_BY_ARG(_no_)
# define _EXPORT
# define _DEPRECATED
#endif
#define _IMPORT

#ifndef __has_builtin
#define __has_builtin(x) 0
#endif


#define B_DEFINE_SYMBOL_VERSION(function, versionedSymbol)	\
	__asm__(".symver " function "," versionedSymbol)

#ifdef __cplusplus
#	define B_DEFINE_WEAK_ALIAS(name, alias_name)	\
		extern "C" __typeof(name) alias_name __attribute__((weak, alias(#name)))
#else
#	define B_DEFINE_WEAK_ALIAS(name, alias_name)	\
		__typeof(name) alias_name __attribute__((weak, alias(#name)))
#endif


#endif	/* _BE_BUILD_H */
