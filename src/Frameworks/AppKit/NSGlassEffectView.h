// NSGlassEffectView.h - the macOS 26 glass material behind a content view, and glass containers
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import <AppKit/NSView.h>

@class NSColor;

typedef NS_ENUM(NSInteger, NSGlassEffectViewStyle) {
    NSGlassEffectViewStyleRegular = 0,
    NSGlassEffectViewStyleClear = 1,
};

@interface NSGlassEffectView : NSView {
    NSView *_contentView;
    CGFloat _cornerRadius;
    NSColor *_tintColor;
    NSGlassEffectViewStyle _style;
}
@property(retain) NSView *contentView;
@property CGFloat cornerRadius;
@property(copy) NSColor *tintColor;
@property NSGlassEffectViewStyle style;
@end

@interface NSGlassEffectContainerView : NSView {
    NSView *_contentView;
    CGFloat _spacing;
}
@property(retain) NSView *contentView;
@property CGFloat spacing;
@end
