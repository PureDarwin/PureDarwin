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

@interface NSObject (NSKeyedUnarchiverDelegate)
- (Class)unarchiver:(NSKeyedUnarchiver *)unarchiver cannotDecodeObjectOfClassName:(NSString *)name originalClasses:(NSArray *)classNames;
@end

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
        NSString *name = [record objectForKey:@"$class"];
        Class cls = [unarchiver classForClassName:name];
        if(cls == Nil)
            cls = [NSKeyedUnarchiver classForClassName:name];
        if(cls == Nil)
            cls = NSClassFromString(name);
        if(cls == Nil && [unarchiver->_delegate respondsToSelector:@selector(unarchiver:cannotDecodeObjectOfClassName:originalClasses:)])
            cls = [unarchiver->_delegate unarchiver:unarchiver cannotDecodeObjectOfClassName:name originalClasses:[NSArray arrayWithObject:name]];
        if(cls == Nil)
            return nil;
        [unarchiver->_containers addObject:[record objectForKey:@"$values"]];
        id result = [[cls alloc] initWithCoder:unarchiver];
        [unarchiver->_containers removeLastObject];
        return [result autorelease];
    }
    return nil;
}

// substitutions shared by every unarchiver, an instance's own map is consulted first
static NSMutableDictionary *globalClasses;

@implementation NSKeyedUnarchiver

+ (void)setClass:(Class)cls forClassName:(NSString *)name {
    @synchronized(self) {
        if(globalClasses == nil)
            globalClasses = [NSMutableDictionary new];
        if(cls == Nil)
            [globalClasses removeObjectForKey:name];
        else
            [globalClasses setObject:cls forKey:name];
    }
}

+ (Class)classForClassName:(NSString *)name {
    @synchronized(self) {
        return name != nil ? [globalClasses objectForKey:name] : Nil;
    }
}

- (void)setClass:(Class)cls forClassName:(NSString *)name {
    if(_classes == nil)
        _classes = [NSMutableDictionary new];
    if(cls == Nil)
        [_classes removeObjectForKey:name];
    else
        [_classes setObject:cls forKey:name];
}

- (Class)classForClassName:(NSString *)name {
    return name != nil ? [_classes objectForKey:name] : Nil;
}

// not retained, like every Cocoa delegate
- (void)setDelegate:(id)delegate {
    _delegate = delegate;
}

- (id)delegate {
    return _delegate;
}

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

// the pre-secure-coding spelling, same reader
- (id)initForReadingWithData:(NSData *)data {
    return [self initForReadingFromData:data error:NULL];
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
    [_classes release];
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

// the bytes live in the archive's container, which outlives the decode as Cocoa requires
- (const uint8_t *)decodeBytesForKey:(NSString *)key returnedLength:(NSUInteger *)length {
    id value = [[_containers lastObject] objectForKey:key];

    if (![value isKindOfClass:[NSData class]]) {
        if (length != NULL)
            *length = 0;
        return NULL;
    }
    if (length != NULL)
        *length = [value length];
    return [value bytes];
}

- (void)finishDecoding {
}

@end
