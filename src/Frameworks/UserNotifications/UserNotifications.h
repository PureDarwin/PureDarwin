// UserNotifications.h - local user notifications
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import <Foundation/Foundation.h>

FOUNDATION_EXPORT NSString * const UNErrorDomain;
FOUNDATION_EXPORT NSString * const UNNotificationDefaultActionIdentifier;
FOUNDATION_EXPORT NSString * const UNNotificationDismissActionIdentifier;

typedef NS_ENUM(NSInteger, UNErrorCode) {
    UNErrorCodeNotificationsNotAllowed = 1,
    UNErrorCodeAttachmentInvalidURL = 100,
    UNErrorCodeNotificationInvalidNoDate = 1400,
    UNErrorCodeNotificationInvalidNoContent = 1401,
};

typedef NS_OPTIONS(NSUInteger, UNAuthorizationOptions) {
    UNAuthorizationOptionBadge = 1 << 0,
    UNAuthorizationOptionSound = 1 << 1,
    UNAuthorizationOptionAlert = 1 << 2,
    UNAuthorizationOptionCarPlay = 1 << 3,
    UNAuthorizationOptionCriticalAlert = 1 << 4,
    UNAuthorizationOptionProvidesAppNotificationSettings = 1 << 5,
    UNAuthorizationOptionProvisional = 1 << 6,
};

typedef NS_ENUM(NSInteger, UNAuthorizationStatus) {
    UNAuthorizationStatusNotDetermined = 0,
    UNAuthorizationStatusDenied,
    UNAuthorizationStatusAuthorized,
    UNAuthorizationStatusProvisional,
};

typedef NS_ENUM(NSInteger, UNNotificationSetting) {
    UNNotificationSettingNotSupported = 0,
    UNNotificationSettingDisabled,
    UNNotificationSettingEnabled,
};

@interface UNNotificationSound : NSObject <NSCopying>
@property (class, readonly, copy) UNNotificationSound *defaultSound;
+ (instancetype)soundNamed:(NSString *)name;
@end

@interface UNNotificationContent : NSObject <NSCopying, NSMutableCopying>
@property (readonly, copy) NSString *title;
@property (readonly, copy) NSString *subtitle;
@property (readonly, copy) NSString *body;
@property (readonly, copy) NSNumber *badge;
@property (readonly, copy) UNNotificationSound *sound;
@property (readonly, copy) NSDictionary *userInfo;
@property (readonly, copy) NSString *threadIdentifier;
@property (readonly, copy) NSString *categoryIdentifier;
@end

@interface UNMutableNotificationContent : UNNotificationContent
@property (copy) NSString *title;
@property (copy) NSString *subtitle;
@property (copy) NSString *body;
@property (copy) NSNumber *badge;
@property (copy) UNNotificationSound *sound;
@property (copy) NSDictionary *userInfo;
@property (copy) NSString *threadIdentifier;
@property (copy) NSString *categoryIdentifier;
@end

@interface UNNotificationTrigger : NSObject <NSCopying>
@property (readonly) BOOL repeats;
@end

@interface UNTimeIntervalNotificationTrigger : UNNotificationTrigger
+ (instancetype)triggerWithTimeInterval:(NSTimeInterval)timeInterval repeats:(BOOL)repeats;
@property (readonly) NSTimeInterval timeInterval;
@end

@interface UNNotificationRequest : NSObject <NSCopying>
+ (instancetype)requestWithIdentifier:(NSString *)identifier content:(UNNotificationContent *)content trigger:(UNNotificationTrigger *)trigger;
@property (readonly, copy) NSString *identifier;
@property (readonly, copy) UNNotificationContent *content;
@property (readonly, copy) UNNotificationTrigger *trigger;
@end

@interface UNNotificationSettings : NSObject
@property (readonly) UNAuthorizationStatus authorizationStatus;
@property (readonly) UNNotificationSetting soundSetting;
@property (readonly) UNNotificationSetting badgeSetting;
@property (readonly) UNNotificationSetting alertSetting;
@property (readonly) UNNotificationSetting notificationCenterSetting;
@end

// there is no notification service here: authorization is denied and requests are refused
@interface UNUserNotificationCenter : NSObject
+ (UNUserNotificationCenter *)currentNotificationCenter;
@property (assign) id delegate;
@property (readonly) BOOL supportsContentExtensions;
- (void)requestAuthorizationWithOptions:(UNAuthorizationOptions)options completionHandler:(void (^)(BOOL granted, NSError *error))completionHandler;
- (void)getNotificationSettingsWithCompletionHandler:(void (^)(UNNotificationSettings *settings))completionHandler;
- (void)addNotificationRequest:(UNNotificationRequest *)request withCompletionHandler:(void (^)(NSError *error))completionHandler;
- (void)getPendingNotificationRequestsWithCompletionHandler:(void (^)(NSArray *requests))completionHandler;
- (void)removePendingNotificationRequestsWithIdentifiers:(NSArray *)identifiers;
- (void)removeAllPendingNotificationRequests;
- (void)getDeliveredNotificationsWithCompletionHandler:(void (^)(NSArray *notifications))completionHandler;
- (void)removeDeliveredNotificationsWithIdentifiers:(NSArray *)identifiers;
- (void)removeAllDeliveredNotifications;
- (void)setNotificationCategories:(NSSet *)categories;
- (void)setBadgeCount:(NSInteger)newBadgeCount withCompletionHandler:(void (^)(NSError *error))completionHandler;
@end
