/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSKeyedUnarchiver.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSData.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSNumber.h>
#import <Foundation/NSPropertyListSerialization.h>
#import <Foundation/NSString.h>

static id unarchiveValue(NSKeyedUnarchiver *unarchiver, id record) {
    if(![record isKindOfClass:[NSDictionary class]])
        return record;

    NSString *type = [record objectForKey:@"$type"];
    if(type == nil)
        return record;
    if([type isEqualToString:@"nil"])
        return nil;
    if([type isEqualToString:@"array"]) {
        NSMutableArray *result = [NSMutableArray array];
        for(id value in [record objectForKey:@"$values"])
            [result addObject:unarchiveValue(unarchiver, value)];
        return result;
    }
    if([type isEqualToString:@"dictionary"]) {
        NSMutableDictionary *result = [NSMutableDictionary dictionary];
        NSArray *keys = [record objectForKey:@"$keys"];
        NSArray *values = [record objectForKey:@"$values"];
        for(NSUInteger index = 0; index < [keys count]; ++index) {
            id key = unarchiveValue(unarchiver, [keys objectAtIndex:index]);
            id value = unarchiveValue(unarchiver, [values objectAtIndex:index]);
            if(key != nil && value != nil)
                [result setObject:value forKey:key];
        }
        return result;
    }
    if([type isEqualToString:@"object"]) {
        Class cls = NSClassFromString([record objectForKey:@"$class"]);
        if(cls == Nil)
            return nil;
        [unarchiver->_containers addObject:[record objectForKey:@"$values"]];
        id result = [[cls alloc] initWithCoder:unarchiver];
        [unarchiver->_containers removeLastObject];
        return [result autorelease];
    }
    return nil;
}

@implementation NSKeyedUnarchiver

+ (id)unarchiveObjectWithData:(NSData *)data {
    NSKeyedUnarchiver *unarchiver = [[self alloc] initForReadingFromData:data error:NULL];
    id result = [unarchiveValue(unarchiver, unarchiver->_root) retain];
    [unarchiver release];
    return [result autorelease];
}

+ (id)unarchivedObjectOfClass:(Class)cls fromData:(NSData *)data error:(NSError **)error {
    id result = [self unarchiveObjectWithData:data];
    return [result isKindOfClass:cls] ? result : nil;
}

- (id)initForReadingFromData:(NSData *)data error:(NSError **)error {
    if((self = [super init])) {
        _data = [data retain];
        _containers = [NSMutableArray new];
        _root = [[NSPropertyListSerialization propertyListWithData:data
            options:NSPropertyListMutableContainersAndLeaves format:NULL error:error] retain];
        if(_root == nil) {
            [self release];
            return nil;
        }
    }
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

- (BOOL)containsValueForKey:(NSString *)key {
    return [[_containers lastObject] objectForKey:key] != nil;
}

- (id)decodeObjectForKey:(NSString *)key {
    return unarchiveValue(self, [[_containers lastObject] objectForKey:key]);
}

- (BOOL)decodeBoolForKey:(NSString *)key {
    id value = [[_containers lastObject] objectForKey:key];
    return value != nil ? [value boolValue] : NO;
}

- (int)decodeIntForKey:(NSString *)key {
    return [[[self->_containers lastObject] objectForKey:key] intValue];
}

- (int32_t)decodeInt32ForKey:(NSString *)key {
    return (int32_t)[self decodeIntForKey:key];
}

- (int64_t)decodeInt64ForKey:(NSString *)key {
    return [[[self->_containers lastObject] objectForKey:key] longLongValue];
}

- (float)decodeFloatForKey:(NSString *)key {
    return [[[self->_containers lastObject] objectForKey:key] floatValue];
}

- (double)decodeDoubleForKey:(NSString *)key {
    return [[[self->_containers lastObject] objectForKey:key] doubleValue];
}

- (void)finishDecoding {
}

@end
