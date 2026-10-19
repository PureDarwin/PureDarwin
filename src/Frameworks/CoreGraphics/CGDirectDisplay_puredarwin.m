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
#include <mach/port.h>
#include <dlfcn.h>
#import <CoreFoundation/CFRuntime.h>
#include <dispatch/dispatch.h>
#import <CoreGraphics/CGDirectDisplayMetal.h>

#define PUREDARWIN_MAIN_DISPLAY_ID ((CGDirectDisplayID)1)

/* A CGDisplayModeRef is opaque, so it can simply carry the size. */
// a CF object like Apple's, so CFRetain, CFRelease and CF collections work on modes
struct CGDisplayMode {
    CFRuntimeBase base;
    size_t width;
    size_t height;
    double refresh;
    int32_t modeID;
};

static CFTypeID displayModeTypeID = _kCFRuntimeNotATypeID;
static const CFRuntimeClass displayModeClass = { 0, "CGDisplayMode", NULL, NULL, NULL, NULL, NULL, NULL, NULL };

CFTypeID CGDisplayModeGetTypeID(void) {
    static dispatch_once_t once;

    dispatch_once(&once, ^{
        displayModeTypeID = _CFRuntimeRegisterClass(&displayModeClass);
    });
    return displayModeTypeID;
}

static const struct CGDirectDisplayModeProvider *modeProvider;

struct reconfigurationCallback {
    CGDisplayReconfigurationCallBack callback;
    void *userInfo;
};

static struct reconfigurationCallback reconfigurationCallbacks[16];
static int reconfigurationCallbackCount;

static CGRect mainDisplayBounds = { { 0, 0 }, { 0, 0 } };

// set once a caller got the guessed size, so the real one is announced as a change
static bool guessedSizeHandedOut;

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
    guessedSizeHandedOut = true;
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

    // the first size is discovery, unless someone already acted on the guess, and every later
    // one is a mode change
    bool announce = old.size.width > 0 || guessedSizeHandedOut;

    if (announce)
        notifyReconfiguration(kCGDisplayBeginConfigurationFlag);
    mainDisplayBounds = bounds;
    if (announce)
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
    struct CGDisplayMode *mode = (struct CGDisplayMode *)_CFRuntimeCreateInstance(kCFAllocatorDefault,
        CGDisplayModeGetTypeID(), sizeof(struct CGDisplayMode) - sizeof(CFRuntimeBase), NULL);

    if (mode == NULL)
        return NULL;
    mode->width = width;
    mode->height = height;
    mode->refresh = refresh;
    mode->modeID = modeID;
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

// the mode the display had before the first switch, for CGRestorePermanentDisplayConfiguration
static int32_t permanentModeID = -1;

CGError CGDisplaySetDisplayMode(CGDirectDisplayID display, CGDisplayModeRef mode, CFDictionaryRef options) {
    if (mode == NULL)
        return kCGErrorIllegalArgument;
    if (modeProvider == NULL)
        return kCGErrorNotImplemented;
    if (permanentModeID < 0) {
        CGDisplayModeRef current = CGDisplayCopyDisplayMode(display);

        permanentModeID = current->modeID;
        CGDisplayModeRelease(current);
    }
    return modeProvider->setMode(mode->modeID) ? kCGErrorSuccess : kCGErrorFailure;
}

void CGRestorePermanentDisplayConfiguration(void) {
    if (permanentModeID >= 0 && modeProvider != NULL)
        modeProvider->setMode(permanentModeID);
    permanentModeID = -1;
}

const CFStringRef kCGDisplayShowDuplicateLowResolutionModes = CFSTR("kCGDisplayShowDuplicateLowResolutionModes");

// every mode the window server lists is a valid, safe mode, the current one is also the default
uint32_t CGDisplayModeGetIOFlags(CGDisplayModeRef mode) {
    if (mode == NULL)
        return 0;

    uint32_t flags = kDisplayModeValidFlag | kDisplayModeSafeFlag;
    CGRect bounds = currentBounds();

    if (mode->width == (size_t)bounds.size.width && mode->height == (size_t)bounds.size.height)
        flags |= kDisplayModeDefaultFlag;
    return flags;
}

