// NSUserNotification.h - the older notification API
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// with no notification server, delivering records the notification and tells the delegate
#import <Foundation/NSObject.h>

@class NSString, NSDictionary, NSDate, NSArray, NSMutableArray, NSAttributedString, NSDateComponents, NSTimeZone;

FOUNDATION_EXPORT NSString *const NSUserNotificationDefaultSoundName;

typedef NS_ENUM(NSInteger, NSUserNotificationActivationType) {
    NSUserNotificationActivationTypeNone = 0,
    NSUserNotificationActivationTypeContentsClicked = 1,
    NSUserNotificationActivationTypeActionButtonClicked = 2,
    NSUserNotificationActivationTypeReplied = 3,
    NSUserNotificationActivationTypeAdditionalActionClicked = 4,
};

@interface NSUserNotificationAction : NSObject <NSCopying> {
    NSString *_identifier;
    NSString *_title;
}

+ (instancetype)actionWithIdentifier:(NSString *)identifier title:(NSString *)title;

@property (readonly, copy) NSString *identifier;
@property (readonly, copy) NSString *title;

@end

@interface NSUserNotification : NSObject <NSCopying> {
@package
    NSString *_title;
    NSString *_subtitle;
    NSString *_informativeText;
    NSString *_actionButtonTitle;
    NSString *_otherButtonTitle;
    NSString *_soundName;
    NSString *_identifier;
    NSString *_responsePlaceholder;
    NSDictionary *_userInfo;
    NSDate *_deliveryDate;
    NSDate *_actualDeliveryDate;
    NSArray *_additionalActions;
    NSUserNotificationAction *_additionalActivationAction;
    NSAttributedString *_response;
    id _contentImage;
    NSUserNotificationActivationType _activationType;
    BOOL _hasActionButton;
    BOOL _hasReplyButton;
    BOOL _presented;
}

@property (copy) NSString *title;
@property (copy) NSString *subtitle;
@property (copy) NSString *informativeText;
@property (copy) NSString *actionButtonTitle;
@property (copy) NSString *otherButtonTitle;
@property (copy) NSString *soundName;
@property (copy) NSString *identifier;
@property (copy) NSString *responsePlaceholder;
@property (copy) NSDictionary *userInfo;
@property (copy) NSDate *deliveryDate;
@property (readonly, copy) NSDate *actualDeliveryDate;
@property (copy) NSArray *additionalActions;
@property (readonly, copy) NSUserNotificationAction *additionalActivationAction;
@property (readonly, copy) NSAttributedString *response;
@property (copy) id contentImage;
@property (readonly) NSUserNotificationActivationType activationType;
@property BOOL hasActionButton;
@property BOOL hasReplyButton;
@property (readonly, getter=isPresented) BOOL presented;

@end

@interface NSUserNotificationCenter : NSObject {
    id _delegate;
    NSMutableArray *_delivered;
    NSMutableArray *_scheduled;
}

+ (NSUserNotificationCenter *)defaultUserNotificationCenter;

@property (assign) id delegate;
@property (copy) NSArray *scheduledNotifications;
@property (readonly, copy) NSArray *deliveredNotifications;

- (void)scheduleNotification:(NSUserNotification *)notification;
- (void)removeScheduledNotification:(NSUserNotification *)notification;
- (void)deliverNotification:(NSUserNotification *)notification;
- (void)removeDeliveredNotification:(NSUserNotification *)notification;
- (void)removeAllDeliveredNotifications;

@end
