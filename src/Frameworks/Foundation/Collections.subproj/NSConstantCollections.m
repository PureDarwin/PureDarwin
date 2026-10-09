// NSConstantCollections.m - the objects clang lays out for constant @[...] and @{...} literals
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0

// clang fixes their layout in the app's data segment, and points every @[] at one shared empty array

#import <Foundation/NSArray.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSEnumerator.h>
#import <Foundation/NSException.h>
#import <Foundation/NSString.h>

// {isa, count, objects}
@interface NSConstantArray : NSArray {
    NSUInteger _count;
    const id *_objects;
}
@end

// {isa, options, count, keys, objects}
@interface NSConstantDictionary : NSDictionary {
    NSUInteger _options;
    NSUInteger _count;
    const id *_keys;
    const id *_objects;
}
@end

extern char __NSConstantArrayClass __asm__("_OBJC_CLASS_$_NSConstantArray");

// what clang references for an empty @[], an NSConstantArray of no objects
__attribute__((visibility("default")))
struct { void *isa; NSUInteger count; const id *objects; } __NSArray0__struct = { &__NSConstantArrayClass, 0, NULL };

@implementation NSConstantArray

- (NSUInteger)count {
    return _count;
}

- (id)objectAtIndex:(NSUInteger)index {
    if (index >= _count)
        [NSException raise:NSRangeException format:@"index %lu beyond bounds [0 .. %ld]",
                           (unsigned long)index, (long)_count - 1];
    return _objects[index];
}

- (id)objectAtIndexedSubscript:(NSUInteger)index {
    return [self objectAtIndex:index];
}

// CFArrayGetValues sends this to non-CF arrays, so it cannot be left to NSArray's CF-based version
- (void)getObjects:(id __unsafe_unretained *)objects range:(NSRange)range {
    if (NSMaxRange(range) > _count)
        [NSException raise:NSRangeException format:@"range {%lu, %lu} beyond bounds [0 .. %ld]",
                           (unsigned long)range.location, (unsigned long)range.length, (long)_count - 1];
    for (NSUInteger i = 0; i < range.length; i++)
        objects[i] = _objects[range.location + i];
}

- (NSUInteger)countByEnumeratingWithState:(NSFastEnumerationState *)state
                                  objects:(id __unsafe_unretained *)buffer count:(NSUInteger)length {
    if (state->state != 0)
        return 0;
    state->itemsPtr = (id __unsafe_unretained *)_objects;
    state->mutationsPtr = &state->extra[0];
    state->state = 1;
    return _count;
}

// it lives in the app's data segment for the life of the process
- (id)retain {
    return self;
}

- (oneway void)release {
}

- (id)autorelease {
    return self;
}

- (NSUInteger)retainCount {
    return NSUIntegerMax;
}

- (id)copyWithZone:(NSZone *)zone {
    return self;
}

@end

@implementation NSConstantDictionary

- (NSUInteger)count {
    return _count;
}

- (id)objectForKey:(id)key {
    if (key == nil)
        return nil;
    for (NSUInteger i = 0; i < _count; i++) {
        if (_keys[i] == key || [_keys[i] isEqual:key])
            return _objects[i];
    }
    return nil;
}

// CFDictionaryGetKeysAndValues sends these to non-CF dictionaries
- (void)getObjects:(id __unsafe_unretained *)objects andKeys:(id __unsafe_unretained *)keys count:(NSUInteger)count {
    for (NSUInteger i = 0; i < _count && i < count; i++) {
        if (objects != NULL)
            objects[i] = _objects[i];
        if (keys != NULL)
            keys[i] = _keys[i];
    }
}

- (void)getObjects:(id __unsafe_unretained *)objects andKeys:(id __unsafe_unretained *)keys {
    [self getObjects:objects andKeys:keys count:_count];
}

// the rest of what CFDictionary's functions send a dictionary that is not a CFDictionary
- (NSUInteger)countForKey:(id)key {
    return [self objectForKey:key] != nil ? 1 : 0;
}

- (BOOL)containsKey:(id)key {
    return [self objectForKey:key] != nil;
}

- (BOOL)__getValue:(id *)value forKey:(id)key {
    id object = [self objectForKey:key];

    if (object != nil && value != NULL)
        *value = object;
    return object != nil;
}

- (void)__apply:(void (*)(const void *, const void *, void *))applier context:(void *)context {
    for (NSUInteger i = 0; i < _count; i++)
        applier(_keys[i], _objects[i], context);
}

- (NSEnumerator *)keyEnumerator {
    return [[NSArray arrayWithObjects:_keys count:_count] objectEnumerator];
}

- (NSEnumerator *)objectEnumerator {
    return [[NSArray arrayWithObjects:_objects count:_count] objectEnumerator];
}

- (NSArray *)allKeys {
    return [NSArray arrayWithObjects:_keys count:_count];
}

- (NSArray *)allValues {
    return [NSArray arrayWithObjects:_objects count:_count];
}

- (NSUInteger)countByEnumeratingWithState:(NSFastEnumerationState *)state
                                  objects:(id __unsafe_unretained *)buffer count:(NSUInteger)length {
    if (state->state != 0)
        return 0;
    state->itemsPtr = (id __unsafe_unretained *)_keys;
    state->mutationsPtr = &state->extra[0];
    state->state = 1;
    return _count;
}

- (id)retain {
    return self;
}

- (oneway void)release {
}

- (id)autorelease {
    return self;
}

- (NSUInteger)retainCount {
    return NSUIntegerMax;
}

- (id)copyWithZone:(NSZone *)zone {
    return self;
}

@end
