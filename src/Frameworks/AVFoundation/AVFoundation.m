// AVFoundation.m - no cameras, microphones or voices, and a display layer for decoded BGRA frames
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import <Foundation/Foundation.h>
#import <QuartzCore/CALayer.h>
#include <CoreGraphics/CoreGraphics.h>
#include <CoreMedia/CMSampleBuffer.h>
#include <dispatch/dispatch.h>

typedef NSString *AVMediaType;
typedef NSString *AVCaptureDeviceType;
typedef NSString *AVCaptureSessionPreset;
typedef NSString *AVLayerVideoGravity;

NSString *const AVMediaTypeVideo = @"vide";
NSString *const AVMediaTypeAudio = @"soun";
NSString *const AVMediaTypeMuxed = @"muxx";
NSString *const AVCaptureDeviceTypeBuiltInWideAngleCamera = @"AVCaptureDeviceTypeBuiltInWideAngleCamera";
NSString *const AVCaptureDeviceTypeExternalUnknown = @"AVCaptureDeviceTypeExternalUnknown";
NSString *const AVCaptureDeviceTypeExternal = @"AVCaptureDeviceTypeExternal";
NSString *const AVCaptureDeviceTypeBuiltInMicrophone = @"AVCaptureDeviceTypeBuiltInMicrophone";
NSString *const AVCaptureDeviceTypeMicrophone = @"AVCaptureDeviceTypeMicrophone";
NSString *const AVCaptureDeviceWasConnectedNotification = @"AVCaptureDeviceWasConnectedNotification";
NSString *const AVCaptureDeviceWasDisconnectedNotification = @"AVCaptureDeviceWasDisconnectedNotification";
NSString *const AVCaptureSessionDidStartRunningNotification = @"AVCaptureSessionDidStartRunningNotification";
NSString *const AVCaptureSessionDidStopRunningNotification = @"AVCaptureSessionDidStopRunningNotification";
NSString *const AVCaptureSessionRuntimeErrorNotification = @"AVCaptureSessionRuntimeErrorNotification";
NSString *const AVCaptureSessionWasInterruptedNotification = @"AVCaptureSessionWasInterruptedNotification";
NSString *const AVCaptureSessionInterruptionEndedNotification = @"AVCaptureSessionInterruptionEndedNotification";
NSString *const AVCaptureSessionErrorKey = @"AVCaptureSessionErrorKey";
NSString *const AVCaptureSessionPresetHigh = @"AVCaptureSessionPresetHigh";
NSString *const AVCaptureSessionPresetMedium = @"AVCaptureSessionPresetMedium";
NSString *const AVCaptureSessionPresetLow = @"AVCaptureSessionPresetLow";
NSString *const AVCaptureSessionPreset352x288 = @"AVCaptureSessionPreset352x288";
NSString *const AVCaptureSessionPreset640x480 = @"AVCaptureSessionPreset640x480";
NSString *const AVCaptureSessionPreset1280x720 = @"AVCaptureSessionPreset1280x720";
NSString *const AVCaptureSessionPreset1920x1080 = @"AVCaptureSessionPreset1920x1080";
NSString *const AVLayerVideoGravityResize = @"AVLayerVideoGravityResize";
NSString *const AVLayerVideoGravityResizeAspect = @"AVLayerVideoGravityResizeAspect";
NSString *const AVLayerVideoGravityResizeAspectFill = @"AVLayerVideoGravityResizeAspectFill";
NSString *const AVFoundationErrorDomain = @"AVFoundationErrorDomain";
NSString *const AVSpeechSynthesisVoiceIdentifierAlex = @"com.apple.speech.synthesis.voice.Alex";

enum {
    AVErrorUnknown = -11800,
    AVErrorDeviceNotConnected = -11814,
    AVErrorFileFormatNotRecognized = -11828,
    AVErrorDecoderNotFound = -11833,
    AVErrorNoSourceTrack = -11869,
};

typedef NS_ENUM(NSInteger, AVAuthorizationStatus) {
    AVAuthorizationStatusNotDetermined = 0,
    AVAuthorizationStatusRestricted = 1,
    AVAuthorizationStatusDenied = 2,
    AVAuthorizationStatusAuthorized = 3,
};

static NSError *avError(NSInteger code, NSString *reason)
{
    return [NSError errorWithDomain:AVFoundationErrorDomain code:code
                           userInfo:@{ NSLocalizedFailureReasonErrorKey : reason }];
}

