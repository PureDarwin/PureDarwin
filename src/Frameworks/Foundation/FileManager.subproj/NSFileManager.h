/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSFileManager_h
#define NSFileManager_h

#import <Foundation/NSObject.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSEnumerator.h>
#import <Foundation/NSObjCRuntime.h>

#import <Foundation/NSArray.h>
#import <Foundation/NSString.h>

@class NSData, NSDictionary, NSError, NSMutableArray, NSString;

FOUNDATION_EXPORT NSString *const NSFileSize;
FOUNDATION_EXPORT NSString *const NSFileType;
FOUNDATION_EXPORT NSString *const NSFileTypeRegular;
FOUNDATION_EXPORT NSString *const NSFileTypeDirectory;
FOUNDATION_EXPORT NSString *const NSFileTypeSymbolicLink;
FOUNDATION_EXPORT NSString *const NSFileTypeSocket;
FOUNDATION_EXPORT NSString *const NSFileTypeCharacterSpecial;
FOUNDATION_EXPORT NSString *const NSFileTypeBlockSpecial;
FOUNDATION_EXPORT NSString *const NSFileTypeFIFO;
FOUNDATION_EXPORT NSString *const NSFileTypeUnknown;
FOUNDATION_EXPORT NSString *const NSFileModificationDate;
FOUNDATION_EXPORT NSString *const NSFileCreationDate;
FOUNDATION_EXPORT NSString *const NSFileOwnerAccountName;
FOUNDATION_EXPORT NSString *const NSFileGroupOwnerAccountName;
FOUNDATION_EXPORT NSString *const NSFilePosixPermissions;
FOUNDATION_EXPORT NSString *const NSFileSystemFileNumber;
FOUNDATION_EXPORT NSString *const NSFileOwnerAccountID;
FOUNDATION_EXPORT NSString *const NSFileGroupOwnerAccountID;
FOUNDATION_EXPORT NSString *const NSFileReferenceCount;
FOUNDATION_EXPORT NSString *const NSFileDeviceIdentifier;
FOUNDATION_EXPORT NSString *const NSFileSystemNumber;
FOUNDATION_EXPORT NSString *const NSFileImmutable;
FOUNDATION_EXPORT NSString *const NSFileAppendOnly;
FOUNDATION_EXPORT NSString *const NSFileExtensionHidden;
FOUNDATION_EXPORT NSString *const NSFileHFSCreatorCode;
FOUNDATION_EXPORT NSString *const NSFileHFSTypeCode;
FOUNDATION_EXPORT NSString *const NSFileSystemSize;
FOUNDATION_EXPORT NSString *const NSFileSystemFreeSize;
FOUNDATION_EXPORT NSString *const NSFileSystemNodes;
FOUNDATION_EXPORT NSString *const NSFileSystemFreeNodes;

/* Returned by -enumeratorAtPath:; walks a directory tree lazily. */
@interface NSDirectoryEnumerator : NSEnumerator {
    NSMutableArray *_stack;
    NSString *_root;
}

- (instancetype)initWithPath:(NSString *)path;

- (NSDictionary *)fileAttributes;
- (NSDictionary *)directoryAttributes;
- (void)skipDescendents;

@end

@interface NSFileManager : NSObject

+ (NSFileManager *)defaultManager;

- (BOOL)fileExistsAtPath:(NSString *)path;
- (BOOL)fileExistsAtPath:(NSString *)path isDirectory:(BOOL *)isDirectory;
- (BOOL)isReadableFileAtPath:(NSString *)path;
- (BOOL)isWritableFileAtPath:(NSString *)path;
- (BOOL)isExecutableFileAtPath:(NSString *)path;

- (NSArray<NSString *> *)contentsOfDirectoryAtPath:(NSString *)path error:(NSError **)error;
- (NSDictionary *)attributesOfItemAtPath:(NSString *)path error:(NSError **)error;
- (NSDirectoryEnumerator *)enumeratorAtPath:(NSString *)path;

- (NSData *)contentsAtPath:(NSString *)path;
- (BOOL)createFileAtPath:(NSString *)path
                contents:(NSData *)contents
              attributes:(NSDictionary *)attributes;
- (BOOL)createDirectoryAtPath:(NSString *)path
  withIntermediateDirectories:(BOOL)createIntermediates
                   attributes:(NSDictionary *)attributes
                        error:(NSError **)error;
- (BOOL)removeItemAtPath:(NSString *)path error:(NSError **)error;
- (BOOL)copyItemAtPath:(NSString *)source toPath:(NSString *)destination error:(NSError **)error;
- (BOOL)moveItemAtPath:(NSString *)source toPath:(NSString *)destination error:(NSError **)error;
- (BOOL)linkItemAtPath:(NSString *)source toPath:(NSString *)destination error:(NSError **)error;
- (BOOL)createSymbolicLinkAtPath:(NSString *)path withDestinationPath:(NSString *)destination error:(NSError **)error;
- (NSString *)destinationOfSymbolicLinkAtPath:(NSString *)path error:(NSError **)error;
- (NSDictionary *)attributesOfFileSystemForPath:(NSString *)path error:(NSError **)error;

/* Pre-10.5 spellings, still used by GNUstep-era sources. */
- (BOOL)createDirectoryAtPath:(NSString *)path attributes:(NSDictionary *)attributes;
- (BOOL)removeFileAtPath:(NSString *)path handler:(id)handler;
- (BOOL)copyPath:(NSString *)source toPath:(NSString *)destination handler:(id)handler;
- (BOOL)movePath:(NSString *)source toPath:(NSString *)destination handler:(id)handler;
- (BOOL)linkPath:(NSString *)source toPath:(NSString *)destination handler:(id)handler;
- (BOOL)changeFileAttributes:(NSDictionary *)attributes atPath:(NSString *)path;
- (NSDictionary *)fileSystemAttributesAtPath:(NSString *)path;
- (NSString *)pathContentOfSymbolicLinkAtPath:(NSString *)path;
- (BOOL)createSymbolicLinkAtPath:(NSString *)path pathContent:(NSString *)destination;

- (NSString *)currentDirectoryPath;
- (BOOL)changeCurrentDirectoryPath:(NSString *)path;

- (NSString *)stringWithFileSystemRepresentation:(const char *)string
                                           length:(NSUInteger)length;


- (NSDictionary *)fileAttributesAtPath:(NSString *)path traverseLink:(BOOL)traverse;
- (BOOL)isDeletableFileAtPath:(NSString *)path;
- (BOOL)setAttributes:(NSDictionary *)attributes ofItemAtPath:(NSString *)path
                error:(NSError **)error;
- (NSArray *)directoryContentsAtPath:(NSString *)path;

@end

#endif /* NSFileManager_h */

/* The accessors Foundation puts on an attributes dictionary. */
@interface NSDictionary (NSFileAttributes)
- (unsigned long long)fileSize;
- (NSString *)fileType;
- (NSUInteger)filePosixPermissions;
- (NSString *)fileOwnerAccountName;
- (NSString *)fileGroupOwnerAccountName;
- (NSDate *)fileModificationDate;
- (NSDate *)fileCreationDate;
- (NSUInteger)fileSystemFileNumber;
- (NSUInteger)fileSystemNumber;
- (BOOL)fileIsImmutable;
- (BOOL)fileIsAppendOnly;
- (BOOL)fileExtensionHidden;
@end

