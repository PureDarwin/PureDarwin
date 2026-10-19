// NSUserNotification.m - the older notification API
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// with no notification server, delivering records the notification and tells the delegate
#import <Foundation/NSUserNotification.h>
#import <Foundation/NSString.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSDate.h>
#import <Foundation/NSUUID.h>
#include <dispatch/dispatch.h>

NSString *const NSUserNotificationDefaultSoundName = @"DefaultSoundName";

#define COPY_PROPERTY(getter, setter, ivar, type) \
    - (type)getter { return ivar; } \
    - (void)setter(type)value { \
        type old = ivar; \
        ivar = [value copy]; \
        [old release]; \
    }

@implementation NSUserNotificationAction

+ (instancetype)actionWithIdentifier:(NSString *)identifier title:(NSString *)title {
    NSUserNotificationAction *action = [[[self alloc] init] autorelease];

    action->_identifier = [identifier copy];
    action->_title = [title copy];
    return action;
}

- (void)dealloc {
    [_identifier release];
    [_title release];
    [super dealloc];
}

- (id)copyWithZone:(NSZone *)zone {
    return [self retain];
}

- (NSString *)identifier { return _identifier; }
- (NSString *)title { return _title; }

@end

@implementation NSUserNotification

- (instancetype)init {
    if ((self = [super init]) != nil)
        _hasActionButton = YES;
    return self;
}

- (void)dealloc {
    [_title release];
    [_subtitle release];
    [_informativeText release];
    [_actionButtonTitle release];
    [_otherButtonTitle release];
    [_soundName release];
    [_identifier release];
    [_responsePlaceholder release];
    [_userInfo release];
    [_deliveryDate release];
    [_actualDeliveryDate release];
    [_additionalActions release];
    [_additionalActivationAction release];
    [_response release];
    [_contentImage release];
    [super dealloc];
}

- (id)copyWithZone:(NSZone *)zone {
    NSUserNotification *copy = [[NSUserNotification alloc] init];

    copy->_title = [_title copy];
    copy->_subtitle = [_subtitle copy];
    copy->_informativeText = [_informativeText copy];
    copy->_actionButtonTitle = [_actionButtonTitle copy];
    copy->_otherButtonTitle = [_otherButtonTitle copy];
    copy->_soundName = [_soundName copy];
    copy->_identifier = [_identifier copy];
    copy->_responsePlaceholder = [_responsePlaceholder copy];
    copy->_userInfo = [_userInfo copy];
    copy->_deliveryDate = [_deliveryDate copy];
    copy->_actualDeliveryDate = [_actualDeliveryDate copy];
    copy->_additionalActions = [_additionalActions copy];
    copy->_additionalActivationAction = [_additionalActivationAction retain];
    copy->_response = [_response copy];
    copy->_contentImage = [_contentImage retain];
    copy->_activationType = _activationType;
    copy->_hasActionButton = _hasActionButton;
    copy->_hasReplyButton = _hasReplyButton;
    copy->_presented = _presented;
    return copy;
}

COPY_PROPERTY(title, setTitle:, _title, NSString *)
COPY_PROPERTY(subtitle, setSubtitle:, _subtitle, NSString *)
COPY_PROPERTY(informativeText, setInformativeText:, _informativeText, NSString *)
COPY_PROPERTY(actionButtonTitle, setActionButtonTitle:, _actionButtonTitle, NSString *)
COPY_PROPERTY(otherButtonTitle, setOtherButtonTitle:, _otherButtonTitle, NSString *)
COPY_PROPERTY(soundName, setSoundName:, _soundName, NSString *)
COPY_PROPERTY(identifier, setIdentifier:, _identifier, NSString *)
COPY_PROPERTY(responsePlaceholder, setResponsePlaceholder:, _responsePlaceholder, NSString *)
COPY_PROPERTY(userInfo, setUserInfo:, _userInfo, NSDictionary *)
COPY_PROPERTY(deliveryDate, setDeliveryDate:, _deliveryDate, NSDate *)
COPY_PROPERTY(additionalActions, setAdditionalActions:, _additionalActions, NSArray *)

- (NSDate *)actualDeliveryDate { return _actualDeliveryDate; }
- (NSUserNotificationAction *)additionalActivationAction { return _additionalActivationAction; }
- (NSAttributedString *)response { return _response; }
- (id)contentImage { return _contentImage; }

- (void)setContentImage:(id)image {
    id old = _contentImage;
    _contentImage = [image retain];
    [old release];
}

- (NSUserNotificationActivationType)activationType { return _activationType; }
- (BOOL)hasActionButton { return _hasActionButton; }
- (void)setHasActionButton:(BOOL)flag { _hasActionButton = flag; }
- (BOOL)hasReplyButton { return _hasReplyButton; }
- (void)setHasReplyButton:(BOOL)flag { _hasReplyButton = flag; }
- (BOOL)isPresented { return _presented; }

@end

@interface NSObject (NSUserNotificationCenterDelegate)
- (void)userNotificationCenter:(NSUserNotificationCenter *)center didDeliverNotification:(NSUserNotification *)notification;
@end

@implementation NSUserNotificationCenter

+ (NSUserNotificationCenter *)defaultUserNotificationCenter {
    static NSUserNotificationCenter *center;
    static dispatch_once_t once;

    dispatch_once(&once, ^{
        center = [[NSUserNotificationCenter alloc] init];
        center->_delivered = [[NSMutableArray alloc] init];
        center->_scheduled = [[NSMutableArray alloc] init];
    });
    return center;
}

- (id)delegate { return _delegate; }
- (void)setDelegate:(id)delegate { _delegate = delegate; }

- (NSArray *)scheduledNotifications {
    @synchronized(self) {
        return [[_scheduled copy] autorelease];
    }
}

- (void)setScheduledNotifications:(NSArray *)notifications {
    @synchronized(self) {
        [_scheduled setArray:notifications ? notifications : @[]];
    }
}

- (NSArray *)deliveredNotifications {
    @synchronized(self) {
        return [[_delivered copy] autorelease];
    }
}

- (void)scheduleNotification:(NSUserNotification *)notification {
    @synchronized(self) {
        [_scheduled addObject:notification];
    }
}

- (void)removeScheduledNotification:(NSUserNotification *)notification {
    @synchronized(self) {
        [_scheduled removeObjectIdenticalTo:notification];
    }
}

// nothing shows it on screen, so it is delivered but never presented
- (void)deliverNotification:(NSUserNotification *)notification {
    [notification->_actualDeliveryDate release];
    notification->_actualDeliveryDate = [[NSDate date] retain];
    if (notification->_identifier == nil)
        notification->_identifier = [[[NSUUID UUID] UUIDString] copy];
    @synchronized(self) {
        for (NSUInteger i = 0; i < [_delivered count]; i++) {
            if ([[[_delivered objectAtIndex:i] identifier] isEqualToString:notification->_identifier]) {
                [_delivered removeObjectAtIndex:i];
                break;
            }
        }
        [_delivered addObject:notification];
    }
    if ([_delegate respondsToSelector:@selector(userNotificationCenter:didDeliverNotification:)])
        [_delegate userNotificationCenter:self didDeliverNotification:notification];
}

- (void)removeDeliveredNotification:(NSUserNotification *)notification {
    @synchronized(self) {
        [_delivered removeObjectIdenticalTo:notification];
    }
}

- (void)removeAllDeliveredNotifications {
    @synchronized(self) {
        [_delivered removeAllObjects];
    }
}

@end