// capture: devices come from the system, which has no camera or microphone drivers
@interface AVCaptureDevice : NSObject
@end

@implementation AVCaptureDevice

+ (NSArray *)devices { return @[]; }
+ (NSArray *)devicesWithMediaType:(AVMediaType)mediaType { return @[]; }
+ (AVCaptureDevice *)defaultDeviceWithMediaType:(AVMediaType)mediaType { return nil; }
+ (AVCaptureDevice *)deviceWithUniqueID:(NSString *)deviceUniqueID { return nil; }
+ (AVCaptureDevice *)defaultDeviceWithDeviceType:(AVCaptureDeviceType)deviceType mediaType:(AVMediaType)mediaType position:(NSInteger)position
{
    return nil;
}

// nothing could be captured, so access is granted: a prompt would protect no device
+ (AVAuthorizationStatus)authorizationStatusForMediaType:(AVMediaType)mediaType { return AVAuthorizationStatusAuthorized; }

+ (void)requestAccessForMediaType:(AVMediaType)mediaType completionHandler:(void (^)(BOOL granted))handler
{
    if (handler == nil)
        return;
    void (^copy)(BOOL) = [handler copy];
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        copy(YES);
        [copy release];
    });
}

@end

@interface AVCaptureDeviceDiscoverySession : NSObject
@end

@implementation AVCaptureDeviceDiscoverySession

+ (instancetype)discoverySessionWithDeviceTypes:(NSArray *)deviceTypes mediaType:(AVMediaType)mediaType position:(NSInteger)position
{
    return [[[self alloc] init] autorelease];
}

- (NSArray *)devices { return @[]; }
- (NSArray *)supportedMultiCamDeviceSets { return @[]; }

@end

@interface AVCaptureInput : NSObject
@end

@implementation AVCaptureInput
- (NSArray *)ports { return @[]; }
@end

@interface AVCaptureDeviceInput : AVCaptureInput
@end

@implementation AVCaptureDeviceInput

+ (instancetype)deviceInputWithDevice:(AVCaptureDevice *)device error:(NSError **)outError
{
    return [[[self alloc] initWithDevice:device error:outError] autorelease];
}

- (instancetype)initWithDevice:(AVCaptureDevice *)device error:(NSError **)outError
{
    [self release];
    if (outError)
        *outError = avError(AVErrorDeviceNotConnected, @"There are no capture devices.");
    return nil;
}

- (AVCaptureDevice *)device { return nil; }

@end

@interface AVCaptureOutput : NSObject
@end

@implementation AVCaptureOutput
- (NSArray *)connections { return @[]; }
- (id)connectionWithMediaType:(AVMediaType)mediaType { return nil; }
@end

@interface AVCaptureVideoDataOutput : AVCaptureOutput {
    NSDictionary *_videoSettings;
    id _delegate;
    dispatch_queue_t _queue;
    BOOL _discards;
}
@end

@implementation AVCaptureVideoDataOutput

- (instancetype)init
{
    if ((self = [super init]))
        _discards = YES;
    return self;
}

- (void)dealloc
{
    [_videoSettings release];
    if (_queue)
        dispatch_release(_queue);
    [super dealloc];
}

- (NSDictionary *)videoSettings { return _videoSettings; }
- (void)setVideoSettings:(NSDictionary *)settings
{
    [_videoSettings autorelease];
    _videoSettings = [settings copy];
}

- (id)sampleBufferDelegate { return _delegate; }
- (dispatch_queue_t)sampleBufferCallbackQueue { return _queue; }
- (void)setSampleBufferDelegate:(id)delegate queue:(dispatch_queue_t)queue
{
    _delegate = delegate;
    if (queue)
        dispatch_retain(queue);
    if (_queue)
        dispatch_release(_queue);
    _queue = queue;
}

- (BOOL)alwaysDiscardsLateVideoFrames { return _discards; }
- (void)setAlwaysDiscardsLateVideoFrames:(BOOL)discards { _discards = discards; }

- (NSArray *)availableVideoCVPixelFormatTypes
{
    return @[ @(kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange), @(kCVPixelFormatType_32BGRA) ];
}

@end

@interface AVCaptureSession : NSObject {
    NSMutableArray *_inputs, *_outputs;
    NSString *_preset;
    BOOL _running;
}
@end

@implementation AVCaptureSession

- (instancetype)init
{
    if ((self = [super init])) {
        _inputs = [NSMutableArray new];
        _outputs = [NSMutableArray new];
        _preset = [@"AVCaptureSessionPresetHigh" retain];
    }
    return self;
}

