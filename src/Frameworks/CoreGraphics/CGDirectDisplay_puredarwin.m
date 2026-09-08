/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <CoreGraphics/CGDirectDisplay.h>
#import <CoreGraphics/CGDirectDisplay_puredarwin.h>
#import <CoreGraphics/CGColorSpace.h>
#include <stdlib.h>
#include <stdio.h>

#define PUREDARWIN_MAIN_DISPLAY_ID ((CGDirectDisplayID)1)

/* A CGDisplayModeRef is opaque, so it can simply carry the size. */
struct CGDisplayMode {
    size_t width;
    size_t height;
    int32_t retainCount;
};

static CGRect mainDisplayBounds = { { 0, 0 }, { 0, 0 } };

static CGRect currentBounds(void) {
    if (mainDisplayBounds.size.width > 0 && mainDisplayBounds.size.height > 0) {
        return mainDisplayBounds;
    }

    /* Nothing has told us the real size yet. PUREDARWIN_DISPLAY_SIZE lets a
     * headless run pick one; otherwise assume a common desktop mode so
     * geometry maths stays sane instead of dividing by zero. */
    const char *env = getenv("PUREDARWIN_DISPLAY_SIZE");
    if (env != NULL) {
        int w = 0, h = 0;

        if (sscanf(env, "%dx%d", &w, &h) == 2 && w > 0 && h > 0) {
            return CGRectMake(0, 0, w, h);
        }
    }
    return CGRectMake(0, 0, 1920, 1080);
}

void CGDirectDisplaySetMainDisplayBounds(CGRect bounds) {
    mainDisplayBounds = bounds;
}

CGDirectDisplayID CGMainDisplayID(void) {
    return PUREDARWIN_MAIN_DISPLAY_ID;
}

CGError CGGetActiveDisplayList(uint32_t maxDisplays, CGDirectDisplayID *activeDisplays,
                               uint32_t *displayCount) {
    if (displayCount != NULL) {
        *displayCount = 1;
    }
    if (activeDisplays != NULL && maxDisplays >= 1) {
        activeDisplays[0] = PUREDARWIN_MAIN_DISPLAY_ID;
    }
    return kCGErrorSuccess;
}

CGError CGGetOnlineDisplayList(uint32_t maxDisplays, CGDirectDisplayID *onlineDisplays,
                               uint32_t *displayCount) {
    return CGGetActiveDisplayList(maxDisplays, onlineDisplays, displayCount);
}

CGRect CGDisplayBounds(CGDirectDisplayID display) {
    return currentBounds();
}

size_t CGDisplayPixelsWide(CGDirectDisplayID display) {
    return (size_t)currentBounds().size.width;
}

size_t CGDisplayPixelsHigh(CGDirectDisplayID display) {
    return (size_t)currentBounds().size.height;
}

CGColorSpaceRef CGDisplayCopyColorSpace(CGDirectDisplayID display) {
    return CGColorSpaceCreateDeviceRGB();
}

CGDisplayModeRef CGDisplayCopyDisplayMode(CGDirectDisplayID display) {
    struct CGDisplayMode *mode = calloc(1, sizeof(struct CGDisplayMode));
    CGRect bounds = currentBounds();

    mode->width = (size_t)bounds.size.width;
    mode->height = (size_t)bounds.size.height;
    mode->retainCount = 1;
    return mode;
}

CGDisplayModeRef CGDisplayModeRetain(CGDisplayModeRef mode) {
    if (mode != NULL) {
        mode->retainCount++;
    }
    return mode;
}

void CGDisplayModeRelease(CGDisplayModeRef mode) {
    if (mode == NULL) {
        return;
    }
    if (--mode->retainCount <= 0) {
        free(mode);
    }
}

size_t CGDisplayModeGetWidth(CGDisplayModeRef mode) {
    return (mode != NULL) ? mode->width : 0;
}

size_t CGDisplayModeGetHeight(CGDisplayModeRef mode) {
    return (mode != NULL) ? mode->height : 0;
}

/* AppKit asks the window server for the front-to-back window order. Nothing
 * tracks global stacking yet, so each app is its own Wayland client and the
 * compositor owns the order, and this reports none. */
CFArrayRef CGSOrderedWindowNumbers(void) {
    return CFArrayCreate(kCFAllocatorDefault, NULL, 0, &kCFTypeArrayCallBacks);
}
