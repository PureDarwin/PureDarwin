// ScreenCaptureKit.h - capturing windows and displays
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import <Foundation/Foundation.h>
#import <CoreGraphics/CoreGraphics.h>
#import <CoreMedia/CMTime.h>

FOUNDATION_EXPORT NSString * const SCStreamErrorDomain;

typedef NS_ENUM(NSInteger, SCStreamErrorCode) {
    SCStreamErrorUserDeclined = -3801,
    SCStreamErrorFailedToStart = -3802,
    SCStreamErrorMissingEntitlements = -3803,
    SCStreamErrorFailedApplicationConnectionInvalid = -3804,
    SCStreamErrorFailedApplicationConnectionInterrupted = -3805,
    SCStreamErrorFailedNoMatchingApplicationContext = -3806,
    SCStreamErrorAttemptToStartStreamState = -3807,
    SCStreamErrorAttemptToStopStreamState = -3808,
    SCStreamErrorAttemptToUpdateFilterState = -3809,
    SCStreamErrorAttemptToConfigState = -3810,
    SCStreamErrorInternalError = -3811,
    SCStreamErrorInvalidParameter = -3812,
    SCStreamErrorNoWindowList = -3813,
    SCStreamErrorNoDisplayList = -3814,
    SCStreamErrorNoCaptureSource = -3815,
    SCStreamErrorRemovingStream = -3816,
    SCStreamErrorUserStopped = -3817,
};

@interface SCRunningApplication : NSObject
@property (readonly) NSString *bundleIdentifier;
@property (readonly) NSString *applicationName;
@property (readonly) pid_t processID;
@end

@interface SCWindow : NSObject
@property (readonly) CGWindowID windowID;
@property (readonly) CGRect frame;
@property (readonly) NSString *title;
@property (readonly) NSInteger windowLayer;
@property (readonly) SCRunningApplication *owningApplication;
@property (readonly, getter=isOnScreen) BOOL onScreen;
@property (readonly, getter=isActive) BOOL active;
@end

@interface SCDisplay : NSObject
@property (readonly) CGDirectDisplayID displayID;
@property (readonly) NSInteger width;
@property (readonly) NSInteger height;
@property (readonly) CGRect frame;
@end

// nothing is shareable here: the request fails as when the user has declined screen recording
@interface SCShareableContent : NSObject
+ (void)getShareableContentWithCompletionHandler:(void (^)(SCShareableContent *shareableContent, NSError *error))completionHandler;
+ (void)getCurrentProcessShareableContentWithCompletionHandler:(void (^)(SCShareableContent *shareableContent, NSError *error))completionHandler;
+ (void)getShareableContentExcludingDesktopWindows:(BOOL)excludeDesktopWindows onScreenWindowsOnly:(BOOL)onScreenWindowsOnly completionHandler:(void (^)(SCShareableContent *shareableContent, NSError *error))completionHandler;
@property (readonly) NSArray *windows;
@property (readonly) NSArray *displays;
@property (readonly) NSArray *applications;
@end

@interface SCContentFilter : NSObject
- (instancetype)initWithDesktopIndependentWindow:(SCWindow *)window;
- (instancetype)initWithDisplay:(SCDisplay *)display excludingWindows:(NSArray *)excluded;
- (instancetype)initWithDisplay:(SCDisplay *)display includingWindows:(NSArray *)includedWindows;
@property (readonly) CGRect contentRect;
@property (readonly) float pointPixelScale;
@end

@interface SCStreamConfiguration : NSObject
@property size_t width;
@property size_t height;
@property CMTime minimumFrameInterval;
@property OSType pixelFormat;
@property BOOL scalesToFit;
@property BOOL preservesAspectRatio;
@property NSInteger queueDepth;
@property BOOL showsCursor;
@property BOOL capturesAudio;
@property NSInteger sampleRate;
@property NSInteger channelCount;
@property BOOL excludesCurrentProcessAudio;
@property CGRect sourceRect;
@property CGRect destinationRect;
@property (copy) NSString *streamName;
@end

@interface SCRecordingOutputConfiguration : NSObject
@property (copy) NSURL *outputURL;
@property (copy) NSString *videoCodecType;
@property (copy) NSString *outputFileType;
@end

@interface SCRecordingOutput : NSObject
- (instancetype)initWithConfiguration:(SCRecordingOutputConfiguration *)recordingOutputConfiguration delegate:(id)delegate;
@property (readonly) CMTime recordedDuration;
@property (readonly) NSInteger recordedFileSize;
@end

@interface SCStream : NSObject
- (instancetype)initWithFilter:(SCContentFilter *)contentFilter configuration:(SCStreamConfiguration *)streamConfig delegate:(id)delegate;
- (BOOL)addStreamOutput:(id)output type:(NSInteger)type sampleHandlerQueue:(dispatch_queue_t)sampleHandlerQueue error:(NSError **)error;
- (BOOL)removeStreamOutput:(id)output type:(NSInteger)type error:(NSError **)error;
- (BOOL)addRecordingOutput:(SCRecordingOutput *)recordingOutput error:(NSError **)error;
- (BOOL)removeRecordingOutput:(SCRecordingOutput *)recordingOutput error:(NSError **)error;
- (void)updateContentFilter:(SCContentFilter *)contentFilter completionHandler:(void (^)(NSError *error))completionHandler;
- (void)updateConfiguration:(SCStreamConfiguration *)streamConfig completionHandler:(void (^)(NSError *error))completionHandler;
- (void)startCaptureWithCompletionHandler:(void (^)(NSError *error))completionHandler;
- (void)stopCaptureWithCompletionHandler:(void (^)(NSError *error))completionHandler;
@end
