/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSPathUtilities.h>
#import <Foundation/NSArray.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFURL.h>
#include <pwd.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>

/* Returns +1 rather than an autoreleased string: callers here store the result
 * without retaining it, so making these +0 leaves them holding a dangling
 * pointer once the pool drains. Cocoa returns +0; that difference needs the
 * call sites audited before it can be changed. */
static NSString *_string(const char *cString) {
    if (cString == NULL) {
        return nil;
    }
    return (NSString *)CFStringCreateWithCString(kCFAllocatorDefault, cString,
                                                 kCFStringEncodingUTF8);
}

static struct passwd *_currentPasswd(void) {
    return getpwuid(getuid());
}

NSString *NSUserName(void) {
    struct passwd *pw = _currentPasswd();
    return pw != NULL ? _string(pw->pw_name) : _string("");
}

NSString *NSFullUserName(void) {
    struct passwd *pw = _currentPasswd();
    if (pw == NULL || pw->pw_gecos == NULL || pw->pw_gecos[0] == '\0') {
        return NSUserName();
    }
    return _string(pw->pw_gecos);
}

NSString *NSHomeDirectory(void) {
    const char *home = getenv("HOME");
    if (home != NULL && home[0] != '\0') {
        return _string(home);
    }

    struct passwd *pw = _currentPasswd();
    return pw != NULL ? _string(pw->pw_dir) : _string("/");
}

NSString *NSHomeDirectoryForUser(NSString *userName) {
    char buffer[256];
    if (!CFStringGetCString((CFStringRef)userName, buffer, sizeof(buffer),
                            kCFStringEncodingUTF8)) {
        return nil;
    }

    struct passwd *pw = getpwnam(buffer);
    return pw != NULL ? _string(pw->pw_dir) : nil;
}

NSString *NSTemporaryDirectory(void) {
    const char *tmp = getenv("TMPDIR");
    return _string(tmp != NULL && tmp[0] != '\0' ? tmp : "/tmp");
}

NSString *NSOpenStepRootDirectory(void) {
    return _string("/");
}

/* Returns the directory's name under each requested domain's root, in the
 * order Foundation documents: user, local, network, then system. */
static NSString *_searchPathLeaf(NSSearchPathDirectory directory) {
    switch (directory) {
        case NSApplicationDirectory:        return @"Applications";
        case NSDemoApplicationDirectory:    return @"Applications/Demos";
        case NSDeveloperApplicationDirectory: return @"Developer/Applications";
        case NSAdminApplicationDirectory:   return @"Applications/Utilities";
        case NSLibraryDirectory:            return @"Library";
        case NSDeveloperDirectory:          return @"Developer";
        case NSUserDirectory:               return @"Users";
        case NSDocumentationDirectory:      return @"Library/Documentation";
        case NSDocumentDirectory:           return @"Documents";
        case NSCoreServiceDirectory:        return @"Library/CoreServices";
        case NSAutosavedInformationDirectory: return @"Library/Autosave Information";
        case NSDesktopDirectory:            return @"Desktop";
        case NSCachesDirectory:             return @"Library/Caches";
        case NSApplicationSupportDirectory: return @"Library/Application Support";
        case NSDownloadsDirectory:          return @"Downloads";
        case NSInputMethodsDirectory:       return @"Library/Input Methods";
        case NSMoviesDirectory:             return @"Movies";
        case NSMusicDirectory:              return @"Music";
        case NSPicturesDirectory:           return @"Pictures";
        case NSPrinterDescriptionDirectory: return @"Library/Printers/PPDs";
        case NSSharedPublicDirectory:       return @"Public";
        case NSPreferencePanesDirectory:    return @"Library/PreferencePanes";
        case NSApplicationScriptsDirectory: return @"Library/Application Scripts";
        case NSTrashDirectory:              return @".Trash";
        default:                            return nil;
    }
}

NSArray<NSString *> *NSSearchPathForDirectoriesInDomains(
    NSSearchPathDirectory directory, NSSearchPathDomainMask domainMask,
    BOOL expandTilde) {
    NSString *leaf = _searchPathLeaf(directory);
    if (leaf == nil) {
        return [NSArray array];
    }

    NSMutableArray<NSString *> *result = [NSMutableArray array];

    if (domainMask & NSUserDomainMask) {
        NSString *home = expandTilde ? NSHomeDirectory() : @"~";
        [result addObject:[home stringByAppendingPathComponent:leaf]];
    }
    if (domainMask & NSLocalDomainMask) {
        [result addObject:[@"/" stringByAppendingPathComponent:leaf]];
    }
    if (domainMask & NSNetworkDomainMask) {
        [result addObject:[@"/Network" stringByAppendingPathComponent:leaf]];
    }
    if (domainMask & NSSystemDomainMask) {
        [result addObject:[@"/System" stringByAppendingPathComponent:leaf]];
    }

    return result;
}

/* Path arithmetic goes through CFURL where it can, so the edge cases (trailing
 * slashes, "/" itself, extension-less names) match CoreFoundation's. */

@implementation NSString (NSPathUtilities)

- (NSString *)lastPathComponent {
    NSArray<NSString *> *components = [self pathComponents];
    NSUInteger count = [components count];

    if (count == 0) {
        return (NSString *)CFSTR("");
    }
    return [components objectAtIndex:count - 1];
}

