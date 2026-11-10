// NSObject.m - the root class for Argon on Linux, on libobjc2 with its fast retain and release paths
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import <objc/NSObject.h>
#include <objc/runtime.h>
#include <objc/objc-arc.h>
#include <objc/hooks.h>
#include <stdio.h>
#include <stdlib.h>

id objc_retain_fast_np(id obj);
void objc_release_fast_np(id obj);
size_t object_getRetainCount_np(id obj);

@implementation NSObject

+ (void)load {}
+ (void)initialize {}


+ (instancetype)allocWithZone:(struct _NSZone *)zone
{
    (void)zone;
    return class_createInstance(self, 0);
}

+ (instancetype)alloc
{
    return [self allocWithZone:NULL];
}

+ (instancetype)new
{
    return [[self alloc] init];
}

- (instancetype)init
{
    return self;
}

void argon_clearWeak(id object);

- (void)dealloc
{
    argon_clearWeak(self);
    object_dispose(self);
}

// CoreFoundation registers how to recognise and count its own instances, since libobjc2's count lives in
// the word before an object, which a CF allocation does not have
struct argon_cf_hooks {
    int (*owns)(const void *object);
    const void *(*retain)(const void *object);
    void (*release)(const void *object);
    long (*retain_count)(const void *object);
};
struct argon_cf_hooks argon_cf;

- (instancetype)retain
{
    if (argon_cf.owns && argon_cf.owns(self))
        return (id)argon_cf.retain(self);
    return objc_retain_fast_np(self);
}

- (oneway void)release
{
    if (argon_cf.owns && argon_cf.owns(self)) {
        argon_cf.release(self);
        return;
    }
    objc_release_fast_np(self);
}

- (instancetype)autorelease
{
    return objc_autorelease(self);
}

- (NSUInteger)retainCount
{
    if (argon_cf.owns && argon_cf.owns(self))
        return (NSUInteger)argon_cf.retain_count(self);
    return object_getRetainCount_np(self);
}

// classes live forever
+ (id)retain { return self; }
+ (oneway void)release {}
+ (id)autorelease { return self; }
+ (NSUInteger)retainCount { return (NSUInteger)-1; }

- (struct _NSZone *)zone { return NULL; }
- (instancetype)self { return self; }
- (Class)class { return object_getClass(self); }
+ (Class)class { return self; }
- (Class)superclass { return class_getSuperclass(object_getClass(self)); }
+ (Class)superclass { return class_getSuperclass(self); }
- (BOOL)isProxy { return NO; }
- (NSUInteger)hash { return (NSUInteger)(uintptr_t)self; }
+ (NSUInteger)hash { return (NSUInteger)(uintptr_t)self; }
- (BOOL)isEqual:(id)object { return self == object; }
+ (BOOL)isEqual:(id)object { return self == object; }

- (BOOL)isKindOfClass:(Class)aClass
{
    for (Class c = object_getClass(self); c != Nil; c = class_getSuperclass(c)) {
        if (c == aClass)
            return YES;
    }
    return NO;
}

+ (BOOL)isKindOfClass:(Class)aClass
{
    for (Class c = object_getClass(self); c != Nil; c = class_getSuperclass(c)) {
        if (c == aClass)
            return YES;
    }
    return NO;
}

- (BOOL)isMemberOfClass:(Class)aClass { return object_getClass(self) == aClass; }
+ (BOOL)isMemberOfClass:(Class)aClass { return object_getClass(self) == aClass; }

+ (BOOL)isSubclassOfClass:(Class)aClass
{
    for (Class c = self; c != Nil; c = class_getSuperclass(c)) {
        if (c == aClass)
            return YES;
    }
    return NO;
}

- (BOOL)respondsToSelector:(SEL)aSelector { return class_respondsToSelector(object_getClass(self), aSelector); }
+ (BOOL)respondsToSelector:(SEL)aSelector { return class_respondsToSelector(object_getClass(self), aSelector); }
+ (BOOL)instancesRespondToSelector:(SEL)aSelector { return class_respondsToSelector(self, aSelector); }

