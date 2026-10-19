/*
 * Copyright 2003-2019 Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _ENDIAN_H_
#define _ENDIAN_H_


#include <config/HaikuConfig.h>


/* Defines architecture independent endian constants.
 * The constant reflects the byte order, "4" is the most significant byte,
 * "1" the least significant one.
 */
#define LITTLE_ENDIAN	1234
#define BIG_ENDIAN		4321

/* Define the machine BYTE_ORDER depending on platform endianness */
#if defined(__HAIKU_LITTLE_ENDIAN)
#	define BYTE_ORDER		LITTLE_ENDIAN
#elif defined(__HAIKU_BIG_ENDIAN)
#	define BYTE_ORDER		BIG_ENDIAN
#endif

#ifdef PD_LINUXKPI
/* Linux code tests "#ifdef __BIG_ENDIAN", so linuxkpi builds only define the
   macro for the order the machine has (amdgpu byte-swapped every CP IB) */
#if BYTE_ORDER == BIG_ENDIAN
#define __BIG_ENDIAN		BIG_ENDIAN
#else
#define __LITTLE_ENDIAN		LITTLE_ENDIAN
#endif
#else
#define __BIG_ENDIAN		BIG_ENDIAN
#define __LITTLE_ENDIAN		LITTLE_ENDIAN
#endif
#define __BYTE_ORDER		BYTE_ORDER

#endif	/* _ENDIAN_H_ */
