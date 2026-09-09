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
#include <sys/mount.h>
#include <sys/param.h>
#include <fcntl.h>
#include <unistd.h>

NSString *const NSFileSize = @"NSFileSize";
NSString *const NSFileType = @"NSFileType";
NSString *const NSFileOwnerAccountID = @"NSFileOwnerAccountID";
NSString *const NSFileGroupOwnerAccountID = @"NSFileGroupOwnerAccountID";
NSString *const NSFileReferenceCount = @"NSFileReferenceCount";
NSString *const NSFileDeviceIdentifier = @"NSFileDeviceIdentifier";
NSString *const NSFileSystemNumber = @"NSFileSystemNumber";
NSString *const NSFileImmutable = @"NSFileImmutable";
NSString *const NSFileAppendOnly = @"NSFileAppendOnly";
NSString *const NSFileExtensionHidden = @"NSFileExtensionHidden";
NSString *const NSFileHFSCreatorCode = @"NSFileHFSCreatorCode";
NSString *const NSFileHFSTypeCode = @"NSFileHFSTypeCode";
NSString *const NSFileSystemSize = @"NSFileSystemSize";
NSString *const NSFileSystemFreeSize = @"NSFileSystemFreeSize";
NSString *const NSFileSystemNodes = @"NSFileSystemNodes";
NSString *const NSFileSystemFreeNodes = @"NSFileSystemFreeNodes";
NSString *const NSFileTypeRegular = @"NSFileTypeRegular";
NSString *const NSFileTypeSocket = @"NSFileTypeSocket";
NSString *const NSFileTypeCharacterSpecial = @"NSFileTypeCharacterSpecial";
NSString *const NSFileTypeBlockSpecial = @"NSFileTypeBlockSpecial";
NSString *const NSFileTypeFIFO = @"NSFileTypeFIFO";
NSString *const NSFileTypeDirectory = @"NSFileTypeDirectory";
NSString *const NSFileTypeSymbolicLink = @"NSFileTypeSymbolicLink";
NSString *const NSFileTypeUnknown = @"NSFileTypeUnknown";
NSString *const NSFileModificationDate = @"NSFileModificationDate";
NSString *const NSFileCreationDate = @"NSFileCreationDate";
NSString *const NSFileOwnerAccountName = @"NSFileOwnerAccountName";
NSString *const NSFileGroupOwnerAccountName = @"NSFileGroupOwnerAccountName";
NSString *const NSFilePosixPermissions = @"NSFilePosixPermissions";
NSString *const NSFileSystemFileNumber = @"NSFileSystemFileNumber";

/* Depth-first walk, yielding paths relative to the root the way Cocoa does. */
@implementation NSDirectoryEnumerator

- (instancetype)initWithPath:(NSString *)path {
    self = [super init];
    if (self == nil) {
        return nil;
    }
    _root = [path copy];
    _stack = [[NSMutableArray alloc] init];

    NSArray *entries = [[NSFileManager defaultManager] contentsOfDirectoryAtPath:path
                                                                          error:NULL];
    NSUInteger count = [entries count];
    for (NSUInteger i = 0; i < count; i++) {
        [_stack addObject:[entries objectAtIndex:i]];
    }
    return self;
}

- (void)dealloc {
    [_root release];
    [_stack release];
    [super dealloc];
}

- (id)nextObject {
    if ([_stack count] == 0) {
        return nil;
    }

    NSString *relative = [[[_stack objectAtIndex:0] retain] autorelease];
    [_stack removeObjectAtIndex:0];

    NSString *full = [_root stringByAppendingPathComponent:relative];
    BOOL isDirectory = NO;

    if ([[NSFileManager defaultManager] fileExistsAtPath:full isDirectory:&isDirectory] &&
        isDirectory) {
        NSArray *entries = [[NSFileManager defaultManager] contentsOfDirectoryAtPath:full
                                                                               error:NULL];
        NSUInteger count = [entries count];
        for (NSUInteger i = 0; i < count; i++) {
            [_stack addObject:[relative stringByAppendingPathComponent:
                                 [entries objectAtIndex:i]]];
        }
    }
    return relative;
}

