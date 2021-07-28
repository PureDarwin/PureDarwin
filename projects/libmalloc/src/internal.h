/*
 * Copyright (c) 2015 Apple Inc. All rights reserved.
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

#ifndef __INTERNAL_H
#define __INTERNAL_H

// Toggles for fixes for specific Radars. If we get enough of these, we
// probably should create a separate header file for them.
#define RDAR_48993662 1
#define OS_ATOMIC_CONFIG_MEMORY_ORDER_DEPENDENCY 1

#include <Availability.h>
#include <TargetConditionals.h>
#include <_simple.h>
#include <platform/string.h>
#undef memcpy
#define memcpy _platform_memmove
#include <platform/compat.h>
#include <assert.h>
#include <crt_externs.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <libc.h>
#include <libkern/OSAtomic.h>
#include <limits.h>
#include <mach-o/dyld.h>
#include <mach-o/dyld_priv.h>
#include <mach/mach.h>
#include <mach/mach_init.h>
#include <mach/mach_types.h>
#include <mach/mach_vm.h>
#include <mach/shared_region.h>
#include <mach/thread_switch.h>
#include <mach/vm_map.h>
#include <mach/vm_page_size.h>
#include <mach/vm_param.h>
#include <mach/vm_statistics.h>
#include <machine/cpu_capabilities.h>
#include <os/atomic_private.h>
#include <os/crashlog_private.h>
#include <os/lock_private.h>
#include <os/once_private.h>
#include <os/overflow.h>
#if !TARGET_OS_DRIVERKIT
# include <os/feature_private.h>
#endif
#include <os/tsd.h>
#include <paths.h>
#include <pthread/pthread.h>  // _pthread_threadid_self_np_direct()
#include <pthread/private.h>  // _pthread_threadid_self_np_direct()
#include <pthread/tsd_private.h>  // TSD keys
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/cdefs.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/random.h>
#include <sys/types.h>
#include <sys/vmparam.h>
#include <thread_stack_pcs.h>
#include <unistd.h>
#include <xlocale.h>

#include "dtrace.h"
#include "base.h"
#include "trace.h"
#include "platform.h"
#include "debug.h"
#include "locking.h"
#include "bitarray.h"
#include "malloc/malloc.h"
#include "printf.h"
#include "frozen_malloc.h"
#include "legacy_malloc.h"
#include "magazine_malloc.h"
#include "malloc_common.h"
#include "nano_malloc_common.h"
#include "nano_malloc.h"
#include "nanov2_malloc.h"
#include "pguard_malloc.h"
#include "purgeable_malloc.h"
#include "malloc_private.h"
#include "thresholds.h"
#include "vm.h"
#include "magazine_rack.h"
#include "magazine_zone.h"
#include "nano_zone_common.h"
#include "nano_zone.h"
#include "nanov2_zone.h"
#include "magazine_inline.h"
#include "stack_logging.h"
#include "malloc_implementation.h"

MALLOC_NOEXPORT
extern bool malloc_tracing_enabled;

MALLOC_NOEXPORT
extern unsigned malloc_debug_flags;

MALLOC_NOEXPORT
extern bool malloc_space_efficient_enabled;

MALLOC_NOEXPORT
extern bool malloc_medium_space_efficient_enabled;

MALLOC_NOEXPORT MALLOC_NOINLINE
void
malloc_error_break(void);

MALLOC_NOEXPORT MALLOC_NOINLINE MALLOC_USED
int
malloc_gdb_po_unsafe(void);

MALLOC_NOEXPORT __attribute__((always_inline, const))
static inline bool
malloc_traced(void)
{
	return malloc_tracing_enabled;
}

static inline uint32_t
_malloc_cpu_number(void)
{
#if TARGET_OS_SIMULATOR
	size_t n;
	pthread_cpu_number_np(&n);
	return (uint32_t)n;
#else
	return _os_cpu_number();
#endif
}

/*
  * Copies the malloc library's _malloc_msl_lite_hooks_t structure to a given
  * location. Size is passed to allow the structure to  grow. Since this is
  * a temporary arrangement, we don't need to worry about
  * pointer authentication here or in the _malloc_msl_lite_hooks_t structure
  * itself.
  */
struct _malloc_msl_lite_hooks_s;
typedef void (*set_msl_lite_hooks_callout_t) (struct _malloc_msl_lite_hooks_s *hooksp, size_t size);
void set_msl_lite_hooks(set_msl_lite_hooks_callout_t callout);


// pthread reserves 5 TSD keys for libmalloc
#define __TSD_MALLOC_PGUARD_SAMPLE_COUNTER __PTK_LIBMALLOC_KEY0
#define __TSD_MALLOC_UNUSED1               __PTK_LIBMALLOC_KEY1
#define __TSD_MALLOC_UNUSED2               __PTK_LIBMALLOC_KEY2
#define __TSD_MALLOC_UNUSED3               __PTK_LIBMALLOC_KEY3
#define __TSD_MALLOC_UNUSED4               __PTK_LIBMALLOC_KEY4


#endif // __INTERNAL_H
