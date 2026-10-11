/*
 * NSAppearance.h - the light and dark looks a view, window or app draws with.
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */

#import <Foundation/NSObject.h>
#import <AppKit/AppKitExport.h>

@class NSString, NSArray, NSBundle;

typedef NSString *NSAppearanceName;

APPKIT_EXPORT NSAppearanceName const NSAppearanceNameAqua;
APPKIT_EXPORT NSAppearanceName const NSAppearanceNameDarkAqua;
APPKIT_EXPORT NSAppearanceName const NSAppearanceNameVibrantLight;
APPKIT_EXPORT NSAppearanceName const NSAppearanceNameVibrantDark;
APPKIT_EXPORT NSAppearanceName const NSAppearanceNameAccessibilityHighContrastAqua;
APPKIT_EXPORT NSAppearanceName const NSAppearanceNameAccessibilityHighContrastDarkAqua;
APPKIT_EXPORT NSAppearanceName const NSAppearanceNameAccessibilityHighContrastVibrantLight;
APPKIT_EXPORT NSAppearanceName const NSAppearanceNameAccessibilityHighContrastVibrantDark;

@interface NSAppearance : NSObject <NSCoding> {
    NSAppearanceName _name;
}

+ (NSAppearance *)appearanceNamed:(NSAppearanceName)name;
- (instancetype)initWithAppearanceNamed:(NSAppearanceName)name bundle:(NSBundle *)bundle;

@property(readonly, copy) NSAppearanceName name;
@property(readonly) BOOL allowsVibrancy;

// the appearance drawing resolves system colours against
@property(class, readonly, strong) NSAppearance *currentDrawingAppearance;
+ (NSAppearance *)currentAppearance;
+ (void)setCurrentAppearance:(NSAppearance *)appearance;
- (void)performAsCurrentDrawingAppearance:(void (^)(void))block;

- (NSAppearanceName)bestMatchFromAppearancesWithNames:(NSArray *)appearances;

@end

@protocol NSAppearanceCustomization <NSObject>
@property(strong) NSAppearance *appearance;
@property(readonly, strong) NSAppearance *effectiveAppearance;
@end

@interface NSAppearance (PureDarwin)
// YES for the dark aqua and vibrant dark families
- (BOOL)_isDark;
@end