- (void)dealloc
{
    [_inputs release];
    [_outputs release];
    [_preset release];
    [super dealloc];
}

- (NSArray *)inputs { return [[_inputs copy] autorelease]; }
- (NSArray *)outputs { return [[_outputs copy] autorelease]; }
// an input needs a device, and no device exists
- (BOOL)canAddInput:(AVCaptureInput *)input { return NO; }
- (void)addInput:(AVCaptureInput *)input
{
    [NSException raise:NSInvalidArgumentException format:@"-[AVCaptureSession addInput:] cannot add an input with no device"];
}
- (void)removeInput:(AVCaptureInput *)input { [_inputs removeObject:input]; }
- (BOOL)canAddOutput:(AVCaptureOutput *)output { return output != nil && ![_outputs containsObject:output]; }
- (void)addOutput:(AVCaptureOutput *)output
{
    if ([self canAddOutput:output])
        [_outputs addObject:output];
}
- (void)removeOutput:(AVCaptureOutput *)output { [_outputs removeObject:output]; }

- (BOOL)canSetSessionPreset:(AVCaptureSessionPreset)preset { return preset != nil; }
- (AVCaptureSessionPreset)sessionPreset { return _preset; }
- (void)setSessionPreset:(AVCaptureSessionPreset)preset
{
    [_preset autorelease];
    _preset = [preset copy];
}

- (void)beginConfiguration { }
- (void)commitConfiguration { }
- (BOOL)isRunning { return _running; }
- (BOOL)isInterrupted { return NO; }

// a session with no inputs runs and produces nothing, as on macOS
- (void)startRunning
{
    if (_running)
        return;
    _running = YES;
    [[NSNotificationCenter defaultCenter] postNotificationName:AVCaptureSessionDidStartRunningNotification object:self];
}

- (void)stopRunning
{
    if (!_running)
        return;
    _running = NO;
    [[NSNotificationCenter defaultCenter] postNotificationName:AVCaptureSessionDidStopRunningNotification object:self];
}

@end

// speech: there is no synthesizer engine, so no voices and nothing is ever spoken
@interface AVSpeechSynthesisVoice : NSObject
@end

@implementation AVSpeechSynthesisVoice
+ (NSArray *)speechVoices { return @[]; }
+ (NSString *)currentLanguageCode { return [[NSLocale currentLocale] localeIdentifier]; }
+ (instancetype)voiceWithLanguage:(NSString *)languageCode { return nil; }
+ (instancetype)voiceWithIdentifier:(NSString *)identifier { return nil; }
- (NSString *)identifier { return nil; }
- (NSString *)name { return nil; }
- (NSString *)language { return nil; }
- (NSInteger)quality { return 0; }
- (NSInteger)gender { return 0; }
- (NSUInteger)voiceTraits { return 0; }
@end

@interface AVSpeechUtterance : NSObject {
    NSString *_string;
    AVSpeechSynthesisVoice *_voice;
    float _rate, _pitch, _volume;
}
@end

@implementation AVSpeechUtterance

+ (instancetype)speechUtteranceWithString:(NSString *)string { return [[[self alloc] initWithString:string] autorelease]; }

- (instancetype)initWithString:(NSString *)string
{
    if ((self = [super init])) {
        _string = [string copy];
        _rate = 0.5f;
        _pitch = 1.0f;
        _volume = 1.0f;
    }
    return self;
}

- (void)dealloc
{
    [_string release];
    [_voice release];
    [super dealloc];
}

- (NSString *)speechString { return _string; }
- (AVSpeechSynthesisVoice *)voice { return _voice; }
- (void)setVoice:(AVSpeechSynthesisVoice *)voice
{
    [_voice autorelease];
    _voice = [voice retain];
}
- (float)rate { return _rate; }
- (void)setRate:(float)rate { _rate = rate; }
- (float)pitchMultiplier { return _pitch; }
- (void)setPitchMultiplier:(float)pitch { _pitch = pitch; }
- (float)volume { return _volume; }
- (void)setVolume:(float)volume { _volume = volume; }
- (NSTimeInterval)preUtteranceDelay { return 0; }
- (void)setPreUtteranceDelay:(NSTimeInterval)delay { }
- (NSTimeInterval)postUtteranceDelay { return 0; }
- (void)setPostUtteranceDelay:(NSTimeInterval)delay { }

@end

@interface AVSpeechSynthesizer : NSObject {
    id _delegate;
}
@end

