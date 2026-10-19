// NSRunningApplication.h - a running process seen as an application
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import <Foundation/NSObject.h>
#import <Foundation/NSDate.h>
#import <AppKit/AppKitExport.h>
#import <AppKit/NSApplication.h>
#include <sys/types.h>

@class NSString, NSURL, NSImage, NSArray;

typedef NS_OPTIONS(NSUInteger, NSApplicationActivationOptions) {
    NSApplicationActivateAllWindows = 1 << 0,
    NSApplicationActivateIgnoringOtherApps = 1 << 1,
};

@interface NSRunningApplication : NSObject {
    pid_t _pid;
    NSString *_bundleIdentifier;
    NSURL *_bundleURL;
    NSURL *_executableURL;
    NSString *_localizedName;
    NSDate *_launchDate;
}

@property(readonly) pid_t processIdentifier;
@property(readonly, copy) NSString *bundleIdentifier;
@property(readonly, copy) NSURL *bundleURL;
@property(readonly, copy) NSURL *executableURL;
@property(readonly, copy) NSString *localizedName;
@property(readonly, copy) NSDate *launchDate;
@property(readonly, retain) NSImage *icon;
@property(readonly) NSInteger executableArchitecture;
@property(readonly) NSApplicationActivationPolicy activationPolicy;
@property(readonly, getter=isActive) BOOL active;
@property(readonly, getter=isHidden) BOOL hidden;
@property(readonly, getter=isTerminated) BOOL terminated;
@property(readonly, getter=isFinishedLaunching) BOOL finishedLaunching;
@property(readonly) BOOL ownsMenuBar;

+ (NSRunningApplication *)currentApplication;
+ (instancetype)runningApplicationWithProcessIdentifier:(pid_t)pid;
+ (NSArray *)runningApplicationsWithBundleIdentifier:(NSString *)bundleIdentifier;
+ (void)terminateAutomaticallyTerminableApplications;

- (BOOL)activateWithOptions:(NSApplicationActivationOptions)options;
- (BOOL)activateFromApplication:(NSRunningApplication *)application options:(NSApplicationActivationOptions)options;
- (BOOL)activate;
- (BOOL)hide;
- (BOOL)unhide;
- (BOOL)terminate;
- (BOOL)forceTerminate;

@end
