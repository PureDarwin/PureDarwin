// UserNotifications.m - user notifications with no notification service to show them
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// authorization is denied, as when a user turns notifications off, and requests are refused

#import "UserNotifications.h"

NSString * const UNErrorDomain = @"UNErrorDomain";
NSString * const UNNotificationDefaultActionIdentifier = @"com.apple.UNNotificationDefaultActionIdentifier";
NSString * const UNNotificationDismissActionIdentifier = @"com.apple.UNNotificationDismissActionIdentifier";

static NSError *notAllowed(void) {
    return [NSError errorWithDomain:UNErrorDomain code:UNErrorCodeNotificationsNotAllowed
                           userInfo:@{ NSLocalizedDescriptionKey: @"Notifications are not allowed for this application" }];
}

// completion handlers run on a background queue, as the real framework's do
static void later(void (^block)(void)) {
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), block);
}

@implementation UNNotificationSound {
    NSString *_name;
}

+ (UNNotificationSound *)defaultSound {
    return [[[self alloc] init] autorelease];
}

+ (instancetype)soundNamed:(NSString *)name {
    UNNotificationSound *sound = [[[self alloc] init] autorelease];
    sound->_name = [name copy];
    return sound;
}

- (void)dealloc {
    [_name release];
    [super dealloc];
}

- (id)copyWithZone:(NSZone *)zone {
    return [self retain];
}

@end

@interface UNNotificationContent () {
@protected
    NSString *_title, *_subtitle, *_body, *_threadIdentifier, *_categoryIdentifier;
    NSNumber *_badge;
    UNNotificationSound *_sound;
    NSDictionary *_userInfo;
}
@end

@implementation UNNotificationContent

- (void)dealloc {
    [_title release];
    [_subtitle release];
    [_body release];
    [_threadIdentifier release];
    [_categoryIdentifier release];
    [_badge release];
    [_sound release];
    [_userInfo release];
    [super dealloc];
}

- (id)_copyAsClass:(Class)class {
    UNNotificationContent *copy = [[class alloc] init];

    copy->_title = [_title copy];
    copy->_subtitle = [_subtitle copy];
    copy->_body = [_body copy];
    copy->_threadIdentifier = [_threadIdentifier copy];
    copy->_categoryIdentifier = [_categoryIdentifier copy];
    copy->_badge = [_badge copy];
    copy->_sound = [_sound copy];
    copy->_userInfo = [_userInfo copy];
    return copy;
}

- (id)copyWithZone:(NSZone *)zone {
    return [self _copyAsClass:[UNNotificationContent class]];
}

- (id)mutableCopyWithZone:(NSZone *)zone {
    return [self _copyAsClass:[UNMutableNotificationContent class]];
}

- (NSString *)title { return _title ? _title : @""; }
- (NSString *)subtitle { return _subtitle ? _subtitle : @""; }
- (NSString *)body { return _body ? _body : @""; }
- (NSNumber *)badge { return _badge; }
- (UNNotificationSound *)sound { return _sound; }
- (NSDictionary *)userInfo { return _userInfo ? _userInfo : @{}; }
- (NSString *)threadIdentifier { return _threadIdentifier ? _threadIdentifier : @""; }
- (NSString *)categoryIdentifier { return _categoryIdentifier ? _categoryIdentifier : @""; }

@end

#define SETTER(name, ivar, type) - (void)name:(type)value { type old = ivar; ivar = [value copy]; [old release]; }

@implementation UNMutableNotificationContent
@dynamic title, subtitle, body, badge, sound, userInfo, threadIdentifier, categoryIdentifier;
SETTER(setTitle, _title, NSString *)
SETTER(setSubtitle, _subtitle, NSString *)
SETTER(setBody, _body, NSString *)
SETTER(setBadge, _badge, NSNumber *)
SETTER(setSound, _sound, UNNotificationSound *)
SETTER(setUserInfo, _userInfo, NSDictionary *)
SETTER(setThreadIdentifier, _threadIdentifier, NSString *)
SETTER(setCategoryIdentifier, _categoryIdentifier, NSString *)
@end

@implementation UNNotificationTrigger
- (BOOL)repeats { return NO; }
- (id)copyWithZone:(NSZone *)zone { return [self retain]; }
@end

@implementation UNTimeIntervalNotificationTrigger {
    NSTimeInterval _interval;
    BOOL _repeats;
}

