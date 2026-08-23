/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSException.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSString.h>
#include <CoreFoundation/CFString.h>
#include <execinfo.h>
#include <stdlib.h>

NSExceptionName const NSGenericException = @"NSGenericException";
NSExceptionName const NSRangeException = @"NSRangeException";
NSExceptionName const NSInvalidArgumentException = @"NSInvalidArgumentException";
NSExceptionName const NSInternalInconsistencyException = @"NSInternalInconsistencyException";
NSExceptionName const NSMallocException = @"NSMallocException";
NSExceptionName const NSObjectNotAvailableException = @"NSObjectNotAvailableException";
NSExceptionName const NSDestinationInvalidException = @"NSDestinationInvalidException";
NSExceptionName const NSInvalidArchiveOperationException = @"NSInvalidArchiveOperationException";
NSExceptionName const NSInvalidUnarchiveOperationException = @"NSInvalidUnarchiveOperationException";

static NSUncaughtExceptionHandler *_uncaughtHandler = NULL;

NSUncaughtExceptionHandler *NSGetUncaughtExceptionHandler(void) {
    return _uncaughtHandler;
}

void NSSetUncaughtExceptionHandler(NSUncaughtExceptionHandler *handler) {
    _uncaughtHandler = handler;
}

@implementation NSException {
    NSExceptionName _name;
    NSString *_reason;
    NSDictionary *_userInfo;
    void *_returnAddresses[128];
    int _returnAddressCount;
}

+ (instancetype)exceptionWithName:(NSExceptionName)name
                           reason:(NSString *)reason
                         userInfo:(NSDictionary *)userInfo {
    return [[self alloc] initWithName:name reason:reason userInfo:userInfo];
}

+ (void)raise:(NSExceptionName)name format:(NSString *)format, ... {
    va_list args;
    va_start(args, format);
    [self raise:name format:format arguments:args];
    va_end(args);
}

+ (void)raise:(NSExceptionName)name format:(NSString *)format arguments:(va_list)args {
    NSString *reason = (NSString *)CFStringCreateWithFormatAndArguments(
        kCFAllocatorDefault, NULL, (CFStringRef)format, args);
    [[self exceptionWithName:name reason:reason userInfo:nil] raise];
}

- (instancetype)initWithName:(NSExceptionName)name
                      reason:(NSString *)reason
                    userInfo:(NSDictionary *)userInfo {
    self = [super init];
    if (self != nil) {
        _name = name;
        _reason = reason;
        _userInfo = userInfo;
        _returnAddressCount = backtrace(_returnAddresses, 128);
    }
    return self;
}

- (void)raise {
    if (_uncaughtHandler != NULL) {
        _uncaughtHandler(self);
    }
    @throw self;
}

- (NSExceptionName)name {
    return _name;
}

- (NSString *)reason {
    return _reason;
}

- (NSDictionary *)userInfo {
    return _userInfo;
}

- (NSArray *)callStackReturnAddresses {
    NSMutableArray *addresses = [NSMutableArray arrayWithCapacity:(NSUInteger)_returnAddressCount];
    for (int i = 0; i < _returnAddressCount; i++) {
        [addresses addObject:[NSNumber numberWithUnsignedLongLong:(unsigned long long)(uintptr_t)_returnAddresses[i]]];
    }
    return addresses;
}

- (NSArray *)callStackSymbols {
    NSMutableArray<NSString *> *symbols = [NSMutableArray arrayWithCapacity:(NSUInteger)_returnAddressCount];
    char **names = backtrace_symbols(_returnAddresses, _returnAddressCount);
    if (names == NULL) {
        return symbols;
    }

    for (int i = 0; i < _returnAddressCount; i++) {
        [symbols addObject:(NSString *)CFStringCreateWithCString(kCFAllocatorDefault,
                                                                 names[i],
                                                                 kCFStringEncodingUTF8)];
    }
    free(names);
    return symbols;
}

- (NSString *)description {
    return (NSString *)CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
                                                CFSTR("%@: %@"),
                                                (CFStringRef)_name,
                                                (CFStringRef)_reason);
}

@end

void NSAssertionFailure(const char *function, const char *file, int line,
                        NSString *format, ...) {
    va_list args;
    va_start(args, format);
    NSString *reason = (NSString *)CFStringCreateWithFormatAndArguments(
        kCFAllocatorDefault, NULL, (CFStringRef)format, args);
    va_end(args);

    [[NSException exceptionWithName:NSInternalInconsistencyException
                             reason:(NSString *)CFStringCreateWithFormat(
                                        kCFAllocatorDefault, NULL,
                                        CFSTR("%s (%s:%d): %@"),
                                        function, file, line, (CFStringRef)reason)
                           userInfo:nil] raise];
}
