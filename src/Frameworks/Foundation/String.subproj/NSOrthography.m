/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSOrthography.h>
#import <Foundation/NSString.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSDictionary.h>

@implementation NSOrthography

+ (instancetype)orthographyWithDominantScript:(NSString *)script
                                  languageMap:(NSDictionary *)map {
    return [[[self alloc] initWithDominantScript:script languageMap:map] autorelease];
}

- (instancetype)initWithDominantScript:(NSString *)script
                           languageMap:(NSDictionary *)map {
    self = [super init];
    if (self == nil) {
        return nil;
    }
    _dominantScript = [script copy];
    _languageMap = [map copy];
    return self;
}

- (void)dealloc {
    [_dominantScript release];
    [_languageMap release];
    [super dealloc];
}

- (NSString *)dominantScript {
    return _dominantScript;
}

- (NSDictionary *)languageMap {
    return _languageMap;
}

- (NSArray *)languagesForScript:(NSString *)script {
    return [_languageMap objectForKey:script];
}

- (NSString *)dominantLanguageForScript:(NSString *)script {
    return [[self languagesForScript:script] firstObject];
}

- (NSString *)dominantLanguage {
    return [self dominantLanguageForScript:_dominantScript];
}

- (NSArray *)allScripts {
    return [_languageMap allKeys];
}

- (NSArray *)allLanguages {
    NSMutableArray *result = [NSMutableArray array];
    NSArray *scripts = [_languageMap allKeys];
    NSUInteger count = [scripts count];

    for (NSUInteger i = 0; i < count; i++) {
        [result addObjectsFromArray:[_languageMap objectForKey:[scripts objectAtIndex:i]]];
    }
    return result;
}

- (id)copyWithZone:(NSZone *)zone {
    return [self retain];
}

@end
