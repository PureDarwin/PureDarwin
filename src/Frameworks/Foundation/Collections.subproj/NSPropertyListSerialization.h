/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSPropertyListSerialization_h
#define NSPropertyListSerialization_h

#import <Foundation/NSObject.h>
#import <Foundation/NSObjCRuntime.h>

@class NSData, NSString, NSError;

typedef NS_ENUM(NSUInteger, NSPropertyListMutabilityOptions) {
    NSPropertyListImmutable = 0,
    NSPropertyListMutableContainers = 1,
    NSPropertyListMutableContainersAndLeaves = 2,
};

typedef NS_ENUM(NSUInteger, NSPropertyListFormat) {
    NSPropertyListOpenStepFormat = 1,
    NSPropertyListXMLFormat_v1_0 = 100,
    NSPropertyListBinaryFormat_v1_0 = 200,
};

@interface NSPropertyListSerialization : NSObject

+ (id)propertyListFromData:(NSData *)data
          mutabilityOption:(NSPropertyListMutabilityOptions)option
                    format:(NSPropertyListFormat *)format
          errorDescription:(NSString **)errorString;

+ (NSData *)dataFromPropertyList:(id)plist
                          format:(NSPropertyListFormat)format
                errorDescription:(NSString **)errorString;

+ (id)propertyListWithData:(NSData *)data
                   options:(NSPropertyListMutabilityOptions)options
                    format:(NSPropertyListFormat *)format
                     error:(NSError **)error;

+ (NSData *)dataWithPropertyList:(id)plist
                          format:(NSPropertyListFormat)format
                         options:(NSUInteger)options
                           error:(NSError **)error;

@end

#endif /* NSPropertyListSerialization_h */
