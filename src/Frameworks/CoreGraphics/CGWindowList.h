// CGWindowList.h - window list queries and the private connection calls
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import <CoreGraphics/CoreGraphicsExport.h>
#import <CoreGraphics/CGGeometry.h>
#import <CoreGraphics/CGImage.h>
#import <CoreGraphics/CGAffineTransform.h>
#import <CoreGraphics/CGError.h>
#include <CoreFoundation/CFArray.h>
#include <CoreFoundation/CFDictionary.h>

typedef uint32_t CGWindowID;
#define kCGNullWindowID ((CGWindowID)0)

typedef uint32_t CGWindowListOption;
enum {
    kCGWindowListOptionAll = 0,
    kCGWindowListOptionOnScreenOnly = (1 << 0),
    kCGWindowListOptionOnScreenAboveWindow = (1 << 1),
    kCGWindowListOptionOnScreenBelowWindow = (1 << 2),
    kCGWindowListOptionIncludingWindow = (1 << 3),
    kCGWindowListExcludeDesktopElements = (1 << 4),
};

typedef uint32_t CGWindowImageOption;
enum {
    kCGWindowImageDefault = 0,
    kCGWindowImageBoundsIgnoreFraming = (1 << 0),
    kCGWindowImageShouldBeOpaque = (1 << 1),
    kCGWindowImageOnlyShadows = (1 << 2),
    kCGWindowImageBestResolution = (1 << 3),
    kCGWindowImageNominalResolution = (1 << 4),
};

COREGRAPHICS_EXPORT const CFStringRef kCGWindowNumber;
COREGRAPHICS_EXPORT const CFStringRef kCGWindowStoreType;
COREGRAPHICS_EXPORT const CFStringRef kCGWindowLayer;
COREGRAPHICS_EXPORT const CFStringRef kCGWindowBounds;
COREGRAPHICS_EXPORT const CFStringRef kCGWindowSharingState;
COREGRAPHICS_EXPORT const CFStringRef kCGWindowAlpha;
COREGRAPHICS_EXPORT const CFStringRef kCGWindowOwnerPID;
COREGRAPHICS_EXPORT const CFStringRef kCGWindowMemoryUsage;
COREGRAPHICS_EXPORT const CFStringRef kCGWindowOwnerName;
COREGRAPHICS_EXPORT const CFStringRef kCGWindowName;
COREGRAPHICS_EXPORT const CFStringRef kCGWindowIsOnscreen;
COREGRAPHICS_EXPORT const CFStringRef kCGWindowBackingLocationVideoMemory;

COREGRAPHICS_EXPORT CFArrayRef CGWindowListCopyWindowInfo(CGWindowListOption option, CGWindowID relativeToWindow);
COREGRAPHICS_EXPORT CFArrayRef CGWindowListCreateDescriptionFromArray(CFArrayRef windowArray);
COREGRAPHICS_EXPORT CGImageRef CGWindowListCreateImage(CGRect screenBounds, CGWindowListOption listOption, CGWindowID windowID, CGWindowImageOption imageOption);
COREGRAPHICS_EXPORT CGImageRef CGWindowListCreateImageFromArray(CGRect screenBounds, CFArrayRef windowArray, CGWindowImageOption imageOption);

typedef int32_t CGSConnectionID;

COREGRAPHICS_EXPORT CGSConnectionID _CGSDefaultConnection(void);
COREGRAPHICS_EXPORT CGError CGSSetWindowTransform(CGSConnectionID cid, CGWindowID wid, CGAffineTransform transform);
COREGRAPHICS_EXPORT CGError CGSSetDenyWindowServerConnections(bool deny);
COREGRAPHICS_EXPORT void CGSShutdownServerConnections(void);
