/*
 * Copyright 2009-2010, Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _CONFIG_TYPES_H
#define _CONFIG_TYPES_H


#include <config/HaikuConfig.h>


/* fixed-width types -- the __haiku_std_[u]int* types correspond to the POSIX
   [u]int*_t types, the _haiku_[u]int* types to the BeOS [u]int* types. If
   __HAIKU_BEOS_COMPATIBLE_TYPES is not defined both sets are identical. Once
   we drop compatibility for good, we can consolidate the types.
*/
typedef signed char			__haiku_std_int8;
typedef unsigned char		__haiku_std_uint8;
typedef signed short		__haiku_std_int16;
typedef unsigned short		__haiku_std_uint16;
typedef signed int			__haiku_std_int32;
typedef unsigned int		__haiku_std_uint32;
#ifdef __HAIKU_ARCH_64_BIT
typedef signed long			__haiku_std_int64;
typedef unsigned long		__haiku_std_uint64;
#else
typedef signed long long	__haiku_std_int64;
typedef unsigned long long	__haiku_std_uint64;
#endif

typedef __haiku_std_int8	__haiku_int8;
typedef __haiku_std_uint8	__haiku_uint8;
typedef __haiku_std_int16	__haiku_int16;
typedef __haiku_std_uint16	__haiku_uint16;
#ifdef __HAIKU_BEOS_COMPATIBLE_TYPES
typedef signed long int		__haiku_int32;
typedef unsigned long int	__haiku_uint32;
#else
typedef __haiku_std_int32	__haiku_int32;
typedef __haiku_std_uint32	__haiku_uint32;
#endif
typedef __haiku_std_int64	__haiku_int64;
typedef __haiku_std_uint64	__haiku_uint64;

/* address types */
typedef signed long int		__haiku_saddr_t;
typedef	unsigned long int	__haiku_addr_t;

#ifdef __HAIKU_ARCH_PHYSICAL_64_BIT
	typedef __haiku_int64	__haiku_phys_saddr_t;
	typedef __haiku_uint64	__haiku_phys_addr_t;
#else
	typedef	__haiku_int32	__haiku_phys_saddr_t;
	typedef	__haiku_uint32	__haiku_phys_addr_t;
#endif

/* address type limits */
#ifdef __HAIKU_ARCH_64_BIT
#	define __HAIKU_SADDR_MAX		(0x7FFFFFFFFFFFFFFFLL)
#	define __HAIKU_ADDR_MAX			(0xFFFFFFFFFFFFFFFFULL)
#else
#	define __HAIKU_SADDR_MAX		(0x7FFFFFFF)
#	define __HAIKU_ADDR_MAX			(0xFFFFFFFFU)
#endif
#define __HAIKU_SADDR_MIN			(-__HAIKU_SADDR_MAX-1)

#ifdef __HAIKU_ARCH_PHYSICAL_64_BIT
#	define __HAIKU_PHYS_SADDR_MAX	(0x7FFFFFFFFFFFFFFFLL)
#	define __HAIKU_PHYS_ADDR_MAX	(0xFFFFFFFFFFFFFFFFULL)
#else
#	define __HAIKU_PHYS_SADDR_MAX	(0x7FFFFFFF)
#	define __HAIKU_PHYS_ADDR_MAX	(0xFFFFFFFFU)
#endif
#define __HAIKU_PHYS_SADDR_MIN		(-__HAIKU_SADDR_MAX-1)


/* printf()/scanf() format prefixes */
#define	__HAIKU_STD_PRI_PREFIX_32	""
#ifdef __HAIKU_ARCH_64_BIT
#	define	__HAIKU_STD_PRI_PREFIX_64	"l"
#else
#	define	__HAIKU_STD_PRI_PREFIX_64	"ll"
#endif

#ifdef __HAIKU_BEOS_COMPATIBLE_TYPES
#	define	__HAIKU_PRI_PREFIX_32		"l"
#else
#	define	__HAIKU_PRI_PREFIX_32		__HAIKU_STD_PRI_PREFIX_32
#endif
#define	__HAIKU_PRI_PREFIX_64			__HAIKU_STD_PRI_PREFIX_64

#define __HAIKU_PRI_PREFIX_ADDR			"l"

#ifdef __HAIKU_ARCH_PHYSICAL_64_BIT
#	define __HAIKU_PRI_PREFIX_PHYS_ADDR	__HAIKU_PRI_PREFIX_64
#else
#	define __HAIKU_PRI_PREFIX_PHYS_ADDR	__HAIKU_PRI_PREFIX_32
#endif


/* a generic address type wide enough for virtual and physical addresses */
#if __HAIKU_ARCH_BITS >= __HAIKU_ARCH_PHYSICAL_BITS
	typedef __haiku_addr_t					__haiku_generic_addr_t;
#	define __HAIKU_GENERIC_ADDR_MAX			__HAIKU_ADDR_MAX
#	define __HAIKU_PRI_PREFIX_GENERIC_ADDR	__HAIKU_PRI_PREFIX_ADDR
#else
	typedef __haiku_phys_addr_t				__haiku_generic_addr_t;
#	define __HAIKU_GENERIC_ADDR_MAX			__HAIKU_PHYS_ADDR_MAX
#	define __HAIKU_PRI_PREFIX_GENERIC_ADDR	__HAIKU_PRI_PREFIX_PHYS_ADDR
#endif


#endif	/* _CONFIG_TYPES_H */