// the compositor scans out 32-bit BGRA
CFStringRef CGDisplayModeCopyPixelEncoding(CGDisplayModeRef mode) {
    if (mode == NULL)
        return NULL;
    return CFStringCreateWithCString(kCFAllocatorDefault, IO32BitDirectPixels, kCFStringEncodingASCII);
}

// one display, always on, never mirrored, no EDID to read numbers from
boolean_t CGDisplayIsMain(CGDirectDisplayID display) {
    return display == PUREDARWIN_MAIN_DISPLAY_ID;
}

boolean_t CGDisplayIsActive(CGDirectDisplayID display) {
    return display == PUREDARWIN_MAIN_DISPLAY_ID;
}

boolean_t CGDisplayIsOnline(CGDirectDisplayID display) {
    return display == PUREDARWIN_MAIN_DISPLAY_ID;
}

boolean_t CGDisplayIsAsleep(CGDirectDisplayID display) {
    return false;
}

CGDirectDisplayID CGDisplayMirrorsDisplay(CGDirectDisplayID display) {
    return kCGNullDirectDisplay;
}

uint32_t CGDisplayVendorNumber(CGDirectDisplayID display) {
    return 0;
}

uint32_t CGDisplayModelNumber(CGDirectDisplayID display) {
    return 0;
}

uint32_t CGDisplaySerialNumber(CGDirectDisplayID display) {
    return 0;
}

uint32_t CGDisplayUnitNumber(CGDirectDisplayID display) {
    return 0;
}

double CGDisplayRotation(CGDirectDisplayID display) {
    return 0;
}

// the framebuffer the compositor scans out through, found in the registry at run time so CoreGraphics
// doesn't link IOKit. Callers walk up from it to the PCI display device
io_service_t CGDisplayIOServicePort(CGDirectDisplayID display) {
    static io_service_t framebuffer;
    static dispatch_once_t once;

    if (display != PUREDARWIN_MAIN_DISPLAY_ID)
        return MACH_PORT_NULL;
    dispatch_once(&once, ^{
        void *iokit = dlopen("/System/Library/Frameworks/IOKit.framework/IOKit", RTLD_LAZY);
        CFMutableDictionaryRef (*matching)(const char *) = iokit ? dlsym(iokit, "IOServiceMatching") : NULL;
        io_service_t (*lookup)(mach_port_t, CFDictionaryRef) = iokit ? dlsym(iokit, "IOServiceGetMatchingService") : NULL;

        if (matching != NULL && lookup != NULL)
            framebuffer = lookup(MACH_PORT_NULL, matching("IOFramebuffer"));
    });
    return framebuffer;
}

CGOpenGLDisplayMask CGDisplayIDToOpenGLDisplayMask(CGDirectDisplayID display) {
    return display == PUREDARWIN_MAIN_DISPLAY_ID ? 1 : 0;
}

CGDirectDisplayID CGOpenGLDisplayMaskToDisplayID(CGOpenGLDisplayMask mask) {
    return (mask & 1) ? PUREDARWIN_MAIN_DISPLAY_ID : kCGNullDirectDisplay;
}

// capturing only records the state, the compositor keeps drawing other clients
static bool displaysCaptured;

CGError CGDisplayCapture(CGDirectDisplayID display) {
    if (display != PUREDARWIN_MAIN_DISPLAY_ID)
        return kCGErrorIllegalArgument;
    displaysCaptured = true;
    return kCGErrorSuccess;
}

CGError CGDisplayCaptureWithOptions(CGDirectDisplayID display, CGCaptureOptions options) {
    return CGDisplayCapture(display);
}

CGError CGDisplayRelease(CGDirectDisplayID display) {
    displaysCaptured = false;
    return kCGErrorSuccess;
}

CGError CGCaptureAllDisplays(void) {
    displaysCaptured = true;
    return kCGErrorSuccess;
}

CGError CGCaptureAllDisplaysWithOptions(CGCaptureOptions options) {
    return CGCaptureAllDisplays();
}

CGError CGReleaseAllDisplays(void) {
    displaysCaptured = false;
    return kCGErrorSuccess;
}

boolean_t CGDisplayIsCaptured(CGDirectDisplayID display) {
    return displaysCaptured && display == PUREDARWIN_MAIN_DISPLAY_ID;
}

// the level macOS puts its capture shield at, above everything apps can use
CGWindowLevel CGShieldingWindowLevel(void) {
    return 2147483630;
}

