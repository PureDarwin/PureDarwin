/*
 * Copyright (c) 2000-2013 Apple Inc. All rights reserved.
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
/*
 * Copyright 1996 1995 by Open Software Foundation, Inc. 1997 1996 1995 1994 1993 1992 1991
 *              All Rights Reserved
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appears in all copies and
 * that both the copyright notice and this permission notice appear in
 * supporting documentation.
 *
 * OSF DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE.
 *
 * IN NO EVENT SHALL OSF BE LIABLE FOR ANY SPECIAL, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT,
 * NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */
/*
 * MkLinux
 */

/*
 * POSIX Threads - IEEE 1003.1c
 */

#ifndef _POSIX_PTHREAD_INTERNALS_H
#define _POSIX_PTHREAD_INTERNALS_H

#ifndef PTHREAD_LAYOUT_SPI
#define PTHREAD_LAYOUT_SPI 1
#endif

#include <TargetConditionals.h>

#if TARGET_IPHONE_SIMULATOR
#error Unsupported target
#endif

#include "types_internal.h"   // has to come first as it hides the SDK types
#include "offsets_internal.h" // included to validate the offsets at build time

#include <_simple.h>
#include <platform/string.h>
#include <platform/compat.h>

#include <sys/ulock.h>
#include <sys/reason.h>

#include <os/alloc_once_private.h>
#include <os/atomic_private.h>
#include <os/crashlog_private.h>
#include <os/lock_private.h>
#include <os/overflow.h>
#include <os/reason_private.h>
#include <os/semaphore_private.h>

#include <pthread/bsdthread_private.h>
#include <pthread/workqueue_syscalls.h>

#include "pthread.h"
#include "pthread_spis.h"
#include "pthread/private.h"
#include "pthread/dependency_private.h"
#include "pthread/spinlock_private.h"
#include "pthread/workqueue_private.h"
#include "pthread/introspection_private.h"
#include "pthread/qos_private.h"
#include "pthread/tsd_private.h"
#include "pthread/stack_np.h"

#include "imports_internal.h"
#include "prototypes_internal.h"
#include "exports_internal.h"
#include "inline_internal.h"

#include "pthread.h"
#include "pthread_spis.h"
#include "inline_internal.h"
#include "kern/kern_internal.h"


#endif /* _POSIX_PTHREAD_INTERNALS_H */
