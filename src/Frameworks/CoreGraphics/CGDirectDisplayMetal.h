// SPDX-License-Identifier: MIT
// the Metal device that drives a display
#import <CoreGraphics/CGDirectDisplay.h>

#ifdef __OBJC__
@protocol MTLDevice;
COREGRAPHICS_EXPORT id<MTLDevice> CGDirectDisplayCopyCurrentMetalDevice(CGDirectDisplayID display);
#endif
