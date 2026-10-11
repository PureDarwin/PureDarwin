/* Copyright (c) 2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <CoreGraphics/CGGeometry.h>
#import <CoreFoundation/CFString.h>

typedef struct O2ColorSpace *CGColorSpaceRef;

typedef enum {
    kCGRenderingIntentDefault,
    kCGRenderingIntentAbsoluteColorimetric,
    kCGRenderingIntentRelativeColorimetric,
    kCGRenderingIntentSaturation,
    kCGRenderingIntentPerceptual,
} CGColorRenderingIntent;

typedef enum {
    kCGColorSpaceModelUnknown = -1,
    kCGColorSpaceModelMonochrome,
    kCGColorSpaceModelRGB,
    kCGColorSpaceModelCMYK,
    kCGColorSpaceModelLab,
    kCGColorSpaceModelDeviceN,
    kCGColorSpaceModelIndexed,
    kCGColorSpaceModelPattern,
} CGColorSpaceModel;

COREGRAPHICS_EXPORT CGColorSpaceRef CGColorSpaceRetain(CGColorSpaceRef colorSpace);
COREGRAPHICS_EXPORT void CGColorSpaceRelease(CGColorSpaceRef colorSpace);

COREGRAPHICS_EXPORT CGColorSpaceRef CGColorSpaceCreateDeviceRGB(void);
COREGRAPHICS_EXPORT CGColorSpaceRef CGColorSpaceCreateDeviceGray(void);
COREGRAPHICS_EXPORT CGColorSpaceRef CGColorSpaceCreateDeviceCMYK(void);
COREGRAPHICS_EXPORT CGColorSpaceRef CGColorSpaceCreatePattern(CGColorSpaceRef baseSpace);

// named spaces map onto Onyx2D's device spaces by model (no ICC profiles), unknown names give NULL
COREGRAPHICS_EXPORT CGColorSpaceRef CGColorSpaceCreateWithName(CFStringRef name);

COREGRAPHICS_EXPORT const CFStringRef kCGColorSpaceGenericGray;
COREGRAPHICS_EXPORT const CFStringRef kCGColorSpaceGenericGrayGamma2_2;
COREGRAPHICS_EXPORT const CFStringRef kCGColorSpaceLinearGray;
COREGRAPHICS_EXPORT const CFStringRef kCGColorSpaceExtendedGray;
COREGRAPHICS_EXPORT const CFStringRef kCGColorSpaceExtendedLinearGray;
COREGRAPHICS_EXPORT const CFStringRef kCGColorSpaceGenericRGB;
COREGRAPHICS_EXPORT const CFStringRef kCGColorSpaceGenericRGBLinear;
COREGRAPHICS_EXPORT const CFStringRef kCGColorSpaceSRGB;
COREGRAPHICS_EXPORT const CFStringRef kCGColorSpaceLinearSRGB;
COREGRAPHICS_EXPORT const CFStringRef kCGColorSpaceExtendedSRGB;
COREGRAPHICS_EXPORT const CFStringRef kCGColorSpaceExtendedLinearSRGB;
COREGRAPHICS_EXPORT const CFStringRef kCGColorSpaceDisplayP3;
COREGRAPHICS_EXPORT const CFStringRef kCGColorSpaceExtendedLinearDisplayP3;
COREGRAPHICS_EXPORT const CFStringRef kCGColorSpaceGenericCMYK;
COREGRAPHICS_EXPORT const CFStringRef kCGColorSpaceAdobeRGB1998;
COREGRAPHICS_EXPORT const CFStringRef kCGColorSpaceDCIP3;
COREGRAPHICS_EXPORT const CFStringRef kCGColorSpaceITUR_709;
COREGRAPHICS_EXPORT const CFStringRef kCGColorSpaceITUR_2020;
COREGRAPHICS_EXPORT const CFStringRef kCGColorSpaceExtendedLinearITUR_2020;
COREGRAPHICS_EXPORT const CFStringRef kCGColorSpaceITUR_2100_HLG;
COREGRAPHICS_EXPORT const CFStringRef kCGColorSpaceITUR_2100_PQ;
// the name a space was made with (CGColorSpaceCreateWithName), NULL for the device spaces
COREGRAPHICS_EXPORT CFStringRef CGColorSpaceGetName(CGColorSpaceRef space);

COREGRAPHICS_EXPORT CGColorSpaceModel CGColorSpaceGetModel(CGColorSpaceRef self);
COREGRAPHICS_EXPORT size_t CGColorSpaceGetNumberOfComponents(CGColorSpaceRef self);
