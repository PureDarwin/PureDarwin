/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSKeyedArchiver.h>
#import <Foundation/NSException.h>
#import <Foundation/NSString.h>
#include <objc/runtime.h>

static void NSKeyedArchiverUnimplemented(id self, SEL _cmd) {
    [NSException raise:NSInternalInconsistencyException
                format:@"%@ %s: keyed archiving is not implemented",
                       NSStringFromClass([self class]), sel_getName(_cmd)];
}

@implementation NSKeyedArchiver

+ (NSData *)archivedDataWithRootObject:(id)object {
    NSKeyedArchiverUnimplemented(self, _cmd);
    return nil;
}

+ (NSData *)archivedDataWithRootObject:(id)object
                 requiringSecureCoding:(BOOL)secure
                                 error:(NSError **)error {
    NSKeyedArchiverUnimplemented(self, _cmd);
    return nil;
}

- (instancetype)initRequiringSecureCoding:(BOOL)secure {
    NSKeyedArchiverUnimplemented(self, _cmd);
    [self release];
    return nil;
}

- (instancetype)initForWritingWithMutableData:(NSMutableData *)data {
    NSKeyedArchiverUnimplemented(self, _cmd);
    [self release];
    return nil;
}

- (BOOL)allowsKeyedCoding {
    return YES;
}

- (void)finishEncoding {
}

- (NSData *)encodedData {
    NSKeyedArchiverUnimplemented(self, _cmd);
    return nil;
}

@end