// gamma tables are kept per process, the compositor has no gamma ramp to apply them to yet
#define GAMMA_CAPACITY 256
static CGGammaValue gammaTable[3][GAMMA_CAPACITY];
static uint32_t gammaSamples;

uint32_t CGDisplayGammaTableCapacity(CGDirectDisplayID display) {
    return GAMMA_CAPACITY;
}

CGError CGSetDisplayTransferByTable(CGDirectDisplayID display, uint32_t tableSize, const CGGammaValue *redTable,
                                    const CGGammaValue *greenTable, const CGGammaValue *blueTable) {
    if (display != PUREDARWIN_MAIN_DISPLAY_ID || tableSize > GAMMA_CAPACITY)
        return kCGErrorIllegalArgument;
    if (tableSize && (redTable == NULL || greenTable == NULL || blueTable == NULL))
        return kCGErrorIllegalArgument;
    memcpy(gammaTable[0], redTable, tableSize * sizeof(CGGammaValue));
    memcpy(gammaTable[1], greenTable, tableSize * sizeof(CGGammaValue));
    memcpy(gammaTable[2], blueTable, tableSize * sizeof(CGGammaValue));
    gammaSamples = tableSize;
    return kCGErrorSuccess;
}

CGError CGGetDisplayTransferByTable(CGDirectDisplayID display, uint32_t capacity, CGGammaValue *redTable,
                                    CGGammaValue *greenTable, CGGammaValue *blueTable, uint32_t *sampleCount) {
    if (display != PUREDARWIN_MAIN_DISPLAY_ID)
        return kCGErrorIllegalArgument;

    uint32_t n = gammaSamples ? gammaSamples : GAMMA_CAPACITY;

    if (n > capacity)
        n = capacity;
    for (uint32_t i = 0; i < n; i++) {
        // until a table is set the ramp is linear
        CGGammaValue linear = (CGGammaValue)i / (CGGammaValue)((gammaSamples ? gammaSamples : GAMMA_CAPACITY) - 1);

        if (redTable) redTable[i] = gammaSamples ? gammaTable[0][i] : linear;
        if (greenTable) greenTable[i] = gammaSamples ? gammaTable[1][i] : linear;
        if (blueTable) blueTable[i] = gammaSamples ? gammaTable[2][i] : linear;
    }
    if (sampleCount)
        *sampleCount = n;
    return kCGErrorSuccess;
}

void CGDisplayRestoreColorSyncSettings(void) {
    gammaSamples = 0;
}

// posted events are never suppressed locally, there is no event tap path for them to echo through
CGError CGSetLocalEventsSuppressionInterval(CFTimeInterval seconds) {
    return kCGErrorSuccess;
}

// screen recording has no consent store, so it reads as not granted, like a fresh macOS install
bool CGPreflightScreenCaptureAccess(void) {
    return false;
}

bool CGRequestScreenCaptureAccess(void) {
    return false;
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
    if (mode != NULL)
        CFRetain(mode);
    return mode;
}

