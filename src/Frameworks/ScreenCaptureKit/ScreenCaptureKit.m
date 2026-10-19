// ScreenCaptureKit.m - screen capture where the window server offers none
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// content requests fail as when recording permission is refused, and streams never start

#import "ScreenCaptureKit.h"

NSString * const SCStreamErrorDomain = @"com.apple.ScreenCaptureKit.SCStreamErrorDomain";

static NSError *streamError(SCStreamErrorCode code, NSString *description) {
    return [NSError errorWithDomain:SCStreamErrorDomain code:code
                           userInfo:@{ NSLocalizedDescriptionKey: description }];
}

static void later(void (^block)(void)) {
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), block);
}

@implementation SCRunningApplication
- (NSString *)bundleIdentifier { return @""; }
- (NSString *)applicationName { return @""; }
- (pid_t)processID { return 0; }
@end

@implementation SCWindow
- (CGWindowID)windowID { return 0; }
- (CGRect)frame { return CGRectZero; }
- (NSString *)title { return nil; }
- (NSInteger)windowLayer { return 0; }
- (SCRunningApplication *)owningApplication { return nil; }
- (BOOL)isOnScreen { return NO; }
- (BOOL)isActive { return NO; }
@end

@implementation SCDisplay
- (CGDirectDisplayID)displayID { return 0; }
- (NSInteger)width { return 0; }
- (NSInteger)height { return 0; }
- (CGRect)frame { return CGRectZero; }
@end

@implementation SCShareableContent

+ (void)getShareableContentWithCompletionHandler:(void (^)(SCShareableContent *, NSError *))completionHandler {
    if (completionHandler == nil)
        return;
    void (^call)(SCShareableContent *, NSError *) = [completionHandler copy];
    later(^{
        call(nil, streamError(SCStreamErrorUserDeclined, @"Screen capture is not available on this system."));
        [call release];
    });
}

+ (void)getCurrentProcessShareableContentWithCompletionHandler:(void (^)(SCShareableContent *, NSError *))completionHandler {
    [self getShareableContentWithCompletionHandler:completionHandler];
}

+ (void)getShareableContentExcludingDesktopWindows:(BOOL)excludeDesktopWindows onScreenWindowsOnly:(BOOL)onScreenWindowsOnly completionHandler:(void (^)(SCShareableContent *, NSError *))completionHandler {
    [self getShareableContentWithCompletionHandler:completionHandler];
}

- (NSArray *)windows { return @[]; }
- (NSArray *)displays { return @[]; }
- (NSArray *)applications { return @[]; }

@end

@implementation SCContentFilter
- (instancetype)initWithDesktopIndependentWindow:(SCWindow *)window { return [super init]; }
- (instancetype)initWithDisplay:(SCDisplay *)display excludingWindows:(NSArray *)excluded { return [super init]; }
- (instancetype)initWithDisplay:(SCDisplay *)display includingWindows:(NSArray *)includedWindows { return [super init]; }
- (CGRect)contentRect { return CGRectZero; }
- (float)pointPixelScale { return 1.0f; }
@end

@implementation SCStreamConfiguration {
    size_t _width, _height;
    CMTime _minimumFrameInterval;
    OSType _pixelFormat;
    BOOL _scalesToFit, _preservesAspectRatio, _showsCursor, _capturesAudio, _excludesCurrentProcessAudio;
    NSInteger _queueDepth, _sampleRate, _channelCount;
    CGRect _sourceRect, _destinationRect;
    NSString *_streamName;
}

- (instancetype)init {
    if ((self = [super init]) != nil) {
        _width = 1920;
        _height = 1080;
        _minimumFrameInterval = kCMTimeZero;
        _pixelFormat = 'BGRA';
        _queueDepth = 8;
        _showsCursor = YES;
        _preservesAspectRatio = YES;
        _sampleRate = 48000;
        _channelCount = 2;
    }
    return self;
}

- (void)dealloc {
    [_streamName release];
    [super dealloc];
}

