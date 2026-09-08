/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSValueTransformer.h>
#import <Foundation/NSString.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSNumber.h>
#import <Foundation/NSNull.h>

NSValueTransformerName const NSNegateBooleanTransformerName = @"NSNegateBoolean";
NSValueTransformerName const NSIsNilTransformerName = @"NSIsNil";
NSValueTransformerName const NSIsNotNilTransformerName = @"NSIsNotNil";

static NSMutableDictionary *transformers(void) {
    static NSMutableDictionary *shared = nil;

    if (shared == nil) {
        shared = [[NSMutableDictionary alloc] init];
    }
    return shared;
}

@interface NSNegateBooleanTransformer : NSValueTransformer
@end

@implementation NSNegateBooleanTransformer

+ (BOOL)allowsReverseTransformation {
    return YES;
}

- (id)transformedValue:(id)value {
    return [NSNumber numberWithBool:![value boolValue]];
}

- (id)reverseTransformedValue:(id)value {
    return [self transformedValue:value];
}

@end

@interface NSIsNilTransformer : NSValueTransformer
@end

@implementation NSIsNilTransformer

- (id)transformedValue:(id)value {
    BOOL isNil = (value == nil || value == (id)[NSNull null]);

    return [NSNumber numberWithBool:isNil];
}

@end

@interface NSIsNotNilTransformer : NSValueTransformer
@end

@implementation NSIsNotNilTransformer

- (id)transformedValue:(id)value {
    BOOL isNil = (value == nil || value == (id)[NSNull null]);

    return [NSNumber numberWithBool:!isNil];
}

@end

@implementation NSValueTransformer

+ (void)initialize {
    if (self != [NSValueTransformer class]) {
        return;
    }
    /* The three transformers Cocoa registers by default. */
    [self setValueTransformer:[[[NSNegateBooleanTransformer alloc] init] autorelease]
                      forName:NSNegateBooleanTransformerName];
    [self setValueTransformer:[[[NSIsNilTransformer alloc] init] autorelease]
                      forName:NSIsNilTransformerName];
    [self setValueTransformer:[[[NSIsNotNilTransformer alloc] init] autorelease]
                      forName:NSIsNotNilTransformerName];
}

+ (void)setValueTransformer:(NSValueTransformer *)transformer
                    forName:(NSValueTransformerName)name {
    if (transformer == nil) {
        [transformers() removeObjectForKey:name];
    } else {
        [transformers() setObject:transformer forKey:name];
    }
}

+ (NSValueTransformer *)valueTransformerForName:(NSValueTransformerName)name {
    return [transformers() objectForKey:name];
}

+ (NSArray *)valueTransformerNames {
    return [transformers() allKeys];
}

+ (Class)transformedValueClass {
    return [NSObject class];
}

+ (BOOL)allowsReverseTransformation {
    return NO;
}

- (id)transformedValue:(id)value {
    return value;
}

- (id)reverseTransformedValue:(id)value {
    return [self transformedValue:value];
}

@end
