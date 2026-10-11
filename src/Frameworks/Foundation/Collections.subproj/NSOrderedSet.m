/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

// NSOrderedSet and NSMutableOrderedSet, an array for the order and a set for membership, after Apple's class reference

#import <Foundation/NSOrderedSet.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSSet.h>
#import <Foundation/NSIndexSet.h>
#import <Foundation/NSString.h>
#import <Foundation/NSException.h>
#import <Foundation/NSCoder.h>
#import <Foundation/NSKeyValueCoding.h>
#include <stdarg.h>

static void PDRangeCheck(id self, SEL cmd, NSUInteger index, NSUInteger count) {
    if (index >= count)
        [NSException raise:NSRangeException format:@"*** -[%@ %@]: index %lu beyond bounds [0 .. %ld]",
                           NSStringFromClass([self class]), NSStringFromSelector(cmd), (unsigned long)index, (long)count - 1];
}

static void PDNilCheck(id self, SEL cmd, id object) {
    if (object == nil)
        [NSException raise:NSInvalidArgumentException format:@"*** -[%@ %@]: object cannot be nil",
                           NSStringFromClass([self class]), NSStringFromSelector(cmd)];
}

// the backing store stays out of the public header, as in Apple's
@interface NSOrderedSet () {
@protected
    id _array;
    id _set;
}
- (void)_appendObject:(id)object copy:(BOOL)copy;
@end

@implementation NSOrderedSet

#define ARRAY ((NSMutableArray *)_array)
#define SET ((NSMutableSet *)_set)

+ (instancetype)orderedSet {
    return [[[self alloc] init] autorelease];
}

+ (instancetype)orderedSetWithObject:(id)object {
    return [[[self alloc] initWithObject:object] autorelease];
}

+ (instancetype)orderedSetWithObjects:(id)firstObject, ... {
    NSMutableArray *objects = [NSMutableArray array];
    va_list args;

    va_start(args, firstObject);
    for (id o = firstObject; o != nil; o = va_arg(args, id))
        [objects addObject:o];
    va_end(args);
    return [[[self alloc] initWithArray:objects] autorelease];
}

+ (instancetype)orderedSetWithObjects:(const id *)objects count:(NSUInteger)count {
    return [[[self alloc] initWithObjects:objects count:count] autorelease];
}

+ (instancetype)orderedSetWithArray:(NSArray *)array {
    return [[[self alloc] initWithArray:array] autorelease];
}

+ (instancetype)orderedSetWithArray:(NSArray *)array range:(NSRange)range copyItems:(BOOL)flag {
    return [[[self alloc] initWithArray:array range:range copyItems:flag] autorelease];
}

+ (instancetype)orderedSetWithSet:(NSSet *)set {
    return [[[self alloc] initWithSet:set] autorelease];
}

+ (instancetype)orderedSetWithSet:(NSSet *)set copyItems:(BOOL)flag {
    return [[[self alloc] initWithSet:set copyItems:flag] autorelease];
}

+ (instancetype)orderedSetWithOrderedSet:(NSOrderedSet *)set {
    return [[[self alloc] initWithOrderedSet:set] autorelease];
}

+ (instancetype)orderedSetWithOrderedSet:(NSOrderedSet *)set range:(NSRange)range copyItems:(BOOL)flag {
    return [[[self alloc] initWithOrderedSet:set range:range copyItems:flag] autorelease];
}

- (instancetype)init {
    if ((self = [super init]) != nil) {
        _array = [[NSMutableArray alloc] init];
        _set = [[NSMutableSet alloc] init];
    }
    return self;
}

// the one place objects come in, a later duplicate of an earlier object is dropped
- (void)_appendObject:(id)object copy:(BOOL)copy {
    if (copy)
        object = [[object copy] autorelease];
    if ([SET containsObject:object])
        return;
    [ARRAY addObject:object];
    [SET addObject:object];
}

- (instancetype)initWithObject:(id)object {
    if ((self = [self init]) != nil)
        [self _appendObject:object copy:NO];
    return self;
}

- (instancetype)initWithObjects:(id)firstObject, ... {
    va_list args;

    if ((self = [self init]) == nil)
        return nil;
    va_start(args, firstObject);
    for (id o = firstObject; o != nil; o = va_arg(args, id))
        [self _appendObject:o copy:NO];
    va_end(args);
    return self;
}

- (instancetype)initWithObjects:(const id *)objects count:(NSUInteger)count {
    if ((self = [self init]) == nil)
        return nil;
    for (NSUInteger i = 0; i < count; i++) {
        PDNilCheck(self, _cmd, objects[i]);
        [self _appendObject:objects[i] copy:NO];
    }
    return self;
}

