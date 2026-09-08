#import <Onyx2D/O2Font_ttf.h>
#import <Onyx2D/O2TTFDecoder.h>
#import <Onyx2D/O2DataProvider.h>
#import <Onyx2D/O2Font_freetype.h>

#include <fontconfig/fontconfig.h>


@implementation O2Font_ttf

-initWithDataProvider:(O2DataProviderRef)provider {
   O2TTFDecoderRef decoder=O2TTFDecoderCreate(provider);
   
   _nameToGlyph=O2TTFDecoderGetPostScriptNameMapTable(decoder,&_numberOfGlyphs);
   _glyphLocations=O2TTFDecoderGetGlyphLocations(decoder,_numberOfGlyphs);
   return self;
}

-(O2Glyph)glyphWithGlyphName:(NSString *)name {
   return (O2Glyph)(int)NSMapGet(_nameToGlyph,name);
}

O2FontRef O2FontCreateWithFontName_platform(NSString *name) {
#ifdef FREETYPE_PRESENT
   FcPattern *pattern=FcNameParse((const FcChar8 *)[name UTF8String]);
   FcPattern *match=nil;
   FcChar8 *filename=nil;
   O2FontRef result=nil;

   if(pattern==nil)
    return nil;

   FcConfigSubstitute(NULL,pattern,FcMatchPattern);
   FcDefaultSubstitute(pattern);

   FcResult matchResult;
   match=FcFontMatch(NULL,pattern,&matchResult);
   if(match!=nil &&
      FcPatternGetString(match,FC_FILE,0,&filename)==FcResultMatch){
    O2DataProviderRef provider=O2DataProviderCreateWithFilename((const char *)filename);

    result=[[O2Font_freetype alloc] initWithDataProvider:provider];
    O2DataProviderRelease(provider);
   }

   if(match!=nil)
    FcPatternDestroy(match);
   FcPatternDestroy(pattern);

   return result;
#else
   return nil;
#endif
}

O2FontRef O2FontCreateWithDataProvider_platform(O2DataProviderRef provider) {
   return nil;
}

@end
