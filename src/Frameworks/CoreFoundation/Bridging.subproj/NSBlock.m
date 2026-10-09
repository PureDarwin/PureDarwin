// NSBlock.m - the Objective-C classes behind the Blocks runtime's _NSConcrete*Block storage
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0

// libSystem reserves the class storage, CoreFoundation fills it in, as Apple's does
// so any process with CF loaded can message a block (retain, copy, put it in a collection)

#import <objc/NSObject.h>
#include <objc/runtime.h>

extern void *_Block_copy(const void *block);
extern void _Block_release(const void *block);

extern void *_NSConcreteStackBlock[32];
extern void *_NSConcreteGlobalBlock[32];
extern void *_NSConcreteMallocBlock[32];

// objc4 exports it without a public header entry
extern Class objc_initializeClassPair(Class superclass, const char *name, Class cls, Class meta);

// copying a block is the Blocks runtime's copy, which is what moves it off the stack
@interface NSBlock : NSObject
@end

@implementation NSBlock

- (id)copyWithZone:(void *)zone {
    return (id)_Block_copy((const void *)self);
}

- (id)copy {
    return (id)_Block_copy((const void *)self);
}

@end

// a stack block is promoted to the heap by retain, and release must not free stack memory
static id __NSStackBlockRetain(id self, SEL _cmd) {
    return (id)_Block_copy((const void *)self);
}

static void __NSBlockReleaseNoop(id self, SEL _cmd) {
}

static NSUInteger __NSStackBlockRetainCount(id self, SEL _cmd) {
    return 1;
}

// a global block is a compile-time constant with no lifetime to manage
static id __NSGlobalBlockRetain(id self, SEL _cmd) {
    return self;
}

static id __NSGlobalBlockCopy(id self, SEL _cmd, void *zone) {
    return self;
}

static NSUInteger __NSGlobalBlockRetainCount(id self, SEL _cmd) {
    return NSUIntegerMax;
}

// a heap block is reference counted by the Blocks runtime
static id __NSMallocBlockRetain(id self, SEL _cmd) {
    return (id)_Block_copy((const void *)self);
}

static void __NSMallocBlockRelease(id self, SEL _cmd) {
    _Block_release((const void *)self);
}

// the class objects live in libSystem's arrays where the ABI puts them, their metaclasses here
static void *__NSStackBlockMeta[32];
static void *__NSGlobalBlockMeta[32];
static void *__NSMallocBlockMeta[32];

// a second load finds the name taken, and the first load's classes are still installed
static Class __NSBuildBlockClass(void *storage, void *metaStorage, const char *name) {
    return objc_initializeClassPair([NSBlock class], name, (Class)storage, (Class)metaStorage);
}

__attribute__((constructor))
static void __NSBlockClassesInit(void) {
    Class cls;

    cls = __NSBuildBlockClass(_NSConcreteStackBlock, __NSStackBlockMeta, "__NSStackBlock__");
    if (cls != Nil) {
        class_addMethod(cls, @selector(retain), (IMP)__NSStackBlockRetain, "@@:");
        class_addMethod(cls, @selector(release), (IMP)__NSBlockReleaseNoop, "v@:");
        class_addMethod(cls, @selector(retainCount), (IMP)__NSStackBlockRetainCount, "Q@:");
        objc_registerClassPair(cls);
    }

    cls = __NSBuildBlockClass(_NSConcreteGlobalBlock, __NSGlobalBlockMeta, "__NSGlobalBlock__");
    if (cls != Nil) {
        class_addMethod(cls, @selector(retain), (IMP)__NSGlobalBlockRetain, "@@:");
        class_addMethod(cls, @selector(release), (IMP)__NSBlockReleaseNoop, "v@:");
        class_addMethod(cls, @selector(copyWithZone:), (IMP)__NSGlobalBlockCopy, "@@:^v");
        class_addMethod(cls, @selector(retainCount), (IMP)__NSGlobalBlockRetainCount, "Q@:");
        objc_registerClassPair(cls);
    }

    cls = __NSBuildBlockClass(_NSConcreteMallocBlock, __NSMallocBlockMeta, "__NSMallocBlock__");
    if (cls != Nil) {
        class_addMethod(cls, @selector(retain), (IMP)__NSMallocBlockRetain, "@@:");
        class_addMethod(cls, @selector(release), (IMP)__NSMallocBlockRelease, "v@:");
        objc_registerClassPair(cls);
    }
}
