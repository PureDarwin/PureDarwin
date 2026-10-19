/* Copyright (c) 2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <CoreGraphics/CGColor.h>
#import <Onyx2D/O2Color.h>
#import <Onyx2D/O2ColorSpace.h>
#import <CoreFoundation/CFRuntime.h>
#include <dispatch/dispatch.h>

CGColorRef CGColorRetain(CGColorRef self) {
   return O2ColorRetain(self);
}

void CGColorRelease(CGColorRef self) {
   O2ColorRelease(self);
}

CGColorRef CGColorCreate(CGColorSpaceRef colorSpace,const CGFloat *components) {
   return O2ColorCreate(colorSpace,components);
}

CGColorRef CGColorCreateGenericGray(CGFloat gray,CGFloat a) {
   return O2ColorCreateGenericGray(gray,a);
}

CGColorRef CGColorCreateGenericRGB(CGFloat r,CGFloat g,CGFloat b,CGFloat a) {
    return O2ColorCreateGenericRGB(r,g,b,a);
}

CGColorRef CGColorCreateGenericCMYK(CGFloat c,CGFloat m,CGFloat y,CGFloat k,CGFloat a) {
   return O2ColorCreateGenericCMYK(c,m,y,k,a);
}

CGColorRef CGColorCreateWithPattern(CGColorSpaceRef colorSpace,CGPatternRef pattern,const CGFloat *components) {
   return O2ColorCreateWithPattern(colorSpace,pattern,components);
}

CGColorRef CGColorCreateCopy(CGColorRef self) {
   return O2ColorCreateCopy(self);
}

CGColorRef CGColorCreateCopyWithAlpha(CGColorRef self,CGFloat a) {
   return O2ColorCreateCopyWithAlpha(self,a);
}

bool CGColorEqualToColor(CGColorRef self,CGColorRef other) {
   return O2ColorEqualToColor(self,other);
}

CGColorSpaceRef CGColorGetColorSpace(CGColorRef self) {
   return O2ColorGetColorSpace(self);
}

size_t CGColorGetNumberOfComponents(CGColorRef self) {
   return O2ColorGetNumberOfComponents(self);
}

const CGFloat *CGColorGetComponents(CGColorRef self) {
   return O2ColorGetComponents(self);
}

CGFloat CGColorGetAlpha(CGColorRef self) {
   return O2ColorGetAlpha(self);
}

CGPatternRef CGColorGetPattern(CGColorRef self) {
   return O2ColorGetPattern(self);
}

const CFStringRef kCGColorWhite=(CFStringRef)@"kCGColorWhite";
const CFStringRef kCGColorBlack=(CFStringRef)@"kCGColorBlack";
const CFStringRef kCGColorClear=(CFStringRef)@"kCGColorClear";

// O2Color is an ObjC class, so CFGetTypeID asks it through -_cfTypeID
static CFTypeID colorTypeID=_kCFRuntimeNotATypeID;

static const CFRuntimeClass colorClass={ 0, "CGColor", NULL, NULL, NULL, NULL, NULL, NULL, NULL };

CFTypeID CGColorGetTypeID(void) {
   static dispatch_once_t once;

   dispatch_once(&once,^{
    colorTypeID=_CFRuntimeRegisterClass(&colorClass);
   });
   return colorTypeID;
}

@implementation O2Color (CGColorTypeID)

-(CFTypeID)_cfTypeID {
   return CGColorGetTypeID();
}

@end

CGColorRef CGColorCreateSRGB(CGFloat r,CGFloat g,CGFloat b,CGFloat a) {
   CGColorSpaceRef space=CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
   CGFloat components[4]={ r, g, b, a };
   CGColorRef result=CGColorCreate(space,components);

   CGColorSpaceRelease(space);
   return result;
}

// the constant colors live for the life of the process, as Apple's do
CGColorRef CGColorGetConstantColor(CFStringRef name) {
   static CGColorRef white,black,clear;
   static dispatch_once_t once;

   dispatch_once(&once,^{
    CGColorSpaceRef gray=CGColorSpaceCreateWithName(kCGColorSpaceGenericGray);
    CGFloat w[2]={ 1, 1 },b[2]={ 0, 1 },c[2]={ 0, 0 };

    white=CGColorCreate(gray,w);
    black=CGColorCreate(gray,b);
    clear=CGColorCreate(gray,c);
    CGColorSpaceRelease(gray);
   });

   if(name==NULL)
    return NULL;
   if(CFEqual(name,kCGColorWhite))
    return white;
   if(CFEqual(name,kCGColorBlack))
    return black;
   if(CFEqual(name,kCGColorClear))
    return clear;
   return NULL;
}
