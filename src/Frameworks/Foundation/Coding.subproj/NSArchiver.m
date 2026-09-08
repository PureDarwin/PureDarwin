/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSArchiver.h>
#import <Foundation/NSException.h>
#import <Foundation/NSString.h>
#include <objc/runtime.h>

static void NSArchiverUnimplemented(id self, SEL _cmd) {
    [NSException raise:NSInternalInconsistencyException
                format:@"%@ %s: sequential archiving is not implemented",
                       NSStringFromClass([self class]), sel_getName(_cmd)];
}

@implementation NSArchiver

+ (NSData *)archivedDataWithRootObject:(id)root {
    NSArchiverUnimplemented(self, _cmd);
    return nil;
}

+ (BOOL)archiveRootObject:(id)root toFile:(NSString *)path {
    NSArchiverUnimplemented(self, _cmd);
    return NO;
}

- (instancetype)initForWritingWithMutableData:(NSMutableData *)data {
    NSArchiverUnimplemented(self, _cmd);
    [self release];
    return nil;
}

- (NSMutableData *)archiverData {
    NSArchiverUnimplemented(self, _cmd);
    return nil;
}

@end

@implementation NSUnarchiver

+ (id)unarchiveObjectWithData:(NSData *)data {
    NSArchiverUnimplemented(self, _cmd);
    return nil;
}

+ (id)unarchiveObjectWithFile:(NSString *)path {
    NSArchiverUnimplemented(self, _cmd);
    return nil;
}

- (instancetype)initForReadingWithData:(NSData *)data {
    NSArchiverUnimplemented(self, _cmd);
    [self release];
    return nil;
}

- (BOOL)isAtEnd {
    return YES;
}

@end
