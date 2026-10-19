// NSWorkspaceOpenConfiguration.h - how NSWorkspace opens an application, and the modern NSWorkspace API
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import <Foundation/NSObject.h>
#import <AppKit/NSWorkspace.h>

@class NSArray, NSDictionary, NSURL, NSRunningApplication, NSError, NSScreen;

@interface NSWorkspaceOpenConfiguration : NSObject <NSCopying>

+ (instancetype)configuration;

@property BOOL promptsUserIfNeeded;
@property BOOL addsToRecentItems;
@property BOOL activates;
@property BOOL hides;
@property BOOL hidesOthers;
@property BOOL createsNewApplicationInstance;
@property BOOL allowsRunningApplicationSubstitution;
@property(copy) NSArray *arguments;
@property(copy) NSDictionary *environment;
@property(copy) id appleEvent;

@end

APPKIT_EXPORT NSString *const NSWorkspaceDesktopImageScalingKey;
APPKIT_EXPORT NSString *const NSWorkspaceDesktopImageAllowClippingKey;
APPKIT_EXPORT NSString *const NSWorkspaceDesktopImageFillColorKey;

@interface NSWorkspace (OpenConfiguration)

@property(readonly, copy) NSArray *runningApplications;
@property(readonly, retain) NSRunningApplication *frontmostApplication;
@property(readonly, retain) NSRunningApplication *menuBarOwningApplication;

@property(readonly) BOOL accessibilityDisplayShouldIncreaseContrast;
@property(readonly) BOOL accessibilityDisplayShouldDifferentiateWithoutColor;
@property(readonly) BOOL accessibilityDisplayShouldReduceTransparency;
@property(readonly) BOOL accessibilityDisplayShouldReduceMotion;
@property(readonly) BOOL accessibilityDisplayShouldInvertColors;
@property(readonly, getter=isVoiceOverEnabled) BOOL voiceOverEnabled;
@property(readonly, getter=isSwitchControlEnabled) BOOL switchControlEnabled;

- (void)openApplicationAtURL:(NSURL *)applicationURL configuration:(NSWorkspaceOpenConfiguration *)configuration
           completionHandler:(void (^)(NSRunningApplication *app, NSError *error))completionHandler;
- (void)openURL:(NSURL *)url configuration:(NSWorkspaceOpenConfiguration *)configuration
    completionHandler:(void (^)(NSRunningApplication *app, NSError *error))completionHandler;
- (void)openURLs:(NSArray *)urls withApplicationAtURL:(NSURL *)applicationURL configuration:(NSWorkspaceOpenConfiguration *)configuration
    completionHandler:(void (^)(NSRunningApplication *app, NSError *error))completionHandler;
- (NSRunningApplication *)launchApplicationAtURL:(NSURL *)url options:(NSUInteger)options configuration:(NSDictionary *)configuration error:(NSError **)error;
- (BOOL)launchAppWithBundleIdentifier:(NSString *)bundleIdentifier options:(NSUInteger)options
    additionalEventParamDescriptor:(id)descriptor launchIdentifier:(NSNumber **)identifier;
- (BOOL)openURLs:(NSArray *)urls withAppBundleIdentifier:(NSString *)bundleIdentifier options:(NSUInteger)options
    additionalEventParamDescriptor:(id)descriptor launchIdentifiers:(NSArray **)identifiers;

- (NSURL *)URLForApplicationWithBundleIdentifier:(NSString *)bundleIdentifier;
- (NSURL *)URLForApplicationToOpenURL:(NSURL *)url;
- (NSArray *)URLsForApplicationsToOpenURL:(NSURL *)url;
- (NSURL *)URLForApplicationToOpenContentType:(id)contentType;
- (void)setDefaultApplicationAtURL:(NSURL *)applicationURL toOpenContentType:(id)contentType completionHandler:(void (^)(NSError *error))completionHandler;
- (void)setDefaultApplicationAtURL:(NSURL *)applicationURL toOpenURLsWithScheme:(NSString *)urlScheme completionHandler:(void (^)(NSError *error))completionHandler;

- (void)recycleURLs:(NSArray *)urls completionHandler:(void (^)(NSDictionary *newURLs, NSError *error))handler;

- (NSURL *)desktopImageURLForScreen:(NSScreen *)screen;
- (NSDictionary *)desktopImageOptionsForScreen:(NSScreen *)screen;
- (BOOL)setDesktopImageURL:(NSURL *)url forScreen:(NSScreen *)screen options:(NSDictionary *)options error:(NSError **)error;

@end