+ (instancetype)triggerWithTimeInterval:(NSTimeInterval)timeInterval repeats:(BOOL)repeats {
    UNTimeIntervalNotificationTrigger *trigger = [[[self alloc] init] autorelease];
    trigger->_interval = timeInterval;
    trigger->_repeats = repeats;
    return trigger;
}

- (NSTimeInterval)timeInterval { return _interval; }
- (BOOL)repeats { return _repeats; }

@end

@implementation UNNotificationRequest {
    NSString *_identifier;
    UNNotificationContent *_content;
    UNNotificationTrigger *_trigger;
}

+ (instancetype)requestWithIdentifier:(NSString *)identifier content:(UNNotificationContent *)content trigger:(UNNotificationTrigger *)trigger {
    UNNotificationRequest *request = [[[self alloc] init] autorelease];
    request->_identifier = [identifier copy];
    request->_content = [content copy];
    request->_trigger = [trigger copy];
    return request;
}

- (void)dealloc {
    [_identifier release];
    [_content release];
    [_trigger release];
    [super dealloc];
}

- (id)copyWithZone:(NSZone *)zone { return [self retain]; }
- (NSString *)identifier { return _identifier; }
- (UNNotificationContent *)content { return _content; }
- (UNNotificationTrigger *)trigger { return _trigger; }

@end

@implementation UNNotificationSettings
- (UNAuthorizationStatus)authorizationStatus { return UNAuthorizationStatusDenied; }
- (UNNotificationSetting)soundSetting { return UNNotificationSettingNotSupported; }
- (UNNotificationSetting)badgeSetting { return UNNotificationSettingNotSupported; }
- (UNNotificationSetting)alertSetting { return UNNotificationSettingNotSupported; }
- (UNNotificationSetting)notificationCenterSetting { return UNNotificationSettingNotSupported; }
@end

@implementation UNUserNotificationCenter {
    id _delegate;
}

+ (UNUserNotificationCenter *)currentNotificationCenter {
    static UNUserNotificationCenter *shared;
    static dispatch_once_t once;

    dispatch_once(&once, ^{ shared = [[UNUserNotificationCenter alloc] init]; });
    return shared;
}

- (id)delegate { return _delegate; }
- (void)setDelegate:(id)delegate { _delegate = delegate; }
- (BOOL)supportsContentExtensions { return NO; }

- (void)requestAuthorizationWithOptions:(UNAuthorizationOptions)options completionHandler:(void (^)(BOOL, NSError *))completionHandler {
    if (completionHandler == nil)
        return;
    void (^call)(BOOL, NSError *) = [completionHandler copy];
    later(^{ call(NO, notAllowed()); [call release]; });
}

- (void)getNotificationSettingsWithCompletionHandler:(void (^)(UNNotificationSettings *))completionHandler {
    if (completionHandler == nil)
        return;
    void (^call)(UNNotificationSettings *) = [completionHandler copy];
    later(^{ call([[[UNNotificationSettings alloc] init] autorelease]); [call release]; });
}

- (void)addNotificationRequest:(UNNotificationRequest *)request withCompletionHandler:(void (^)(NSError *))completionHandler {
    if (completionHandler == nil)
        return;
    void (^call)(NSError *) = [completionHandler copy];
    later(^{ call(notAllowed()); [call release]; });
}

- (void)getPendingNotificationRequestsWithCompletionHandler:(void (^)(NSArray *))completionHandler {
    if (completionHandler == nil)
        return;
    void (^call)(NSArray *) = [completionHandler copy];
    later(^{ call(@[]); [call release]; });
}

- (void)getDeliveredNotificationsWithCompletionHandler:(void (^)(NSArray *))completionHandler {
    [self getPendingNotificationRequestsWithCompletionHandler:completionHandler];
}

- (void)removePendingNotificationRequestsWithIdentifiers:(NSArray *)identifiers {}
- (void)removeAllPendingNotificationRequests {}
- (void)removeDeliveredNotificationsWithIdentifiers:(NSArray *)identifiers {}
- (void)removeAllDeliveredNotifications {}
- (void)setNotificationCategories:(NSSet *)categories {}

- (void)setBadgeCount:(NSInteger)newBadgeCount withCompletionHandler:(void (^)(NSError *))completionHandler {
    [self addNotificationRequest:nil withCompletionHandler:completionHandler];
}

@end
