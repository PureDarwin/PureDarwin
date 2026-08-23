/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSBundle.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSString.h>
#import <Foundation/NSURL.h>
#include <CoreFoundation/CFBundle.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFURL.h>

/* Backed by CFBundle: -bundlePath and friends unwrap the CFBundleRef we hold
 * rather than reimplementing bundle layout. */

@implementation NSBundle {
    CFBundleRef _bundle;
}

+ (NSBundle *)mainBundle {
    static NSBundle *shared = nil;
    if (shared == nil) {
        CFBundleRef main = CFBundleGetMainBundle();
        if (main == NULL) {
            return nil;
        }
        shared = [[self alloc] init];
        shared->_bundle = (CFBundleRef)CFRetain(main);
    }
    return shared;
}

+ (NSBundle *)bundleWithPath:(NSString *)path {
    return [[self alloc] initWithPath:path];
}

+ (NSBundle *)bundleForClass:(Class)aClass {
    return [self mainBundle];
}

- (instancetype)initWithPath:(NSString *)path {
    self = [super init];
    if (self == nil) {
        return nil;
    }

    CFURLRef url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault,
                                                 (CFStringRef)path,
                                                 kCFURLPOSIXPathStyle, true);
    if (url == NULL) {
        return nil;
    }

    _bundle = CFBundleCreate(kCFAllocatorDefault, url);
    CFRelease(url);

    return _bundle != NULL ? self : nil;
}

- (void)dealloc {
    if (_bundle != NULL) {
        CFRelease(_bundle);
    }
}

- (NSString *)bundlePath {
    return [[self bundleURL] path];
}

- (NSURL *)bundleURL {
    return (NSURL *)CFBundleCopyBundleURL(_bundle);
}

- (NSString *)bundleIdentifier {
    CFStringRef identifier = CFBundleGetIdentifier(_bundle);
    return identifier == NULL ? nil : (NSString *)CFRetain(identifier);
}

- (NSString *)resourcePath {
    CFURLRef url = CFBundleCopyResourcesDirectoryURL(_bundle);
    if (url == NULL) {
        return nil;
    }

    NSString *path = (NSString *)CFURLCopyFileSystemPath(url, kCFURLPOSIXPathStyle);
    CFRelease(url);
    return path;
}

- (NSDictionary *)infoDictionary {
    CFDictionaryRef info = CFBundleGetInfoDictionary(_bundle);
    return info == NULL ? nil : (NSDictionary *)CFRetain(info);
}

- (id)objectForInfoDictionaryKey:(NSString *)key {
    CFTypeRef value = CFBundleGetValueForInfoDictionaryKey(_bundle, (CFStringRef)key);
    return value == NULL ? nil : (__bridge id)value;
}

- (NSString *)pathForResource:(NSString *)name ofType:(NSString *)extension {
    return [self pathForResource:name ofType:extension inDirectory:nil];
}

- (NSString *)pathForResource:(NSString *)name
                       ofType:(NSString *)extension
                  inDirectory:(NSString *)subpath {
    CFURLRef url = CFBundleCopyResourceURL(_bundle, (CFStringRef)name,
                                           (CFStringRef)extension,
                                           (CFStringRef)subpath);
    if (url == NULL) {
        return nil;
    }

    NSString *path = (NSString *)CFURLCopyFileSystemPath(url, kCFURLPOSIXPathStyle);
    CFRelease(url);
    return path;
}

@end
