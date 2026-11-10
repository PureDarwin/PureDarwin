/* Copyright (c) 2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <CoreGraphics/CGColorSpace.h>
#import <objc/runtime.h>
#import <Onyx2D/O2ColorSpace.h>
#import <dispatch/dispatch.h>
#include <math.h>
#include <string.h>

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

static void pdICCPut32(uint8_t *p,uint32_t v) {
   p[0]=v>>24; p[1]=v>>16; p[2]=v>>8; p[3]=v;
}

static void pdICCPutFixed(uint8_t *p,double v) {
   pdICCPut32(p,(uint32_t)(int32_t)lround(v*65536.0));
}

static size_t pdICCMluc(uint8_t *p,const char *text) {
   size_t i,length=strlen(text);

   memcpy(p,"mluc",4);
   pdICCPut32(p+8,1);
   pdICCPut32(p+12,12);
   memcpy(p+16,"enUS",4);
   pdICCPut32(p+20,length*2);
   pdICCPut32(p+24,28);
   for(i=0;i<length;i++){
    p[28+i*2]=0;
    p[29+i*2]=text[i];
   }
   return (28+length*2+3)&~3;
}

static size_t pdICCXYZ(uint8_t *p,double x,double y,double z) {
   memcpy(p,"XYZ ",4);
   pdICCPutFixed(p+8,x);
   pdICCPutFixed(p+12,y);
   pdICCPutFixed(p+16,z);
   return 20;
}

// an ICC v4 display profile for sRGB, primaries adapted to the D50 PCS and the IEC 61966-2-1 transfer curve
static CFDataRef pdCreateSRGBProfile(void) {
   static const char *tags[]={ "desc","cprt","wtpt","rXYZ","gXYZ","bXYZ","rTRC","gTRC","bTRC" };
   uint8_t  profile[1024];
   uint32_t offsets[9],sizes[9];
   size_t   at=128+4+9*12;
   int      i;

   memset(profile,0,sizeof(profile));
   offsets[0]=at; at+=sizes[0]=pdICCMluc(profile+at,"sRGB IEC61966-2.1");
   offsets[1]=at; at+=sizes[1]=pdICCMluc(profile+at,"PureDarwin");
   offsets[2]=at; at+=sizes[2]=pdICCXYZ(profile+at,0.9642,1.0,0.8249);
   offsets[3]=at; at+=sizes[3]=pdICCXYZ(profile+at,0.4360747,0.2225045,0.0139322);
   offsets[4]=at; at+=sizes[4]=pdICCXYZ(profile+at,0.3850649,0.7168786,0.0971045);
   offsets[5]=at; at+=sizes[5]=pdICCXYZ(profile+at,0.1430804,0.0606169,0.7141733);

   // parametric curve type 3, Y = (aX + b)^g from d up, cX below it
   memcpy(profile+at,"para",4);
   profile[at+9]=3;
   pdICCPutFixed(profile+at+12,2.4);
   pdICCPutFixed(profile+at+16,1.0/1.055);
   pdICCPutFixed(profile+at+20,0.055/1.055);
   pdICCPutFixed(profile+at+24,1.0/12.92);
   pdICCPutFixed(profile+at+28,0.04045);
   for(i=6;i<9;i++){
    offsets[i]=at;
    sizes[i]=32;
   }
   at+=32;

   pdICCPut32(profile,at);
   pdICCPut32(profile+8,0x04300000);
   memcpy(profile+12,"mntr",4);
   memcpy(profile+16,"RGB ",4);
   memcpy(profile+20,"XYZ ",4);
   memcpy(profile+36,"acsp",4);
   memcpy(profile+40,"APPL",4);
   pdICCPutFixed(profile+68,0.9642);
   pdICCPutFixed(profile+72,1.0);
   pdICCPutFixed(profile+76,0.8249);
   pdICCPut32(profile+128,9);
   for(i=0;i<9;i++){
    memcpy(profile+132+i*12,tags[i],4);
    pdICCPut32(profile+136+i*12,offsets[i]);
    pdICCPut32(profile+140+i*12,sizes[i]);
   }
   return CFDataCreate(NULL,profile,at);
}

// Onyx2D's RGB spaces render as sRGB, so they answer with an sRGB profile as macOS does for its RGB spaces
CFDataRef CGColorSpaceCopyICCData(CGColorSpaceRef space) {
   static CFDataRef srgb;
   static dispatch_once_t once;

   if(space==NULL || CGColorSpaceGetModel(space)!=kCGColorSpaceModelRGB)
    return NULL;
   dispatch_once(&once,^{ srgb=pdCreateSRGBProfile(); });
   return srgb!=NULL ? CFRetain(srgb) : NULL;
}

// a named space is described by its name, the only description an ICC-less space has
CFPropertyListRef CGColorSpaceCopyPropertyList(CGColorSpaceRef space) {
   CFStringRef name=CGColorSpaceGetName(space);

   return name ? CFRetain(name) : NULL;
}
