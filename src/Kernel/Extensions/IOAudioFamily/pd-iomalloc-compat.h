/*
 * Copyright (c) 2026 PureDarwin contributors. SPDX-License-Identifier: MIT
 *
 * The typed/data IOKit allocators over the ones this XNU has.
 *
 * Apple modernised IOKit's allocators (IOMallocType/IOFreeType for single
 * objects, IO*Data for untyped buffers) and applied that change across the
 * published IOAudioFamily sources, including the tags that predate it. This
 * tree's XNU is 20.5.0 (macOS 11.4) and only has the older IOMalloc/IOMallocZero
 * /IONew/IODelete/IOFree, so every published IOAudioFamily fails to compile.
 *
 * Defining the newer spellings in terms of the older ones keeps the vendored
 * Apple source byte-for-byte unmodified, which is worth more than avoiding a
 * header: IOAudioFamily can then be re-vendored from upstream without carrying
 * a patch set.
 *
 * The mapping is exact rather than approximate:
 *   - IOMallocType is a *zeroing* single-object allocator, so it maps to
 *     IOMallocZero(sizeof(type)), not IOMalloc.
 *   - IOMallocData is not zeroing; IOMallocZeroData is.
 *   - The typed-array forms are IONew/IODelete, which already have these
 *     semantics here.
 *
 * The one thing this does not reproduce is the newer allocators' type-segregated
 * heaps (kalloc_type). That is a hardening property, not a semantic one: code
 * built against this behaves identically, it just does not get the separate
 * per-type zones. Nothing in IOAudioFamily depends on that.
 */

#ifndef PD_IOMALLOC_COMPAT_H
#define PD_IOMALLOC_COMPAT_H

#include <IOKit/IOLib.h>

#ifndef IOMallocType
#define IOMallocType(type)              ((type *)IOMallocZero(sizeof(type)))
#endif

#ifndef IOFreeType
/* IOFree() must not be handed NULL; the callers here can legitimately free a
 * pointer that was never allocated after an early failure. */
#define IOFreeType(ptr, type)                                           \
    do {                                                                \
        void *__pd_ptr = (void *)(ptr);                                 \
        if (__pd_ptr != NULL) {                                         \
            IOFree(__pd_ptr, sizeof(type));                             \
        }                                                               \
    } while (0)
#endif

#ifndef IOMallocData
#define IOMallocData(size)              IOMalloc(size)
#endif

#ifndef IOMallocZeroData
#define IOMallocZeroData(size)          IOMallocZero(size)
#endif

#ifndef IOFreeData
#define IOFreeData(ptr, size)                                           \
    do {                                                                \
        void *__pd_ptr = (void *)(ptr);                                 \
        if (__pd_ptr != NULL) {                                         \
            IOFree(__pd_ptr, (size));                                   \
        }                                                               \
    } while (0)
#endif

#ifndef IONewData
#define IONewData(type, count)          IONew(type, count)
#endif

#ifndef IODeleteData
#define IODeleteData(ptr, type, count)  IODelete(ptr, type, count)
#endif

#endif /* PD_IOMALLOC_COMPAT_H */
