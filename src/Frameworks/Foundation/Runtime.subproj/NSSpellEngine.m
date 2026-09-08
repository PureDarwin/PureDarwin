/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSSpellEngine.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSString.h>

@implementation NSSpellEngine

+ (NSArray *)allSpellEngines {
    return [NSArray array];
}

- (NSArray *)languages {
    return [NSArray array];
}

- (NSRange)checkSpellingOfString:(NSString *)string
                      startingAt:(NSInteger)start
                        language:(NSString *)language
                           wrap:(BOOL)wrap
         inSpellDocumentWithTag:(NSInteger)tag
                      wordCount:(NSInteger *)wordCount {
    if (wordCount != NULL) {
        *wordCount = 0;
    }
    return NSMakeRange(NSNotFound, 0);
}

- (NSArray *)guessesForWord:(NSString *)word inLanguage:(NSString *)language {
    return [NSArray array];
}

@end
