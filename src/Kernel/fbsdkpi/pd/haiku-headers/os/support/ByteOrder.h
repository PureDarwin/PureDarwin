/*
 * Copyright 2007, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _BYTE_ORDER_H
#define _BYTE_ORDER_H


#include <BeBuild.h>
#include <endian.h>
#include <SupportDefs.h>
#include <TypeConstants.h>
	/* for convenience */


/* swap directions */
typedef enum {
	B_SWAP_HOST_TO_LENDIAN,
	B_SWAP_HOST_TO_BENDIAN,
	B_SWAP_LENDIAN_TO_HOST,
	B_SWAP_BENDIAN_TO_HOST,
	B_SWAP_ALWAYS
} swap_action;


/* always swap macros */
#define B_SWAP_DOUBLE(arg)   __swap_double(arg)
#define B_SWAP_FLOAT(arg)    __swap_float(arg)
#define B_SWAP_INT64(arg)    __swap_int64(arg)
#define B_SWAP_INT32(arg)    __swap_int32(arg)
#define B_SWAP_INT16(arg)    __swap_int16(arg)

#if BYTE_ORDER == LITTLE_ENDIAN
/* Host is little endian */

#define B_HOST_IS_LENDIAN 1
#define B_HOST_IS_BENDIAN 0

/* Host native to little endian */
#define B_HOST_TO_LENDIAN_DOUBLE(arg)	(double)(arg)
#define B_HOST_TO_LENDIAN_FLOAT(arg)	(float)(arg)
#define B_HOST_TO_LENDIAN_INT64(arg)	(uint64)(arg)
#define B_HOST_TO_LENDIAN_INT32(arg)	(uint32)(arg)
#define B_HOST_TO_LENDIAN_INT16(arg)	(uint16)(arg)

/* Little endian to host native */
#define B_LENDIAN_TO_HOST_DOUBLE(arg)	(double)(arg)
#define B_LENDIAN_TO_HOST_FLOAT(arg)	(float)(arg)
#define B_LENDIAN_TO_HOST_INT64(arg)	(uint64)(arg)
#define B_LENDIAN_TO_HOST_INT32(arg)	(uint32)(arg)
#define B_LENDIAN_TO_HOST_INT16(arg)	(uint16)(arg)

/* Host native to big endian */
#define B_HOST_TO_BENDIAN_DOUBLE(arg)	__swap_double(arg)
#define B_HOST_TO_BENDIAN_FLOAT(arg)	__swap_float(arg)
#define B_HOST_TO_BENDIAN_INT64(arg)	__swap_int64(arg)
#define B_HOST_TO_BENDIAN_INT32(arg)	__swap_int32(arg)
#define B_HOST_TO_BENDIAN_INT16(arg)	__swap_int16(arg)

/* Big endian to host native */
#define B_BENDIAN_TO_HOST_DOUBLE(arg)	__swap_double(arg)
#define B_BENDIAN_TO_HOST_FLOAT(arg)	__swap_float(arg)
#define B_BENDIAN_TO_HOST_INT64(arg)	__swap_int64(arg)
#define B_BENDIAN_TO_HOST_INT32(arg)	__swap_int32(arg)
#define B_BENDIAN_TO_HOST_INT16(arg)	__swap_int16(arg)

#else	/* BYTE_ORDER */
/* Host is big endian */

#define B_HOST_IS_LENDIAN 0
#define B_HOST_IS_BENDIAN 1

/* Host native to little endian */
#define B_HOST_TO_LENDIAN_DOUBLE(arg)	__swap_double(arg)
#define B_HOST_TO_LENDIAN_FLOAT(arg)	__swap_float(arg)
#define B_HOST_TO_LENDIAN_INT64(arg)	__swap_int64(arg)
#define B_HOST_TO_LENDIAN_INT32(arg)	__swap_int32(arg)
#define B_HOST_TO_LENDIAN_INT16(arg)	__swap_int16(arg)

/* Little endian to host native */
#define B_LENDIAN_TO_HOST_DOUBLE(arg)	__swap_double(arg)
#define B_LENDIAN_TO_HOST_FLOAT(arg)	__swap_float(arg)
#define B_LENDIAN_TO_HOST_INT64(arg)	__swap_int64(arg)
#define B_LENDIAN_TO_HOST_INT32(arg)	__swap_int32(arg)
#define B_LENDIAN_TO_HOST_INT16(arg)	__swap_int16(arg)

/* Host native to big endian */
#define B_HOST_TO_BENDIAN_DOUBLE(arg)	(double)(arg)
#define B_HOST_TO_BENDIAN_FLOAT(arg)	(float)(arg)
#define B_HOST_TO_BENDIAN_INT64(arg)	(uint64)(arg)
#define B_HOST_TO_BENDIAN_INT32(arg)	(uint32)(arg)
#define B_HOST_TO_BENDIAN_INT16(arg)	(uint16)(arg)

/* Big endian to host native */
#define B_BENDIAN_TO_HOST_DOUBLE(arg)	(double)(arg)
#define B_BENDIAN_TO_HOST_FLOAT(arg)	(float)(arg)
#define B_BENDIAN_TO_HOST_INT64(arg)	(uint64)(arg)
#define B_BENDIAN_TO_HOST_INT32(arg)	(uint32)(arg)
#define B_BENDIAN_TO_HOST_INT16(arg)	(uint16)(arg)

#endif	/* BYTE_ORDER */


#ifdef __cplusplus
extern "C" {
#endif	/* __cplusplus */

extern status_t swap_data(type_code type, void *data, size_t length,
	swap_action action);
extern bool is_type_swapped(type_code type);

/* Private implementations */
extern double __swap_double(double arg);
extern float  __swap_float(float arg);
#if __GNUC__ >= 4
#define __swap_int64(arg)	(uint64)__builtin_bswap64(arg)
#define __swap_int32(arg)	(uint32)__builtin_bswap32(arg)
#define __swap_int16(arg)	(uint16)__builtin_bswap16(arg)
#else
extern uint64 __swap_int64(uint64 arg);
extern uint32 __swap_int32(uint32 arg);
extern uint16 __swap_int16(uint16 arg);
#endif

#ifdef __cplusplus
}
#endif	/*  __cplusplus */

#endif	/* _BYTE_ORDER_H */
