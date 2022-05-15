/*
 * data.m
 * libclosure
 *
 * Copyright (c) 2008-2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 *
 */

#if HAVE_OBJC

/*
 * NSBlock support
 *
 * We define these classes, and CF will later when it initializes
 * inject NSBlock as the real parent for __NSStackBlock__, __NSMallocBlock__,
 * __NSAutoBlock__ and __NSGlobalBlock__.
 */

#include <Foundation/NSObject.h>
#include "Block_private.h"

// the class for stack instances created by block expressions
__attribute__((objc_nonlazy_class, visibility("hidden")))
@interface __NSStackBlock__ : NSObject
@end

@implementation __NSStackBlock__

- (id)retain {
    return self;
}

- (oneway void)release {
}

- (NSUInteger)retainCount {
    return 1;
}

- (id)autorelease {
    return self;
}

@end

// the class for Blocks copied under retain/release (non-GC) scenarios
__attribute__((objc_nonlazy_class, visibility("hidden")))
@interface __NSMallocBlock__ : NSObject
@end

@implementation __NSMallocBlock__

- (id)retain {
    _Block_copy((const void *)self);
    return self;
}

- (oneway void)release {
    _Block_release((const void *)self);
}

- (NSUInteger)retainCount {
    // this is wrong, but there's no way to get the right info
    return 1;
}

- (BOOL)_tryRetain {
    return _Block_tryRetain(self);
}

- (BOOL)_isDeallocating {
    return _Block_isDeallocating(self);
}

@end

// the class for Blocks copied under GC scenarios
// Note: even though we no longer support garbage collection, we leave this in for compatibility reasons.
__attribute__((objc_nonlazy_class, visibility("hidden")))
@interface __NSAutoBlock__ : NSObject
@end

@implementation __NSAutoBlock__

- (id)copy {
    return self;
}

- (id)copyWithZone:(NSZone *)zone {
    (void)zone;
    return self;
}

@end

// inherit copy methods
__attribute__((objc_nonlazy_class, visibility("hidden")))
@interface __NSFinalizingBlock__ : __NSAutoBlock__
@end

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-implementations"
// Note: even though we no longer support garbage collection, we leave this in for compatibility reasons.
@implementation __NSFinalizingBlock__
// A __NSFinalizingBlock will always execute the dispose helper
// This helps under GC when there are resources to recover, like C++ dtor stuff
- (void)finalize {
    struct Block_layout *layout = (struct Block_layout *)self;
#if defined(BLOCK_DESCRIPTOR_1) || TARGET_OS_OSX
    struct Block_descriptor_1 *desc1 = layout->descriptor;
    if (layout->flags & BLOCK_HAS_COPY_DISPOSE) {
        struct Block_descriptor_2 *desc2 = (struct Block_descriptor_2 *)(desc1 + 1);
#ifdef _Block_set_function_pointer
        _Block_get_dispose_fn(desc2)(self);
#else
        desc2->dispose(self);
#endif
    }
#else
    // iOS 4.2, Mac OS X 10.6 and earlier, Windows all use the older layout of blocks
    if (layout->flags & BLOCK_HAS_COPY_DISPOSE) layout->descriptor->dispose(self);
#endif
    return;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunreachable-code"
    [super finalize]; // unnecessary, hence return statement above; this statement is to shut up the compiler
#pragma GCC diagnostic pop
}
@end
#pragma GCC diagnostic pop

// the class for blocks in global scope
__attribute__((objc_nonlazy_class, visibility("hidden")))
@interface __NSGlobalBlock__ : NSObject
@end

@implementation __NSGlobalBlock__

- (id)copy {
    return self;
}

- (id)copyWithZone:(NSZone *)zone {
    (void)zone;
    return self;
}

- (oneway void) release {
}

- (id) retain {
    return self;
}

- (NSUInteger)retainCount {
    return 1;
}

- (BOOL)_tryRetain {
    return YES;
}

- (BOOL)_isDeallocating {
    return NO;
}

@end

// The following class is not a Block but rather a wrapper object around a __block variable.
//
__attribute__((objc_nonlazy_class, visibility("hidden")))
@interface __NSBlockVariable__ : NSObject
{
    //struct Block_byref, minus the duplicated isa
    __strong struct Block_byref *forwarding;    // Compaction:  made strong so pointer can be updated.
    int flags;//refcount;
    int size;
    void (*byref_keep)(struct Block_byref *dst, struct Block_byref *src);
    void (*byref_destroy)(struct Block_byref *);
    // The all important field; this forces a layout map that GC can peruse.
    // The compiler, of course, doesn't see this and emits code using its own layout info.
    /* __weak */ id containedObject;
}

@end

@implementation __NSBlockVariable__
@end

#endif