- (instancetype)initWithArray:(NSArray *)array {
    return [self initWithArray:array range:NSMakeRange(0, [array count]) copyItems:NO];
}

- (instancetype)initWithArray:(NSArray *)array copyItems:(BOOL)flag {
    return [self initWithArray:array range:NSMakeRange(0, [array count]) copyItems:flag];
}

- (instancetype)initWithArray:(NSArray *)array range:(NSRange)range copyItems:(BOOL)flag {
    if ((self = [self init]) == nil)
        return nil;
    for (NSUInteger i = range.location; i < NSMaxRange(range); i++)
        [self _appendObject:[array objectAtIndex:i] copy:flag];
    return self;
}

- (instancetype)initWithSet:(NSSet *)set {
    return [self initWithSet:set copyItems:NO];
}

- (instancetype)initWithSet:(NSSet *)set copyItems:(BOOL)flag {
    return [self initWithArray:[set allObjects] copyItems:flag];
}

- (instancetype)initWithOrderedSet:(NSOrderedSet *)set {
    return [self initWithArray:[set array] copyItems:NO];
}

- (instancetype)initWithOrderedSet:(NSOrderedSet *)set copyItems:(BOOL)flag {
    return [self initWithArray:[set array] copyItems:flag];
}

- (instancetype)initWithOrderedSet:(NSOrderedSet *)set range:(NSRange)range copyItems:(BOOL)flag {
    return [self initWithArray:[set array] range:range copyItems:flag];
}

- (void)dealloc {
    [_array release];
    [_set release];
    [super dealloc];
}

- (id)copyWithZone:(NSZone *)zone {
    if ([self class] == [NSOrderedSet class])
        return [self retain];
    return [[NSOrderedSet alloc] initWithArray:ARRAY];
}

- (id)mutableCopyWithZone:(NSZone *)zone {
    return [[NSMutableOrderedSet alloc] initWithArray:ARRAY];
}

- (void)encodeWithCoder:(NSCoder *)coder {
    if ([coder allowsKeyedCoding])
        [coder encodeObject:[NSArray arrayWithArray:ARRAY] forKey:@"NS.objects"];
    else
        [coder encodeObject:[NSArray arrayWithArray:ARRAY]];
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    NSArray *objects = [coder allowsKeyedCoding] ? [coder decodeObjectForKey:@"NS.objects"] : [coder decodeObject];

    return [self initWithArray:objects];
}

- (NSUInteger)count {
    return [ARRAY count];
}

- (id)objectAtIndex:(NSUInteger)index {
    PDRangeCheck(self, _cmd, index, [ARRAY count]);
    return [ARRAY objectAtIndex:index];
}

- (id)objectAtIndexedSubscript:(NSUInteger)index {
    return [self objectAtIndex:index];
}

- (NSUInteger)indexOfObject:(id)object {
    if (object == nil || ![SET containsObject:object])
        return NSNotFound;
    return [ARRAY indexOfObject:object];
}

- (NSUInteger)indexOfObject:(id)object inSortedRange:(NSRange)range options:(NSBinarySearchingOptions)options
            usingComparator:(NSComparator)comparator {
    NSUInteger low = range.location, high = NSMaxRange(range), found = NSNotFound;

    if ((options & NSBinarySearchingFirstEqual) && (options & NSBinarySearchingLastEqual))
        [NSException raise:NSInvalidArgumentException format:@"both NSBinarySearchingFirstEqual and NSBinarySearchingLastEqual"];
    while (low < high) {
        NSUInteger mid = low + (high - low) / 2;
        NSComparisonResult r = comparator([ARRAY objectAtIndex:mid], object);

        if (r == NSOrderedSame) {
            found = mid;
            // insertion alone takes whichever equal object it lands on first, as Cocoa does
            if (options & NSBinarySearchingFirstEqual)
                high = mid;
            else if (options & NSBinarySearchingLastEqual)
                low = mid + 1;
            else
                break;
        } else if (r == NSOrderedAscending) {
            low = mid + 1;
        } else {
            high = mid;
        }
    }
    if (options & NSBinarySearchingInsertionIndex) {
        if (found == NSNotFound)
            return low;
        return (options & NSBinarySearchingLastEqual) ? found + 1 : found;
    }
    return found;
}

- (NSUInteger)indexOfObjectPassingTest:(BOOL (^)(id, NSUInteger, BOOL *))predicate {
    return [self indexOfObjectWithOptions:0 passingTest:predicate];
}

