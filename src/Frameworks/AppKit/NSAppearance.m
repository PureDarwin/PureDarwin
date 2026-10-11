/*
 * NSAppearance.m - the light and dark looks a view, window or app draws with.
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */

#import <AppKit/NSAppearance.h>
#import <AppKit/NSApplication.h>
#import <AppKit/NSWindow.h>
#import <AppKit/NSView.h>
#import <Foundation/NSDistributedNotificationCenter.h>
#import <Foundation/NSNotification.h>
#import <Foundation/NSKeyedArchiver.h>
#import <CoreFoundation/CFPreferences.h>
#import <objc/runtime.h>
#include <time.h>

NSAppearanceName const NSAppearanceNameAqua = @"NSAppearanceNameAqua";
NSAppearanceName const NSAppearanceNameDarkAqua = @"NSAppearanceNameDarkAqua";
NSAppearanceName const NSAppearanceNameVibrantLight = @"NSAppearanceNameVibrantLight";
NSAppearanceName const NSAppearanceNameVibrantDark = @"NSAppearanceNameVibrantDark";
NSAppearanceName const NSAppearanceNameAccessibilityHighContrastAqua = @"NSAppearanceNameAccessibilityHighContrastAqua";
NSAppearanceName const NSAppearanceNameAccessibilityHighContrastDarkAqua = @"NSAppearanceNameAccessibilityHighContrastDarkAqua";
NSAppearanceName const NSAppearanceNameAccessibilityHighContrastVibrantLight = @"NSAppearanceNameAccessibilityHighContrastVibrantLight";
NSAppearanceName const NSAppearanceNameAccessibilityHighContrastVibrantDark = @"NSAppearanceNameAccessibilityHighContrastVibrantDark";

// the notification macOS posts when the system switches between light and dark
static NSString *const PDInterfaceThemeChanged = @"AppleInterfaceThemeChangedNotification";

static NSAppearance *currentDrawing;
static NSAppearance *systemAppearance;
static char appearanceKey;

static BOOL isDarkName(NSString *name)
{
    return [name isEqualToString:NSAppearanceNameDarkAqua] || [name isEqualToString:NSAppearanceNameVibrantDark] ||
           [name isEqualToString:NSAppearanceNameAccessibilityHighContrastDarkAqua] ||
           [name isEqualToString:NSAppearanceNameAccessibilityHighContrastVibrantDark];
}

// light or dark from the global AppleInterfaceStyle, auto follows the clock until there is a sunset to follow
static NSAppearanceName systemAppearanceName(void)
{
    CFStringRef any = kCFPreferencesAnyApplication;
    CFPropertyListRef style, automatic;
    BOOL dark = NO;

    CFPreferencesAppSynchronize(any);
    style = CFPreferencesCopyAppValue(CFSTR("AppleInterfaceStyle"), any);
    automatic = CFPreferencesCopyAppValue(CFSTR("AppleInterfaceStyleSwitchesAutomatically"), any);
    if (automatic != NULL && CFGetTypeID(automatic) == CFBooleanGetTypeID() && CFBooleanGetValue(automatic)) {
        time_t now = time(NULL);
        struct tm local;

        localtime_r(&now, &local);
        dark = local.tm_hour >= 19 || local.tm_hour < 7;
    } else if (style != NULL && CFGetTypeID(style) == CFStringGetTypeID()) {
        dark = CFStringCompare(style, CFSTR("Dark"), kCFCompareCaseInsensitive) == kCFCompareEqualTo;
    }
    if (style)
        CFRelease(style);
    if (automatic)
        CFRelease(automatic);
    return dark ? NSAppearanceNameDarkAqua : NSAppearanceNameAqua;
}

@implementation NSAppearance

+ (NSAppearance *)appearanceNamed:(NSAppearanceName)name
{
    static NSMutableDictionary *named;
    NSAppearance *appearance;

    if (name == nil)
        return nil;
    if (named == nil)
        named = [[NSMutableDictionary alloc] init];
    appearance = [named objectForKey:name];
    if (appearance == nil) {
        appearance = [[[self alloc] initWithAppearanceNamed:name bundle:nil] autorelease];
        if (appearance)
            [named setObject:appearance forKey:name];
    }
    return appearance;
}

- (instancetype)initWithAppearanceNamed:(NSAppearanceName)name bundle:(NSBundle *)bundle
{
    if ((self = [super init]) == nil)
        return nil;
    _name = [name copy];
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)coder
{
    NSString *name = [coder allowsKeyedCoding] ? [coder decodeObjectForKey:@"NSAppearanceName"] : nil;

    return [self initWithAppearanceNamed:name ?: NSAppearanceNameAqua bundle:nil];
}

- (void)encodeWithCoder:(NSCoder *)coder
{
    if ([coder allowsKeyedCoding])
        [coder encodeObject:_name forKey:@"NSAppearanceName"];
}

