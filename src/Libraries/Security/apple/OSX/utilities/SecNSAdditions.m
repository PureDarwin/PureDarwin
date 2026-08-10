//
//  SecNSAdditions.m
//  utilities
//


#import <utilities/SecNSAdditions.h>
#include <utilities/SecCFWrappers.h>

// MARK: NSArray

@implementation NSArray (compactDescription)
- (NSMutableString*) concatenateWithSeparator: (NSString*) separator {
    NSMutableString* result = [NSMutableString string];
    NSString* currentSeparator = @"";

    for(NSString* string in self) {
        [result appendString:currentSeparator];
        [result appendString:[string description]];
        currentSeparator = separator;
    }

    return result;
}
@end

// MARK: NSDictionary

@implementation NSDictionary (SOSDictionaryFormat)
- (NSString*) compactDescription
{
    NSMutableArray *elements = [NSMutableArray array];
    [self enumerateKeysAndObjectsUsingBlock: ^(NSString *key, id obj, BOOL *stop) {
        [elements addObject: [key stringByAppendingString: @":"]];
        if ([obj isKindOfClass:[NSArray class]]) {
            [elements addObject: [(NSArray *)obj componentsJoinedByString: @" "]];
        } else {
            [elements addObject: [NSString stringWithFormat:@"%@", obj]];
        }
    }];
    return [elements componentsJoinedByString: @" "];
}
@end


@implementation NSMutableDictionary (FindAndRemove)
-(NSObject*)extractObjectForKey:(NSString*)key
{
    NSObject* result = [self objectForKey:key];
    [self removeObjectForKey: key];
    return result;
}
@end



// MARK: NSSet

@implementation NSSet (Emptiness)
- (bool) isEmpty
{
    return [self count] == 0;
}
@end

@implementation NSSet (HasElements)
- (bool) containsElementsNotIn: (NSSet*) other
{
    __block bool hasElements = false;
    [self enumerateObjectsUsingBlock:^(id  _Nonnull obj, BOOL * _Nonnull stop) {
        if (![other containsObject:obj]) {
            hasElements = true;
            *stop = true;
        }
    }];
    return hasElements;
}

@end

@implementation NSSet (compactDescription)
- (NSString*) shortDescription {
    return [NSString stringWithFormat: @"{[%@]}", [[[self allObjects] sortedArrayUsingSelector:@selector(compare:)] concatenateWithSeparator: @", "]];
}
@end

@implementation NSSet (Stringizing)
- (NSString*) sortedElementsJoinedByString: (NSString*) separator {
    return [self sortedElementsTruncated: 0 JoinedByString: separator];
}

- (NSString*) sortedElementsTruncated: (NSUInteger) length JoinedByString: (NSString*) separator
{
    NSMutableArray* strings = [NSMutableArray array];

    [self enumerateObjectsUsingBlock:^(id  _Nonnull obj, BOOL * _Nonnull stop) {
        NSString *stringToInsert = nil;
        if ([obj isKindOfClass: [NSString class]]) {
            stringToInsert = obj;
        } else {
            stringToInsert = [obj description];
        }

        if (length > 0 && length < stringToInsert.length) {
            stringToInsert = [stringToInsert substringToIndex:length];
        }

        [strings insertObject:stringToInsert atIndex:0];
    }];

    [strings sortUsingSelector: @selector(compare:)];

    return [strings componentsJoinedByString:separator];
}
@end

@implementation NSMutableData (filledAndClipped)
+ (instancetype) dataWithSpace: (NSUInteger) initialSize DEREncode: (uint8_t*(^)(size_t size, uint8_t *buffer)) initialization {
    NSMutableData* result = [NSMutableData dataWithLength: initialSize];

    uint8_t* beginning = result.mutableBytes;
    uint8_t* encode_begin = initialization(initialSize, beginning);

    if (beginning <= encode_begin && encode_begin <= (encode_begin + initialSize)) {
        [result replaceBytesInRange:NSMakeRange(0, encode_begin - beginning) withBytes:NULL length:0];
    } else {
        result = nil;
    }

    return result;
}
@end