- (NSUInteger)indexOfObjectWithOptions:(NSEnumerationOptions)options passingTest:(BOOL (^)(id, NSUInteger, BOOL *))predicate {
    return [ARRAY indexOfObjectWithOptions:options passingTest:predicate];
}

- (NSIndexSet *)indexesOfObjectsPassingTest:(BOOL (^)(id, NSUInteger, BOOL *))predicate {
    return [self indexesOfObjectsWithOptions:0 passingTest:predicate];
}

- (NSIndexSet *)indexesOfObjectsWithOptions:(NSEnumerationOptions)options passingTest:(BOOL (^)(id, NSUInteger, BOOL *))predicate {
    NSMutableIndexSet *indexes = [NSMutableIndexSet indexSet];

    [self enumerateObjectsWithOptions:options usingBlock:^(id obj, NSUInteger idx, BOOL *stop) {
        if (predicate(obj, idx, stop))
            [indexes addIndex:idx];
    }];
    return indexes;
}

- (void)getObjects:(id *)objects range:(NSRange)range {
    for (NSUInteger i = 0; i < range.length; i++)
        objects[i] = [self objectAtIndex:range.location + i];
}

- (NSArray *)objectsAtIndexes:(NSIndexSet *)indexes {
    NSMutableArray *result = [NSMutableArray array];

    [indexes enumerateIndexesUsingBlock:^(NSUInteger idx, BOOL *stop) {
        [result addObject:[self objectAtIndex:idx]];
    }];
    return result;
}

- (id)firstObject {
    return [ARRAY firstObject];
}

- (id)lastObject {
    return [ARRAY lastObject];
}

- (BOOL)containsObject:(id)object {
    return object != nil && [SET containsObject:object];
}

- (BOOL)isEqualToOrderedSet:(NSOrderedSet *)other {
    if (other == self)
        return YES;
    return [other isKindOfClass:[NSOrderedSet class]] && [ARRAY isEqualToArray:[other array]];
}

- (BOOL)isEqual:(id)other {
    return [other isKindOfClass:[NSOrderedSet class]] && [self isEqualToOrderedSet:other];
}

- (NSUInteger)hash {
    return [ARRAY count];
}

- (BOOL)intersectsOrderedSet:(NSOrderedSet *)other {
    for (id o in ARRAY) {
        if ([other containsObject:o])
            return YES;
    }
    return NO;
}

- (BOOL)intersectsSet:(NSSet *)set {
    return [SET intersectsSet:set];
}

- (BOOL)isSubsetOfOrderedSet:(NSOrderedSet *)other {
    for (id o in ARRAY) {
        if (![other containsObject:o])
            return NO;
    }
    return YES;
}

- (BOOL)isSubsetOfSet:(NSSet *)set {
    return [SET isSubsetOfSet:set];
}

- (NSEnumerator *)objectEnumerator {
    return [[NSArray arrayWithArray:ARRAY] objectEnumerator];
}

- (NSEnumerator *)reverseObjectEnumerator {
    return [[NSArray arrayWithArray:ARRAY] reverseObjectEnumerator];
}

- (NSOrderedSet *)reversedOrderedSet {
    return [NSOrderedSet orderedSetWithArray:[[self reverseObjectEnumerator] allObjects]];
}

- (NSArray *)array {
    return [NSArray arrayWithArray:ARRAY];
}

- (NSSet *)set {
    return [NSSet setWithSet:SET];
}

- (NSUInteger)countByEnumeratingWithState:(NSFastEnumerationState *)state objects:(id *)buffer count:(NSUInteger)len {
    return [ARRAY countByEnumeratingWithState:state objects:buffer count:len];
}

- (void)enumerateObjectsUsingBlock:(void (^)(id, NSUInteger, BOOL *))block {
    [self enumerateObjectsWithOptions:0 usingBlock:block];
}

- (void)enumerateObjectsWithOptions:(NSEnumerationOptions)options usingBlock:(void (^)(id, NSUInteger, BOOL *))block {
    NSArray *snapshot = [NSArray arrayWithArray:ARRAY];
    NSUInteger count = [snapshot count];
    BOOL stop = NO;

    for (NSUInteger n = 0; n < count && !stop; n++) {
        NSUInteger i = (options & NSEnumerationReverse) ? count - 1 - n : n;

        block([snapshot objectAtIndex:i], i, &stop);
    }
}

