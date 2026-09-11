/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSKeyedArchiver.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSData.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSNumber.h>
#import <Foundation/NSPropertyListSerialization.h>
#import <Foundation/NSString.h>
#import <CoreFoundation/CFNumber.h>

static id archiveValue(NSKeyedArchiver *archiver, id object) {
    if(object == nil)
        return [NSDictionary dictionaryWithObject:@"nil" forKey:@"$type"];
    if(object == (id)kCFBooleanTrue || object == (id)kCFBooleanFalse ||
       [object isKindOfClass:[NSString class]] ||
       [object isKindOfClass:[NSNumber class]] ||
       [object isKindOfClass:[NSData class]])
        return object;
    if([object isKindOfClass:[NSArray class]]) {
        NSMutableArray *values = [NSMutableArray array];
        for(id value in object)
            [values addObject:archiveValue(archiver, value)];
        return [NSDictionary dictionaryWithObjects:@[@"array", values]
                                           forKeys:@[@"$type", @"$values"]];
    }
    if([object isKindOfClass:[NSDictionary class]]) {
        NSMutableArray *keys = [NSMutableArray array];
        NSMutableArray *values = [NSMutableArray array];
        for(id key in [object allKeys]) {
            [keys addObject:archiveValue(archiver, key)];
            [values addObject:archiveValue(archiver, [object objectForKey:key])];
        }
        return [NSDictionary dictionaryWithObjects:@[@"dictionary", keys, values]
                                           forKeys:@[@"$type", @"$keys", @"$values"]];
    }

    NSMutableDictionary *values = [NSMutableDictionary dictionary];
    NSDictionary *record = [NSDictionary dictionaryWithObjects:
        @[@"object", NSStringFromClass([object class]), values]
        forKeys:@[@"$type", @"$class", @"$values"]];
    [archiver->_containers addObject:values];
    [object encodeWithCoder:archiver];
    [archiver->_containers removeLastObject];
    return record;
}

@implementation NSKeyedArchiver

+ (NSData *)archivedDataWithRootObject:(id)object {
    NSKeyedArchiver *archiver = [[self alloc] initRequiringSecureCoding:NO];
    [archiver->_root release];
    archiver->_root = [archiveValue(archiver, object) retain];
    NSData *data = [[archiver encodedData] retain];
    [archiver release];
    return [data autorelease];
}

+ (NSData *)archivedDataWithRootObject:(id)object
                 requiringSecureCoding:(BOOL)secure
                                 error:(NSError **)error {
    NSKeyedArchiver *archiver = [[self alloc] initRequiringSecureCoding:secure];
    [archiver->_root release];
    archiver->_root = [archiveValue(archiver, object) retain];
    NSData *data = [[archiver encodedData] retain];
    [archiver release];
    return [data autorelease];
}

- (instancetype)initRequiringSecureCoding:(BOOL)secure {
    if((self = [super init])) {
        _requiresSecureCoding = secure;
        _containers = [NSMutableArray new];
        _root = [NSMutableDictionary new];
        [_containers addObject:_root];
    }
    return self;
}

- (instancetype)initForWritingWithMutableData:(NSMutableData *)data {
    if((self = [self initRequiringSecureCoding:NO]))
        _data = [data retain];
    return self;
}

- (void)dealloc {
    [_data release];
    [_containers release];
    [_root release];
    [super dealloc];
}

- (BOOL)allowsKeyedCoding {
    return YES;
}

- (void)encodeObject:(id)object forKey:(NSString *)key {
    [[_containers lastObject] setObject:archiveValue(self, object) forKey:key];
}

- (void)encodeBool:(BOOL)value forKey:(NSString *)key {
    [[_containers lastObject] setObject:[NSNumber numberWithBool:value] forKey:key];
}

- (void)encodeInt:(int)value forKey:(NSString *)key {
    [[_containers lastObject] setObject:[NSNumber numberWithInt:value] forKey:key];
}

- (void)encodeInt32:(int32_t)value forKey:(NSString *)key {
    [self encodeInt:(int)value forKey:key];
}

- (void)encodeInt64:(int64_t)value forKey:(NSString *)key {
    [[_containers lastObject] setObject:[NSNumber numberWithLongLong:value] forKey:key];
}

- (void)encodeFloat:(float)value forKey:(NSString *)key {
    [[_containers lastObject] setObject:[NSNumber numberWithFloat:value] forKey:key];
}

- (void)encodeDouble:(double)value forKey:(NSString *)key {
    [[_containers lastObject] setObject:[NSNumber numberWithDouble:value] forKey:key];
}

- (void)finishEncoding {
    if(_data != nil)
        [_data setData:[self encodedData]];
}

- (NSData *)encodedData {
    return [NSPropertyListSerialization dataWithPropertyList:_root
        format:NSPropertyListBinaryFormat_v1_0 options:0 error:NULL];
}

@end
