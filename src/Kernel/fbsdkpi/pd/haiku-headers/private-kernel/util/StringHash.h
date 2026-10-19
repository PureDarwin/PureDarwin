/*
 * Copyright 2002-2013, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Copyright 2001-2002, Travis Geiselbrecht. All rights reserved.
 * Distributed under the terms of the NewOS License.
 */
#ifndef _KERNEL_UTIL_STRING_HASH_H
#define _KERNEL_UTIL_STRING_HASH_H


#include <SupportDefs.h>


#ifdef __cplusplus
extern "C" {
#endif


uint32 hash_hash_string(const char* string);
uint32 hash_hash_string_part(const char* string, size_t maxLength);


#ifdef __cplusplus
}
#endif


#endif	/* _KERNEL_UTIL_STRING_HASH_H */
