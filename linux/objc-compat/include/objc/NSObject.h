// Apple's root class and protocol for Argon on Linux, implemented in NSObject.m over libobjc2
#pragma once
#include <objc/runtime.h>
#include <stddef.h>

#include <objc/NSObjCRuntime.h>
struct _NSZone;
@class NSString, NSMethodSignature, NSInvocation;
@class Protocol;

@protocol NSObject
- (BOOL)isEqual:(id)object;
- (NSUInteger)hash;
- (Class)superclass;
- (Class)class;
- (instancetype)self;
- (id)performSelector:(SEL)aSelector;
- (id)performSelector:(SEL)aSelector withObject:(id)object;
- (id)performSelector:(SEL)aSelector withObject:(id)object1 withObject:(id)object2;
- (BOOL)isProxy;
- (BOOL)isKindOfClass:(Class)aClass;
- (BOOL)isMemberOfClass:(Class)aClass;
- (BOOL)conformsToProtocol:(Protocol *)aProtocol;
- (BOOL)respondsToSelector:(SEL)aSelector;
- (instancetype)retain;
- (oneway void)release;
- (instancetype)autorelease;
- (NSUInteger)retainCount;
- (struct _NSZone *)zone;
- (NSString *)description;
@optional
- (NSString *)debugDescription;
@end

__attribute__((objc_root_class))
@interface NSObject <NSObject> {
    Class isa;
}
+ (void)load;
+ (void)initialize;
- (instancetype)init;
+ (instancetype)new;
+ (instancetype)allocWithZone:(struct _NSZone *)zone;
+ (instancetype)alloc;
- (void)dealloc;
- (id)copy;
- (id)mutableCopy;
+ (id)copyWithZone:(struct _NSZone *)zone;
+ (id)mutableCopyWithZone:(struct _NSZone *)zone;
+ (BOOL)instancesRespondToSelector:(SEL)aSelector;
+ (BOOL)conformsToProtocol:(Protocol *)protocol;
- (IMP)methodForSelector:(SEL)aSelector;
+ (IMP)instanceMethodForSelector:(SEL)aSelector;
- (void)doesNotRecognizeSelector:(SEL)aSelector;
- (id)forwardingTargetForSelector:(SEL)aSelector;
- (void)forwardInvocation:(NSInvocation *)anInvocation;
- (NSMethodSignature *)methodSignatureForSelector:(SEL)aSelector;
+ (NSMethodSignature *)instanceMethodSignatureForSelector:(SEL)aSelector;
+ (BOOL)isSubclassOfClass:(Class)aClass;
+ (BOOL)resolveClassMethod:(SEL)sel;
+ (BOOL)resolveInstanceMethod:(SEL)sel;
+ (NSUInteger)hash;
+ (Class)superclass;
+ (Class)class;
+ (NSString *)description;
+ (NSString *)debugDescription;
@end
