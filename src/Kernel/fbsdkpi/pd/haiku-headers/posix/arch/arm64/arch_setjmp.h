/*
 * Copyright 2019 Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */

#ifndef _ARCH_SETJMP_H_
#define _ARCH_SETJMP_H_


#define _SETJMP_BUF_SZ (32)
typedef __int128_t __jmp_buf[_SETJMP_BUF_SZ];

#endif	/* _ARCH_SETJMP_H_ */
