/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSArchiver.h>
#import <Foundation/NSException.h>
#import <Foundation/NSKeyedArchiver.h>
#import <Foundation/NSData.h>
#import <Foundation/NSString.h>
#include <objc/runtime.h>

/* Backed by the keyed archiver rather than an NeXT typed stream.
 * That is enough for a round trip, which is what Distributed Objects and
 * the workspace tools need but the bytes are NOT the sequential format,
 * so genuine .gorm files and archives from GNUstep will not load. */

static void NSArchiverUnimplemented(id self, SEL _cmd) {
    [NSException raise:NSInternalInconsistencyException
                format:@"%@ %s: sequential archiving is not implemented",
                       NSStringFromClass([self class]), sel_getName(_cmd)];
}

@implementation NSArchiver

+ (NSData *)archivedDataWithRootObject:(id)root {
    return [NSKeyedArchiver archivedDataWithRootObject:root];
}

+ (BOOL)archiveRootObject:(id)root toFile:(NSString *)path {
    NSData *data = [NSKeyedArchiver archivedDataWithRootObject:root];

    return (data != nil) ? [data writeToFile:path atomically:YES] : NO;
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
    return [NSKeyedUnarchiver unarchiveObjectWithData:data];
}

+ (id)unarchiveObjectWithFile:(NSString *)path {
    NSData *data = [NSData dataWithContentsOfFile:path];

    return (data != nil) ? [NSKeyedUnarchiver unarchiveObjectWithData:data] : nil;
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
