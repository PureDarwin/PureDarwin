// AXCustomContent.m - extra labelled content an accessibility client can read for an element
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#import <Foundation/Foundation.h>

typedef NS_ENUM(NSUInteger, AXCustomContentImportance) {
    AXCustomContentImportanceDefault,
    AXCustomContentImportanceHigh,
};

@interface AXCustomContent : NSObject <NSCopying, NSSecureCoding> {
    NSAttributedString *_label;
    NSAttributedString *_value;
    AXCustomContentImportance _importance;
}
@end

@implementation AXCustomContent

+ (BOOL)supportsSecureCoding {
    return YES;
}

+ (instancetype)customContentWithAttributedLabel:(NSAttributedString *)label attributedValue:(NSAttributedString *)value {
    AXCustomContent *content = [[[self alloc] init] autorelease];

    content->_label = [label copy];
    content->_value = [value copy];
    return content;
}

+ (instancetype)customContentWithLabel:(NSString *)label value:(NSString *)value {
    return [self customContentWithAttributedLabel:[[[NSAttributedString alloc] initWithString:label ? label : @""] autorelease]
                                  attributedValue:[[[NSAttributedString alloc] initWithString:value ? value : @""] autorelease]];
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    if ((self = [super init]) != nil) {
        _label = [[coder decodeObjectForKey:@"label"] copy];
        _value = [[coder decodeObjectForKey:@"value"] copy];
        _importance = [coder decodeIntegerForKey:@"importance"];
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)coder {
    [coder encodeObject:_label forKey:@"label"];
    [coder encodeObject:_value forKey:@"value"];
    [coder encodeInteger:_importance forKey:@"importance"];
}

- (void)dealloc {
    [_label release];
    [_value release];
    [super dealloc];
}

- (id)copyWithZone:(NSZone *)zone {
    AXCustomContent *copy = [[AXCustomContent alloc] init];

    copy->_label = [_label copy];
    copy->_value = [_value copy];
    copy->_importance = _importance;
    return copy;
}

- (NSString *)label { return [_label string]; }
- (NSString *)value { return [_value string]; }
- (NSAttributedString *)attributedLabel { return _label; }
- (NSAttributedString *)attributedValue { return _value; }
- (AXCustomContentImportance)importance { return _importance; }
- (void)setImportance:(AXCustomContentImportance)importance { _importance = importance; }

@end
