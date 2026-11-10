#import <CoreGraphics/CGImageSource.h>
#import <Onyx2D/O2ImageSource.h>

@interface _O2ImageSource : O2ImageSource
@end

CGImageSourceRef CGImageSourceCreateWithData(CFDataRef data,CFDictionaryRef options) {
	return (CGImageSourceRef)[O2ImageSource newImageSourceWithData:data options:options];
}

CGImageSourceRef CGImageSourceCreateWithDataProvider(CGDataProviderRef provider,CFDictionaryRef options) {
	return (CGImageSourceRef)[O2ImageSource newImageSourceWithDataProvider:(O2DataProvider *)provider options:options];
}

// the formats Onyx2D has decoders for
CFArrayRef CGImageSourceCopyTypeIdentifiers(void) {
	const void *types[]={ CFSTR("public.png"), CFSTR("public.jpeg"), CFSTR("com.compuserve.gif"),
	 CFSTR("com.microsoft.bmp"), CFSTR("public.tiff"), CFSTR("com.apple.icns") };

	return CFArrayCreate(kCFAllocatorDefault,types,sizeof(types)/sizeof(types[0]),&kCFTypeArrayCallBacks);
}

CGImageSourceRef CGImageSourceCreateWithURL(CFURLRef url,CFDictionaryRef options) {
	return (CGImageSourceRef)[O2ImageSource newImageSourceWithURL:(NSURL *)url options:options];
}

void CGImageSourceRelease(CGImageSourceRef self) {
   if(self!=NULL)
    [(id)self release];
}

size_t CGImageSourceGetCount(CGImageSourceRef self) {
   return [(O2ImageSource *)self count];
}

CGImageRef CGImageSourceCreateImageAtIndex(CGImageSourceRef self,size_t index,CFDictionaryRef options) {
   return [(O2ImageSource *)self createImageAtIndex:index options:options];
}

CFDictionaryRef CGImageSourceCopyPropertiesAtIndex(CGImageSourceRef self, size_t index,CFDictionaryRef options) {
   return (CFDictionaryRef)[(O2ImageSource *)self copyPropertiesAtIndex:index options:options];
}

CFStringRef CGImageSourceGetType(CGImageSourceRef self)
{
    return [(O2ImageSource *)self type];
}

