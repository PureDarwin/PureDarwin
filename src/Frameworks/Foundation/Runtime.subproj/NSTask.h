/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSTask_h
#define NSTask_h

#import <Foundation/NSObject.h>
#import <Foundation/NSObjCRuntime.h>

@class NSString, NSArray, NSDictionary;

FOUNDATION_EXPORT NSString * const NSTaskDidTerminateNotification;

typedef NS_ENUM(NSInteger, NSTaskTerminationReason) {
    NSTaskTerminationReasonExit = 1,
    NSTaskTerminationReasonUncaughtSignal = 2
};

@interface NSTask : NSObject {
    NSString *_launchPath;
    NSArray *_arguments;
    NSDictionary *_environment;
    NSString *_currentDirectoryPath;
    id _standardInput;
    id _standardOutput;
    id _standardError;
    int _processIdentifier;
    int _terminationStatus;
    NSTaskTerminationReason _terminationReason;
    BOOL _isRunning;
    BOOL _hasTerminated;
}

+ (NSTask *)launchedTaskWithLaunchPath:(NSString *)path arguments:(NSArray *)arguments;

- (void)setLaunchPath:(NSString *)path;
- (NSString *)launchPath;
- (void)setArguments:(NSArray *)arguments;
- (NSArray *)arguments;
- (void)setEnvironment:(NSDictionary *)environment;
- (NSDictionary *)environment;
- (void)setCurrentDirectoryPath:(NSString *)path;
- (NSString *)currentDirectoryPath;

- (void)setStandardInput:(id)input;
- (id)standardInput;
- (void)setStandardOutput:(id)output;
- (id)standardOutput;
- (void)setStandardError:(id)error;
- (id)standardError;

- (void)launch;
- (void)interrupt;
- (void)terminate;
- (BOOL)suspend;
- (BOOL)resume;

- (int)processIdentifier;
- (BOOL)isRunning;
- (int)terminationStatus;
- (NSTaskTerminationReason)terminationReason;

- (void)waitUntilExit;

@end

#endif /* NSTask_h */