- (void)enumerateObjectsAtIndexes:(NSIndexSet *)indexes options:(NSEnumerationOptions)options
                       usingBlock:(void (^)(id, NSUInteger, BOOL *))block {
    [indexes enumerateIndexesWithOptions:options usingBlock:^(NSUInteger idx, BOOL *stop) {
        block([self objectAtIndex:idx], idx, stop);
    }];
}

- (NSArray *)sortedArrayUsingComparator:(NSComparator)comparator {
    return [ARRAY sortedArrayUsingComparator:comparator];
}

- (NSArray *)sortedArrayWithOptions:(NSSortOptions)options usingComparator:(NSComparator)comparator {
    NSMutableArray *sorted = [NSMutableArray arrayWithArray:ARRAY];

    [sorted sortWithOptions:options usingComparator:comparator];
    return sorted;
}

// KVC maps over the members, keeping the result an ordered set, @-operators go to the array
- (id)valueForKey:(NSString *)key {
    if ([key hasPrefix:@"@"])
        return [[self array] valueForKeyPath:key];
    NSMutableOrderedSet *values = [NSMutableOrderedSet orderedSet];

    for (id o in ARRAY) {
        id v = [o valueForKey:key];

        if (v != nil)
            [values addObject:v];
    }
    return [[values copy] autorelease];
}

- (void)setValue:(id)value forKey:(NSString *)key {
    for (id o in ARRAY)
        [o setValue:value forKey:key];
}

- (NSString *)descriptionWithLocale:(id)locale indent:(NSUInteger)level {
    NSMutableString *out = [NSMutableString string];
    NSString *pad = [@"" stringByPaddingToLength:level * 4 withString:@" " startingAtIndex:0];
    NSUInteger count = [ARRAY count];

    [out appendFormat:@"%@{(\n", pad];
    for (NSUInteger i = 0; i < count; i++) {
        id o = [ARRAY objectAtIndex:i];
        NSString *d = [o respondsToSelector:@selector(descriptionWithLocale:indent:)]
                          ? [o descriptionWithLocale:locale indent:level + 1]
                          : [o description];

        [out appendFormat:@"%@    %@%@\n", pad, d, i + 1 < count ? @"," : @""];
    }
    [out appendFormat:@"%@)}", pad];
    return out;
}

- (NSString *)descriptionWithLocale:(id)locale {
    return [self descriptionWithLocale:locale indent:0];
}

- (NSString *)description {
    return [self descriptionWithLocale:nil indent:0];
}

@end

@implementation NSMutableOrderedSet

+ (instancetype)orderedSetWithCapacity:(NSUInteger)capacity {
    return [[[self alloc] initWithCapacity:capacity] autorelease];
}

- (instancetype)initWithCapacity:(NSUInteger)capacity {
    return [self init];
}

- (id)copyWithZone:(NSZone *)zone {
    return [[NSOrderedSet alloc] initWithArray:ARRAY];
}

- (void)addObject:(id)object {
    PDNilCheck(self, _cmd, object);
    [self _appendObject:object copy:NO];
}

- (void)addObjects:(const id *)objects count:(NSUInteger)count {
    for (NSUInteger i = 0; i < count; i++)
        [self addObject:objects[i]];
}

- (void)addObjectsFromArray:(NSArray *)array {
    for (id o in array)
        [self addObject:o];
}

// an object already in the set is left where it is, as Cocoa does
- (void)insertObject:(id)object atIndex:(NSUInteger)index {
    PDNilCheck(self, _cmd, object);
    if (index > [ARRAY count])
        PDRangeCheck(self, _cmd, index, [ARRAY count]);
    if ([SET containsObject:object])
        return;
    [ARRAY insertObject:object atIndex:index];
    [SET addObject:object];
}

- (void)insertObjects:(NSArray *)objects atIndexes:(NSIndexSet *)indexes {
    __block NSUInteger n = 0;

    [indexes enumerateIndexesUsingBlock:^(NSUInteger idx, BOOL *stop) {
        [self insertObject:[objects objectAtIndex:n++] atIndex:idx];
    }];
}

- (void)setObject:(id)object atIndex:(NSUInteger)index {
    if (index == [ARRAY count])
        [self insertObject:object atIndex:index];
    else
        [self replaceObjectAtIndex:index withObject:object];
}

- (void)setObject:(id)object atIndexedSubscript:(NSUInteger)index {
    [self setObject:object atIndex:index];
}

