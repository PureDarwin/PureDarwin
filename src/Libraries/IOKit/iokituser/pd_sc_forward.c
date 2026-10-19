// pd_sc_forward.c - SCDynamicStore entry points that clients bind through IOKit
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#include <CoreFoundation/CoreFoundation.h>
#include <dlfcn.h>

// IOKit loads before SystemConfiguration, so these look the real ones up on first use
static void *scSymbol(const char *name)
{
    static void *handle;

    if (handle == NULL)
        handle = dlopen("/System/Library/Frameworks/SystemConfiguration.framework/SystemConfiguration", RTLD_LAZY | RTLD_GLOBAL);
    return handle ? dlsym(handle, name) : NULL;
}

typedef const struct __SCDynamicStore *SCDynamicStoreRef;
typedef void (*SCDynamicStoreCallBack)(SCDynamicStoreRef, CFArrayRef, void *);

SCDynamicStoreRef SCDynamicStoreCreate(CFAllocatorRef allocator, CFStringRef name, SCDynamicStoreCallBack callout, void *context)
{
    SCDynamicStoreRef (*fn)(CFAllocatorRef, CFStringRef, SCDynamicStoreCallBack, void *) = scSymbol("SCDynamicStoreCreate");

    return fn ? fn(allocator, name, callout, context) : NULL;
}

CFRunLoopSourceRef SCDynamicStoreCreateRunLoopSource(CFAllocatorRef allocator, SCDynamicStoreRef store, CFIndex order)
{
    CFRunLoopSourceRef (*fn)(CFAllocatorRef, SCDynamicStoreRef, CFIndex) = scSymbol("SCDynamicStoreCreateRunLoopSource");

    return fn ? fn(allocator, store, order) : NULL;
}

Boolean SCDynamicStoreSetNotificationKeys(SCDynamicStoreRef store, CFArrayRef keys, CFArrayRef patterns)
{
    Boolean (*fn)(SCDynamicStoreRef, CFArrayRef, CFArrayRef) = scSymbol("SCDynamicStoreSetNotificationKeys");

    return fn ? fn(store, keys, patterns) : false;
}

int SCError(void)
{
    int (*fn)(void) = scSymbol("SCError");

    return fn ? fn() : 0;
}
