// NSURLFileTypeMappings.h - file name extensions and MIME types, both ways
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#import <Foundation/NSObject.h>

@class NSString, NSArray, NSMutableDictionary;

@interface NSURLFileTypeMappings : NSObject {
    NSMutableDictionary *_typeForExtension;
    NSMutableDictionary *_extensionsForType;
}

+ (NSURLFileTypeMappings *)sharedMappings;

- (NSString *)MIMETypeForExtension:(NSString *)extension;
- (NSArray *)extensionsForMIMEType:(NSString *)type;
- (NSString *)preferredExtensionForMIMEType:(NSString *)type;

@end