- (void)dealloc
{
    [_name release];
    [super dealloc];
}

- (NSAppearanceName)name
{
    return _name;
}

- (BOOL)allowsVibrancy
{
    return [_name hasPrefix:@"NSAppearanceNameVibrant"] || [_name hasPrefix:@"NSAppearanceNameAccessibilityHighContrastVibrant"];
}

- (BOOL)_isDark
{
    return isDarkName(_name);
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"<%@: %p %@>", [self class], self, _name];
}

+ (NSAppearance *)currentDrawingAppearance
{
    return currentDrawing ?: [NSApp effectiveAppearance] ?: [self appearanceNamed:systemAppearanceName()];
}

+ (NSAppearance *)currentAppearance
{
    return [self currentDrawingAppearance];
}

+ (void)setCurrentAppearance:(NSAppearance *)appearance
{
    [appearance retain];
    [currentDrawing release];
    currentDrawing = appearance;
}

- (void)performAsCurrentDrawingAppearance:(void (^)(void))block
{
    NSAppearance *saved = currentDrawing;

    currentDrawing = self;
    block();
    currentDrawing = saved;
}

// dark asks match dark names first, light asks light ones
- (NSAppearanceName)bestMatchFromAppearancesWithNames:(NSArray *)appearances
{
    BOOL dark = [self _isDark];

    if ([appearances containsObject:_name])
        return _name;
    for (NSAppearanceName name in appearances)
        if (isDarkName(name) == dark)
            return name;
    return nil;
}

@end

// views draw with the appearance they inherit set as the drawing appearance
void _NSAppearanceDrawView(NSView *view, void (^draw)(void))
{
    NSAppearance *saved = currentDrawing;

    currentDrawing = [view effectiveAppearance];
    draw();
    currentDrawing = saved;
}

@implementation NSApplication (NSAppearance)

- (void)_systemAppearanceChanged:(NSNotification *)note
{
    NSAppearance *next = [NSAppearance appearanceNamed:systemAppearanceName()];

    if (next == systemAppearance)
        return;
    [self willChangeValueForKey:@"effectiveAppearance"];
    systemAppearance = next;
    [self didChangeValueForKey:@"effectiveAppearance"];
    for (NSWindow *window in [self windows]) {
        NSView *frame = [[window contentView] superview] ?: [window contentView];

        [frame setNeedsDisplay:YES];
        for (NSView *view in [frame subviews])
            [view setNeedsDisplay:YES];
    }
}

- (NSAppearance *)appearance
{
    return objc_getAssociatedObject(self, &appearanceKey);
}

- (void)setAppearance:(NSAppearance *)appearance
{
    [self willChangeValueForKey:@"effectiveAppearance"];
    objc_setAssociatedObject(self, &appearanceKey, appearance, OBJC_ASSOCIATION_RETAIN);
    [self didChangeValueForKey:@"effectiveAppearance"];
}

- (NSAppearance *)effectiveAppearance
{
    NSAppearance *own = [self appearance];

    if (own)
        return own;
    if (systemAppearance == nil) {
        systemAppearance = [NSAppearance appearanceNamed:systemAppearanceName()];
        [[NSDistributedNotificationCenter defaultCenter] addObserver:self selector:@selector(_systemAppearanceChanged:)
                                                                name:PDInterfaceThemeChanged object:nil];
    }
    return systemAppearance;
}

@end

@implementation NSWindow (NSAppearance)

- (NSAppearance *)appearance
{
    return objc_getAssociatedObject(self, &appearanceKey);
}

- (void)setAppearance:(NSAppearance *)appearance
{
    objc_setAssociatedObject(self, &appearanceKey, appearance, OBJC_ASSOCIATION_RETAIN);
    [[[self contentView] superview] setNeedsDisplay:YES];
}

- (NSAppearance *)effectiveAppearance
{
    return [self appearance] ?: [NSApp effectiveAppearance] ?: [NSAppearance appearanceNamed:systemAppearanceName()];
}

@end

@implementation NSView (NSAppearance)

- (NSAppearance *)appearance
{
    return objc_getAssociatedObject(self, &appearanceKey);
}

- (void)setAppearance:(NSAppearance *)appearance
{
    objc_setAssociatedObject(self, &appearanceKey, appearance, OBJC_ASSOCIATION_RETAIN);
    [self setNeedsDisplay:YES];
}

- (NSAppearance *)effectiveAppearance
{
    NSView *view;

    for (view = self; view != nil; view = [view superview]) {
        NSAppearance *own = objc_getAssociatedObject(view, &appearanceKey);

        if (own)
            return own;
    }
    return [self window] ? [[self window] effectiveAppearance] : [NSApp effectiveAppearance];
}

@end