- (void)replaceObjectAtIndex:(NSUInteger)index withObject:(id)object {
    id old;

    PDNilCheck(self, _cmd, object);
    PDRangeCheck(self, _cmd, index, [ARRAY count]);
    old = [ARRAY objectAtIndex:index];
    if ([SET containsObject:object] && ![old isEqual:object])
        return;
    [SET removeObject:old];
    [ARRAY replaceObjectAtIndex:index withObject:object];
    [SET addObject:object];
}

- (void)replaceObjectsInRange:(NSRange)range withObjects:(const id *)objects count:(NSUInteger)count {
    [self removeObjectsInRange:range];
    for (NSUInteger i = 0; i < count; i++)
        [self insertObject:objects[i] atIndex:MIN(range.location + i, [ARRAY count])];
}

- (void)replaceObjectsAtIndexes:(NSIndexSet *)indexes withObjects:(NSArray *)objects {
    __block NSUInteger n = 0;

    [indexes enumerateIndexesUsingBlock:^(NSUInteger idx, BOOL *stop) {
        [self replaceObjectAtIndex:idx withObject:[objects objectAtIndex:n++]];
    }];
}

- (void)exchangeObjectAtIndex:(NSUInteger)index1 withObjectAtIndex:(NSUInteger)index2 {
    PDRangeCheck(self, _cmd, index1, [ARRAY count]);
    PDRangeCheck(self, _cmd, index2, [ARRAY count]);
    [ARRAY exchangeObjectAtIndex:index1 withObjectAtIndex:index2];
}

// the moved objects keep their order and land so the first is at index in the result
- (void)moveObjectsAtIndexes:(NSIndexSet *)indexes toIndex:(NSUInteger)index {
    NSArray *moving = [self objectsAtIndexes:indexes];

    [ARRAY removeObjectsAtIndexes:indexes];
    if (index > [ARRAY count])
        PDRangeCheck(self, _cmd, index, [ARRAY count]);
    [ARRAY insertObjects:moving atIndexes:[NSIndexSet indexSetWithIndexesInRange:NSMakeRange(index, [moving count])]];
}

- (void)removeObject:(id)object {
    if (object == nil || ![SET containsObject:object])
        return;
    [ARRAY removeObjectAtIndex:[ARRAY indexOfObject:object]];
    [SET removeObject:object];
}

- (void)removeObjectAtIndex:(NSUInteger)index {
    PDRangeCheck(self, _cmd, index, [ARRAY count]);
    [SET removeObject:[ARRAY objectAtIndex:index]];
    [ARRAY removeObjectAtIndex:index];
}

- (void)removeObjectsInRange:(NSRange)range {
    [self removeObjectsAtIndexes:[NSIndexSet indexSetWithIndexesInRange:range]];
}

- (void)removeObjectsAtIndexes:(NSIndexSet *)indexes {
    for (id o in [self objectsAtIndexes:indexes])
        [SET removeObject:o];
    [ARRAY removeObjectsAtIndexes:indexes];
}

- (void)removeObjectsInArray:(NSArray *)array {
    for (id o in array)
        [self removeObject:o];
}

- (void)removeAllObjects {
    [ARRAY removeAllObjects];
    [SET removeAllObjects];
}

- (void)intersectOrderedSet:(NSOrderedSet *)other {
    for (id o in [NSArray arrayWithArray:ARRAY]) {
        if (![other containsObject:o])
            [self removeObject:o];
    }
}

- (void)minusOrderedSet:(NSOrderedSet *)other {
    for (id o in other)
        [self removeObject:o];
}

- (void)unionOrderedSet:(NSOrderedSet *)other {
    for (id o in other)
        [self addObject:o];
}

- (void)intersectSet:(NSSet *)other {
    for (id o in [NSArray arrayWithArray:ARRAY]) {
        if (![other containsObject:o])
            [self removeObject:o];
    }
}

- (void)minusSet:(NSSet *)other {
    for (id o in other)
        [self removeObject:o];
}

- (void)unionSet:(NSSet *)other {
    for (id o in other)
        [self addObject:o];
}

- (void)sortUsingComparator:(NSComparator)comparator {
    [ARRAY sortUsingComparator:comparator];
}

- (void)sortWithOptions:(NSSortOptions)options usingComparator:(NSComparator)comparator {
    [ARRAY sortWithOptions:options usingComparator:comparator];
}

- (void)sortRange:(NSRange)range options:(NSSortOptions)options usingComparator:(NSComparator)comparator {
    NSMutableArray *part = [NSMutableArray arrayWithArray:[ARRAY subarrayWithRange:range]];

    [part sortWithOptions:options usingComparator:comparator];
    [ARRAY replaceObjectsInRange:range withObjectsFromArray:part];
}

@end
