/* Copyright (c) 2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <CoreGraphics/CGColorSpace.h>
#import <objc/runtime.h>
#import <Onyx2D/O2ColorSpace.h>

CGColorSpaceRef CGColorSpaceRetain(CGColorSpaceRef colorSpace) {
   return O2ColorSpaceRetain(colorSpace);
}

void CGColorSpaceRelease(CGColorSpaceRef colorSpace) {
   O2ColorSpaceRelease(colorSpace);
}

CGColorSpaceRef CGColorSpaceCreateDeviceRGB() {
   return O2ColorSpaceCreateDeviceRGB();
}

CGColorSpaceRef CGColorSpaceCreateDeviceGray() {
   return O2ColorSpaceCreateDeviceGray();
}

CGColorSpaceRef CGColorSpaceCreateDeviceCMYK() {
   return O2ColorSpaceCreateDeviceCMYK();
}

const CFStringRef kCGColorSpaceGenericGray = CFSTR("kCGColorSpaceGenericGray");
const CFStringRef kCGColorSpaceGenericGrayGamma2_2 = CFSTR("kCGColorSpaceGenericGrayGamma2_2");
const CFStringRef kCGColorSpaceLinearGray = CFSTR("kCGColorSpaceLinearGray");
const CFStringRef kCGColorSpaceExtendedGray = CFSTR("kCGColorSpaceExtendedGray");
const CFStringRef kCGColorSpaceExtendedLinearGray = CFSTR("kCGColorSpaceExtendedLinearGray");
const CFStringRef kCGColorSpaceGenericRGB = CFSTR("kCGColorSpaceGenericRGB");
const CFStringRef kCGColorSpaceGenericRGBLinear = CFSTR("kCGColorSpaceGenericRGBLinear");
const CFStringRef kCGColorSpaceSRGB = CFSTR("kCGColorSpaceSRGB");
const CFStringRef kCGColorSpaceLinearSRGB = CFSTR("kCGColorSpaceLinearSRGB");
const CFStringRef kCGColorSpaceExtendedSRGB = CFSTR("kCGColorSpaceExtendedSRGB");
const CFStringRef kCGColorSpaceExtendedLinearSRGB = CFSTR("kCGColorSpaceExtendedLinearSRGB");
const CFStringRef kCGColorSpaceDisplayP3 = CFSTR("kCGColorSpaceDisplayP3");
const CFStringRef kCGColorSpaceExtendedLinearDisplayP3 = CFSTR("kCGColorSpaceExtendedLinearDisplayP3");
const CFStringRef kCGColorSpaceGenericCMYK = CFSTR("kCGColorSpaceGenericCMYK");
const CFStringRef kCGColorSpaceAdobeRGB1998 = CFSTR("kCGColorSpaceAdobeRGB1998");
const CFStringRef kCGColorSpaceDCIP3 = CFSTR("kCGColorSpaceDCIP3");
const CFStringRef kCGColorSpaceITUR_709 = CFSTR("kCGColorSpaceITUR_709");
const CFStringRef kCGColorSpaceITUR_2020 = CFSTR("kCGColorSpaceITUR_2020");
const CFStringRef kCGColorSpaceExtendedLinearITUR_2020 = CFSTR("kCGColorSpaceExtendedLinearITUR_2020");
const CFStringRef kCGColorSpaceITUR_2100_HLG = CFSTR("kCGColorSpaceITUR_2100_HLG");
const CFStringRef kCGColorSpaceITUR_2100_PQ = CFSTR("kCGColorSpaceITUR_2100_PQ");

static char colorSpaceNameKey;

CGColorSpaceRef CGColorSpaceCreateWithName(CFStringRef name) {
   if(name == NULL)
      return NULL;

   const CFStringRef gray[] = { kCGColorSpaceGenericGray, kCGColorSpaceGenericGrayGamma2_2, kCGColorSpaceLinearGray,
                                kCGColorSpaceExtendedGray, kCGColorSpaceExtendedLinearGray };
   const CFStringRef rgb[] = { kCGColorSpaceGenericRGB, kCGColorSpaceGenericRGBLinear, kCGColorSpaceSRGB,
                               kCGColorSpaceLinearSRGB, kCGColorSpaceExtendedSRGB, kCGColorSpaceExtendedLinearSRGB,
                               kCGColorSpaceDisplayP3, kCGColorSpaceExtendedLinearDisplayP3, kCGColorSpaceAdobeRGB1998,
                               kCGColorSpaceDCIP3, kCGColorSpaceITUR_709, kCGColorSpaceITUR_2020,
                               kCGColorSpaceExtendedLinearITUR_2020, kCGColorSpaceITUR_2100_HLG, kCGColorSpaceITUR_2100_PQ };

   CGColorSpaceRef space = NULL;

   for(size_t i = 0; i < sizeof(gray) / sizeof(gray[0]) && !space; i++) {
      if(CFEqual(name, gray[i]))
         space = O2ColorSpaceCreateDeviceGray();
   }
   for(size_t i = 0; i < sizeof(rgb) / sizeof(rgb[0]) && !space; i++) {
      if(CFEqual(name, rgb[i]))
         space = O2ColorSpaceCreateDeviceRGB();
   }
   if(!space && CFEqual(name, kCGColorSpaceGenericCMYK))
      space = O2ColorSpaceCreateDeviceCMYK();
   if(space)
      objc_setAssociatedObject((id)space, &colorSpaceNameKey, (id)name, OBJC_ASSOCIATION_COPY);
   return space;
}

CFStringRef CGColorSpaceGetName(CGColorSpaceRef space) {
   return space ? (CFStringRef)objc_getAssociatedObject((id)space, &colorSpaceNameKey) : NULL;
}

CGColorSpaceRef CGColorSpaceCreatePattern(CGColorSpaceRef baseSpace) {
   return O2ColorSpaceCreatePattern(baseSpace);
}

CGColorSpaceModel CGColorSpaceGetModel(CGColorSpaceRef self) {
   return O2ColorSpaceGetModel(self);
}

size_t CGColorSpaceGetNumberOfComponents(CGColorSpaceRef self) {
   return O2ColorSpaceGetNumberOfComponents(self);
}

// Onyx2D's spaces are device spaces without an ICC profile, Apple answers NULL for those too
CFDataRef CGColorSpaceCopyICCData(CGColorSpaceRef space) {
   return NULL;
}
