// NSUserActivity.m - what the user is doing, for Handoff and search
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// neither runs here, so an activity holds its state and becoming current only marks it
#import <Foundation/NSUserActivity.h>
#import <Foundation/NSString.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSSet.h>
#import <Foundation/NSURL.h>
#import <Foundation/NSDate.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSBundle.h>
#import <Foundation/NSException.h>

NSString *const NSUserActivityTypeBrowsingWeb = @"NSUserActivityTypeBrowsingWeb";

static NSUserActivity *currentActivity;

#define COPY_SETTER(setter, ivar, type) \
    - (void)setter(type)value { \
        type old = ivar; \
        ivar = [value copy]; \
        [old release]; \
    }

@implementation NSUserActivity

- (instancetype)initWithActivityType:(NSString *)activityType {
    if ((self = [super init]) != nil) {
        _activityType = [activityType copy];
        _userInfo = [[NSMutableDictionary alloc] init];
        _eligibleForHandoff = YES;
        _valid = YES;
    }
    return self;
}

- (instancetype)init {
    NSString *type = [[[NSBundle mainBundle] infoDictionary] objectForKey:@"NSUserActivityTypes"];

    if ([type isKindOfClass:[NSArray class]])
        type = [(NSArray *)type count] ? [(NSArray *)type objectAtIndex:0] : nil;
    return [self initWithActivityType:[type isKindOfClass:[NSString class]] ? type : @""];
}

- (void)dealloc {
    if (currentActivity == self)
        currentActivity = nil;
    [_activityType release];
    [_title release];
    [_userInfo release];
    [_requiredUserInfoKeys release];
    [_webpageURL release];
    [_referrerURL release];
    [_expirationDate release];
    [_keywords release];
    [_persistentIdentifier release];
    [super dealloc];
}

- (NSString *)activityType { return _activityType; }
- (NSString *)title { return _title; }
COPY_SETTER(setTitle:, _title, NSString *)
- (NSDictionary *)userInfo { return [[_userInfo copy] autorelease]; }

- (void)setUserInfo:(NSDictionary *)userInfo {
    [_userInfo setDictionary:userInfo ? userInfo : @{}];
}

- (void)addUserInfoEntriesFromDictionary:(NSDictionary *)otherDictionary {
    [_userInfo addEntriesFromDictionary:otherDictionary];
}

- (NSSet *)requiredUserInfoKeys { return _requiredUserInfoKeys; }
COPY_SETTER(setRequiredUserInfoKeys:, _requiredUserInfoKeys, NSSet *)
- (BOOL)needsSave { return _needsSave; }
- (void)setNeedsSave:(BOOL)flag { _needsSave = flag; }
- (NSURL *)webpageURL { return _webpageURL; }

// macOS only takes http and https pages here
- (void)setWebpageURL:(NSURL *)url {
    NSString *scheme = [[url scheme] lowercaseString];

    if (url != nil && ![scheme isEqualToString:@"http"] && ![scheme isEqualToString:@"https"])
        [NSException raise:NSInvalidArgumentException format:@"%@ is not an http or https URL", url];
    NSURL *old = _webpageURL;
    _webpageURL = [url copy];
    [old release];
}

- (NSURL *)referrerURL { return _referrerURL; }
COPY_SETTER(setReferrerURL:, _referrerURL, NSURL *)
- (NSDate *)expirationDate { return _expirationDate; }
COPY_SETTER(setExpirationDate:, _expirationDate, NSDate *)
- (NSSet *)keywords { return _keywords; }
COPY_SETTER(setKeywords:, _keywords, NSSet *)
- (BOOL)supportsContinuationStreams { return _supportsContinuationStreams; }
- (void)setSupportsContinuationStreams:(BOOL)flag { _supportsContinuationStreams = flag; }
- (id)delegate { return _delegate; }
- (void)setDelegate:(id)delegate { _delegate = delegate; }
- (NSUserActivityPersistentIdentifier)persistentIdentifier { return _persistentIdentifier; }
COPY_SETTER(setPersistentIdentifier:, _persistentIdentifier, NSString *)
- (BOOL)isEligibleForHandoff { return _eligibleForHandoff; }
- (void)setEligibleForHandoff:(BOOL)flag { _eligibleForHandoff = flag; }
- (BOOL)isEligibleForSearch { return _eligibleForSearch; }
- (void)setEligibleForSearch:(BOOL)flag { _eligibleForSearch = flag; }
- (BOOL)isEligibleForPublicIndexing { return _eligibleForPublicIndexing; }
- (void)setEligibleForPublicIndexing:(BOOL)flag { _eligibleForPublicIndexing = flag; }
- (BOOL)isEligibleForPrediction { return _eligibleForPrediction; }
- (void)setEligibleForPrediction:(BOOL)flag { _eligibleForPrediction = flag; }

- (void)becomeCurrent {
    if (_valid)
        currentActivity = self;
}

- (void)resignCurrent {
    if (currentActivity == self)
        currentActivity = nil;
}

- (void)invalidate {
    [self resignCurrent];
    _valid = NO;
}

@end