@implementation AVSpeechSynthesizer

- (id)delegate { return _delegate; }
- (void)setDelegate:(id)delegate { _delegate = delegate; }
- (BOOL)isSpeaking { return NO; }
- (BOOL)isPaused { return NO; }

// an utterance is cancelled at once, there being nothing to speak it with
- (void)speakUtterance:(AVSpeechUtterance *)utterance
{
    if (utterance == nil)
        [NSException raise:NSInvalidArgumentException format:@"-[AVSpeechSynthesizer speakUtterance:] nil utterance"];
    [self retain];
    [utterance retain];
    dispatch_async(dispatch_get_main_queue(), ^{
        if ([_delegate respondsToSelector:@selector(speechSynthesizer:didCancelSpeechUtterance:)])
            [_delegate speechSynthesizer:self didCancelSpeechUtterance:utterance];
        [utterance release];
        [self release];
    });
}

- (BOOL)stopSpeakingAtBoundary:(NSInteger)boundary { return NO; }
- (BOOL)pauseSpeakingAtBoundary:(NSInteger)boundary { return NO; }
- (BOOL)continueSpeaking { return NO; }

@end

@interface NSObject (AVSpeechSynthesizerDelegate)
- (void)speechSynthesizer:(AVSpeechSynthesizer *)synthesizer didCancelSpeechUtterance:(AVSpeechUtterance *)utterance;
@end

// assets: there are no demuxers, so an asset has no tracks and loading its keys fails
typedef NS_ENUM(NSInteger, AVKeyValueStatus) {
    AVKeyValueStatusUnknown,
    AVKeyValueStatusLoading,
    AVKeyValueStatusLoaded,
    AVKeyValueStatusFailed,
    AVKeyValueStatusCancelled,
};

@interface AVAsset : NSObject
@end

@implementation AVAsset
- (NSArray *)tracks { return @[]; }
- (NSArray *)tracksWithMediaType:(AVMediaType)mediaType { return @[]; }
- (CMTime)duration { return kCMTimeInvalid; }
- (BOOL)isPlayable { return NO; }
- (BOOL)isReadable { return NO; }

- (AVKeyValueStatus)statusOfValueForKey:(NSString *)key error:(NSError **)outError
{
    if (outError)
        *outError = avError(AVErrorFileFormatNotRecognized, @"No media formats can be read on this system.");
    return AVKeyValueStatusFailed;
}

- (void)loadValuesAsynchronouslyForKeys:(NSArray *)keys completionHandler:(void (^)(void))handler
{
    if (handler == nil)
        return;
    void (^copy)(void) = [handler copy];
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        copy();
        [copy release];
    });
}
@end

@interface AVURLAsset : AVAsset {
    NSURL *_URL;
}
@end

@implementation AVURLAsset

+ (instancetype)URLAssetWithURL:(NSURL *)URL options:(NSDictionary *)options
{
    return [[[self alloc] initWithURL:URL options:options] autorelease];
}

- (instancetype)initWithURL:(NSURL *)URL options:(NSDictionary *)options
{
    if ((self = [super init]))
        _URL = [URL copy];
    return self;
}

- (void)dealloc
{
    [_URL release];
    [super dealloc];
}

- (NSURL *)URL { return _URL; }

@end

@interface AVAssetReader : NSObject
@end

@implementation AVAssetReader

+ (instancetype)assetReaderWithAsset:(AVAsset *)asset error:(NSError **)outError
{
    return [[[self alloc] initWithAsset:asset error:outError] autorelease];
}

- (instancetype)initWithAsset:(AVAsset *)asset error:(NSError **)outError
{
    [self release];
    if (outError)
        *outError = avError(AVErrorDecoderNotFound, @"No media formats can be read on this system.");
    return nil;
}

@end

@interface AVAssetReaderOutput : NSObject
@end

@implementation AVAssetReaderOutput
- (CMSampleBufferRef)copyNextSampleBuffer { return NULL; }
- (BOOL)alwaysCopiesSampleData { return YES; }
- (void)setAlwaysCopiesSampleData:(BOOL)copies { }
@end

@interface AVAssetReaderTrackOutput : AVAssetReaderOutput
@end

@implementation AVAssetReaderTrackOutput

+ (instancetype)assetReaderTrackOutputWithTrack:(id)track outputSettings:(NSDictionary *)settings
{
    return [[[self alloc] initWithTrack:track outputSettings:settings] autorelease];
}

- (instancetype)initWithTrack:(id)track outputSettings:(NSDictionary *)settings
{
    return [super init];
}

