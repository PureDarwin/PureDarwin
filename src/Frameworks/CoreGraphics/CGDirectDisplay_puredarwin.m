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
#include <string.h>

#define PUREDARWIN_MAIN_DISPLAY_ID ((CGDirectDisplayID)1)

/* A CGDisplayModeRef is opaque, so it can simply carry the size. */
struct CGDisplayMode {
    size_t width;
    size_t height;
    int32_t retainCount;
    double refresh;
    int32_t modeID;
};

static const struct CGDirectDisplayModeProvider *modeProvider;

struct reconfigurationCallback {
    CGDisplayReconfigurationCallBack callback;
    void *userInfo;
};

static struct reconfigurationCallback reconfigurationCallbacks[16];
static int reconfigurationCallbackCount;

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

static void notifyReconfiguration(CGDisplayChangeSummaryFlags flags) {
    // copied so a callback can remove itself while we walk the list
    struct reconfigurationCallback copy[16];
    int count = reconfigurationCallbackCount;

    memcpy(copy, reconfigurationCallbacks, sizeof(copy));
    for (int i = 0; i < count; i++) {
        copy[i].callback(PUREDARWIN_MAIN_DISPLAY_ID, flags, copy[i].userInfo);
    }
}

void CGDirectDisplaySetMainDisplayBounds(CGRect bounds) {
    CGRect old = mainDisplayBounds;

    if (CGRectEqualToRect(old, bounds))
        return;

    // the first size is discovery, every later one is a mode change
    if (old.size.width > 0)
        notifyReconfiguration(kCGDisplayBeginConfigurationFlag);
    mainDisplayBounds = bounds;
    if (old.size.width > 0)
        notifyReconfiguration(kCGDisplaySetModeFlag | kCGDisplayDesktopShapeChangedFlag);
}

void CGDirectDisplaySetModeProvider(const struct CGDirectDisplayModeProvider *provider) {
    modeProvider = provider;
}

CGError CGDisplayRegisterReconfigurationCallback(CGDisplayReconfigurationCallBack callback, void *userInfo) {
    if (callback == NULL)
        return kCGErrorIllegalArgument;
    if (reconfigurationCallbackCount >= 16)
        return kCGErrorRangeCheck;

    reconfigurationCallbacks[reconfigurationCallbackCount].callback = callback;
    reconfigurationCallbacks[reconfigurationCallbackCount].userInfo = userInfo;
    reconfigurationCallbackCount++;
    return kCGErrorSuccess;
}

CGError CGDisplayRemoveReconfigurationCallback(CGDisplayReconfigurationCallBack callback, void *userInfo) {
    for (int i = 0; i < reconfigurationCallbackCount; i++) {
        if (reconfigurationCallbacks[i].callback != callback || reconfigurationCallbacks[i].userInfo != userInfo)
            continue;

        memmove(&reconfigurationCallbacks[i], &reconfigurationCallbacks[i + 1],
                (reconfigurationCallbackCount - i - 1) * sizeof(struct reconfigurationCallback));
        reconfigurationCallbackCount--;
        return kCGErrorSuccess;
    }
    return kCGErrorIllegalArgument;
}

// decouples the cursor from mouse motion for mouse-look, the Wayland WindowServer has no pointer lock yet,
// so events keep their deltas and the cursor stays attached
CGError CGAssociateMouseAndMouseCursorPosition(boolean_t connected) {
    (void)connected;
    return kCGErrorSuccess;
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

static CGDisplayModeRef newMode(size_t width, size_t height, double refresh, int32_t modeID) {
    struct CGDisplayMode *mode = calloc(1, sizeof(struct CGDisplayMode));

    mode->width = width;
    mode->height = height;
    mode->refresh = refresh;
    mode->modeID = modeID;
    mode->retainCount = 1;
    return mode;
}

CGDisplayModeRef CGDisplayCopyDisplayMode(CGDirectDisplayID display) {
    struct CGDirectDisplayModeInfo modes[64];
    CGRect bounds = currentBounds();
    int count = modeProvider != NULL ? modeProvider->copyModes(modes, 64) : 0;

    // wl_output reports the new size before the head marks its mode current, the size decides
    for (int i = 0; i < count; i++) {
        if (modes[i].width == (size_t)bounds.size.width && modes[i].height == (size_t)bounds.size.height)
            return newMode(modes[i].width, modes[i].height, modes[i].refresh, modes[i].modeID);
    }
    return newMode((size_t)bounds.size.width, (size_t)bounds.size.height, 60.0, 0);
}

static const void *modeArrayRetain(CFAllocatorRef allocator, const void *value) {
    return CGDisplayModeRetain((CGDisplayModeRef)value);
}

static void modeArrayRelease(CFAllocatorRef allocator, const void *value) {
    CGDisplayModeRelease((CGDisplayModeRef)value);
}

CFArrayRef CGDisplayCopyAllDisplayModes(CGDirectDisplayID display, CFDictionaryRef options) {
    CFArrayCallBacks callbacks = { 0, modeArrayRetain, modeArrayRelease, NULL, NULL };
    struct CGDirectDisplayModeInfo modes[64];
    int count = modeProvider != NULL ? modeProvider->copyModes(modes, 64) : 0;
    CFMutableArrayRef result = CFArrayCreateMutable(kCFAllocatorDefault, 0, &callbacks);

    // without a compositor that lists modes the current one is all there is
    if (count == 0) {
        CGDisplayModeRef mode = CGDisplayCopyDisplayMode(display);

        CFArrayAppendValue(result, mode);
        CGDisplayModeRelease(mode);
    }
    for (int i = 0; i < count; i++) {
        CGDisplayModeRef mode = newMode(modes[i].width, modes[i].height, modes[i].refresh, modes[i].modeID);

        CFArrayAppendValue(result, mode);
        CGDisplayModeRelease(mode);
    }
    return result;
}

CGError CGDisplaySetDisplayMode(CGDirectDisplayID display, CGDisplayModeRef mode, CFDictionaryRef options) {
    if (mode == NULL)
        return kCGErrorIllegalArgument;
    if (modeProvider == NULL)
        return kCGErrorNotImplemented;
    return modeProvider->setMode(mode->modeID) ? kCGErrorSuccess : kCGErrorFailure;
}

int32_t CGDisplayModeGetIODisplayModeID(CGDisplayModeRef mode) {
    return mode != NULL ? mode->modeID : 0;
}

bool CGDisplayModeIsUsableForDesktopGUI(CGDisplayModeRef mode) {
    return mode != NULL && mode->width >= 800 && mode->height >= 600;
}

size_t CGDisplayModeGetPixelWidth(CGDisplayModeRef mode) {
    return CGDisplayModeGetWidth(mode);
}

size_t CGDisplayModeGetPixelHeight(CGDisplayModeRef mode) {
    return CGDisplayModeGetHeight(mode);
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

double CGDisplayModeGetRefreshRate(CGDisplayModeRef mode) {
    return mode != NULL ? mode->refresh : 0;
}

/* AppKit asks the window server for the front-to-back window order. Nothing
 * tracks global stacking yet, so each app is its own Wayland client and the
 * compositor owns the order, and this reports none. */
CFArrayRef CGSOrderedWindowNumbers(void) {
    return CFArrayCreate(kCFAllocatorDefault, NULL, 0, &kCFTypeArrayCallBacks);
}