- (size_t)width { return _width; }
- (void)setWidth:(size_t)value { _width = value; }
- (size_t)height { return _height; }
- (void)setHeight:(size_t)value { _height = value; }
- (CMTime)minimumFrameInterval { return _minimumFrameInterval; }
- (void)setMinimumFrameInterval:(CMTime)value { _minimumFrameInterval = value; }
- (OSType)pixelFormat { return _pixelFormat; }
- (void)setPixelFormat:(OSType)value { _pixelFormat = value; }
- (BOOL)scalesToFit { return _scalesToFit; }
- (void)setScalesToFit:(BOOL)value { _scalesToFit = value; }
- (BOOL)preservesAspectRatio { return _preservesAspectRatio; }
- (void)setPreservesAspectRatio:(BOOL)value { _preservesAspectRatio = value; }
- (NSInteger)queueDepth { return _queueDepth; }
- (void)setQueueDepth:(NSInteger)value { _queueDepth = value; }
- (BOOL)showsCursor { return _showsCursor; }
- (void)setShowsCursor:(BOOL)value { _showsCursor = value; }
- (BOOL)capturesAudio { return _capturesAudio; }
- (void)setCapturesAudio:(BOOL)value { _capturesAudio = value; }
- (NSInteger)sampleRate { return _sampleRate; }
- (void)setSampleRate:(NSInteger)value { _sampleRate = value; }
- (NSInteger)channelCount { return _channelCount; }
- (void)setChannelCount:(NSInteger)value { _channelCount = value; }
- (BOOL)excludesCurrentProcessAudio { return _excludesCurrentProcessAudio; }
- (void)setExcludesCurrentProcessAudio:(BOOL)value { _excludesCurrentProcessAudio = value; }
- (CGRect)sourceRect { return _sourceRect; }
- (void)setSourceRect:(CGRect)value { _sourceRect = value; }
- (CGRect)destinationRect { return _destinationRect; }
- (void)setDestinationRect:(CGRect)value { _destinationRect = value; }
- (NSString *)streamName { return _streamName; }
- (void)setStreamName:(NSString *)value { [_streamName release]; _streamName = [value copy]; }

@end

@implementation SCRecordingOutputConfiguration {
    NSURL *_outputURL;
    NSString *_videoCodecType, *_outputFileType;
}

- (void)dealloc {
    [_outputURL release];
    [_videoCodecType release];
    [_outputFileType release];
    [super dealloc];
}

- (NSURL *)outputURL { return _outputURL; }
- (void)setOutputURL:(NSURL *)value { [_outputURL release]; _outputURL = [value copy]; }
- (NSString *)videoCodecType { return _videoCodecType; }
- (void)setVideoCodecType:(NSString *)value { [_videoCodecType release]; _videoCodecType = [value copy]; }
- (NSString *)outputFileType { return _outputFileType; }
- (void)setOutputFileType:(NSString *)value { [_outputFileType release]; _outputFileType = [value copy]; }

@end

@implementation SCRecordingOutput
- (instancetype)initWithConfiguration:(SCRecordingOutputConfiguration *)recordingOutputConfiguration delegate:(id)delegate { return [super init]; }
- (CMTime)recordedDuration { return kCMTimeZero; }
- (NSInteger)recordedFileSize { return 0; }
@end

@implementation SCStream

- (instancetype)initWithFilter:(SCContentFilter *)contentFilter configuration:(SCStreamConfiguration *)streamConfig delegate:(id)delegate {
    return [super init];
}

- (BOOL)addStreamOutput:(id)output type:(NSInteger)type sampleHandlerQueue:(dispatch_queue_t)sampleHandlerQueue error:(NSError **)error {
    return YES;
}

- (BOOL)removeStreamOutput:(id)output type:(NSInteger)type error:(NSError **)error {
    return YES;
}

- (BOOL)addRecordingOutput:(SCRecordingOutput *)recordingOutput error:(NSError **)error {
    return YES;
}

- (BOOL)removeRecordingOutput:(SCRecordingOutput *)recordingOutput error:(NSError **)error {
    return YES;
}

- (void)_fail:(void (^)(NSError *))completionHandler code:(SCStreamErrorCode)code {
    if (completionHandler == nil)
        return;
    void (^call)(NSError *) = [completionHandler copy];
    later(^{
        call(streamError(code, @"Screen capture is not available on this system."));
        [call release];
    });
}

- (void)updateContentFilter:(SCContentFilter *)contentFilter completionHandler:(void (^)(NSError *))completionHandler {
    [self _fail:completionHandler code:SCStreamErrorAttemptToUpdateFilterState];
}

- (void)updateConfiguration:(SCStreamConfiguration *)streamConfig completionHandler:(void (^)(NSError *))completionHandler {
    [self _fail:completionHandler code:SCStreamErrorAttemptToConfigState];
}

- (void)startCaptureWithCompletionHandler:(void (^)(NSError *))completionHandler {
    [self _fail:completionHandler code:SCStreamErrorFailedToStart];
}

- (void)stopCaptureWithCompletionHandler:(void (^)(NSError *))completionHandler {
    [self _fail:completionHandler code:SCStreamErrorAttemptToStopStreamState];
}

@end

SCStreamFrameInfo const SCStreamFrameInfoStatus = @"SCStreamUpdateFrameStatus";
SCStreamFrameInfo const SCStreamFrameInfoDisplayTime = @"SCStreamUpdateFrameDisplayTime";
SCStreamFrameInfo const SCStreamFrameInfoScaleFactor = @"SCStreamUpdateFrameScaleFactor";
SCStreamFrameInfo const SCStreamFrameInfoContentScale = @"SCStreamUpdateFrameContentScale";
SCStreamFrameInfo const SCStreamFrameInfoContentRect = @"SCStreamUpdateFrameContentRect";
SCStreamFrameInfo const SCStreamFrameInfoBoundingRect = @"SCStreamUpdateFrameBoundingRect";
SCStreamFrameInfo const SCStreamFrameInfoScreenRect = @"SCStreamUpdateFrameScreenRect";
SCStreamFrameInfo const SCStreamFrameInfoDirtyRects = @"SCStreamUpdateFrameDirtyRects";
SCStreamFrameInfo const SCStreamFrameInfoPresenterOverlayContentRect = @"SCStreamUpdateFramePresenterOverlayContentRect";

