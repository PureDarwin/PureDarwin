/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSFormatter.h>
#import <Foundation/NSString.h>

@implementation NSFormatter

- (NSString *)stringForObjectValue:(id)object {
    return nil;
}

/* Defaults to the display string; editable formatters override it. */
- (NSString *)editingStringForObjectValue:(id)object {
    return [self stringForObjectValue:object];
}

- (NSAttributedString *)attributedStringForObjectValue:(id)object
                                 withDefaultAttributes:(NSDictionary *)attributes {
    return nil;
}

- (BOOL)getObjectValue:(out id *)object
             forString:(NSString *)string
      errorDescription:(out NSString **)error {
    return NO;
}

- (BOOL)getObjectValue:(out id *)object
             forString:(NSString *)string
                 range:(inout NSRange *)range
                 error:(out NSError **)error {
    NSString *description = nil;

    if (![self getObjectValue:object forString:string errorDescription:&description]) {
        return NO;
    }
    if (range != NULL) {
        range->location = 0;
        range->length = [string length];
    }
    return YES;
}

/* Every partial string is acceptable unless a subclass says otherwise. */
- (BOOL)isPartialStringValid:(NSString *)partialString
            newEditingString:(NSString **)newString
            errorDescription:(NSString **)error {
    if (newString != NULL) {
        *newString = nil;
    }
    if (error != NULL) {
        *error = nil;
    }
    return YES;
}

- (id)copyWithZone:(NSZone *)zone {
    return [self retain];
}

- (void)encodeWithCoder:(NSCoder *)coder {
}

- (id)initWithCoder:(NSCoder *)coder {
    return [self init];
}

@end