- (NSString *)stringByDeletingLastPathComponent {
    CFURLRef url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, (CFStringRef)self,
                                                 kCFURLPOSIXPathStyle, false);
    if (url == NULL) {
        return self;
    }

    CFURLRef parent = CFURLCreateCopyDeletingLastPathComponent(kCFAllocatorDefault, url);
    CFRelease(url);
    if (parent == NULL) {
        return self;
    }

    CFStringRef path = CFURLCopyFileSystemPath(parent, kCFURLPOSIXPathStyle);
    CFRelease(parent);
    return (NSString *)path;
}

- (NSString *)pathExtension {
    CFURLRef url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, (CFStringRef)self,
                                                 kCFURLPOSIXPathStyle, false);
    if (url == NULL) {
        return (NSString *)CFSTR("");
    }

    CFStringRef extension = CFURLCopyPathExtension(url);
    CFRelease(url);
    return extension != NULL ? (NSString *)extension : (NSString *)CFSTR("");
}

- (NSString *)stringByDeletingPathExtension {
    CFURLRef url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, (CFStringRef)self,
                                                 kCFURLPOSIXPathStyle, false);
    if (url == NULL) {
        return self;
    }

    CFURLRef stripped = CFURLCreateCopyDeletingPathExtension(kCFAllocatorDefault, url);
    CFRelease(url);
    if (stripped == NULL) {
        return self;
    }

    CFStringRef path = CFURLCopyFileSystemPath(stripped, kCFURLPOSIXPathStyle);
    CFRelease(stripped);
    return (NSString *)path;
}

- (NSString *)stringByAppendingPathComponent:(NSString *)component {
    CFMutableStringRef path = CFStringCreateMutableCopy(kCFAllocatorDefault, 0,
                                                        (CFStringRef)self);

    if (CFStringGetLength(path) > 0 && !CFStringHasSuffix(path, CFSTR("/"))) {
        CFStringAppend(path, CFSTR("/"));
    }
    CFStringAppend(path, (CFStringRef)component);

    return (NSString *)path;
}

- (NSString *)stringByAppendingPathExtension:(NSString *)extension {
    CFMutableStringRef path = CFStringCreateMutableCopy(kCFAllocatorDefault, 0,
                                                        (CFStringRef)self);
    CFStringAppend(path, CFSTR("."));
    CFStringAppend(path, (CFStringRef)extension);
    return (NSString *)path;
}

- (NSArray<NSString *> *)pathComponents {
    NSMutableArray<NSString *> *components = [NSMutableArray arrayWithCapacity:0];
    CFArrayRef parts = CFStringCreateArrayBySeparatingStrings(kCFAllocatorDefault,
                                                              (CFStringRef)self,
                                                              CFSTR("/"));
    if (parts == NULL) {
        return components;
    }

    if ([self isAbsolutePath]) {
        [components addObject:(NSString *)CFSTR("/")];
    }

    CFIndex count = CFArrayGetCount(parts);
    for (CFIndex i = 0; i < count; i++) {
        CFStringRef part = CFArrayGetValueAtIndex(parts, i);
        if (CFStringGetLength(part) > 0) {
            [components addObject:(__bridge NSString *)part];
        }
    }
    CFRelease(parts);

    return components;
}

- (BOOL)isAbsolutePath {
    return CFStringHasPrefix((CFStringRef)self, CFSTR("/")) ? YES : NO;
}


- (NSString *)stringByExpandingTildeInPath {
    if (![self hasPrefix:@"~"]) {
        return self;
    }
    if ([self length] == 1 || [self characterAtIndex:1] == '/') {
        NSString *rest = ([self length] > 1) ? [self substringFromIndex:1] : @"";

        return [NSHomeDirectory() stringByAppendingString:rest];
    }
    return self;
}

- (NSString *)stringByAbbreviatingWithTildeInPath {
    NSString *home = NSHomeDirectory();

    if ([home length] > 0 && [self hasPrefix:home]) {
        return [@"~" stringByAppendingString:[self substringFromIndex:[home length]]];
    }
    return self;
}

/* Expands a leading tilde and removes "." and empty components, resolving
 * ".." lexically the way the path APIs specify. */
- (NSString *)stringByStandardizingPath {
    NSString *expanded = [self stringByExpandingTildeInPath];
    NSArray *parts = [expanded pathComponents];
    NSMutableArray *kept = [NSMutableArray array];
    BOOL absolute = [expanded isAbsolutePath];

    for (NSString *part in parts) {
        if ([part isEqualToString:@"."] || [part length] == 0) {
            continue;
        }
        if ([part isEqualToString:@".."] && [kept count] > 0) {
            NSString *last = [kept lastObject];

            if (![last isEqualToString:@".."] && ![last isEqualToString:@"/"]) {
                [kept removeObjectAtIndex:[kept count] - 1];
                continue;
            }
        }
        [kept addObject:part];
    }

    NSMutableString *result = [NSMutableString string];
    NSUInteger index = 0;

    for (NSString *part in kept) {
        if ([part isEqualToString:@"/"]) {
            continue;
        }
        if (index++ > 0 || absolute) {
            [result appendString:@"/"];
        }
        [result appendString:part];
    }
    if ([result length] == 0) {
        return absolute ? @"/" : @"";
    }
    return result;
}

/* realpath(3) resolves the symlinks; a path that does not exist is returned
 * standardised instead, matching the documented behaviour. */
- (NSString *)stringByResolvingSymlinksInPath {
    NSString *standardized = [self stringByStandardizingPath];
    char resolved[PATH_MAX];

    if (realpath([standardized fileSystemRepresentation], resolved) != NULL) {
        return [NSString stringWithUTF8String:resolved];
    }
    return standardized;
}

@end
