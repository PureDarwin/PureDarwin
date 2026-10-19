// CGWindowList.m - window list queries and the private connection calls
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// each app is its own Wayland client and only the compositor knows every window,
// so the list holds none and capture fails, as on macOS without screen recording consent

#import <CoreGraphics/CGWindowList.h>
#include <stdatomic.h>
#include <unistd.h>

const CFStringRef kCGWindowNumber = CFSTR("kCGWindowNumber");
const CFStringRef kCGWindowStoreType = CFSTR("kCGWindowStoreType");
const CFStringRef kCGWindowLayer = CFSTR("kCGWindowLayer");
const CFStringRef kCGWindowBounds = CFSTR("kCGWindowBounds");
const CFStringRef kCGWindowSharingState = CFSTR("kCGWindowSharingState");
const CFStringRef kCGWindowAlpha = CFSTR("kCGWindowAlpha");
const CFStringRef kCGWindowOwnerPID = CFSTR("kCGWindowOwnerPID");
const CFStringRef kCGWindowMemoryUsage = CFSTR("kCGWindowMemoryUsage");
const CFStringRef kCGWindowOwnerName = CFSTR("kCGWindowOwnerName");
const CFStringRef kCGWindowName = CFSTR("kCGWindowName");
const CFStringRef kCGWindowIsOnscreen = CFSTR("kCGWindowIsOnscreen");
const CFStringRef kCGWindowBackingLocationVideoMemory = CFSTR("kCGWindowBackingLocationVideoMemory");

CFArrayRef CGWindowListCopyWindowInfo(CGWindowListOption option, CGWindowID relativeToWindow)
{
    return CFArrayCreate(kCFAllocatorDefault, NULL, 0, &kCFTypeArrayCallBacks);
}

CFArrayRef CGWindowListCreateDescriptionFromArray(CFArrayRef windowArray)
{
    if (windowArray == NULL)
        return NULL;
    return CFArrayCreate(kCFAllocatorDefault, NULL, 0, &kCFTypeArrayCallBacks);
}

CGImageRef CGWindowListCreateImage(CGRect screenBounds, CGWindowListOption listOption, CGWindowID windowID,
    CGWindowImageOption imageOption)
{
    return NULL;
}

CGImageRef CGWindowListCreateImageFromArray(CGRect screenBounds, CFArrayRef windowArray, CGWindowImageOption imageOption)
{
    return NULL;
}

static atomic_bool denyConnections;

// one connection per process, numbered by pid so it is stable and never 0
CGSConnectionID _CGSDefaultConnection(void)
{
    return (CGSConnectionID)getpid();
}

// window numbers belong to the compositor, so no window here can be found
CGError CGSSetWindowTransform(CGSConnectionID cid, CGWindowID wid, CGAffineTransform transform)
{
    return kCGErrorIllegalArgument;
}

CGError CGSSetDenyWindowServerConnections(bool deny)
{
    atomic_store(&denyConnections, deny);
    return kCGErrorSuccess;
}

// CoreGraphics itself holds no window server connection, AppKit's goes through WindowServer
void CGSShutdownServerConnections(void)
{
}
