// NSColorSampler.m - picks a color from anywhere on screen
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import <AppKit/NSColorSampler.h>
#import <AppKit/NSColor.h>

@implementation NSColorSampler

// reading other windows' pixels needs a screen capture path the window server lacks,
// so the pick ends as if the user pressed Escape
- (void)showSamplerWithSelectionHandler:(void (^)(NSColor *selectedColor))selectionHandler {
    if (selectionHandler)
        selectionHandler(nil);
}

@end
