/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSFormatter_h
#define NSFormatter_h

#import <Foundation/NSObject.h>
#import <Foundation/NSObjCRuntime.h>
#import <Foundation/NSRange.h>

@class NSString, NSAttributedString, NSDictionary, NSError;

/* Abstract: a concrete formatter overrides stringForObjectValue: and, if it
 * accepts input, getObjectValue:forString:errorDescription:. */
@interface NSFormatter : NSObject <NSCopying, NSCoding>

- (NSString *)stringForObjectValue:(id)object;
- (NSString *)editingStringForObjectValue:(id)object;
- (NSAttributedString *)attributedStringForObjectValue:(id)object
                                 withDefaultAttributes:(NSDictionary *)attributes;

- (BOOL)getObjectValue:(out id *)object
             forString:(NSString *)string
      errorDescription:(out NSString **)error;

- (BOOL)getObjectValue:(out id *)object
             forString:(NSString *)string
                 range:(inout NSRange *)range
                 error:(out NSError **)error;

- (BOOL)isPartialStringValid:(NSString *)partialString
            newEditingString:(NSString **)newString
            errorDescription:(NSString **)error;

@end

#endif /* NSFormatter_h */