void CGDisplayModeRelease(CGDisplayModeRef mode) {
    if (mode != NULL)
        CFRelease(mode);
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

// the values Apple's CGWindowLevelForKey returns, measured on macOS Sonoma
CGWindowLevel CGWindowLevelForKey(CGWindowLevelKey key) {
    static const int32_t levels[kCGNumberOfWindowLevelKeys] = {
        INT32_MIN, INT32_MIN + 1, INT32_MIN + 25, -20, 0,
        3, 3, 20, 24, 25, 8, 101, 500, 1000,
        INT32_MAX - 16, 102, 200, 19, INT32_MIN + 45, INT32_MAX - 17, 1500
    };

    if (key < 0 || key >= kCGNumberOfWindowLevelKeys) {
        return INT32_MIN;
    }
    return levels[key];
}

boolean_t CGDisplayIsBuiltin(CGDirectDisplayID display) {
    return false;
}

// no EDID reaches CoreGraphics yet, so the size assumes 96 dpi
CGSize CGDisplayScreenSize(CGDirectDisplayID display) {
    if (display != PUREDARWIN_MAIN_DISPLAY_ID) {
        return CGSizeMake(0, 0);
    }

    CGRect bounds = currentBounds();

    return CGSizeMake(bounds.size.width * 25.4 / 96, bounds.size.height * 25.4 / 96);
}

// Wayland gives clients no way to move the pointer
CGError CGWarpMouseCursorPosition(CGPoint newCursorPosition) {
    return kCGErrorCannotComplete;
}

// capture needs the compositor, so it fails like macOS without screen recording consent
CGImageRef CGDisplayCreateImage(CGDirectDisplayID display) {
    return NULL;
}

CGImageRef CGDisplayCreateImageForRect(CGDirectDisplayID display, CGRect rect) {
    return NULL;
}

const CFStringRef kCGDisplayStreamSourceRect = CFSTR("kCGDisplayStreamSourceRect");
const CFStringRef kCGDisplayStreamDestinationRect = CFSTR("kCGDisplayStreamDestinationRect");
const CFStringRef kCGDisplayStreamPreserveAspectRatio = CFSTR("kCGDisplayStreamPreserveAspectRatio");
const CFStringRef kCGDisplayStreamColorSpace = CFSTR("kCGDisplayStreamColorSpace");
const CFStringRef kCGDisplayStreamMinimumFrameTime = CFSTR("kCGDisplayStreamMinimumFrameTime");
const CFStringRef kCGDisplayStreamShowCursor = CFSTR("kCGDisplayStreamShowCursor");
const CFStringRef kCGDisplayStreamQueueDepth = CFSTR("kCGDisplayStreamQueueDepth");
const CFStringRef kCGDisplayStreamYCbCrMatrix = CFSTR("kCGDisplayStreamYCbCrMatrix");
const CFStringRef kCGDisplayStreamYCbCrMatrix_ITU_R_709_2 = CFSTR("kCGDisplayStreamYCbCrMatrix_ITU_R_709_2");
const CFStringRef kCGDisplayStreamYCbCrMatrix_ITU_R_601_4 = CFSTR("kCGDisplayStreamYCbCrMatrix_ITU_R_601_4");
const CFStringRef kCGDisplayStreamYCbCrMatrix_SMPTE_240M_1995 = CFSTR("kCGDisplayStreamYCbCrMatrix_SMPTE_240M_1995");

// no stream can be created, so the stream calls only see NULL and answer as for a stopped stream
CGDisplayStreamRef CGDisplayStreamCreate(CGDirectDisplayID display, size_t outputWidth, size_t outputHeight,
    int32_t pixelFormat, CFDictionaryRef properties, CGDisplayStreamFrameAvailableHandler handler) {
    return NULL;
}

CGDisplayStreamRef CGDisplayStreamCreateWithDispatchQueue(CGDirectDisplayID display, size_t outputWidth,
    size_t outputHeight, int32_t pixelFormat, CFDictionaryRef properties, dispatch_queue_t queue,
    CGDisplayStreamFrameAvailableHandler handler) {
    return NULL;
}

CGError CGDisplayStreamStart(CGDisplayStreamRef displayStream) {
    return kCGErrorIllegalArgument;
}

CGError CGDisplayStreamStop(CGDisplayStreamRef displayStream) {
    return kCGErrorIllegalArgument;
}

CFRunLoopSourceRef CGDisplayStreamGetRunLoopSource(CGDisplayStreamRef displayStream) {
    return NULL;
}

const CGRect *CGDisplayStreamUpdateGetRects(CGDisplayStreamUpdateRef updateRef,
    CGDisplayStreamUpdateRectType rectType, size_t *rectCount) {
    if (rectCount != NULL) {
        *rectCount = 0;
    }
    return NULL;
}

// the one display is driven by the system default Metal device. Metal is looked up at run time so
// CoreGraphics doesn't pull the GPU stack into every process
id<MTLDevice> CGDirectDisplayCopyCurrentMetalDevice(CGDirectDisplayID display) {
    if (display != PUREDARWIN_MAIN_DISPLAY_ID)
        return nil;

    id (*create)(void) = (id (*)(void))dlsym(RTLD_DEFAULT, "MTLCreateSystemDefaultDevice");

    if (create == NULL) {
        void *metal = dlopen("/System/Library/Frameworks/Metal.framework/Metal", RTLD_LAZY);

        if (metal != NULL)
            create = (id (*)(void))dlsym(metal, "MTLCreateSystemDefaultDevice");
    }
    return create != NULL ? create() : nil;
}
