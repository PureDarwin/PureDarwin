/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSSortDescriptor_h
#define NSSortDescriptor_h

#import <Foundation/NSObject.h>
#import <Foundation/NSObjCRuntime.h>

@class NSString, NSArray;

@interface NSSortDescriptor : NSObject <NSCopying> {
    NSString *_key;
    BOOL _ascending;
    SEL _selector;
}

+ (instancetype)sortDescriptorWithKey:(NSString *)key ascending:(BOOL)ascending;
+ (instancetype)sortDescriptorWithKey:(NSString *)key
                            ascending:(BOOL)ascending
                             selector:(SEL)selector;

- (instancetype)initWithKey:(NSString *)key ascending:(BOOL)ascending;
- (instancetype)initWithKey:(NSString *)key
                  ascending:(BOOL)ascending
                   selector:(SEL)selector;

- (NSString *)key;
- (BOOL)ascending;
- (SEL)selector;

- (NSSortDescriptor *)reversedSortDescriptor;
- (NSComparisonResult)compareObject:(id)first toObject:(id)second;

@end

#endif /* NSSortDescriptor_h */
