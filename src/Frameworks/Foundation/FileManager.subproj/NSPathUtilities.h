/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSPathUtilities_h
#define NSPathUtilities_h

#import <Foundation/NSObjCRuntime.h>
#import <Foundation/NSString.h>

#import <Foundation/NSArray.h>

FOUNDATION_EXPORT NSString *NSUserName(void);
FOUNDATION_EXPORT NSString *NSFullUserName(void);
FOUNDATION_EXPORT NSString *NSHomeDirectory(void);
FOUNDATION_EXPORT NSString *NSHomeDirectoryForUser(NSString *userName);
FOUNDATION_EXPORT NSString *NSTemporaryDirectory(void);
FOUNDATION_EXPORT NSString *NSOpenStepRootDirectory(void);

typedef NS_ENUM(NSUInteger, NSSearchPathDirectory) {
    NSApplicationDirectory = 1,
    NSDemoApplicationDirectory,
    NSDeveloperApplicationDirectory,
    NSAdminApplicationDirectory,
    NSLibraryDirectory,
    NSDeveloperDirectory,
    NSUserDirectory,
    NSDocumentationDirectory,
    NSDocumentDirectory,
    NSCoreServiceDirectory,
    NSAutosavedInformationDirectory = 11,
    NSDesktopDirectory = 12,
    NSCachesDirectory = 13,
    NSApplicationSupportDirectory = 14,
    NSDownloadsDirectory = 15,
    NSInputMethodsDirectory = 16,
    NSMoviesDirectory = 17,
    NSMusicDirectory = 18,
    NSPicturesDirectory = 19,
    NSPrinterDescriptionDirectory = 20,
    NSSharedPublicDirectory = 21,
    NSPreferencePanesDirectory = 22,
    NSApplicationScriptsDirectory = 23,
    NSItemReplacementDirectory = 99,
    NSAllApplicationsDirectory = 100,
    NSAllLibrariesDirectory = 101,
    NSTrashDirectory = 102
};

typedef NS_OPTIONS(NSUInteger, NSSearchPathDomainMask) {
    NSUserDomainMask = 1,
    NSLocalDomainMask = 2,
    NSNetworkDomainMask = 4,
    NSSystemDomainMask = 8,
    NSAllDomainsMask = 0x0ffff
};

FOUNDATION_EXPORT NSArray<NSString *> *NSSearchPathForDirectoriesInDomains(
    NSSearchPathDirectory directory, NSSearchPathDomainMask domainMask,
    BOOL expandTilde);

@interface NSString (NSPathUtilities)

- (NSString *)lastPathComponent;
- (NSString *)stringByDeletingLastPathComponent;
- (NSString *)pathExtension;
- (NSString *)stringByDeletingPathExtension;
- (NSString *)stringByAppendingPathComponent:(NSString *)component;
- (NSString *)stringByAppendingPathExtension:(NSString *)extension;
- (NSArray<NSString *> *)pathComponents;
- (BOOL)isAbsolutePath;
- (NSString *)stringByStandardizingPath;
- (NSString *)stringByResolvingSymlinksInPath;
- (NSString *)stringByExpandingTildeInPath;
- (NSString *)stringByAbbreviatingWithTildeInPath;

@end

#endif /* NSPathUtilities_h */
