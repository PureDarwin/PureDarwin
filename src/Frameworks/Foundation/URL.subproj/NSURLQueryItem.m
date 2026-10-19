// NSURLQueryItem.m - one name and value pair from a URL's query
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#import <Foundation/NSURLQueryItem.h>
#import <Foundation/NSString.h>
#import <Foundation/NSCoder.h>

@implementation NSURLQueryItem

+ (BOOL)supportsSecureCoding {
    return YES;
}

+ (instancetype)queryItemWithName:(NSString *)name value:(NSString *)value {
    return [[[self alloc] initWithName:name value:value] autorelease];
}

- (instancetype)initWithName:(NSString *)name value:(NSString *)value {
    if ((self = [super init]) != nil) {
        _name = [name copy];
        _value = [value copy];
    }
    return self;
}

- (instancetype)init {
    return [self initWithName:@"" value:nil];
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    return [self initWithName:[coder decodeObjectForKey:@"NS.name"] value:[coder decodeObjectForKey:@"NS.value"]];
}

- (void)encodeWithCoder:(NSCoder *)coder {
    [coder encodeObject:_name forKey:@"NS.name"];
    [coder encodeObject:_value forKey:@"NS.value"];
}

- (void)dealloc {
    [_name release];
    [_value release];
    [super dealloc];
}

- (id)copyWithZone:(NSZone *)zone {
    return [self retain];
}

- (NSString *)name {
    return _name;
}

- (NSString *)value {
    return _value;
}

- (BOOL)isEqual:(id)other {
    if (other == self)
        return YES;
    if (![other isKindOfClass:[NSURLQueryItem class]])
        return NO;
    NSURLQueryItem *item = other;
    return [_name isEqualToString:item->_name] && (_value == item->_value || [_value isEqualToString:item->_value]);
}

- (NSUInteger)hash {
    return [_name hash] ^ [_value hash];
}

- (NSString *)description {
    return [NSString stringWithFormat:@"<%@ %p> {name = %@, value = %@}", [self class], self, _name, _value];
}

@end
