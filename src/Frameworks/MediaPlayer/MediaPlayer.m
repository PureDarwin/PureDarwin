// MediaPlayer.m - now-playing information and remote commands
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// nothing shows the information or sends commands yet, both are kept and the targets stay idle

#import <Foundation/Foundation.h>
#include <CoreGraphics/CoreGraphics.h>

NSString *const MPMediaItemPropertyTitle = @"title";
NSString *const MPMediaItemPropertyArtist = @"artist";
NSString *const MPMediaItemPropertyAlbumTitle = @"albumTitle";
NSString *const MPMediaItemPropertyArtwork = @"artwork";
NSString *const MPMediaItemPropertyPlaybackDuration = @"playbackDuration";
NSString *const MPNowPlayingInfoPropertyElapsedPlaybackTime = @"MPNowPlayingInfoPropertyElapsedPlaybackTime";
NSString *const MPNowPlayingInfoPropertyPlaybackRate = @"MPNowPlayingInfoPropertyPlaybackRate";
NSString *const MPNowPlayingInfoPropertyMediaType = @"MPNowPlayingInfoPropertyMediaType";

typedef NS_ENUM(NSUInteger, MPNowPlayingPlaybackState) {
    MPNowPlayingPlaybackStateUnknown = 0,
    MPNowPlayingPlaybackStatePlaying,
    MPNowPlayingPlaybackStatePaused,
    MPNowPlayingPlaybackStateStopped,
    MPNowPlayingPlaybackStateInterrupted,
};

typedef NS_ENUM(NSInteger, MPRemoteCommandHandlerStatus) {
    MPRemoteCommandHandlerStatusSuccess = 0,
    MPRemoteCommandHandlerStatusNoSuchContent = 100,
    MPRemoteCommandHandlerStatusNoActionableNowPlayingItem = 110,
    MPRemoteCommandHandlerStatusDeviceNotFound = 120,
    MPRemoteCommandHandlerStatusCommandFailed = 200,
};

@interface MPMediaItemArtwork : NSObject {
    CGRect _bounds;
    id (^_handler)(CGSize);
}
@end

@implementation MPMediaItemArtwork

- (instancetype)initWithBoundsSize:(CGSize)boundsSize requestHandler:(id (^)(CGSize size))requestHandler
{
    if ((self = [super init])) {
        _bounds = CGRectMake(0, 0, boundsSize.width, boundsSize.height);
        _handler = [requestHandler copy];
    }
    return self;
}

- (instancetype)initWithImage:(id)image
{
    return [self initWithBoundsSize:CGSizeMake(0, 0) requestHandler:^id(CGSize size) { return image; }];
}

- (void)dealloc
{
    [_handler release];
    [super dealloc];
}

- (CGRect)bounds { return _bounds; }
- (id)imageWithSize:(CGSize)size { return _handler ? _handler(size) : nil; }

@end

@interface MPNowPlayingInfoCenter : NSObject {
    NSDictionary *_info;
    MPNowPlayingPlaybackState _state;
}
@end

@implementation MPNowPlayingInfoCenter

+ (MPNowPlayingInfoCenter *)defaultCenter
{
    static MPNowPlayingInfoCenter *center;
    static dispatch_once_t once;
    dispatch_once(&once, ^{ center = [[MPNowPlayingInfoCenter alloc] init]; });
    return center;
}

- (NSDictionary *)nowPlayingInfo
{
    @synchronized(self) {
        return [[_info retain] autorelease];
    }
}

- (void)setNowPlayingInfo:(NSDictionary *)info
{
    @synchronized(self) {
        [_info autorelease];
        _info = [info copy];
    }
}

- (MPNowPlayingPlaybackState)playbackState { return _state; }
- (void)setPlaybackState:(MPNowPlayingPlaybackState)state { _state = state; }

@end

@interface MPRemoteCommandEvent : NSObject
@end

@implementation MPRemoteCommandEvent
@end

@interface MPRemoteCommand : NSObject {
    NSMutableArray *_targets;
    BOOL _enabled;
}
@end

@implementation MPRemoteCommand

- (instancetype)init
{
    if ((self = [super init])) {
        _targets = [NSMutableArray new];
        _enabled = YES;
    }
    return self;
}

