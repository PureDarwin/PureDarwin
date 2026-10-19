// CTRunDelegate.h - callbacks that size a run themselves
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#ifndef __CORETEXT_CTRUNDELEGATE__
#define __CORETEXT_CTRUNDELEGATE__

#import <CoreFoundation/CoreFoundation.h>
#import <CoreGraphics/CGGeometry.h>
#import <CoreText/CoreTextExport.h>

typedef const struct __CTRunDelegate *CTRunDelegateRef;

CORETEXT_EXPORT CFTypeID CTRunDelegateGetTypeID(void);

typedef void (*CTRunDelegateDeallocateCallback)(void *refCon);
typedef CGFloat (*CTRunDelegateGetAscentCallback)(void *refCon);
typedef CGFloat (*CTRunDelegateGetDescentCallback)(void *refCon);
typedef CGFloat (*CTRunDelegateGetWidthCallback)(void *refCon);

typedef struct {
    CFIndex version;
    CTRunDelegateDeallocateCallback dealloc;
    CTRunDelegateGetAscentCallback getAscent;
    CTRunDelegateGetDescentCallback getDescent;
    CTRunDelegateGetWidthCallback getWidth;
} CTRunDelegateCallbacks;

enum {
    kCTRunDelegateVersion1 = 1,
    kCTRunDelegateCurrentVersion = kCTRunDelegateVersion1
};

CORETEXT_EXPORT CTRunDelegateRef CTRunDelegateCreate(const CTRunDelegateCallbacks *callbacks, void *refCon);
CORETEXT_EXPORT void *CTRunDelegateGetRefCon(CTRunDelegateRef runDelegate);

#endif
