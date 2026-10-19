/*
 * Quartz Display Services
 * Copyright (C) 2024 Zoe Knox <zoe@ravynsoft.com>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdint.h>
#include <CoreGraphics/CoreGraphicsExport.h>

#define kCGNumReservedWindowLevels 16 // tested on Sonoma
typedef int32_t CGWindowLevel;
typedef enum CGWindowLevelKey : int32_t {
    kCGBaseWindowLevelKey = 0,
    kCGMinimumWindowLevelKey,
    kCGDesktopWindowLevelKey,
    kCGBackstopMenuLevelKey,
    kCGNormalWindowLevelKey,
    kCGFloatingWindowLevelKey,
    kCGTornOffMenuWindowLevelKey,
    kCGDockWindowLevelKey,
    kCGMainMenuWindowLevelKey,
    kCGStatusWindowLevelKey,
    kCGModalPanelWindowLevelKey,
    kCGPopUpMenuWindowLevelKey,
    kCGDraggingWindowLevelKey,
    kCGScreenSaverWindowLevelKey,
    kCGMaximumWindowLevelKey,
    kCGOverlayWindowLevelKey,
    kCGHelpWindowLevelKey,
    kCGUtilityWindowLevelKey,
    kCGDesktopIconWindowLevelKey,
    kCGCursorWindowLevelKey,
    kCGAssistiveTechHighWindowLevelKey,
    kCGNumberOfWindowLevelKeys,
} CGWindowLevelKey;

COREGRAPHICS_EXPORT CGWindowLevel CGWindowLevelForKey(CGWindowLevelKey key);

#define kCGBaseWindowLevel CGWindowLevelForKey(kCGBaseWindowLevelKey)
#define kCGMinimumWindowLevel CGWindowLevelForKey(kCGMinimumWindowLevelKey)
#define kCGDesktopWindowLevel CGWindowLevelForKey(kCGDesktopWindowLevelKey)
#define kCGDesktopIconWindowLevel CGWindowLevelForKey(kCGDesktopIconWindowLevelKey)
#define kCGBackstopMenuLevel CGWindowLevelForKey(kCGBackstopMenuLevelKey)
#define kCGNormalWindowLevel CGWindowLevelForKey(kCGNormalWindowLevelKey)
#define kCGFloatingWindowLevel CGWindowLevelForKey(kCGFloatingWindowLevelKey)
#define kCGTornOffMenuWindowLevel CGWindowLevelForKey(kCGTornOffMenuWindowLevelKey)
#define kCGDockWindowLevel CGWindowLevelForKey(kCGDockWindowLevelKey)
#define kCGMainMenuWindowLevel CGWindowLevelForKey(kCGMainMenuWindowLevelKey)
#define kCGStatusWindowLevel CGWindowLevelForKey(kCGStatusWindowLevelKey)
#define kCGModalPanelWindowLevel CGWindowLevelForKey(kCGModalPanelWindowLevelKey)
#define kCGPopUpMenuWindowLevel CGWindowLevelForKey(kCGPopUpMenuWindowLevelKey)
#define kCGDraggingWindowLevel CGWindowLevelForKey(kCGDraggingWindowLevelKey)
#define kCGScreenSaverWindowLevel CGWindowLevelForKey(kCGScreenSaverWindowLevelKey)
#define kCGMaximumWindowLevel CGWindowLevelForKey(kCGMaximumWindowLevelKey)
#define kCGOverlayWindowLevel CGWindowLevelForKey(kCGOverlayWindowLevelKey)
#define kCGHelpWindowLevel CGWindowLevelForKey(kCGHelpWindowLevelKey)
#define kCGUtilityWindowLevel CGWindowLevelForKey(kCGUtilityWindowLevelKey)
#define kCGCursorWindowLevel CGWindowLevelForKey(kCGCursorWindowLevelKey)
#define kCGAssistiveTechHighWindowLevel CGWindowLevelForKey(kCGAssistiveTechHighWindowLevelKey)