+ (BOOL)conformsToProtocol:(Protocol *)protocol
{
    for (Class c = self; c != Nil; c = class_getSuperclass(c)) {
        if (class_conformsToProtocol(c, protocol))
            return YES;
    }
    return NO;
}

- (BOOL)conformsToProtocol:(Protocol *)protocol
{
    return [object_getClass(self) conformsToProtocol:protocol];
}

- (IMP)methodForSelector:(SEL)aSelector { return class_getMethodImplementation(object_getClass(self), aSelector); }
+ (IMP)methodForSelector:(SEL)aSelector { return class_getMethodImplementation(object_getClass(self), aSelector); }
+ (IMP)instanceMethodForSelector:(SEL)aSelector { return class_getMethodImplementation(self, aSelector); }

- (id)performSelector:(SEL)aSelector
{
    return ((id (*)(id, SEL))objc_msgSend)(self, aSelector);
}

- (id)performSelector:(SEL)aSelector withObject:(id)object
{
    return ((id (*)(id, SEL, id))objc_msgSend)(self, aSelector, object);
}

- (id)performSelector:(SEL)aSelector withObject:(id)object1 withObject:(id)object2
{
    return ((id (*)(id, SEL, id, id))objc_msgSend)(self, aSelector, object1, object2);
}

+ (id)performSelector:(SEL)aSelector
{
    return ((id (*)(id, SEL))objc_msgSend)(self, aSelector);
}

- (id)copy { return [(id)self copyWithZone:NULL]; }
- (id)mutableCopy { return [(id)self mutableCopyWithZone:NULL]; }
+ (id)copyWithZone:(struct _NSZone *)zone { (void)zone; return self; }
+ (id)mutableCopyWithZone:(struct _NSZone *)zone { (void)zone; return self; }

+ (BOOL)resolveClassMethod:(SEL)sel { (void)sel; return NO; }
+ (BOOL)resolveInstanceMethod:(SEL)sel { (void)sel; return NO; }
- (id)forwardingTargetForSelector:(SEL)aSelector { (void)aSelector; return nil; }
+ (id)forwardingTargetForSelector:(SEL)aSelector { (void)aSelector; return nil; }

// raises NSInvalidArgumentException as Apple's root class does, once Foundation is loaded
static void argon_unrecognized(const char *kind, const char *cls, SEL aSelector, const void *object)
{
    char reason[512];
    Class exception = objc_getClass("NSException"), string = objc_getClass("NSString");

    snprintf(reason, sizeof(reason), "%s[%s %s]: unrecognized selector sent to %s %p", kind, cls,
             sel_getName(aSelector), kind[0] == '-' ? "instance" : "class", object);
    if (exception != Nil && string != Nil) {
        id name = ((id (*)(id, SEL, const char *))objc_msgSend)((id)string, sel_registerName("stringWithUTF8String:"), "NSInvalidArgumentException");
        id text = ((id (*)(id, SEL, const char *))objc_msgSend)((id)string, sel_registerName("stringWithUTF8String:"), reason);
        id e = ((id (*)(id, SEL, id, id, id))objc_msgSend)((id)exception, sel_registerName("exceptionWithName:reason:userInfo:"), name, text, nil);

        ((void (*)(id, SEL))objc_msgSend)(e, sel_registerName("raise"));
    }
    fprintf(stderr, "%s\n", reason);
    abort();
}

- (void)doesNotRecognizeSelector:(SEL)aSelector
{
    argon_unrecognized("-", class_getName(object_getClass(self)), aSelector, self);
}

+ (void)doesNotRecognizeSelector:(SEL)aSelector
{
    argon_unrecognized("+", class_getName(self), aSelector, self);
}

- (void)forwardInvocation:(NSInvocation *)anInvocation
{
    (void)anInvocation;
    [self doesNotRecognizeSelector:_cmd];
}

- (NSMethodSignature *)methodSignatureForSelector:(SEL)aSelector { (void)aSelector; return nil; }
+ (NSMethodSignature *)instanceMethodSignatureForSelector:(SEL)aSelector { (void)aSelector; return nil; }

- (NSString *)description { return nil; }
+ (NSString *)description { return nil; }
+ (NSString *)debugDescription { return [self description]; }
- (NSString *)debugDescription { return [self description]; }

@end
