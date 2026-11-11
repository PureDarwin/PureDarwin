/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef CGDirectDisplay_puredarwin_h
#define CGDirectDisplay_puredarwin_h

#import <CoreGraphics/CoreGraphicsExport.h>
#import <CoreGraphics/CGGeometry.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

COREGRAPHICS_EXPORT void CGDirectDisplaySetMainDisplayBounds(CGRect bounds);

// one display mode as the window server reports it
struct CGDirectDisplayModeInfo {
    size_t width;
    size_t height;
    double refresh;
    int32_t modeID;
    bool current;
};

// the window server lists and switches modes, CoreGraphics only forwards
struct CGDirectDisplayModeProvider {
    int (*copyModes)(struct CGDirectDisplayModeInfo *modes, int max);
    bool (*setMode)(int32_t modeID);
};

COREGRAPHICS_EXPORT void CGDirectDisplaySetModeProvider(const struct CGDirectDisplayModeProvider *provider);

// asked for the display before its size is known, the window server connects there and reports it
COREGRAPHICS_EXPORT void CGDirectDisplaySetDiscoverer(void (*discover)(void));

// the window server locks the pointer for mouse-look and takes warps while it is locked
struct CGDirectDisplayPointerProvider {
    bool (*associate)(bool connected);
    bool (*warp)(CGPoint point);
    // adds one hide or one show to the cursor's nesting count
    void (*hide)(bool hide);
};

COREGRAPHICS_EXPORT void CGDirectDisplaySetPointerProvider(const struct CGDirectDisplayPointerProvider *provider);

#endif /* CGDirectDisplay_puredarwin_h */
