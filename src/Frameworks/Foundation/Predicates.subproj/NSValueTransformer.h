/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSValueTransformer_h
#define NSValueTransformer_h

#import <Foundation/NSObject.h>
#import <Foundation/NSObjCRuntime.h>

@class NSString, NSArray;

typedef NSString *NSValueTransformerName;

FOUNDATION_EXPORT NSValueTransformerName const NSNegateBooleanTransformerName;
FOUNDATION_EXPORT NSValueTransformerName const NSIsNilTransformerName;
FOUNDATION_EXPORT NSValueTransformerName const NSIsNotNilTransformerName;

@interface NSValueTransformer : NSObject

+ (void)setValueTransformer:(NSValueTransformer *)transformer
                    forName:(NSValueTransformerName)name;
+ (NSValueTransformer *)valueTransformerForName:(NSValueTransformerName)name;
+ (NSArray *)valueTransformerNames;

+ (Class)transformedValueClass;
+ (BOOL)allowsReverseTransformation;

- (id)transformedValue:(id)value;
- (id)reverseTransformedValue:(id)value;

@end

#endif /* NSValueTransformer_h */
