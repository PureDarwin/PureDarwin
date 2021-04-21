/*
 * Copyright (c) 2018 Apple Inc. All rights reserved.
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

#ifndef _POSIX_PTHREAD_OFFSETS_H
#define _POSIX_PTHREAD_OFFSETS_H

#if defined(__i386__)
#define _PTHREAD_STRUCT_DIRECT_STACKADDR_OFFSET   140
#define _PTHREAD_STRUCT_DIRECT_STACKBOTTOM_OFFSET 144
#elif __LP64__
#define _PTHREAD_STRUCT_DIRECT_STACKADDR_OFFSET   -48
#define _PTHREAD_STRUCT_DIRECT_STACKBOTTOM_OFFSET -40
#else
#define _PTHREAD_STRUCT_DIRECT_STACKADDR_OFFSET   -36
#define _PTHREAD_STRUCT_DIRECT_STACKBOTTOM_OFFSET -32
#endif

#ifndef __ASSEMBLER__
#include "pthread/private.h" // for other _PTHREAD_STRUCT_DIRECT_*_OFFSET

#define check_backward_offset(field, value) \
		_Static_assert(offsetof(struct pthread_s, tsd) + value == \
				offsetof(struct pthread_s, field), #value " is correct")
#define check_forward_offset(field, value) \
		_Static_assert(offsetof(struct pthread_s, field) == value, \
				#value " is correct")

check_forward_offset(tsd, _PTHREAD_STRUCT_DIRECT_TSD_OFFSET);
check_backward_offset(thread_id, _PTHREAD_STRUCT_DIRECT_THREADID_OFFSET);
#if defined(__i386__)
check_forward_offset(stackaddr, _PTHREAD_STRUCT_DIRECT_STACKADDR_OFFSET);
check_forward_offset(stackbottom, _PTHREAD_STRUCT_DIRECT_STACKBOTTOM_OFFSET);
#else
check_backward_offset(stackaddr, _PTHREAD_STRUCT_DIRECT_STACKADDR_OFFSET);
check_backward_offset(stackbottom, _PTHREAD_STRUCT_DIRECT_STACKBOTTOM_OFFSET);
#endif

#endif // __ASSEMBLER__

#endif /* _POSIX_PTHREAD_OFFSETS_H */
