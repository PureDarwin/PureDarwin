/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSFileManager.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSData.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSError.h>
#import <Foundation/NSNumber.h>
#import <Foundation/NSPathUtilities.h>
#import <Foundation/NSString.h>
#include <CoreFoundation/CFString.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

NSString *const NSFileSize = @"NSFileSize";
NSString *const NSFileType = @"NSFileType";
NSString *const NSFileTypeRegular = @"NSFileTypeRegular";
NSString *const NSFileTypeDirectory = @"NSFileTypeDirectory";
NSString *const NSFileTypeSymbolicLink = @"NSFileTypeSymbolicLink";
NSString *const NSFileTypeUnknown = @"NSFileTypeUnknown";

/* Paths cross into POSIX as UTF-8; PATH_MAX-bounded so nothing here allocates. */
static BOOL _fsPath(NSString *path, char *buffer, size_t size) {
    if (path == nil) {
        return NO;
    }
    return CFStringGetCString((CFStringRef)path, buffer, (CFIndex)size,
                              kCFStringEncodingUTF8) ? YES : NO;
}

static NSString *_string(const char *cString) {
    return (NSString *)CFStringCreateWithCString(kCFAllocatorDefault, cString,
                                                 kCFStringEncodingUTF8);
}

static void _setPOSIXError(NSError **error) {
    if (error != NULL) {
        *error = [NSError errorWithDomain:@"NSPOSIXErrorDomain" code:errno];
    }
}

@implementation NSFileManager

+ (NSFileManager *)defaultManager {
    static NSFileManager *shared = nil;
    if (shared == nil) {
        shared = [[self alloc] init];
    }
    return shared;
}

- (BOOL)fileExistsAtPath:(NSString *)path {
    return [self fileExistsAtPath:path isDirectory:NULL];
}

- (BOOL)fileExistsAtPath:(NSString *)path isDirectory:(BOOL *)isDirectory {
    char buffer[PATH_MAX];
    struct stat info;

    if (!_fsPath(path, buffer, sizeof(buffer)) || stat(buffer, &info) != 0) {
        return NO;
    }
    if (isDirectory != NULL) {
        *isDirectory = S_ISDIR(info.st_mode) ? YES : NO;
    }
    return YES;
}

- (BOOL)isReadableFileAtPath:(NSString *)path {
    char buffer[PATH_MAX];
    return _fsPath(path, buffer, sizeof(buffer)) && access(buffer, R_OK) == 0;
}

- (BOOL)isWritableFileAtPath:(NSString *)path {
    char buffer[PATH_MAX];
    return _fsPath(path, buffer, sizeof(buffer)) && access(buffer, W_OK) == 0;
}

- (BOOL)isExecutableFileAtPath:(NSString *)path {
    char buffer[PATH_MAX];
    return _fsPath(path, buffer, sizeof(buffer)) && access(buffer, X_OK) == 0;
}

- (NSArray<NSString *> *)contentsOfDirectoryAtPath:(NSString *)path error:(NSError **)error {
    char buffer[PATH_MAX];
    if (!_fsPath(path, buffer, sizeof(buffer))) {
        _setPOSIXError(error);
        return nil;
    }

    DIR *dir = opendir(buffer);
    if (dir == NULL) {
        _setPOSIXError(error);
        return nil;
    }

    NSMutableArray<NSString *> *entries = [NSMutableArray arrayWithCapacity:0];
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        [entries addObject:_string(entry->d_name)];
    }
    closedir(dir);

    return entries;
}

- (NSDictionary *)attributesOfItemAtPath:(NSString *)path error:(NSError **)error {
    char buffer[PATH_MAX];
    struct stat info;

    if (!_fsPath(path, buffer, sizeof(buffer)) || lstat(buffer, &info) != 0) {
        _setPOSIXError(error);
        return nil;
    }

    NSString *type = NSFileTypeUnknown;
    if (S_ISREG(info.st_mode)) {
        type = NSFileTypeRegular;
    } else if (S_ISDIR(info.st_mode)) {
        type = NSFileTypeDirectory;
    } else if (S_ISLNK(info.st_mode)) {
        type = NSFileTypeSymbolicLink;
    }

    NSMutableDictionary *attributes = [NSMutableDictionary dictionaryWithCapacity:2];
    [attributes setObject:[NSNumber numberWithUnsignedLongLong:(unsigned long long)info.st_size]
                   forKey:NSFileSize];
    [attributes setObject:type forKey:NSFileType];
    return attributes;
}

