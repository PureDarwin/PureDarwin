// GameKit.m - Game Center on a system without a Game Center service
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// players never sign in, requests fail with GKErrorNotSupported and the views say Game Center is unavailable

#import "GameKit.h"

NSString * const GKErrorDomain = @"GKErrorDomain";

// what the view controllers tell their delegates
@protocol GKPDDelegateMessages
- (void)turnBasedMatchmakerViewController:(GKTurnBasedMatchmakerViewController *)viewController didFailWithError:(NSError *)error;
- (void)turnBasedMatchmakerViewControllerWasCancelled:(GKTurnBasedMatchmakerViewController *)viewController;
- (void)gameCenterViewControllerDidFinish:(GKGameCenterViewController *)viewController;
@end

static NSError *unsupported(void) {
    return [NSError errorWithDomain:GKErrorDomain code:GKErrorNotSupported
                           userInfo:@{ NSLocalizedDescriptionKey: @"Game Center is not available on this system." }];
}

// completion handlers run on the main queue, after the call returns, as GameKit's do
static void later(void (^block)(void)) {
    dispatch_async(dispatch_get_main_queue(), block);
}

@implementation GKBasePlayer
- (NSString *)displayName { return @""; }
@end

@implementation GKPlayer
- (NSString *)playerID { return @""; }
- (NSString *)gamePlayerID { return @""; }
- (NSString *)teamPlayerID { return @""; }
- (NSString *)alias { return @""; }
@end

@implementation GKLocalPlayer {
    void (^_authenticateHandler)(NSViewController *, NSError *);
}

+ (GKLocalPlayer *)localPlayer {
    static GKLocalPlayer *shared;
    static dispatch_once_t once;

    dispatch_once(&once, ^{ shared = [[GKLocalPlayer alloc] init]; });
    return shared;
}

+ (GKLocalPlayer *)local {
    return [self localPlayer];
}

- (BOOL)isAuthenticated {
    return NO;
}

- (BOOL)isUnderage {
    return NO;
}

- (void (^)(NSViewController *, NSError *))authenticateHandler {
    return _authenticateHandler;
}

// setting the handler starts authentication, which fails at once
- (void)setAuthenticateHandler:(void (^)(NSViewController *, NSError *))handler {
    void (^old)(NSViewController *, NSError *) = _authenticateHandler;
    _authenticateHandler = [handler copy];
    [old release];
    if (handler == nil)
        return;

    void (^call)(NSViewController *, NSError *) = [handler copy];
    later(^{
        call(nil, unsupported());
        [call release];
    });
}

- (void)registerListener:(id)listener {
}

- (void)unregisterListener:(id)listener {
}

- (void)unregisterAllListeners {
}

@end

@implementation GKAchievement {
    NSString *_identifier;
    double _percentComplete;
    BOOL _showsCompletionBanner;
}

