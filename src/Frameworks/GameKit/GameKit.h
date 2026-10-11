// GameKit.h - Game Center players, achievements and turn-based matches
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import <AppKit/AppKit.h>

FOUNDATION_EXPORT NSString * const GKErrorDomain;

typedef NS_ENUM(NSInteger, GKErrorCode) {
    GKErrorUnknown = 1,
    GKErrorCancelled = 2,
    GKErrorCommunicationsFailure = 3,
    GKErrorUserDenied = 4,
    GKErrorInvalidCredentials = 5,
    GKErrorNotAuthenticated = 6,
    GKErrorAuthenticationInProgress = 7,
    GKErrorInvalidPlayer = 8,
    GKErrorScoreNotSet = 9,
    GKErrorParentalControlsBlocked = 10,
    GKErrorPlayerStatusExceedsMaximumLength = 11,
    GKErrorPlayerStatusInvalid = 12,
    GKErrorMatchRequestInvalid = 13,
    GKErrorUnderage = 14,
    GKErrorGameUnrecognized = 15,
    GKErrorNotSupported = 16,
};

typedef NS_ENUM(NSInteger, GKTurnBasedMatchStatus) {
    GKTurnBasedMatchStatusUnknown = 0,
    GKTurnBasedMatchStatusOpen = 1,
    GKTurnBasedMatchStatusEnded = 2,
    GKTurnBasedMatchStatusMatching = 3
};

typedef NS_ENUM(NSInteger, GKTurnBasedMatchOutcome) {
    GKTurnBasedMatchOutcomeNone = 0,
    GKTurnBasedMatchOutcomeQuit = 1,
    GKTurnBasedMatchOutcomeWon = 2,
    GKTurnBasedMatchOutcomeLost = 3,
    GKTurnBasedMatchOutcomeTied = 4,
    GKTurnBasedMatchOutcomeTimeExpired = 5,
    GKTurnBasedMatchOutcomeFirst = 6,
    GKTurnBasedMatchOutcomeSecond = 7,
    GKTurnBasedMatchOutcomeThird = 8,
    GKTurnBasedMatchOutcomeFourth = 9,
    GKTurnBasedMatchOutcomeCustomRange = 0x00FF0000
};

typedef NS_ENUM(NSInteger, GKGameCenterViewControllerState) {
    GKGameCenterViewControllerStateDefault = -1,
    GKGameCenterViewControllerStateLeaderboards = 0,
    GKGameCenterViewControllerStateAchievements = 1,
    GKGameCenterViewControllerStateChallenges = 2,
    GKGameCenterViewControllerStateLocalPlayerProfile = 3,
    GKGameCenterViewControllerStateDashboard = 4,
    GKGameCenterViewControllerStateLocalPlayerFriendsList = 5
};

@interface GKBasePlayer : NSObject
@property (readonly, copy) NSString *displayName;
@end

@interface GKPlayer : GKBasePlayer
@property (readonly, copy) NSString *playerID;
@property (readonly, copy) NSString *gamePlayerID;
@property (readonly, copy) NSString *teamPlayerID;
@property (readonly, copy) NSString *alias;
@end

// this system has no Game Center account: the local player never authenticates
@interface GKLocalPlayer : GKPlayer
+ (GKLocalPlayer *)localPlayer;
@property (class, readonly) GKLocalPlayer *local;
@property (readonly, getter=isAuthenticated) BOOL authenticated;
@property (readonly, getter=isUnderage) BOOL underage;
@property (copy) void (^authenticateHandler)(NSViewController *viewController, NSError *error);
- (void)registerListener:(id)listener;
- (void)unregisterListener:(id)listener;
- (void)unregisterAllListeners;
@end