- (NSData *)contentsAtPath:(NSString *)path {
    char buffer[PATH_MAX];
    if (!_fsPath(path, buffer, sizeof(buffer))) {
        return nil;
    }

    FILE *file = fopen(buffer, "rb");
    if (file == NULL) {
        return nil;
    }

    CFMutableDataRef data = CFDataCreateMutable(kCFAllocatorDefault, 0);
    unsigned char chunk[65536];
    size_t got;
    while ((got = fread(chunk, 1, sizeof(chunk), file)) > 0) {
        CFDataAppendBytes(data, chunk, (CFIndex)got);
    }
    fclose(file);

    return (NSData *)data;
}

- (BOOL)createFileAtPath:(NSString *)path
                contents:(NSData *)contents
              attributes:(NSDictionary *)attributes {
    char buffer[PATH_MAX];
    if (!_fsPath(path, buffer, sizeof(buffer))) {
        return NO;
    }

    FILE *file = fopen(buffer, "wb");
    if (file == NULL) {
        return NO;
    }

    BOOL ok = YES;
    NSUInteger length = [contents length];
    if (length > 0) {
        ok = fwrite([contents bytes], 1, length, file) == length;
    }
    fclose(file);

    return ok;
}

- (BOOL)createDirectoryAtPath:(NSString *)path
  withIntermediateDirectories:(BOOL)createIntermediates
                   attributes:(NSDictionary *)attributes
                        error:(NSError **)error {
    if (createIntermediates) {
        NSString *parent = [path stringByDeletingLastPathComponent];
        if ([parent length] > 0 && ![self fileExistsAtPath:parent]) {
            if (![self createDirectoryAtPath:parent
                 withIntermediateDirectories:YES
                                  attributes:attributes
                                       error:error]) {
                return NO;
            }
        }
    }

    char buffer[PATH_MAX];
    if (!_fsPath(path, buffer, sizeof(buffer))) {
        _setPOSIXError(error);
        return NO;
    }

    if (mkdir(buffer, 0777) != 0) {
        if (createIntermediates && errno == EEXIST) {
            return YES;
        }
        _setPOSIXError(error);
        return NO;
    }
    return YES;
}

- (BOOL)removeItemAtPath:(NSString *)path error:(NSError **)error {
    char buffer[PATH_MAX];
    if (!_fsPath(path, buffer, sizeof(buffer))) {
        _setPOSIXError(error);
        return NO;
    }

    struct stat info;
    if (lstat(buffer, &info) != 0) {
        _setPOSIXError(error);
        return NO;
    }

    if (S_ISDIR(info.st_mode)) {
        NSArray<NSString *> *entries = [self contentsOfDirectoryAtPath:path error:error];
        NSUInteger count = [entries count];
        for (NSUInteger i = 0; i < count; i++) {
            NSString *child = [path stringByAppendingPathComponent:[entries objectAtIndex:i]];
            if (![self removeItemAtPath:child error:error]) {
                return NO;
            }
        }
        if (rmdir(buffer) != 0) {
            _setPOSIXError(error);
            return NO;
        }
        return YES;
    }

    if (unlink(buffer) != 0) {
        _setPOSIXError(error);
        return NO;
    }
    return YES;
}

- (NSString *)currentDirectoryPath {
    char buffer[PATH_MAX];
    if (getcwd(buffer, sizeof(buffer)) == NULL) {
        return nil;
    }
    return _string(buffer);
}

- (BOOL)changeCurrentDirectoryPath:(NSString *)path {
    char buffer[PATH_MAX];
    return _fsPath(path, buffer, sizeof(buffer)) && chdir(buffer) == 0;
}

@end
