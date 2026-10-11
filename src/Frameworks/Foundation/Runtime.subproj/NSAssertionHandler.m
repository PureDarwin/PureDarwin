// NSAssertionHandler.m - what NSAssert calls in code built against Apple's headers
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0

#import <Foundation/NSAssertionHandler.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSException.h>
#import <Foundation/NSString.h>
#import <Foundation/NSThread.h>
#include <objc/runtime.h>

NSString * const NSAssertionHandlerKey = @"NSAssertionHandler";

@implementation NSAssertionHandler

// one per thread, kept in the thread dictionary where a program may also install its own
+ (NSAssertionHandler *)currentHandler {
    NSMutableDictionary *dictionary = [[NSThread currentThread] threadDictionary];
    NSAssertionHandler *handler = [dictionary objectForKey:NSAssertionHandlerKey];

    if (handler == nil) {
        handler = [[[NSAssertionHandler alloc] init] autorelease];
        [dictionary setObject:handler forKey:NSAssertionHandlerKey];
    }
    return handler;
}

- (void)handleFailureInMethod:(SEL)selector object:(id)object file:(NSString *)fileName
                   lineNumber:(NSInteger)line description:(NSString *)format, ... {
    va_list arguments;

    va_start(arguments, format);
    NSString *reason = format != nil ? [[[NSString alloc] initWithFormat:format arguments:arguments] autorelease] : @"";
    va_end(arguments);

    BOOL isClass = object != nil && class_isMetaClass(object_getClass(object));
    NSLog(@"*** Assertion failure in %c[%s %@], %@:%ld", isClass ? '+' : '-',
          object != nil ? class_getName(isClass ? (Class)object : object_getClass(object)) : "nil",
          NSStringFromSelector(selector), fileName, (long)line);
    [NSException raise:NSInternalInconsistencyException format:@"%@", reason];
}

- (void)handleFailureInFunction:(NSString *)functionName file:(NSString *)fileName
                     lineNumber:(NSInteger)line description:(NSString *)format, ... {
    va_list arguments;

    va_start(arguments, format);
    NSString *reason = format != nil ? [[[NSString alloc] initWithFormat:format arguments:arguments] autorelease] : @"";
    va_end(arguments);

    NSLog(@"*** Assertion failure in %@, %@:%ld", functionName, fileName, (long)line);
    [NSException raise:NSInternalInconsistencyException format:@"%@", reason];
}

@end
