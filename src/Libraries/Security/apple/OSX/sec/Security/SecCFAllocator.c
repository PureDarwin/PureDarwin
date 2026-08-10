/*
 * Copyright (c) 2016 Apple Inc. All Rights Reserved.
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

#include <Security/SecCFAllocator.h>
#include <CoreFoundation/CoreFoundation.h>
#include <corecrypto/cc.h>
#include <malloc/malloc.h>

static CFAllocatorContext sDefaultCtx;

static CFStringRef SecCFAllocatorCopyDescription(const void *info) {
    return CFSTR("Custom CFAllocator for sensitive data that zeroizes on deallocate");
}

// primary goal of this allocator is to clear memory when it is deallocated
static void SecCFAllocatorDeallocate(void *ptr, void *info) {
    if (!ptr) return;
    size_t sz = malloc_size(ptr);
    if(sz) cc_clear(sz, ptr);

    sDefaultCtx.deallocate(ptr, info);
}

CFAllocatorRef SecCFAllocatorZeroize(void) {
    static dispatch_once_t sOnce = 0;
    static CFAllocatorRef sAllocator = NULL;
    dispatch_once(&sOnce, ^{
        CFAllocatorGetContext(kCFAllocatorMallocZone, &sDefaultCtx);

        CFAllocatorContext ctx = {0,
            sDefaultCtx.info,
            sDefaultCtx.retain,
            sDefaultCtx.release,
            SecCFAllocatorCopyDescription,
            sDefaultCtx.allocate,
            sDefaultCtx.reallocate,
            SecCFAllocatorDeallocate,
            sDefaultCtx.preferredSize};

        sAllocator = CFAllocatorCreate(NULL, &ctx);
    });

    return sAllocator;
}