@implementation SCContentSharingPickerConfiguration {
    SCContentSharingPickerMode _allowedPickerModes;
    NSArray *_excludedWindowIDs;
    NSArray *_excludedBundleIDs;
    BOOL _allowsChangingSelectedContent;
}

- (void)dealloc {
    [_excludedWindowIDs release];
    [_excludedBundleIDs release];
    [super dealloc];
}

- (id)copyWithZone:(NSZone *)zone {
    SCContentSharingPickerConfiguration *copy = [[SCContentSharingPickerConfiguration alloc] init];
    copy->_allowedPickerModes = _allowedPickerModes;
    copy->_excludedWindowIDs = [_excludedWindowIDs copy];
    copy->_excludedBundleIDs = [_excludedBundleIDs copy];
    copy->_allowsChangingSelectedContent = _allowsChangingSelectedContent;
    return copy;
}

- (SCContentSharingPickerMode)allowedPickerModes { return _allowedPickerModes; }
- (void)setAllowedPickerModes:(SCContentSharingPickerMode)modes { _allowedPickerModes = modes; }
- (NSArray *)excludedWindowIDs { return _excludedWindowIDs; }

- (void)setExcludedWindowIDs:(NSArray *)ids {
    [_excludedWindowIDs autorelease];
    _excludedWindowIDs = [ids copy];
}

- (NSArray *)excludedBundleIDs { return _excludedBundleIDs; }

- (void)setExcludedBundleIDs:(NSArray *)ids {
    [_excludedBundleIDs autorelease];
    _excludedBundleIDs = [ids copy];
}

- (BOOL)allowsChangingSelectedContent { return _allowsChangingSelectedContent; }
- (void)setAllowsChangingSelectedContent:(BOOL)flag { _allowsChangingSelectedContent = flag; }

@end

@interface NSObject (SCContentSharingPickerObserver)
- (void)contentSharingPickerStartDidFailWithError:(NSError *)error;
@end

// the system picker has nothing to show, so presenting it fails for every observer
@implementation SCContentSharingPicker {
    SCContentSharingPickerConfiguration *_defaultConfiguration;
    NSNumber *_maximumStreamCount;
    // observers are not retained, as on macOS
    NSMutableArray *_observers;
    BOOL _active;
}

+ (SCContentSharingPicker *)sharedPicker {
    static SCContentSharingPicker *shared;
    static dispatch_once_t once;

    dispatch_once(&once, ^{
        shared = [[SCContentSharingPicker alloc] init];
        shared->_observers = [[NSMutableArray alloc] init];
        shared->_defaultConfiguration = [[SCContentSharingPickerConfiguration alloc] init];
    });
    return shared;
}

- (SCContentSharingPickerConfiguration *)defaultConfiguration { return _defaultConfiguration; }

- (void)setDefaultConfiguration:(SCContentSharingPickerConfiguration *)configuration {
    [_defaultConfiguration autorelease];
    _defaultConfiguration = [configuration copy];
}

- (NSNumber *)maximumStreamCount { return _maximumStreamCount; }

- (void)setMaximumStreamCount:(NSNumber *)count {
    [_maximumStreamCount autorelease];
    _maximumStreamCount = [count retain];
}

- (BOOL)isActive { return _active; }
- (void)setActive:(BOOL)flag { _active = flag; }

- (void)addObserver:(id)observer {
    @synchronized(self) {
        [_observers addObject:[NSValue valueWithNonretainedObject:observer]];
    }
}

- (void)removeObserver:(id)observer {
    @synchronized(self) {
        [_observers removeObject:[NSValue valueWithNonretainedObject:observer]];
    }
}

- (void)setConfiguration:(SCContentSharingPickerConfiguration *)pickerConfig forStream:(SCStream *)stream {
}

- (void)present {
    NSArray *observers;

    @synchronized(self) {
        NSMutableArray *list = [[NSMutableArray alloc] init];
        for (NSValue *value in _observers)
            [list addObject:[value nonretainedObjectValue]];
        observers = list;
    }
    later(^{
        NSError *error = streamError(SCStreamErrorUserDeclined, @"Screen capture is not available on this system.");
        for (id observer in observers) {
            if ([observer respondsToSelector:@selector(contentSharingPickerStartDidFailWithError:)])
                [observer contentSharingPickerStartDidFailWithError:error];
        }
        [observers release];
    });
}

- (void)presentPickerUsingContentStyle:(NSInteger)contentStyle { [self present]; }
- (void)presentPickerForStream:(SCStream *)stream { [self present]; }
- (void)presentPickerForStream:(SCStream *)stream usingContentStyle:(NSInteger)contentStyle { [self present]; }

@end
