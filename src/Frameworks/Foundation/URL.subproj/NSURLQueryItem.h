// NSURLQueryItem.h - one name and value pair from a URL's query
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#import <Foundation/NSObject.h>

@class NSString;

@interface NSURLQueryItem : NSObject <NSSecureCoding, NSCopying> {
    NSString *_name;
    NSString *_value;
}

+ (instancetype)queryItemWithName:(NSString *)name value:(NSString *)value;
- (instancetype)initWithName:(NSString *)name value:(NSString *)value;

@property (readonly, copy) NSString *name;
@property (readonly, copy) NSString *value;

@end