@end

// shows the newest enqueued frame as the layer's contents, converting BGRA pixel buffers to images
typedef NS_ENUM(NSInteger, AVQueuedSampleBufferRenderingStatus) {
    AVQueuedSampleBufferRenderingStatusUnknown,
    AVQueuedSampleBufferRenderingStatusRendering,
    AVQueuedSampleBufferRenderingStatusFailed,
};

@interface AVSampleBufferDisplayLayer : CALayer {
    CMTimebaseRef _timebase;
    NSString *_gravity;
    AVQueuedSampleBufferRenderingStatus _status;
    NSError *_error;
    BOOL _preventsCapture;
}
@end

static void releasePixels(void *info, const void *data, size_t size)
{
    CVPixelBufferUnlockBaseAddress((CVPixelBufferRef)info, kCVPixelBufferLock_ReadOnly);
    CVPixelBufferRelease((CVPixelBufferRef)info);
}

static CGImageRef createImage(CVPixelBufferRef pixels)
{
    if (CVPixelBufferGetPixelFormatType(pixels) != kCVPixelFormatType_32BGRA)
        return NULL;
    if (CVPixelBufferLockBaseAddress(pixels, kCVPixelBufferLock_ReadOnly) != kCVReturnSuccess)
        return NULL;
    size_t width = CVPixelBufferGetWidth(pixels), height = CVPixelBufferGetHeight(pixels);
    size_t stride = CVPixelBufferGetBytesPerRow(pixels);
    CGDataProviderRef provider = CGDataProviderCreateWithData(CVPixelBufferRetain(pixels), CVPixelBufferGetBaseAddress(pixels),
                                                              stride * height, releasePixels);
    CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
    CGImageRef image = CGImageCreate(width, height, 8, 32, stride, space,
                                     kCGBitmapByteOrder32Little | kCGImageAlphaPremultipliedFirst, provider, NULL, false,
                                     kCGRenderingIntentDefault);
    CGColorSpaceRelease(space);
    CGDataProviderRelease(provider);
    return image;
}

@implementation AVSampleBufferDisplayLayer

- (instancetype)init
{
    if ((self = [super init]))
        _gravity = [AVLayerVideoGravityResizeAspect retain];
    return self;
}

- (void)dealloc
{
    if (_timebase)
        CFRelease(_timebase);
    [_gravity release];
    [_error release];
    [super dealloc];
}

- (CMTimebaseRef)controlTimebase { return _timebase; }
- (void)setControlTimebase:(CMTimebaseRef)timebase
{
    if (timebase)
        CFRetain(timebase);
    if (_timebase)
        CFRelease(_timebase);
    _timebase = timebase;
}

- (AVLayerVideoGravity)videoGravity { return _gravity; }
- (void)setVideoGravity:(AVLayerVideoGravity)gravity
{
    [_gravity autorelease];
    _gravity = [gravity copy];
}

- (AVQueuedSampleBufferRenderingStatus)status { return _status; }
- (NSError *)error { return _error; }
- (BOOL)isReadyForMoreMediaData { return YES; }
- (BOOL)requiresFlushToResumeDecoding { return NO; }
- (BOOL)preventsCapture { return _preventsCapture; }
- (void)setPreventsCapture:(BOOL)prevents { _preventsCapture = prevents; }

- (void)enqueueSampleBuffer:(CMSampleBufferRef)sampleBuffer
{
    CVImageBufferRef pixels = CMSampleBufferGetImageBuffer(sampleBuffer);
    CGImageRef image = pixels ? createImage(pixels) : NULL;

    // compressed samples would need a decoder, which this system does not have
    if (image == NULL) {
        _status = AVQueuedSampleBufferRenderingStatusFailed;
        [_error release];
        _error = [avError(AVErrorDecoderNotFound, @"Only decoded BGRA frames can be displayed.") retain];
        return;
    }
    _status = AVQueuedSampleBufferRenderingStatusRendering;
    [self setContents:(id)image];
    CGImageRelease(image);
}

- (void)flush { }
- (void)flushAndRemoveImage { [self setContents:nil]; }

- (void)requestMediaDataWhenReadyOnQueue:(dispatch_queue_t)queue usingBlock:(void (^)(void))block
{
    if (queue == NULL || block == nil)
        return;
    void (^copy)(void) = [block copy];
    dispatch_async(queue, ^{
        copy();
        [copy release];
    });
}

- (void)stopRequestingMediaData { }

@end
