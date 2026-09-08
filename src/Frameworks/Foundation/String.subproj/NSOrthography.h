/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSOrthography_h
#define NSOrthography_h

#import <Foundation/NSObject.h>

@class NSString, NSArray, NSDictionary;

@interface NSOrthography : NSObject <NSCopying> {
    NSString *_dominantScript;
    NSDictionary *_languageMap;
}

+ (instancetype)orthographyWithDominantScript:(NSString *)script
                                  languageMap:(NSDictionary *)map;

- (instancetype)initWithDominantScript:(NSString *)script
                           languageMap:(NSDictionary *)map;

- (NSString *)dominantScript;
- (NSDictionary *)languageMap;
- (NSArray *)languagesForScript:(NSString *)script;
- (NSString *)dominantLanguageForScript:(NSString *)script;
- (NSString *)dominantLanguage;
- (NSArray *)allScripts;
- (NSArray *)allLanguages;

@end

#endif /* NSOrthography_h */
