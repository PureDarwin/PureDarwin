/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSKeyedUnarchiver.h>
#import <Foundation/NSException.h>
#import <Foundation/NSString.h>
#include <objc/runtime.h>

static void NSKeyedUnarchiverUnimplemented(id self, SEL _cmd) {
    [NSException raise:NSInternalInconsistencyException
                format:@"%@ %s: keyed unarchiving is not implemented",
                       NSStringFromClass([self class]), sel_getName(_cmd)];
}

@implementation NSKeyedUnarchiver

+ (id)unarchiveObjectWithData:(NSData *)data {
    NSKeyedUnarchiverUnimplemented(self, _cmd);
    return nil;
}

+ (id)unarchivedObjectOfClass:(Class)cls fromData:(NSData *)data error:(NSError **)error {
    NSKeyedUnarchiverUnimplemented(self, _cmd);
    return nil;
}

- (id)initForReadingFromData:(NSData *)data error:(NSError **)error {
    NSKeyedUnarchiverUnimplemented(self, _cmd);
    [self release];
    return nil;
}

- (BOOL)allowsKeyedCoding {
    return YES;
}

- (void)finishDecoding {
}

@end
