// NSMenuItemBadge.m - the count or label shown at the trailing edge of a menu item
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import <AppKit/NSMenuItemBadge.h>
#import <Foundation/Foundation.h>
#import <objc/runtime.h>

@implementation NSMenuItemBadge

+ (instancetype)updatesWithCount:(NSInteger)itemCount
{
    return [[[self alloc] initWithCount:itemCount type:NSMenuItemBadgeTypeUpdates] autorelease];
}

+ (instancetype)newItemsWithCount:(NSInteger)itemCount
{
    return [[[self alloc] initWithCount:itemCount type:NSMenuItemBadgeTypeNewItems] autorelease];
}

+ (instancetype)alertsWithCount:(NSInteger)itemCount
{
    return [[[self alloc] initWithCount:itemCount type:NSMenuItemBadgeTypeAlerts] autorelease];
}

// the string is the count, followed by the word for the type as macOS spells it
- (instancetype)initWithCount:(NSInteger)itemCount type:(NSMenuItemBadgeType)type
{
    static NSString *const singular[] = { @"", @"Update", @"New Item", @"Alert" };
    static NSString *const plural[] = { @"", @"Updates", @"New Items", @"Alerts" };

    if ((self = [super init]) == nil)
        return nil;
    _itemCount = itemCount;
    _type = type;
    if (type == NSMenuItemBadgeTypeNone || type > NSMenuItemBadgeTypeAlerts)
        _stringValue = [[NSString alloc] initWithFormat:@"%ld", (long)itemCount];
    else
        _stringValue = [[NSString alloc] initWithFormat:@"%ld %@", (long)itemCount, itemCount == 1 ? singular[type] : plural[type]];
    return self;
}

- (instancetype)initWithCount:(NSInteger)itemCount
{
    return [self initWithCount:itemCount type:NSMenuItemBadgeTypeNone];
}

- (instancetype)initWithString:(NSString *)string
{
    if ((self = [super init]) == nil)
        return nil;
    _stringValue = [string copy];
    return self;
}

- (void)dealloc
{
    [_stringValue release];
    [super dealloc];
}

- (id)copyWithZone:(NSZone *)zone
{
    return [self retain];
}

- (NSInteger)itemCount
{
    return _itemCount;
}

- (NSMenuItemBadgeType)type
{
    return _type;
}

- (NSString *)stringValue
{
    return _stringValue;
}

@end

static char PDBadgeKey;

@implementation NSMenuItem (Badge)

- (NSMenuItemBadge *)badge
{
    return objc_getAssociatedObject(self, &PDBadgeKey);
}

- (void)setBadge:(NSMenuItemBadge *)badge
{
    objc_setAssociatedObject(self, &PDBadgeKey, badge, OBJC_ASSOCIATION_COPY_NONATOMIC);
    [[self menu] itemChanged:self];
}

@end
