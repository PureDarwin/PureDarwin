/*
 * Copyright 2003-2021, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _TLS_H
#define _TLS_H


#include <SupportDefs.h>


/* A maximum of 64 keys is allowed to store in TLS - the key is reserved
 * process-wide. Note that tls_allocate() will return B_NO_MEMORY if you
 * try to exceed this limit.
 */
#define TLS_MAX_KEYS 64


#ifdef __cplusplus
extern "C" {
#endif	/* __cplusplus */

extern int32 tls_allocate(void);
extern void *tls_get(int32 index);
extern void **tls_address(int32 index);
extern void tls_set(int32 index, void *value);

#ifdef __cplusplus
}
#endif	/* __cplusplus */

#endif	/* _TLS_H */
