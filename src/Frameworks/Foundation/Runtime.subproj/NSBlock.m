/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

/* God this code is ugly */

#import <Foundation/NSObject.h>
#import <Foundation/NSString.h>
#include <objc/runtime.h>

extern void *_Block_copy(const void *block);
extern void _Block_release(const void *block);

extern void *_NSConcreteStackBlock[32];
extern void *_NSConcreteGlobalBlock[32];
extern void *_NSConcreteMallocBlock[32];

/* Declared here because objc4 exports it without a public header entry. */
extern Class objc_initializeClassPair(Class superclass, const char *name,
                                      Class cls, Class meta);

/* Common behaviour: copying a block is the Blocks runtime's copy, which is what
 * moves it off the stack. */
@interface NSBlock : NSObject
@end

@implementation NSBlock

- (id)copyWithZone:(NSZone *)zone
{
    return (id)_Block_copy((const void *)self);
}

- (id)copy
{
    return (id)_Block_copy((const void *)self);
}

@end

/* A stack block is not heap-allocated: retaining one promotes it to the heap,
 * and releasing one must not free stack memory. */
static id __NSStackBlockRetain(id self, SEL _cmd)
{
    return (id)_Block_copy((const void *)self);
}

static void __NSBlockReleaseNoop(id self, SEL _cmd)
{
}

static NSUInteger __NSStackBlockRetainCount(id self, SEL _cmd)
{
    return 1;
}

/* A global block is a compile-time constant with no lifetime to manage. */
static id __NSGlobalBlockRetain(id self, SEL _cmd)
{
    return self;
}

static id __NSGlobalBlockCopy(id self, SEL _cmd, NSZone *zone)
{
    return self;
}

static NSUInteger __NSGlobalBlockRetainCount(id self, SEL _cmd)
{
    return NSUIntegerMax;
}

/* A heap block is reference counted by the Blocks runtime. */
static id __NSMallocBlockRetain(id self, SEL _cmd)
{
    return (id)_Block_copy((const void *)self);
}

static void __NSMallocBlockRelease(id self, SEL _cmd)
{
    _Block_release((const void *)self);
}

/* Metaclass storage. The class objects themselves go in libSystem's arrays,
 * which is where the ABI requires them; their metaclasses have no fixed
 * address and so live here. */
static void *__NSStackBlockMeta[32];
static void *__NSGlobalBlockMeta[32];
static void *__NSMallocBlockMeta[32];

static Class __NSBuildBlockClass(void *storage, void *metaStorage,
                                 const char *name)
{
    Class cls = objc_initializeClassPair([NSBlock class], name,
                                         (Class)storage, (Class)metaStorage);

    /* A second load would find the name already taken; the first one's classes
     * are still installed, so there is nothing to do. */
    return cls;
}

__attribute__((constructor))
static void __NSBlockClassesInit(void)
{
    Class cls;

    cls = __NSBuildBlockClass(_NSConcreteStackBlock, __NSStackBlockMeta,
                              "__NSStackBlock__");
    if (cls != Nil) {
        class_addMethod(cls, @selector(retain), (IMP)__NSStackBlockRetain, "@@:");
        class_addMethod(cls, @selector(release), (IMP)__NSBlockReleaseNoop, "v@:");
        class_addMethod(cls, @selector(retainCount),
                        (IMP)__NSStackBlockRetainCount, "Q@:");
        objc_registerClassPair(cls);
    }

    cls = __NSBuildBlockClass(_NSConcreteGlobalBlock, __NSGlobalBlockMeta,
                              "__NSGlobalBlock__");
    if (cls != Nil) {
        class_addMethod(cls, @selector(retain), (IMP)__NSGlobalBlockRetain, "@@:");
        class_addMethod(cls, @selector(release), (IMP)__NSBlockReleaseNoop, "v@:");
        class_addMethod(cls, @selector(copyWithZone:),
                        (IMP)__NSGlobalBlockCopy, "@@:^v");
        class_addMethod(cls, @selector(retainCount),
                        (IMP)__NSGlobalBlockRetainCount, "Q@:");
        objc_registerClassPair(cls);
    }

    cls = __NSBuildBlockClass(_NSConcreteMallocBlock, __NSMallocBlockMeta,
                              "__NSMallocBlock__");
    if (cls != Nil) {
        class_addMethod(cls, @selector(retain), (IMP)__NSMallocBlockRetain, "@@:");
        class_addMethod(cls, @selector(release), (IMP)__NSMallocBlockRelease, "v@:");
        objc_registerClassPair(cls);
    }
}
