/*
 * Copyright 2018-2019 Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _ARCH_SETJMP_H_
#define _ARCH_SETJMP_H_


#define _SETJMP_BUF_SZ (1+(2*12)+1)
typedef unsigned long __jmp_buf[_SETJMP_BUF_SZ];


#endif  /* _ARCH_SETJMP_H_ */
