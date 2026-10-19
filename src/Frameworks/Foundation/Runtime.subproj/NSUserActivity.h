// NSUserActivity.h - what the user is doing, for Handoff and search
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// neither runs here, so an activity holds its state and becoming current only marks it
#import <Foundation/NSObject.h>

@class NSString, NSDictionary, NSSet, NSURL, NSDate, NSMutableDictionary;

typedef NSString *NSUserActivityPersistentIdentifier;

FOUNDATION_EXPORT NSString *const NSUserActivityTypeBrowsingWeb;

@interface NSUserActivity : NSObject {
    NSString *_activityType;
    NSString *_title;
    NSMutableDictionary *_userInfo;
    NSSet *_requiredUserInfoKeys;
    NSURL *_webpageURL;
    NSURL *_referrerURL;
    NSDate *_expirationDate;
    NSSet *_keywords;
    NSString *_persistentIdentifier;
    id _delegate;
    BOOL _needsSave;
    BOOL _eligibleForHandoff;
    BOOL _eligibleForSearch;
    BOOL _eligibleForPublicIndexing;
    BOOL _eligibleForPrediction;
    BOOL _supportsContinuationStreams;
    BOOL _valid;
}

- (instancetype)initWithActivityType:(NSString *)activityType;

@property (readonly, copy) NSString *activityType;
@property (copy) NSString *title;
@property (copy) NSDictionary *userInfo;
@property (copy) NSSet *requiredUserInfoKeys;
@property BOOL needsSave;
@property (copy) NSURL *webpageURL;
@property (copy) NSURL *referrerURL;
@property (copy) NSDate *expirationDate;
@property (copy) NSSet *keywords;
@property BOOL supportsContinuationStreams;
@property (assign) id delegate;
@property (copy) NSUserActivityPersistentIdentifier persistentIdentifier;
@property (getter=isEligibleForHandoff) BOOL eligibleForHandoff;
@property (getter=isEligibleForSearch) BOOL eligibleForSearch;
@property (getter=isEligibleForPublicIndexing) BOOL eligibleForPublicIndexing;
@property (getter=isEligibleForPrediction) BOOL eligibleForPrediction;

- (void)addUserInfoEntriesFromDictionary:(NSDictionary *)otherDictionary;
- (void)becomeCurrent;
- (void)resignCurrent;
- (void)invalidate;

@end
