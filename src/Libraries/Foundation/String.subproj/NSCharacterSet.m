/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSCharacterSet.h>
#include <CoreFoundation/CFCharacterSet.h>

/* Bridged straight onto CFCharacterSet, the same way NSArray is onto CFArray. */

@implementation NSCharacterSet

static NSCharacterSet *_predefined(CFCharacterSetPredefinedSet which) {
    return (NSCharacterSet *)CFCharacterSetCreateCopy(kCFAllocatorDefault,
                                                      CFCharacterSetGetPredefined(which));
}

+ (NSCharacterSet *)alphanumericCharacterSet {
    return _predefined(kCFCharacterSetAlphaNumeric);
}

+ (NSCharacterSet *)controlCharacterSet {
    return _predefined(kCFCharacterSetControl);
}

+ (NSCharacterSet *)decimalDigitCharacterSet {
    return _predefined(kCFCharacterSetDecimalDigit);
}

+ (NSCharacterSet *)letterCharacterSet {
    return _predefined(kCFCharacterSetLetter);
}

+ (NSCharacterSet *)lowercaseLetterCharacterSet {
    return _predefined(kCFCharacterSetLowercaseLetter);
}

+ (NSCharacterSet *)newlineCharacterSet {
    return _predefined(kCFCharacterSetNewline);
}

+ (NSCharacterSet *)punctuationCharacterSet {
    return _predefined(kCFCharacterSetPunctuation);
}

+ (NSCharacterSet *)uppercaseLetterCharacterSet {
    return _predefined(kCFCharacterSetUppercaseLetter);
}

+ (NSCharacterSet *)whitespaceAndNewlineCharacterSet {
    return _predefined(kCFCharacterSetWhitespaceAndNewline);
}

+ (NSCharacterSet *)whitespaceCharacterSet {
    return _predefined(kCFCharacterSetWhitespace);
}

+ (NSCharacterSet *)characterSetWithCharactersInString:(NSString *)string {
    return (NSCharacterSet *)CFCharacterSetCreateWithCharactersInString(
        kCFAllocatorDefault, (CFStringRef)string);
}

+ (NSCharacterSet *)characterSetWithRange:(NSRange)range {
    return (NSCharacterSet *)CFCharacterSetCreateWithCharactersInRange(
        kCFAllocatorDefault, CFRangeMake((CFIndex)range.location, (CFIndex)range.length));
}

- (BOOL)characterIsMember:(unichar)character {
    return CFCharacterSetIsCharacterMember((CFCharacterSetRef)self,
                                           (UniChar)character) ? YES : NO;
}

- (NSCharacterSet *)invertedSet {
    return (NSCharacterSet *)CFCharacterSetCreateInvertedSet(kCFAllocatorDefault,
                                                             (CFCharacterSetRef)self);
}

@end

@implementation NSMutableCharacterSet

+ (NSMutableCharacterSet *)characterSetWithRange:(NSRange)range {
    CFMutableCharacterSetRef set = CFCharacterSetCreateMutable(kCFAllocatorDefault);
    CFCharacterSetAddCharactersInRange(set, CFRangeMake((CFIndex)range.location,
                                                        (CFIndex)range.length));
    return (NSMutableCharacterSet *)set;
}

- (void)addCharactersInRange:(NSRange)range {
    CFCharacterSetAddCharactersInRange((CFMutableCharacterSetRef)self,
                                       CFRangeMake((CFIndex)range.location,
                                                   (CFIndex)range.length));
}

- (void)addCharactersInString:(NSString *)string {
    CFCharacterSetAddCharactersInString((CFMutableCharacterSetRef)self,
                                        (CFStringRef)string);
}

- (void)removeCharactersInRange:(NSRange)range {
    CFCharacterSetRemoveCharactersInRange((CFMutableCharacterSetRef)self,
                                          CFRangeMake((CFIndex)range.location,
                                                      (CFIndex)range.length));
}

- (void)removeCharactersInString:(NSString *)string {
    CFCharacterSetRemoveCharactersInString((CFMutableCharacterSetRef)self,
                                           (CFStringRef)string);
}

- (void)invert {
    CFCharacterSetInvert((CFMutableCharacterSetRef)self);
}

@end
