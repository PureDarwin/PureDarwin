// NSMenuItemBadge.h - the count or label shown at the trailing edge of a menu item
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import <Foundation/NSObject.h>
#import <AppKit/NSMenuItem.h>

@class NSString;

typedef NS_ENUM(NSInteger, NSMenuItemBadgeType) {
    NSMenuItemBadgeTypeNone = 0,
    NSMenuItemBadgeTypeUpdates,
    NSMenuItemBadgeTypeNewItems,
    NSMenuItemBadgeTypeAlerts,
};

@interface NSMenuItemBadge : NSObject <NSCopying> {
    NSInteger _itemCount;
    NSMenuItemBadgeType _type;
    NSString *_stringValue;
}
+ (instancetype)updatesWithCount:(NSInteger)itemCount;
+ (instancetype)newItemsWithCount:(NSInteger)itemCount;
+ (instancetype)alertsWithCount:(NSInteger)itemCount;
- (instancetype)initWithCount:(NSInteger)itemCount type:(NSMenuItemBadgeType)type;
- (instancetype)initWithCount:(NSInteger)itemCount;
- (instancetype)initWithString:(NSString *)string;
@property(readonly) NSInteger itemCount;
@property(readonly) NSMenuItemBadgeType type;
@property(readonly, copy) NSString *stringValue;
@end

@interface NSMenuItem (Badge)
@property(copy) NSMenuItemBadge *badge;
@end
