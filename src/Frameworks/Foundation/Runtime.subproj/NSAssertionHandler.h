// NSAssertionHandler.h - what NSAssert calls in code built against Apple's headers
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0

#import <Foundation/NSObject.h>

@class NSString;

FOUNDATION_EXPORT NSString * const NSAssertionHandlerKey;

// what NSAssert and NSCAssert in code built against Apple's headers call when a condition fails
@interface NSAssertionHandler : NSObject

+ (NSAssertionHandler *)currentHandler;

- (void)handleFailureInMethod:(SEL)selector object:(id)object file:(NSString *)fileName
                   lineNumber:(NSInteger)line description:(NSString *)format, ...;
- (void)handleFailureInFunction:(NSString *)functionName file:(NSString *)fileName
                     lineNumber:(NSInteger)line description:(NSString *)format, ...;

@end