@interface GKAchievement : NSObject <NSCoding>
- (instancetype)initWithIdentifier:(NSString *)identifier;
@property (copy) NSString *identifier;
@property double percentComplete;
@property (readonly, getter=isCompleted) BOOL completed;
@property (readonly, retain) NSDate *lastReportedDate;
@property BOOL showsCompletionBanner;
+ (void)loadAchievementsWithCompletionHandler:(void (^)(NSArray *achievements, NSError *error))completionHandler;
+ (void)resetAchievementsWithCompletionHandler:(void (^)(NSError *error))completionHandler;
+ (void)reportAchievements:(NSArray *)achievements withCompletionHandler:(void (^)(NSError *error))completionHandler;
@end

@interface GKMatchRequest : NSObject
@property NSUInteger minPlayers;
@property NSUInteger maxPlayers;
@property NSUInteger defaultNumberOfPlayers;
@property (copy) NSString *inviteMessage;
@property (retain) NSArray *recipients;
@property NSUInteger playerGroup;
@property uint32_t playerAttributes;
@end

@interface GKTurnBasedParticipant : NSObject
@property (readonly, retain) GKPlayer *player;
@property (readonly, copy) NSDate *lastTurnDate;
@property (readonly) NSInteger status;
@property GKTurnBasedMatchOutcome matchOutcome;
@property (readonly, copy) NSDate *timeoutDate;
@end

@interface GKTurnBasedMatch : NSObject
@property (readonly, copy) NSString *matchID;
@property (readonly, copy) NSDate *creationDate;
@property (readonly, retain) NSArray *participants;
@property (readonly) GKTurnBasedMatchStatus status;
@property (readonly, retain) GKTurnBasedParticipant *currentParticipant;
@property (readonly, retain) NSData *matchData;
@property (copy) NSString *message;
+ (void)findMatchForRequest:(GKMatchRequest *)request withCompletionHandler:(void (^)(GKTurnBasedMatch *match, NSError *error))completionHandler;
+ (void)loadMatchesWithCompletionHandler:(void (^)(NSArray *matches, NSError *error))completionHandler;
+ (void)loadMatchWithID:(NSString *)matchID withCompletionHandler:(void (^)(GKTurnBasedMatch *match, NSError *error))completionHandler;
- (void)loadMatchDataWithCompletionHandler:(void (^)(NSData *matchData, NSError *error))completionHandler;
- (void)endTurnWithNextParticipants:(NSArray *)nextParticipants turnTimeout:(NSTimeInterval)timeout matchData:(NSData *)matchData completionHandler:(void (^)(NSError *error))completionHandler;
- (void)endMatchInTurnWithMatchData:(NSData *)matchData completionHandler:(void (^)(NSError *error))completionHandler;
- (void)participantQuitInTurnWithOutcome:(GKTurnBasedMatchOutcome)matchOutcome nextParticipants:(NSArray *)nextParticipants turnTimeout:(NSTimeInterval)timeout matchData:(NSData *)matchData completionHandler:(void (^)(NSError *error))completionHandler;
- (void)participantQuitOutOfTurnWithOutcome:(GKTurnBasedMatchOutcome)matchOutcome withCompletionHandler:(void (^)(NSError *error))completionHandler;
- (void)removeWithCompletionHandler:(void (^)(NSError *error))completionHandler;
- (void)saveCurrentTurnWithMatchData:(NSData *)matchData completionHandler:(void (^)(NSError *error))completionHandler;
@end

@interface GKTurnBasedMatchmakerViewController : NSViewController
- (instancetype)initWithMatchRequest:(GKMatchRequest *)request;
@property (assign) id turnBasedMatchmakerDelegate;
@property BOOL showExistingMatches;
@end

@interface GKGameCenterViewController : NSViewController
@property (assign) id gameCenterDelegate;
@property GKGameCenterViewControllerState viewState;
@property (copy) NSString *leaderboardIdentifier;
@end

@interface GKDialogController : NSResponder
+ (GKDialogController *)sharedDialogController;
@property (assign) NSWindow *parentWindow;
- (BOOL)presentViewController:(NSViewController *)viewController;
- (void)dismiss:(id)sender;
@end