- (instancetype)initWithIdentifier:(NSString *)identifier {
    if ((self = [super init]) != nil)
        _identifier = [identifier copy];
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    if ((self = [super init]) != nil) {
        _identifier = [[coder decodeObjectForKey:@"identifier"] copy];
        _percentComplete = [coder decodeDoubleForKey:@"percentComplete"];
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)coder {
    [coder encodeObject:_identifier forKey:@"identifier"];
    [coder encodeDouble:_percentComplete forKey:@"percentComplete"];
}

- (void)dealloc {
    [_identifier release];
    [super dealloc];
}

- (NSString *)identifier { return _identifier; }
- (void)setIdentifier:(NSString *)identifier { [_identifier release]; _identifier = [identifier copy]; }
- (double)percentComplete { return _percentComplete; }
- (void)setPercentComplete:(double)value { _percentComplete = value; }
- (BOOL)isCompleted { return _percentComplete >= 100.0; }
- (NSDate *)lastReportedDate { return nil; }
- (BOOL)showsCompletionBanner { return _showsCompletionBanner; }
- (void)setShowsCompletionBanner:(BOOL)flag { _showsCompletionBanner = flag; }

+ (void)loadAchievementsWithCompletionHandler:(void (^)(NSArray *, NSError *))completionHandler {
    if (completionHandler == nil)
        return;
    void (^call)(NSArray *, NSError *) = [completionHandler copy];
    later(^{ call(nil, unsupported()); [call release]; });
}

+ (void)resetAchievementsWithCompletionHandler:(void (^)(NSError *))completionHandler {
    if (completionHandler == nil)
        return;
    void (^call)(NSError *) = [completionHandler copy];
    later(^{ call(unsupported()); [call release]; });
}

+ (void)reportAchievements:(NSArray *)achievements withCompletionHandler:(void (^)(NSError *))completionHandler {
    [self resetAchievementsWithCompletionHandler:completionHandler];
}

@end

@implementation GKMatchRequest {
    NSUInteger _minPlayers, _maxPlayers, _defaultNumberOfPlayers, _playerGroup;
    uint32_t _playerAttributes;
    NSString *_inviteMessage;
    NSArray *_recipients;
}

- (instancetype)init {
    if ((self = [super init]) != nil) {
        _minPlayers = 2;
        _maxPlayers = 2;
        _defaultNumberOfPlayers = 2;
    }
    return self;
}

- (void)dealloc {
    [_inviteMessage release];
    [_recipients release];
    [super dealloc];
}

- (NSUInteger)minPlayers { return _minPlayers; }
- (void)setMinPlayers:(NSUInteger)value { _minPlayers = value; }
- (NSUInteger)maxPlayers { return _maxPlayers; }
- (void)setMaxPlayers:(NSUInteger)value { _maxPlayers = value; }
- (NSUInteger)defaultNumberOfPlayers { return _defaultNumberOfPlayers; }
- (void)setDefaultNumberOfPlayers:(NSUInteger)value { _defaultNumberOfPlayers = value; }
- (NSString *)inviteMessage { return _inviteMessage; }
- (void)setInviteMessage:(NSString *)value { [_inviteMessage release]; _inviteMessage = [value copy]; }
- (NSArray *)recipients { return _recipients; }
- (void)setRecipients:(NSArray *)value { [_recipients release]; _recipients = [value retain]; }
- (NSUInteger)playerGroup { return _playerGroup; }
- (void)setPlayerGroup:(NSUInteger)value { _playerGroup = value; }
- (uint32_t)playerAttributes { return _playerAttributes; }
- (void)setPlayerAttributes:(uint32_t)value { _playerAttributes = value; }

@end

@implementation GKTurnBasedParticipant {
    GKTurnBasedMatchOutcome _matchOutcome;
}
- (GKPlayer *)player { return nil; }
- (NSDate *)lastTurnDate { return nil; }
- (NSInteger)status { return 0; }
- (GKTurnBasedMatchOutcome)matchOutcome { return _matchOutcome; }
- (void)setMatchOutcome:(GKTurnBasedMatchOutcome)outcome { _matchOutcome = outcome; }
- (NSDate *)timeoutDate { return nil; }
@end

// matches only ever come from Game Center, so none exist here
@implementation GKTurnBasedMatch {
    NSString *_message;
}

- (void)dealloc {
    [_message release];
    [super dealloc];
}

- (NSString *)matchID { return nil; }
- (NSDate *)creationDate { return nil; }
- (NSArray *)participants { return @[]; }
- (GKTurnBasedMatchStatus)status { return GKTurnBasedMatchStatusUnknown; }
- (GKTurnBasedParticipant *)currentParticipant { return nil; }
- (NSData *)matchData { return nil; }
- (NSString *)message { return _message; }
- (void)setMessage:(NSString *)message { [_message release]; _message = [message copy]; }

+ (void)findMatchForRequest:(GKMatchRequest *)request withCompletionHandler:(void (^)(GKTurnBasedMatch *, NSError *))completionHandler {
    [self loadMatchWithID:nil withCompletionHandler:completionHandler];
}

+ (void)loadMatchesWithCompletionHandler:(void (^)(NSArray *, NSError *))completionHandler {
    if (completionHandler == nil)
        return;
    void (^call)(NSArray *, NSError *) = [completionHandler copy];
    later(^{ call(nil, unsupported()); [call release]; });
}

+ (void)loadMatchWithID:(NSString *)matchID withCompletionHandler:(void (^)(GKTurnBasedMatch *, NSError *))completionHandler {
    if (completionHandler == nil)
        return;
    void (^call)(GKTurnBasedMatch *, NSError *) = [completionHandler copy];
    later(^{ call(nil, unsupported()); [call release]; });
}

- (void)loadMatchDataWithCompletionHandler:(void (^)(NSData *, NSError *))completionHandler {
    if (completionHandler == nil)
        return;
    void (^call)(NSData *, NSError *) = [completionHandler copy];
    later(^{ call(nil, unsupported()); [call release]; });
}

- (void)_fail:(void (^)(NSError *))completionHandler {
    if (completionHandler == nil)
        return;
    void (^call)(NSError *) = [completionHandler copy];
    later(^{ call(unsupported()); [call release]; });
}

- (void)endTurnWithNextParticipants:(NSArray *)nextParticipants turnTimeout:(NSTimeInterval)timeout matchData:(NSData *)matchData completionHandler:(void (^)(NSError *))completionHandler {
    [self _fail:completionHandler];
}

- (void)endMatchInTurnWithMatchData:(NSData *)matchData completionHandler:(void (^)(NSError *))completionHandler {
    [self _fail:completionHandler];
}

- (void)participantQuitInTurnWithOutcome:(GKTurnBasedMatchOutcome)matchOutcome nextParticipants:(NSArray *)nextParticipants turnTimeout:(NSTimeInterval)timeout matchData:(NSData *)matchData completionHandler:(void (^)(NSError *))completionHandler {
    [self _fail:completionHandler];
}

- (void)participantQuitOutOfTurnWithOutcome:(GKTurnBasedMatchOutcome)matchOutcome withCompletionHandler:(void (^)(NSError *))completionHandler {
    [self _fail:completionHandler];
}

- (void)removeWithCompletionHandler:(void (^)(NSError *))completionHandler {
    [self _fail:completionHandler];
}

- (void)saveCurrentTurnWithMatchData:(NSData *)matchData completionHandler:(void (^)(NSError *))completionHandler {
    [self _fail:completionHandler];
}

@end

// a view that says Game Center is unavailable, with a button that closes it
static NSView *unavailableView(id target, SEL action) {
    NSView *view = [[[NSView alloc] initWithFrame:NSMakeRect(0, 0, 360, 120)] autorelease];
    NSTextField *label = [[[NSTextField alloc] initWithFrame:NSMakeRect(20, 60, 320, 40)] autorelease];
    NSButton *button = [[[NSButton alloc] initWithFrame:NSMakeRect(260, 16, 80, 28)] autorelease];

    [label setStringValue:@"Game Center is not available on this system."];
    [label setEditable:NO];
    [label setBordered:NO];
    [label setDrawsBackground:NO];
    [button setTitle:@"OK"];
    [button setTarget:target];
    [button setAction:action];
    [view addSubview:label];
    [view addSubview:button];
    return view;
}

@implementation GKTurnBasedMatchmakerViewController {
    id _delegate;
    BOOL _showExistingMatches;
}

- (instancetype)initWithMatchRequest:(GKMatchRequest *)request {
    if ((self = [super initWithNibName:nil bundle:nil]) != nil)
        _showExistingMatches = YES;
    return self;
}

- (void)loadView {
    [self setView:unavailableView(self, @selector(_done:))];
}

// the matchmaker can only fail here, which its delegate hears about as an error
- (void)_done:(id)sender {
    [[GKDialogController sharedDialogController] dismiss:self];
    if ([_delegate respondsToSelector:@selector(turnBasedMatchmakerViewController:didFailWithError:)])
        [_delegate turnBasedMatchmakerViewController:self didFailWithError:unsupported()];
    else if ([_delegate respondsToSelector:@selector(turnBasedMatchmakerViewControllerWasCancelled:)])
        [_delegate turnBasedMatchmakerViewControllerWasCancelled:self];
}

- (id)turnBasedMatchmakerDelegate { return _delegate; }
- (void)setTurnBasedMatchmakerDelegate:(id)delegate { _delegate = delegate; }
- (BOOL)showExistingMatches { return _showExistingMatches; }
- (void)setShowExistingMatches:(BOOL)flag { _showExistingMatches = flag; }

@end

@implementation GKGameCenterViewController {
    id _delegate;
    GKGameCenterViewControllerState _viewState;
    NSString *_leaderboardIdentifier;
}

- (instancetype)init {
    return [self initWithNibName:nil bundle:nil];
}

- (void)dealloc {
    [_leaderboardIdentifier release];
    [super dealloc];
}

- (void)loadView {
    [self setView:unavailableView(self, @selector(_done:))];
}

- (void)_done:(id)sender {
    if ([_delegate respondsToSelector:@selector(gameCenterViewControllerDidFinish:)])
        [_delegate gameCenterViewControllerDidFinish:self];
    else
        [[GKDialogController sharedDialogController] dismiss:self];
}

- (id)gameCenterDelegate { return _delegate; }
- (void)setGameCenterDelegate:(id)delegate { _delegate = delegate; }
- (GKGameCenterViewControllerState)viewState { return _viewState; }
- (void)setViewState:(GKGameCenterViewControllerState)state { _viewState = state; }
- (NSString *)leaderboardIdentifier { return _leaderboardIdentifier; }
- (void)setLeaderboardIdentifier:(NSString *)identifier { [_leaderboardIdentifier release]; _leaderboardIdentifier = [identifier copy]; }

@end

// presents a Game Center view controller in its own panel, over the parent window
@implementation GKDialogController {
    NSWindow *_parentWindow;
    NSPanel *_panel;
    NSViewController *_presented;
}

+ (GKDialogController *)sharedDialogController {
    static GKDialogController *shared;
    static dispatch_once_t once;

    dispatch_once(&once, ^{ shared = [[GKDialogController alloc] init]; });
    return shared;
}

- (NSWindow *)parentWindow { return _parentWindow; }
- (void)setParentWindow:(NSWindow *)window { _parentWindow = window; }

- (BOOL)presentViewController:(NSViewController *)viewController {
    if (_presented != nil || viewController == nil)
        return NO;

    NSView *view = [viewController view];
    _presented = [viewController retain];
    _panel = [[NSPanel alloc] initWithContentRect:[view frame] styleMask:NSTitledWindowMask
                                          backing:NSBackingStoreBuffered defer:NO];
    [_panel setTitle:@"Game Center"];
    [_panel setContentView:view];
    [_panel makeKeyAndOrderFront:nil];
    return YES;
}

- (void)dismiss:(id)sender {
    [_panel orderOut:nil];
    [_panel release];
    _panel = nil;
    [_presented release];
    _presented = nil;
}

@end
