/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSException_h
#define NSException_h

#import <Foundation/NSObject.h>
#import <Foundation/NSObjCRuntime.h>

@class NSString, NSDictionary, NSArray;

typedef NSString *NSExceptionName NS_TYPED_EXTENSIBLE_ENUM;

FOUNDATION_EXPORT NSExceptionName const NSGenericException;
FOUNDATION_EXPORT NSExceptionName const NSRangeException;
FOUNDATION_EXPORT NSExceptionName const NSInvalidArgumentException;
FOUNDATION_EXPORT NSExceptionName const NSInternalInconsistencyException;
FOUNDATION_EXPORT NSExceptionName const NSMallocException;
FOUNDATION_EXPORT NSExceptionName const NSObjectNotAvailableException;
FOUNDATION_EXPORT NSExceptionName const NSDestinationInvalidException;
FOUNDATION_EXPORT NSExceptionName const NSInvalidArchiveOperationException;
FOUNDATION_EXPORT NSExceptionName const NSInvalidUnarchiveOperationException;

@interface NSException : NSObject

+ (instancetype)exceptionWithName:(NSExceptionName)name
                           reason:(NSString *)reason
                         userInfo:(NSDictionary *)userInfo;

+ (void)raise:(NSExceptionName)name format:(NSString *)format, ...
    __attribute__((format(__NSString__, 2, 3)));
+ (void)raise:(NSExceptionName)name format:(NSString *)format arguments:(va_list)args
    __attribute__((format(__NSString__, 2, 0)));

- (instancetype)initWithName:(NSExceptionName)name
                      reason:(NSString *)reason
                    userInfo:(NSDictionary *)userInfo NS_DESIGNATED_INITIALIZER;

- (void)raise;

- (NSExceptionName)name;
- (NSString *)reason;
- (NSDictionary *)userInfo;
- (NSArray *)callStackReturnAddresses;
- (NSArray *)callStackSymbols;

@end

/* Uncaught exceptions land here; the default handler logs and aborts. */
typedef void NSUncaughtExceptionHandler(NSException *exception);

FOUNDATION_EXPORT NSUncaughtExceptionHandler *NSGetUncaughtExceptionHandler(void);
FOUNDATION_EXPORT void NSSetUncaughtExceptionHandler(NSUncaughtExceptionHandler *handler);

FOUNDATION_EXPORT void NSAssertionFailure(const char *function, const char *file,
                                          int line, NSString *format, ...)
    __attribute__((format(__NSString__, 4, 5)));

#define NSAssert(condition, desc, ...) \
    do { \
        if (__builtin_expect(!(condition), 0)) { \
            NSAssertionFailure(__PRETTY_FUNCTION__, __FILE__, __LINE__, \
                               (desc), ##__VA_ARGS__); \
        } \
    } while (0)

#define NSAssert1(c, d, a1)                     NSAssert((c), (d), (a1))
#define NSAssert2(c, d, a1, a2)                 NSAssert((c), (d), (a1), (a2))
#define NSAssert3(c, d, a1, a2, a3)             NSAssert((c), (d), (a1), (a2), (a3))
#define NSAssert4(c, d, a1, a2, a3, a4)         NSAssert((c), (d), (a1), (a2), (a3), (a4))
#define NSAssert5(c, d, a1, a2, a3, a4, a5)     NSAssert((c), (d), (a1), (a2), (a3), (a4), (a5))

#define NSCAssert(condition, desc, ...)         NSAssert((condition), (desc), ##__VA_ARGS__)
#define NSCAssert1(c, d, a1)                    NSAssert((c), (d), (a1))
#define NSCAssert2(c, d, a1, a2)                NSAssert((c), (d), (a1), (a2))
#define NSCAssert3(c, d, a1, a2, a3)            NSAssert((c), (d), (a1), (a2), (a3))
#define NSCAssert4(c, d, a1, a2, a3, a4)        NSAssert((c), (d), (a1), (a2), (a3), (a4))
#define NSCAssert5(c, d, a1, a2, a3, a4, a5)    NSAssert((c), (d), (a1), (a2), (a3), (a4), (a5))

#define NSParameterAssert(condition) \
    NSAssert((condition), @"Invalid parameter not satisfying: %s", #condition)
#define NSCParameterAssert(condition) NSParameterAssert(condition)

/* The pre-@try exception macros; still used by ported Cocotron-era code. */
#define NS_DURING           @try {
#define NS_HANDLER          } @catch (NSException *localException) {
#define NS_ENDHANDLER       }
#define NS_VALUERETURN(v, t) return (v)
#define NS_VOIDRETURN       return

#endif /* NSException_h */
