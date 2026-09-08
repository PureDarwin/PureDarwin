/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSPropertyListSerialization.h>
#import <Foundation/NSData.h>
#import <Foundation/NSString.h>
#include <CoreFoundation/CFPropertyList.h>

/* CoreFoundation already has a property-list parser and writer; this is the
 * ObjC spelling over CFPropertyList. */

static CFPropertyListFormat CFFormatForNS(NSPropertyListFormat format) {
    switch (format) {
        case NSPropertyListOpenStepFormat:    return kCFPropertyListOpenStepFormat;
        case NSPropertyListBinaryFormat_v1_0: return kCFPropertyListBinaryFormat_v1_0;
        default:                              return kCFPropertyListXMLFormat_v1_0;
    }
}

@implementation NSPropertyListSerialization

+ (id)propertyListFromData:(NSData *)data
          mutabilityOption:(NSPropertyListMutabilityOptions)option
                    format:(NSPropertyListFormat *)format
          errorDescription:(NSString **)errorString {
    CFPropertyListFormat cfFormat = 0;
    CFErrorRef error = NULL;
    CFPropertyListRef plist = CFPropertyListCreateWithData(kCFAllocatorDefault,
        (CFDataRef)data, (CFOptionFlags)option, &cfFormat, &error);

    if (plist == NULL) {
        if (errorString != NULL) {
            *errorString = @"property list could not be read";
        }
        if (error != NULL) {
            CFRelease(error);
        }
        return nil;
    }
    if (format != NULL) {
        *format = (NSPropertyListFormat)cfFormat;
    }
    return [(id)plist autorelease];
}

+ (NSData *)dataFromPropertyList:(id)plist
                          format:(NSPropertyListFormat)format
                errorDescription:(NSString **)errorString {
    CFErrorRef error = NULL;
    CFDataRef data = CFPropertyListCreateData(kCFAllocatorDefault,
        (CFPropertyListRef)plist, CFFormatForNS(format), 0, &error);

    if (data == NULL) {
        if (errorString != NULL) {
            *errorString = @"property list could not be written";
        }
        if (error != NULL) {
            CFRelease(error);
        }
        return nil;
    }
    return [(NSData *)data autorelease];
}

+ (id)propertyListWithData:(NSData *)data
                   options:(NSPropertyListMutabilityOptions)options
                    format:(NSPropertyListFormat *)format
                     error:(NSError **)error {
    return [self propertyListFromData:data
                     mutabilityOption:options
                               format:format
                     errorDescription:NULL];
}

+ (NSData *)dataWithPropertyList:(id)plist
                          format:(NSPropertyListFormat)format
                         options:(NSUInteger)options
                           error:(NSError **)error {
    return [self dataFromPropertyList:plist format:format errorDescription:NULL];
}

@end
