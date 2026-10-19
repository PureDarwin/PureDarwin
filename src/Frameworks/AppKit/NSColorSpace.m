/* Copyright (c) 2006-2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <AppKit/NSColorSpace.h>

@implementation NSColorSpace

+(NSColorSpace *)deviceRGBColorSpace {
   CGColorSpaceRef device=CGColorSpaceCreateDeviceRGB();
   NSColorSpace   *result=[[[self alloc] initWithCGColorSpace:device] autorelease];
   
   CGColorSpaceRelease(device);
   
   return result;
}

static NSColorSpace *spaceNamed(CFStringRef name) {
   CGColorSpaceRef space=CGColorSpaceCreateWithName(name);
   NSColorSpace   *result=[[[NSColorSpace alloc] initWithCGColorSpace:space] autorelease];

   CGColorSpaceRelease(space);
   return result;
}

+(NSColorSpace *)genericRGBColorSpace {
   return spaceNamed(kCGColorSpaceGenericRGB);
}

+(NSColorSpace *)sRGBColorSpace {
   return spaceNamed(kCGColorSpaceSRGB);
}

+(NSColorSpace *)extendedSRGBColorSpace {
   return spaceNamed(kCGColorSpaceExtendedSRGB);
}

+(NSColorSpace *)displayP3ColorSpace {
   return spaceNamed(kCGColorSpaceDisplayP3);
}

+(NSColorSpace *)adobeRGB1998ColorSpace {
   return spaceNamed(kCGColorSpaceAdobeRGB1998);
}

+(NSColorSpace *)deviceGrayColorSpace {
   CGColorSpaceRef device=CGColorSpaceCreateDeviceGray();
   NSColorSpace   *result=[[[self alloc] initWithCGColorSpace:device] autorelease];

   CGColorSpaceRelease(device);
   return result;
}

+(NSColorSpace *)genericGrayColorSpace {
   return spaceNamed(kCGColorSpaceGenericGray);
}

+(NSColorSpace *)genericGamma22GrayColorSpace {
   return spaceNamed(kCGColorSpaceGenericGrayGamma2_2);
}

+(NSColorSpace *)extendedGenericGamma22GrayColorSpace {
   return spaceNamed(kCGColorSpaceExtendedGray);
}

+(NSColorSpace *)deviceCMYKColorSpace {
   CGColorSpaceRef device=CGColorSpaceCreateDeviceCMYK();
   NSColorSpace   *result=[[[self alloc] initWithCGColorSpace:device] autorelease];

   CGColorSpaceRelease(device);
   return result;
}

+(NSColorSpace *)genericCMYKColorSpace {
   return spaceNamed(kCGColorSpaceGenericCMYK);
}

-initWithCGColorSpace:(CGColorSpaceRef)cgColorSpace {
   _cgColorSpace=CGColorSpaceRetain(cgColorSpace);
   return self;
}

-(void)dealloc {
   CGColorSpaceRelease(_cgColorSpace);
   [super dealloc];
}

-(CGColorSpaceRef)CGColorSpace {
   return _cgColorSpace;
}

// the CG models share NSColorSpaceModel's numbering
-(NSColorSpaceModel)colorSpaceModel {
   return _cgColorSpace==NULL ? NSColorSpaceModelUnknown : (NSColorSpaceModel)CGColorSpaceGetModel(_cgColorSpace);
}

-(NSInteger)numberOfColorComponents {
   return _cgColorSpace==NULL ? 0 : (NSInteger)CGColorSpaceGetNumberOfComponents(_cgColorSpace);
}

-(NSString *)localizedName {
   CFStringRef name=_cgColorSpace==NULL ? NULL : CGColorSpaceGetName(_cgColorSpace);

   return name!=NULL ? (NSString *)name : @"Device";
}

@end