- (NSDictionary *)fileAttributes {
    return nil;
}

- (NSDictionary *)directoryAttributes {
    return nil;
}

- (void)skipDescendents {
}

@end


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

- (NSString *)stringWithFileSystemRepresentation:(const char *)string
                                           length:(NSUInteger)length {
    if (string == NULL) {
        return nil;
    }

    return [[[NSString alloc] initWithBytes:string
                                     length:length
                                   encoding:NSUTF8StringEncoding] autorelease];
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

/* Copy one regular file's bytes, preserving permission bits. */
static BOOL _copyFileBytes(const char *from, const char *to, mode_t mode,
                           NSError **error) {
    int in = open(from, O_RDONLY);
    if (in < 0) {
        _setPOSIXError(error);
        return NO;
    }

    int out = open(to, O_WRONLY | O_CREAT | O_TRUNC, mode & 07777);
    if (out < 0) {
        _setPOSIXError(error);
        close(in);
        return NO;
    }

    char buffer[65536];
    ssize_t got;
    while ((got = read(in, buffer, sizeof(buffer))) > 0) {
        ssize_t done = 0;
        while (done < got) {
            ssize_t put = write(out, buffer + done, (size_t)(got - done));
            if (put <= 0) {
                _setPOSIXError(error);
                close(in);
                close(out);
                return NO;
            }
            done += put;
        }
    }

    if (got < 0) {
        _setPOSIXError(error);
        close(in);
        close(out);
        return NO;
    }

    close(in);
    close(out);
    return YES;
}

- (BOOL)copyItemAtPath:(NSString *)source toPath:(NSString *)destination
                 error:(NSError **)error {
    char from[PATH_MAX], to[PATH_MAX];
    if (!_fsPath(source, from, sizeof(from)) ||
        !_fsPath(destination, to, sizeof(to))) {
        _setPOSIXError(error);
        return NO;
    }

    struct stat info;
    if (lstat(from, &info) != 0) {
        _setPOSIXError(error);
        return NO;
    }

    if (S_ISLNK(info.st_mode)) {
        char target[PATH_MAX];
        ssize_t length = readlink(from, target, sizeof(target) - 1);
        if (length < 0) {
            _setPOSIXError(error);
            return NO;
        }
        target[length] = '\0';
        if (symlink(target, to) != 0) {
            _setPOSIXError(error);
            return NO;
        }
        return YES;
    }

    if (S_ISDIR(info.st_mode)) {
        if (mkdir(to, info.st_mode & 07777) != 0 && errno != EEXIST) {
            _setPOSIXError(error);
            return NO;
        }

        NSArray<NSString *> *entries = [self contentsOfDirectoryAtPath:source
                                                                 error:error];
        NSUInteger count = [entries count];
        for (NSUInteger i = 0; i < count; i++) {
            NSString *name = [entries objectAtIndex:i];
            if (![self copyItemAtPath:[source stringByAppendingPathComponent:name]
                               toPath:[destination stringByAppendingPathComponent:name]
                                error:error]) {
                return NO;
            }
        }
        return YES;
    }

    return _copyFileBytes(from, to, info.st_mode, error);
}

- (BOOL)moveItemAtPath:(NSString *)source toPath:(NSString *)destination
                 error:(NSError **)error {
    char from[PATH_MAX], to[PATH_MAX];
    if (!_fsPath(source, from, sizeof(from)) ||
        !_fsPath(destination, to, sizeof(to))) {
        _setPOSIXError(error);
        return NO;
    }

    if (rename(from, to) == 0) {
        return YES;
    }

    /* Across devices rename cannot work; fall back to copy and remove. */
    if (errno != EXDEV) {
        _setPOSIXError(error);
        return NO;
    }

    if (![self copyItemAtPath:source toPath:destination error:error]) {
        return NO;
    }
    return [self removeItemAtPath:source error:error];
}

- (BOOL)linkItemAtPath:(NSString *)source toPath:(NSString *)destination
                 error:(NSError **)error {
    char from[PATH_MAX], to[PATH_MAX];
    if (!_fsPath(source, from, sizeof(from)) ||
        !_fsPath(destination, to, sizeof(to))) {
        _setPOSIXError(error);
        return NO;
    }

    if (link(from, to) != 0) {
        _setPOSIXError(error);
        return NO;
    }
    return YES;
}

- (BOOL)createSymbolicLinkAtPath:(NSString *)path
             withDestinationPath:(NSString *)destination
                           error:(NSError **)error {
    char link[PATH_MAX], target[PATH_MAX];
    if (!_fsPath(path, link, sizeof(link)) ||
        !_fsPath(destination, target, sizeof(target))) {
        _setPOSIXError(error);
        return NO;
    }

    if (symlink(target, link) != 0) {
        _setPOSIXError(error);
        return NO;
    }
    return YES;
}

- (NSString *)destinationOfSymbolicLinkAtPath:(NSString *)path
                                        error:(NSError **)error {
    char buffer[PATH_MAX], target[PATH_MAX];
    if (!_fsPath(path, buffer, sizeof(buffer))) {
        _setPOSIXError(error);
        return nil;
    }

    ssize_t length = readlink(buffer, target, sizeof(target) - 1);
    if (length < 0) {
        _setPOSIXError(error);
        return nil;
    }
    target[length] = '\0';

    return [self stringWithFileSystemRepresentation:target];
}

- (NSDictionary *)attributesOfFileSystemForPath:(NSString *)path
                                          error:(NSError **)error {
    char buffer[PATH_MAX];
    if (!_fsPath(path, buffer, sizeof(buffer))) {
        _setPOSIXError(error);
        return nil;
    }

    struct statfs info;
    if (statfs(buffer, &info) != 0) {
        _setPOSIXError(error);
        return nil;
    }

    unsigned long long blockSize = (unsigned long long)info.f_bsize;

    return [NSDictionary dictionaryWithObjectsAndKeys:
        [NSNumber numberWithUnsignedLongLong:blockSize * info.f_blocks],
            NSFileSystemSize,
        [NSNumber numberWithUnsignedLongLong:blockSize * info.f_bavail],
            NSFileSystemFreeSize,
        [NSNumber numberWithUnsignedLongLong:(unsigned long long)info.f_files],
            NSFileSystemNodes,
        [NSNumber numberWithUnsignedLongLong:(unsigned long long)info.f_ffree],
            NSFileSystemFreeNodes,
        [NSNumber numberWithUnsignedLong:(unsigned long)info.f_fsid.val[0]],
            NSFileSystemNumber,
        nil];
}

/* The pre-10.5 spellings. Deprecated by Apple but never removed, and what
 * GNUstep-era sources call; each one forwards to its modern replacement. */

- (BOOL)createDirectoryAtPath:(NSString *)path
                   attributes:(NSDictionary *)attributes {
    if (![self createDirectoryAtPath:path
         withIntermediateDirectories:NO
                          attributes:attributes
                               error:NULL]) {
        return NO;
    }
    if (attributes != nil) {
        [self setAttributes:attributes ofItemAtPath:path error:NULL];
    }
    return YES;
}

- (BOOL)removeFileAtPath:(NSString *)path handler:(id)handler {
    return [self removeItemAtPath:path error:NULL];
}

- (BOOL)copyPath:(NSString *)source toPath:(NSString *)destination
         handler:(id)handler {
    return [self copyItemAtPath:source toPath:destination error:NULL];
}

- (BOOL)movePath:(NSString *)source toPath:(NSString *)destination
         handler:(id)handler {
    return [self moveItemAtPath:source toPath:destination error:NULL];
}

- (BOOL)linkPath:(NSString *)source toPath:(NSString *)destination
         handler:(id)handler {
    return [self linkItemAtPath:source toPath:destination error:NULL];
}

- (BOOL)changeFileAttributes:(NSDictionary *)attributes atPath:(NSString *)path {
    return [self setAttributes:attributes ofItemAtPath:path error:NULL];
}

- (NSDictionary *)fileSystemAttributesAtPath:(NSString *)path {
    return [self attributesOfFileSystemForPath:path error:NULL];
}

- (NSString *)pathContentOfSymbolicLinkAtPath:(NSString *)path {
    return [self destinationOfSymbolicLinkAtPath:path error:NULL];
}

- (BOOL)createSymbolicLinkAtPath:(NSString *)path
                     pathContent:(NSString *)destination {
    return [self createSymbolicLinkAtPath:path
                      withDestinationPath:destination
                                    error:NULL];
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

- (NSDirectoryEnumerator *)enumeratorAtPath:(NSString *)path {
    return [[[NSDirectoryEnumerator alloc] initWithPath:path] autorelease];
}


/* Older spellings of -attributesOfItemAtPath:error: and
 * -contentsOfDirectoryAtPath:error:, which this code still uses. */
- (NSDictionary *)fileAttributesAtPath:(NSString *)path traverseLink:(BOOL)traverse {
    return [self attributesOfItemAtPath:path error:NULL];
}

- (NSArray *)directoryContentsAtPath:(NSString *)path {
    return [self contentsOfDirectoryAtPath:path error:NULL];
}


/* Deletable means the containing directory is writable, which is what the
 * POSIX rules actually turn on. */
- (BOOL)isDeletableFileAtPath:(NSString *)path {
    if (access([path fileSystemRepresentation], F_OK) != 0) {
        return NO;
    }
    return access([[path stringByDeletingLastPathComponent] fileSystemRepresentation],
                  W_OK) == 0;
}


/* Only the attributes with a direct POSIX equivalent are applied; the rest are
 * accepted and ignored, which is what callers setting permissions expect. */
- (BOOL)setAttributes:(NSDictionary *)attributes ofItemAtPath:(NSString *)path
                error:(NSError **)error {
    const char *system = [path fileSystemRepresentation];
    NSNumber *permissions = [attributes objectForKey:NSFilePosixPermissions];
    BOOL ok = YES;

    if (permissions != nil) {
        ok = (chmod(system, (mode_t)[permissions unsignedLongValue]) == 0);
    }

    NSNumber *owner = [attributes objectForKey:NSFileOwnerAccountID];
    NSNumber *group = [attributes objectForKey:NSFileGroupOwnerAccountID];

    if (ok && (owner != nil || group != nil)) {
        ok = (chown(system,
                    (owner != nil) ? (uid_t)[owner unsignedLongValue] : (uid_t)-1,
                    (group != nil) ? (gid_t)[group unsignedLongValue] : (gid_t)-1) == 0);
    }
    if (!ok && error != NULL) {
        *error = nil;
    }
    return ok;
}

@end

@implementation NSDictionary (NSFileAttributes)

- (unsigned long long)fileSize {
    return [[self objectForKey:NSFileSize] unsignedLongLongValue];
}

- (NSString *)fileType {
    return [self objectForKey:NSFileType];
}

- (NSUInteger)filePosixPermissions {
    return (NSUInteger)[[self objectForKey:NSFilePosixPermissions] unsignedLongValue];
}

- (NSString *)fileOwnerAccountName {
    return [self objectForKey:NSFileOwnerAccountName];
}

- (NSString *)fileGroupOwnerAccountName {
    return [self objectForKey:NSFileGroupOwnerAccountName];
}

- (NSDate *)fileModificationDate {
    return [self objectForKey:NSFileModificationDate];
}

- (NSDate *)fileCreationDate {
    return [self objectForKey:NSFileCreationDate];
}

- (NSUInteger)fileSystemFileNumber {
    return (NSUInteger)[[self objectForKey:NSFileSystemFileNumber] unsignedLongValue];
}

- (NSUInteger)fileSystemNumber {
    return (NSUInteger)[[self objectForKey:NSFileSystemNumber] unsignedLongValue];
}

- (BOOL)fileIsImmutable {
    return [[self objectForKey:NSFileImmutable] boolValue];
}

- (BOOL)fileIsAppendOnly {
    return [[self objectForKey:NSFileAppendOnly] boolValue];
}

- (BOOL)fileExtensionHidden {
    return [[self objectForKey:NSFileExtensionHidden] boolValue];
}

@end