- (void)dealloc
{
    [_targets release];
    [super dealloc];
}

- (BOOL)isEnabled { return _enabled; }
- (void)setEnabled:(BOOL)enabled { _enabled = enabled; }

- (void)addTarget:(id)target action:(SEL)action
{
    @synchronized(self) {
        [_targets addObject:@[ target, NSStringFromSelector(action) ]];
    }
}

- (void)removeTarget:(id)target action:(SEL)action
{
    @synchronized(self) {
        for (NSInteger i = (NSInteger)[_targets count] - 1; i >= 0; i--) {
            NSArray *t = [_targets objectAtIndex:i];
            if ([t objectAtIndex:0] == target && (action == NULL || [[t objectAtIndex:1] isEqual:NSStringFromSelector(action)]))
                [_targets removeObjectAtIndex:i];
        }
    }
}

- (void)removeTarget:(id)target
{
    @synchronized(self) {
        for (NSInteger i = (NSInteger)[_targets count] - 1; i >= 0; i--) {
            id t = [_targets objectAtIndex:i];
            if (t == target || ([t isKindOfClass:[NSArray class]] && [t objectAtIndex:0] == target))
                [_targets removeObjectAtIndex:i];
        }
    }
}

// the handler block itself is the token callers remove again
- (id)addTargetWithHandler:(MPRemoteCommandHandlerStatus (^)(MPRemoteCommandEvent *event))handler
{
    id copy = [[handler copy] autorelease];
    @synchronized(self) {
        [_targets addObject:copy];
    }
    return copy;
}

@end

@interface MPChangePlaybackPositionCommand : MPRemoteCommand
@end

@implementation MPChangePlaybackPositionCommand
@end

@interface MPSkipIntervalCommand : MPRemoteCommand {
    NSArray *_intervals;
}
@end

@implementation MPSkipIntervalCommand
- (void)dealloc
{
    [_intervals release];
    [super dealloc];
}
- (NSArray *)preferredIntervals { return _intervals; }
- (void)setPreferredIntervals:(NSArray *)intervals
{
    [_intervals autorelease];
    _intervals = [intervals copy];
}
@end

@interface MPRemoteCommandCenter : NSObject {
    NSMutableDictionary *_commands;
}
@end

@implementation MPRemoteCommandCenter

+ (MPRemoteCommandCenter *)sharedCommandCenter
{
    static MPRemoteCommandCenter *center;
    static dispatch_once_t once;
    dispatch_once(&once, ^{ center = [[MPRemoteCommandCenter alloc] init]; });
    return center;
}

- (instancetype)init
{
    if ((self = [super init]))
        _commands = [NSMutableDictionary new];
    return self;
}

- (id)command:(NSString *)name ofClass:(Class)cls
{
    @synchronized(self) {
        id c = [_commands objectForKey:name];
        if (c == nil) {
            c = [[cls alloc] init];
            [_commands setObject:c forKey:name];
            [c release];
        }
        return c;
    }
}

#define COMMAND(name, cls) \
    - (cls *)name { return [self command:@#name ofClass:[cls class]]; }
COMMAND(playCommand, MPRemoteCommand)
COMMAND(pauseCommand, MPRemoteCommand)
COMMAND(stopCommand, MPRemoteCommand)
COMMAND(togglePlayPauseCommand, MPRemoteCommand)
COMMAND(nextTrackCommand, MPRemoteCommand)
COMMAND(previousTrackCommand, MPRemoteCommand)
COMMAND(seekForwardCommand, MPRemoteCommand)
COMMAND(seekBackwardCommand, MPRemoteCommand)
COMMAND(enableLanguageOptionCommand, MPRemoteCommand)
COMMAND(disableLanguageOptionCommand, MPRemoteCommand)
COMMAND(skipForwardCommand, MPSkipIntervalCommand)
COMMAND(skipBackwardCommand, MPSkipIntervalCommand)
COMMAND(changePlaybackPositionCommand, MPChangePlaybackPositionCommand)
COMMAND(changePlaybackRateCommand, MPRemoteCommand)
COMMAND(changeRepeatModeCommand, MPRemoteCommand)
COMMAND(changeShuffleModeCommand, MPRemoteCommand)

@end
