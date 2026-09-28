/*
 * Copyright (c) 2020 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

// offsets into struct __darwin_mcontext64, __es then __ss then __fs
#define MCONTEXT_OFFSET_X(n)	(24 + 8 * (n))
#define MCONTEXT_OFFSET_F(n)	(288 + 8 * (n))
#define MCONTEXT_OFFSET_FCSR	544

#if !defined(__ASSEMBLER__)
#include <stddef.h>
#include <sys/ucontext.h>

_Static_assert(offsetof(struct __darwin_mcontext64, __ss.__x[0]) == MCONTEXT_OFFSET_X(0),
		"MCONTEXT_OFFSET_X");
_Static_assert(offsetof(struct __darwin_mcontext64, __fs.__f[0]) == MCONTEXT_OFFSET_F(0),
		"MCONTEXT_OFFSET_F");
_Static_assert(offsetof(struct __darwin_mcontext64, __fs.__fcsr) == MCONTEXT_OFFSET_FCSR,
		"MCONTEXT_OFFSET_FCSR");
#endif
