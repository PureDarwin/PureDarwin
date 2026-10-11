/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSBundle.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSString.h>
#import <Foundation/NSURL.h>
#include <CoreFoundation/CFBundle.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFURL.h>
#include <objc/runtime.h>
#include <pthread.h>

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

// the bundle of the binary the class was loaded from: its .framework, or the app it is part of
+ (NSBundle *)bundleForClass:(Class)aClass {
    static NSMutableDictionary *byPath;
    static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    const char *image = aClass != Nil ? class_getImageName(aClass) : NULL;
    NSString *path, *bundlePath = nil;
    NSRange framework;
    NSBundle *bundle;

    if (image == NULL)
        return [self mainBundle];
    path = [NSString stringWithUTF8String:image];
    framework = [path rangeOfString:@".framework/"];
    if (framework.location != NSNotFound)
        bundlePath = [path substringToIndex:NSMaxRange(framework) - 1];
    else if ([path rangeOfString:@".app/Contents/MacOS/"].location != NSNotFound)
        return [self mainBundle];
    // a plain dylib or the main executable has no bundle of its own
    if (bundlePath == nil)
        return [self mainBundle];

    pthread_mutex_lock(&lock);
    if (byPath == nil)
        byPath = [[NSMutableDictionary alloc] init];
    bundle = [byPath objectForKey:bundlePath];
    if (bundle == nil) {
        bundle = [[[NSBundle alloc] initWithPath:bundlePath] autorelease];
        if (bundle != nil)
            [byPath setObject:bundle forKey:bundlePath];
    }
    pthread_mutex_unlock(&lock);
    return bundle ?: [self mainBundle];
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

- (NSURL *)executableURL {
    return (NSURL *)CFBundleCopyExecutableURL(_bundle);
}

- (NSString *)executablePath {
    CFURLRef url = CFBundleCopyExecutableURL(_bundle);
    if (url == NULL) {
        return nil;
    }

    NSString *path = (NSString *)CFURLCopyFileSystemPath(url, kCFURLPOSIXPathStyle);
    CFRelease(url);
    return [path autorelease];
}

// the resources URL from CFBundle is relative to the bundle, so resolve it or the path is "Contents/Resources".
- (NSString *)resourcePath {
    CFURLRef url = CFBundleCopyResourcesDirectoryURL(_bundle);
    if (url == NULL) {
        return nil;
    }

    CFURLRef absolute = CFURLCopyAbsoluteURL(url);
    NSString *path = (NSString *)CFURLCopyFileSystemPath(absolute, kCFURLPOSIXPathStyle);
    CFRelease(absolute);
    CFRelease(url);
    return path;
}

// the InfoPlist.strings values over the Info.plist ones
- (NSDictionary *)localizedInfoDictionary {
    CFDictionaryRef local = CFBundleGetLocalInfoDictionary(_bundle);
    NSMutableDictionary *result = [NSMutableDictionary dictionaryWithDictionary:[[self infoDictionary] autorelease]];

    if (local != NULL)
        [result addEntriesFromDictionary:(NSDictionary *)local];
    return result;
}

+ (NSBundle *)bundleWithIdentifier:(NSString *)identifier {
    CFBundleRef bundle = CFBundleGetBundleWithIdentifier((CFStringRef)identifier);
    CFURLRef url = bundle ? CFBundleCopyBundleURL(bundle) : NULL;
    NSBundle *result;

    if (url == NULL)
        return nil;
    result = [self bundleWithPath:[(NSURL *)url path]];
    CFRelease(url);
    return result;
}

- (NSDictionary *)infoDictionary {
    CFDictionaryRef info = CFBundleGetInfoDictionary(_bundle);
    return info == NULL ? nil : (NSDictionary *)CFRetain(info);
}

- (id)objectForInfoDictionaryKey:(NSString *)key {
    CFTypeRef value = CFBundleGetValueForInfoDictionaryKey(_bundle, (CFStringRef)key);
    return value == NULL ? nil : (__bridge id)value;
}

- (NSURL *)resourceURL {
    return [(NSURL *)CFBundleCopyResourcesDirectoryURL(_bundle) autorelease];
}

- (NSURL *)URLForResource:(NSString *)name withExtension:(NSString *)extension {
    return [self URLForResource:name withExtension:extension subdirectory:nil];
}

- (NSURL *)URLForResource:(NSString *)name
            withExtension:(NSString *)extension
             subdirectory:(NSString *)subpath {
    return [(NSURL *)CFBundleCopyResourceURL(_bundle, (CFStringRef)name, (CFStringRef)extension,
                                             (CFStringRef)subpath) autorelease];
}

- (NSURL *)URLForResource:(NSString *)name
            withExtension:(NSString *)extension
             subdirectory:(NSString *)subpath
             localization:(NSString *)localizationName {
    return [(NSURL *)CFBundleCopyResourceURLForLocalization(_bundle, (CFStringRef)name, (CFStringRef)extension,
                                                            (CFStringRef)subpath, (CFStringRef)localizationName) autorelease];
}

// the .lproj names the bundle carries
- (NSArray<NSString *> *)localizations {
    return [(NSArray *)CFBundleCopyBundleLocalizations(_bundle) autorelease];
}

- (NSArray<NSString *> *)preferredLocalizations {
    return [NSBundle preferredLocalizationsFromArray:[self localizations]];
}

+ (NSArray<NSString *> *)preferredLocalizationsFromArray:(NSArray<NSString *> *)localizationsArray {
    return [(NSArray *)CFBundleCopyPreferredLocalizationsFromArray((CFArrayRef)localizationsArray) autorelease];
}

+ (NSArray<NSString *> *)preferredLocalizationsFromArray:(NSArray<NSString *> *)localizationsArray
                                          forPreferences:(NSArray<NSString *> *)preferencesArray {
    return [(NSArray *)CFBundleCopyLocalizationsForPreferences((CFArrayRef)localizationsArray,
                                                               (CFArrayRef)preferencesArray) autorelease];
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

- (NSArray *)pathsForResourcesOfType:(NSString *)extension
                         inDirectory:(NSString *)subpath {
    CFArrayRef urls = CFBundleCopyResourceURLsOfType(_bundle,
                                                      (CFStringRef)extension,
                                                      (CFStringRef)subpath);
    if (urls == NULL) {
        return [NSArray array];
    }

    CFIndex count = CFArrayGetCount(urls);
    CFMutableArrayRef paths = CFArrayCreateMutable(kCFAllocatorDefault, count,
                                                    &kCFTypeArrayCallBacks);
    for (CFIndex i = 0; i < count; i++) {
        CFURLRef url = (CFURLRef)CFArrayGetValueAtIndex(urls, i);
        CFStringRef path = CFURLCopyFileSystemPath(url, kCFURLPOSIXPathStyle);
        if (path != NULL) {
            CFArrayAppendValue(paths, path);
            CFRelease(path);
        }
    }
    CFRelease(urls);
    return (NSArray *)paths;
}

/* CFBundleLoadExecutableAndReturnError reports why a load failed, which is the
 * whole point of the ...AndReturnError: form; -load and -isLoaded are the older
 * spellings of the same thing. */
- (BOOL)loadAndReturnError:(NSError **)error {
    CFErrorRef cfError = NULL;

    if (CFBundleLoadExecutableAndReturnError(_bundle, &cfError)) {
        return YES;
    }

    if (error != NULL) {
        *error = (NSError *)cfError;
        if (cfError != NULL) {
            CFAutorelease(cfError);
        }
    }
    else if (cfError != NULL) {
        CFRelease(cfError);
    }
    return NO;
}

- (BOOL)load {
    return CFBundleLoadExecutable(_bundle) ? YES : NO;
}

- (BOOL)isLoaded {
    return CFBundleIsExecutableLoaded(_bundle) ? YES : NO;
}

- (Class)principalClass {
    if (!CFBundleLoadExecutable(_bundle)) {
        return Nil;
    }

    CFTypeRef name = CFBundleGetValueForInfoDictionaryKey(
        _bundle, CFSTR("NSPrincipalClass"));
    if (name == NULL || CFGetTypeID(name) != CFStringGetTypeID()) {
        return Nil;
    }
    return NSClassFromString((NSString *)name);
}


// the .strings tables in the bundle's localizations, through CFBundle. A missing key gives VALUE, else the key
- (NSString *)localizedStringForKey:(NSString *)key
                              value:(NSString *)value
                              table:(NSString *)tableName {
    if (key == nil)
        return (value != nil) ? value : @"";
    if ([tableName length] == 0)
        tableName = @"Localizable";

    CFStringRef result = CFBundleCopyLocalizedString(_bundle, (CFStringRef)key,
                                                     (CFStringRef)([value length] ? value : key), (CFStringRef)tableName);

    return result ? [(NSString *)result autorelease] : key;
}


/* The bundle's classes all live in the one image here, so this is a lookup by
 * name rather than a per-bundle class table. */
- (Class)classNamed:(NSString *)className {
    return NSClassFromString(className);
}

@end
