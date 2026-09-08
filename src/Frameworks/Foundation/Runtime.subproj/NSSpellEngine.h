/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSSpellEngine_h
#define NSSpellEngine_h

#import <Foundation/NSObject.h>
#import <Foundation/NSRange.h>

@class NSString, NSArray, NSOrthography;

/* Cocotron's spell-checker plug-in point. No engine ships with PureDarwin, so
 * +allSpellEngines is empty and NSSpellChecker finds nothing to consult. */
@interface NSSpellEngine : NSObject

+ (NSArray *)allSpellEngines;

- (NSArray *)languages;
- (NSRange)checkSpellingOfString:(NSString *)string
                      startingAt:(NSInteger)start
                        language:(NSString *)language
                           wrap:(BOOL)wrap
         inSpellDocumentWithTag:(NSInteger)tag
                      wordCount:(NSInteger *)wordCount;
- (NSArray *)guessesForWord:(NSString *)word inLanguage:(NSString *)language;

@end

#endif /* NSSpellEngine_h */
