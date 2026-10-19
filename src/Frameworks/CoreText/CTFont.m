/* Copyright (c) 2008 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#import <CoreText/CTFont.h>
#import "KTFont.h"
#import "CTInternal.h"
#include FT_TRUETYPE_TABLES_H
#include FT_MULTIPLE_MASTERS_H
#include FT_TRUETYPE_IDS_H
#include <hb-ot.h>

#pragma mark creation

static CTFontRef createWithAttributes(CFDictionaryRef attributes,CGFloat size,const CGAffineTransform *matrix) {
   const CTFontRecord **matches=NULL;
   CFIndex count=CTRegistryCopyMatches(attributes,&matches);
   const CTFontRecord *record=count>0 ? matches[0] : NULL;
   CFNumberRef sizeAttribute=attributes ? CFDictionaryGetValue(attributes,kCTFontSizeAttribute) : NULL;
   CTFontRef font;

   free(matches);
   if(size<=0 && sizeAttribute!=NULL)
    CFNumberGetValue(sizeAttribute,kCFNumberCGFloatType,&size);
   // nothing matched: the system face stands in, as Apple's fallback font does
   if(record==NULL)
    record=CTRegistryDefaultRecord(false);
   if(record==NULL)
    return NULL;
   font=CTFontCreateWithRecord(record,size,matrix,attributes);
   return font;
}

CTFontRef CTFontCreateWithName(CFStringRef name,CGFloat size,const CGAffineTransform *matrix) {
   const void *key=kCTFontNameAttribute;
   CFDictionaryRef attributes;
   CTFontRef font;

   if(name==NULL)
    return createWithAttributes(NULL,size,matrix);
   attributes=CFDictionaryCreate(NULL,&key,(const void **)&name,1,&kCFTypeDictionaryKeyCallBacks,&kCFTypeDictionaryValueCallBacks);
   font=createWithAttributes(attributes,size,matrix);
   CFRelease(attributes);
   return font;
}

CTFontRef CTFontCreateWithNameAndOptions(CFStringRef name,CGFloat size,const CGAffineTransform *matrix,CTFontOptions options) {
   return CTFontCreateWithName(name,size,matrix);
}

CTFontRef CTFontCreateWithFontDescriptor(CTFontDescriptorRef descriptor,CGFloat size,const CGAffineTransform *matrix) {
   CFDictionaryRef attributes=descriptor ? CTFontDescriptorCopyAttributes(descriptor) : NULL;
   CTFontRef font=createWithAttributes(attributes,size,matrix);

   if(attributes) CFRelease(attributes);
   return font;
}

CTFontRef CTFontCreateWithFontDescriptorAndOptions(CTFontDescriptorRef descriptor,CGFloat size,const CGAffineTransform *matrix,CTFontOptions options) {
   return CTFontCreateWithFontDescriptor(descriptor,size,matrix);
}

CTFontRef CTFontCreateWithGraphicsFont(CGFontRef cgFont,CGFloat size,const CGAffineTransform *matrix,CTFontDescriptorRef attributes) {
   KTFont *font=[[KTFont alloc] initWithFont:cgFont size:size>0 ? size : CTDefaultFontSize];

   if(matrix!=NULL)
    font->_matrix=*matrix;
   if(attributes!=NULL)
    font->_ctAttributes=CTFontDescriptorCopyAttributes(attributes);
   return font;
}

CTFontRef CTFontCreateUIFontForLanguage(CTFontUIFontType uiFontType,CGFloat size,CFStringRef language) {
   KTFont *font=[[KTFont alloc] initWithUIFontType:uiFontType size:size language:(NSString *)language];

   // AppKit's KTFont_FT only knows a few UI types, the rest come from the registry
   if(font==nil){
    bool bold=uiFontType==kCTFontEmphasizedSystemFontType || uiFontType==kCTFontSmallEmphasizedSystemFontType ||
              uiFontType==kCTFontMiniEmphasizedSystemFontType || uiFontType==kCTFontWindowTitleFontType ||
              uiFontType==kCTFontAlertHeaderFontType || uiFontType==kCTFontEmphasizedSystemDetailFontType;
    const CTFontRecord *record=NULL;

    if(uiFontType==kCTFontUserFixedPitchFontType){
     const void *key=kCTFontTraitsAttribute;
     uint32_t mono=kCTFontTraitMonoSpace;
     CFDictionaryRef traits=CTCopyTraitsDictionary(mono,0,0,0);
     CFDictionaryRef query=CFDictionaryCreate(NULL,&key,(const void **)&traits,1,&kCFTypeDictionaryKeyCallBacks,&kCFTypeDictionaryValueCallBacks);
     const CTFontRecord **matches=NULL;

     if(CTRegistryCopyMatches(query,&matches)>0)
      record=matches[0];
     free(matches);
     CFRelease(query);
     CFRelease(traits);
    }
    if(record==NULL)
     record=CTRegistryDefaultRecord(bold);
    if(record!=NULL)
     font=CTFontCreateWithRecord(record,size>0 ? size : CTDefaultFontSize,NULL,NULL);
   }
   return font;
}

CTFontRef CTFontCreateCopyWithAttributes(CTFontRef font,CGFloat size,const CGAffineTransform *matrix,CTFontDescriptorRef attributes) {
   CTFontDescriptorRef base=CTFontCopyFontDescriptor(font);
   CFDictionaryRef extra=attributes ? CTFontDescriptorCopyAttributes(attributes) : NULL;
   CTFontDescriptorRef merged=extra ? CTFontDescriptorCreateCopyWithAttributes(base,extra) : (CTFontDescriptorRef)CFRetain(base);
   CTFontRef copy;

   if(extra) CFRelease(extra);

   if(size<=0)
    size=CTFontGetSize(font);
   if(matrix==NULL){
    CGAffineTransform m=CTFontGetMatrix(font);

    copy=CTFontCreateWithFontDescriptor(merged,size,&m);
   }
   else
    copy=CTFontCreateWithFontDescriptor(merged,size,matrix);
   CFRelease(merged);
   CFRelease(base);
   return copy;
}

CTFontRef CTFontCreateCopyWithSymbolicTraits(CTFontRef font,CGFloat size,const CGAffineTransform *matrix,CTFontSymbolicTraits value,CTFontSymbolicTraits mask) {
   CTFontDescriptorRef base=CTFontCopyFontDescriptor(font);
   CTFontDescriptorRef wanted=CTFontDescriptorCreateCopyWithSymbolicTraits(base,(CTFontGetSymbolicTraits(font)&~mask)|(value&mask),mask|kCTFontTraitBold|kCTFontTraitItalic);
   CFArrayRef matches=CTFontDescriptorCreateMatchingFontDescriptors(wanted,NULL);
   CTFontRef copy=NULL;
   CTFontSymbolicTraits want=(CTFontGetSymbolicTraits(font)&~mask)|(value&mask);

   // Apple returns NULL when the family has no face with the traits asked for
   for(CFIndex i=0;matches!=NULL && i<CFArrayGetCount(matches) && copy==NULL;i++){
    CTFontDescriptorRef d=CFArrayGetValueAtIndex(matches,i);
    CFDictionaryRef traits=CTFontDescriptorCopyAttribute(d,kCTFontTraitsAttribute);
    uint32_t symbolic=0;

    if(traits!=NULL){
     CFNumberRef n=CFDictionaryGetValue(traits,kCTFontSymbolicTrait);

     if(n) CFNumberGetValue(n,kCFNumberSInt32Type,&symbolic);
     CFRelease(traits);
    }
    if((symbolic&mask&(kCTFontTraitBold|kCTFontTraitItalic|kCTFontTraitMonoSpace|kCTFontTraitCondensed|kCTFontTraitExpanded))==(want&mask&(kCTFontTraitBold|kCTFontTraitItalic|kCTFontTraitMonoSpace|kCTFontTraitCondensed|kCTFontTraitExpanded))){
     CGAffineTransform m=matrix ? *matrix : CTFontGetMatrix(font);

     copy=CTFontCreateWithFontDescriptor(d,size>0 ? size : CTFontGetSize(font),&m);
    }
   }
   if(matches) CFRelease(matches);
   CFRelease(wanted);
   CFRelease(base);
   return copy;
}

CTFontRef CTFontCreateCopyWithFamily(CTFontRef font,CGFloat size,const CGAffineTransform *matrix,CFStringRef family) {
   CTFontDescriptorRef base=CTFontCopyFontDescriptor(font);
   CTFontDescriptorRef wanted=CTFontDescriptorCreateCopyWithFamily(base,family);
   CTFontDescriptorRef match=CTFontDescriptorCreateMatchingFontDescriptor(wanted,NULL);
   CTFontRef copy=NULL;

   if(match!=NULL){
    CGAffineTransform m=matrix ? *matrix : CTFontGetMatrix(font);

    copy=CTFontCreateWithFontDescriptor(match,size>0 ? size : CTFontGetSize(font),&m);
    CFRelease(match);
   }
   CFRelease(wanted);
   CFRelease(base);
   return copy;
}

bool CTFontHasCharacter(CTFontRef font,UTF32Char c) {
   CTFace *face=[font _ctFace];
   bool has;

   if(face==NULL)
    return false;
   pthread_mutex_lock(&face->lock);
   has=FT_Get_Char_Index(face->ft,c)!=0;
   pthread_mutex_unlock(&face->lock);
   return has;
}

CTFontRef CTFontCreateForStringWithLanguage(CTFontRef current,CFStringRef string,CFRange range,CFStringRef language) {
   CFIndex length=string ? CFStringGetLength(string) : 0;
   UniChar buffer[2];
   UTF32Char c;
   CFIndex units;
   CTFace *face;
   const CTFontRecord *preferred,*fallback;

   if(range.location<0 || range.length<=0 || range.location>=length)
    return current ? (CTFontRef)CFRetain(current) : NULL;
   buffer[0]=CFStringGetCharacterAtIndex(string,range.location);
   buffer[1]=range.location+1<length ? CFStringGetCharacterAtIndex(string,range.location+1) : 0;
   c=CTCodePointAt(buffer,range.location+1<length ? 2 : 1,0,&units);
   if(current!=NULL && CTFontHasCharacter(current,c))
    return (CTFontRef)CFRetain(current);
   face=current ? [current _ctFace] : NULL;
   preferred=face ? CTRegistryRecordForPath(face->path,face->index) : NULL;
   fallback=CTRegistryFallbackForCharacter(c,language,preferred);
   if(fallback==NULL)
    return current ? (CTFontRef)CFRetain(current) : NULL;
   return CTFontCreateWithRecord(fallback,current ? CTFontGetSize(current) : CTDefaultFontSize,NULL,NULL);
}

CTFontRef CTFontCreateForString(CTFontRef current,CFStringRef string,CFRange range) {
   return CTFontCreateForStringWithLanguage(current,string,range,NULL);
}

#pragma mark descriptors and attributes

CTFontDescriptorRef CTFontCopyFontDescriptor(CTFontRef font) {
   return [font _ctDescriptor];
}

CFTypeRef CTFontCopyAttribute(CTFontRef font,CFStringRef attribute) {
   CTFontDescriptorRef d=CTFontCopyFontDescriptor(font);
   CFTypeRef value=CTFontDescriptorCopyAttribute(d,attribute);

   CFRelease(d);
   return value;
}

CGFloat CTFontGetSize(CTFontRef self) {
   return [self pointSize];
}

CGAffineTransform CTFontGetMatrix(CTFontRef font) {
   return [font _ctMatrix];
}

CTFontSymbolicTraits CTFontGetSymbolicTraits(CTFontRef font) {
   CTFace *face=[font _ctFace];
   const CTFontRecord *record=face ? CTRegistryRecordForPath(face->path,face->index) : NULL;

   if(record!=NULL)
    return record->symbolicTraits;
   if(face==NULL)
    return 0;

   CTFontSymbolicTraits traits=0;

   if(face->ft->style_flags&FT_STYLE_FLAG_ITALIC) traits|=kCTFontTraitItalic;
   if(face->ft->style_flags&FT_STYLE_FLAG_BOLD) traits|=kCTFontTraitBold;
   if(FT_IS_FIXED_WIDTH(face->ft)) traits|=kCTFontTraitMonoSpace;
   return traits;
}

CFDictionaryRef CTFontCopyTraits(CTFontRef font) {
   CTFace *face=[font _ctFace];
   const CTFontRecord *record=face ? CTRegistryRecordForPath(face->path,face->index) : NULL;

   if(record!=NULL)
    return CTCopyTraitsDictionary(record->symbolicTraits,record->weight,record->width,record->slant);
   return CTCopyTraitsDictionary(CTFontGetSymbolicTraits(font),0,0,0);
}

// fonts that cover what this one doesn't, best first for the given languages
CFArrayRef CTFontCopyDefaultCascadeListForLanguages(CTFontRef font,CFArrayRef languages) {
   static const UTF32Char samples[]={ 0x4E00, 0x3042, 0xAC00, 0x0627, 0x05D0, 0x0E01, 0x0905, 0x0410, 0x03B1, 0x2603, 0x1F600 };
   CFMutableArrayRef result=CFArrayCreateMutable(NULL,0,&kCFTypeArrayCallBacks);
   CFMutableSetRef seen=CFSetCreateMutable(NULL,0,&kCFTypeSetCallBacks);
   CTFace *face=[font _ctFace];
   const CTFontRecord *preferred=face ? CTRegistryRecordForPath(face->path,face->index) : NULL;
   CFStringRef language=(languages && CFArrayGetCount(languages)>0) ? CFArrayGetValueAtIndex(languages,0) : NULL;

   if(face && face->postScriptName)
    CFSetAddValue(seen,face->postScriptName);
   for(size_t i=0;i<sizeof(samples)/sizeof(samples[0]);i++){
    const CTFontRecord *r=CTRegistryFallbackForCharacter(samples[i],language,preferred);

    if(r==NULL || CFSetContainsValue(seen,r->postScriptName))
     continue;
    CFSetAddValue(seen,r->postScriptName);

    CFDictionaryRef attrs=CTRecordCopyAttributes(r);
    CTFontDescriptorRef d=CTFontDescriptorCreateWithAttributes(attrs);

    CFArrayAppendValue(result,d);
    CFRelease(d);
    CFRelease(attrs);
   }
   CFRelease(seen);
   return result;
}

#pragma mark names

static CFStringRef faceString(CTFontRef font,int which) {
   CTFace *face=[font _ctFace];
   CFStringRef s=NULL;

   if(face==NULL)
    return which==0 ? [font copyName] : NULL;
   switch(which){
    case 0: s=face->fullName; break;
    case 1: s=face->postScriptName; break;
    case 2: s=face->familyName; break;
    case 3: s=face->styleName; break;
   }
   return s ? CFRetain(s) : NULL;
}

CFStringRef CTFontCopyFullName(CTFontRef self) {
   return faceString(self,0);
}

CFStringRef CTFontCopyPostScriptName(CTFontRef font) {
   return faceString(font,1);
}

CFStringRef CTFontCopyFamilyName(CTFontRef font) {
   return faceString(font,2);
}

CFStringRef CTFontCopyDisplayName(CTFontRef font) {
   return faceString(font,0);
}

static int nameIDForKey(CFStringRef key) {
   static const struct { CFStringRef *key; int nameID; } table[]={
    { (CFStringRef *)&kCTFontCopyrightNameKey,0 },{ (CFStringRef *)&kCTFontFamilyNameKey,1 },
    { (CFStringRef *)&kCTFontSubFamilyNameKey,2 },{ (CFStringRef *)&kCTFontStyleNameKey,2 },
    { (CFStringRef *)&kCTFontUniqueNameKey,3 },{ (CFStringRef *)&kCTFontFullNameKey,4 },
    { (CFStringRef *)&kCTFontVersionNameKey,5 },{ (CFStringRef *)&kCTFontPostScriptNameKey,6 },
    { (CFStringRef *)&kCTFontTrademarkNameKey,7 },{ (CFStringRef *)&kCTFontManufacturerNameKey,8 },
    { (CFStringRef *)&kCTFontDesignerNameKey,9 },{ (CFStringRef *)&kCTFontDescriptionNameKey,10 },
    { (CFStringRef *)&kCTFontVendorURLNameKey,11 },{ (CFStringRef *)&kCTFontDesignerURLNameKey,12 },
    { (CFStringRef *)&kCTFontLicenseNameKey,13 },{ (CFStringRef *)&kCTFontLicenseURLNameKey,14 },
    { (CFStringRef *)&kCTFontSampleTextNameKey,19 },{ (CFStringRef *)&kCTFontPostScriptCIDNameKey,20 },
   };

   for(size_t i=0;i<sizeof(table)/sizeof(table[0]);i++)
    if(key!=NULL && CFEqual(key,*table[i].key))
     return table[i].nameID;
   return -1;
}

CFStringRef CTFontCopyName(CTFontRef font,CFStringRef nameKey) {
   CTFace *face=[font _ctFace];
   int nameID=nameIDForKey(nameKey);
   CFStringRef s;

   if(face==NULL || nameID<0)
    return NULL;
   // the typographic family and style names win over the legacy four-style ones
   if(nameID==1 && face->familyName) return CFRetain(face->familyName);
   if(nameID==2 && face->styleName) return CFRetain(face->styleName);
   if(nameID==6 && face->postScriptName) return CFRetain(face->postScriptName);
   s=CTFaceCopyName(face,nameID);
   return s;
}

CFStringRef CTFontCopyLocalizedName(CTFontRef font,CFStringRef nameKey,CFStringRef *actualLanguage) {
   CFStringRef s=CTFontCopyName(font,nameKey);

   if(actualLanguage)
    *actualLanguage=s ? CFSTR("en") : NULL;
   return s;
}

#pragma mark encoding and coverage

CFCharacterSetRef CTFontCopyCharacterSet(CTFontRef font) {
   CTFace *face=[font _ctFace];
   CFMutableCharacterSetRef set=CFCharacterSetCreateMutable(NULL);
   FT_ULong c;
   FT_UInt glyph;

   if(face!=NULL){
    pthread_mutex_lock(&face->lock);
    c=FT_Get_First_Char(face->ft,&glyph);
    while(glyph!=0){
     FT_ULong start=c,end=c;

     // runs of consecutive code points go in as ranges
     for(;;){
      FT_ULong next=FT_Get_Next_Char(face->ft,end,&glyph);

      if(glyph==0 || next!=end+1){
       CFCharacterSetAddCharactersInRange(set,CFRangeMake(start,end-start+1));
       c=next;
       break;
      }
      end=next;
     }
    }
    pthread_mutex_unlock(&face->lock);
   }

   CFCharacterSetRef result=CFCharacterSetCreateCopy(NULL,set);

   CFRelease(set);
   return result;
}

CFStringEncoding CTFontGetStringEncoding(CTFontRef font) {
   return kCFStringEncodingUnicode;
}

CFArrayRef CTFontCopySupportedLanguages(CTFontRef font) {
   static const struct { const char *code; UTF32Char probe; } languages[]={
    { "en",'e' },{ "de",0x00DF },{ "fr",0x00E9 },{ "es",0x00F1 },{ "pl",0x0142 },{ "ru",0x0416 },
    { "el",0x03A9 },{ "he",0x05D0 },{ "ar",0x0627 },{ "th",0x0E01 },{ "hi",0x0905 },{ "ja",0x3042 },
    { "zh",0x4E2D },{ "ko",0xAC00 },{ "vi",0x1EA1 },{ "tr",0x011F },
   };
   CFMutableArrayRef result=CFArrayCreateMutable(NULL,0,&kCFTypeArrayCallBacks);

   for(size_t i=0;i<sizeof(languages)/sizeof(languages[0]);i++){
    if(CTFontHasCharacter(font,languages[i].probe)){
     CFStringRef code=CFStringCreateWithCString(NULL,languages[i].code,kCFStringEncodingASCII);

     CFArrayAppendValue(result,code);
     CFRelease(code);
    }
   }
   return result;
}

bool CTFontGetGlyphsForCharacters(CTFontRef font,const UniChar *characters,CGGlyph *glyphs,CFIndex count) {
   CTFace *face=[font _ctFace];
   bool all=true;

   if(face==NULL){
    [font getGlyphs:glyphs forCharacters:characters length:(unsigned)count];
    for(CFIndex i=0;i<count;i++)
     if(glyphs[i]==0) all=false;
    return all;
   }
   pthread_mutex_lock(&face->lock);
   for(CFIndex i=0;i<count;i++){
    CFIndex units;
    UTF32Char c=CTCodePointAt(characters,count,i,&units);

    // a surrogate pair's glyph goes at the high surrogate, its low surrogate gets 0, as on Apple
    glyphs[i]=FT_Get_Char_Index(face->ft,c);
    if(glyphs[i]==0)
     all=false;
    if(units==2){
     glyphs[i+1]=0;
     i++;
    }
   }
   pthread_mutex_unlock(&face->lock);
   return all;
}

#pragma mark metrics

CGFloat CTFontGetAscent(CTFontRef self) {
   return [self ascender];
}

CGFloat CTFontGetDescent(CTFontRef self) {
   return [self descender];
}

CGFloat CTFontGetLeading(CTFontRef self) {
   return [self leading];
}

unsigned CTFontGetUnitsPerEm(CTFontRef font) {
   CTFace *face=[font _ctFace];

   return face ? (unsigned)face->unitsPerEm : (unsigned)CGFontGetUnitsPerEm([font graphicsFont]);
}

CFIndex CTFontGetGlyphCount(CTFontRef self) {
   return [self numberOfGlyphs];
}

CGRect CTFontGetBoundingBox(CTFontRef self) {
   return [self boundingRect];
}

CGFloat CTFontGetUnderlinePosition(CTFontRef self) {
   return [self underlinePosition];
}

CGFloat CTFontGetUnderlineThickness(CTFontRef self) {
   return [self underlineThickness];
}

CGFloat CTFontGetSlantAngle(CTFontRef self) {
   return [self italicAngle];
}

CGFloat CTFontGetXHeight(CTFontRef self) {
   return [self xHeight];
}

CGFloat CTFontGetCapHeight(CTFontRef self) {
   return [self capHeight];
}

#pragma mark glyphs

CGGlyph CTFontGetGlyphWithName(CTFontRef font,CFStringRef glyphName) {
   CTFace *face=[font _ctFace];
   char name[256];
   CGGlyph glyph=0;

   if(face==NULL || !CFStringGetCString(glyphName,name,sizeof(name),kCFStringEncodingUTF8))
    return 0;
   pthread_mutex_lock(&face->lock);
   glyph=FT_Get_Name_Index(face->ft,name);
   pthread_mutex_unlock(&face->lock);
   return glyph;
}

CFStringRef CTFontCopyNameForGlyph(CTFontRef font,CGGlyph glyph) {
   CTFace *face=[font _ctFace];
   char name[256];
   CFStringRef result=NULL;

   if(face==NULL)
    return NULL;
   pthread_mutex_lock(&face->lock);
   if(FT_HAS_GLYPH_NAMES(face->ft) && FT_Get_Glyph_Name(face->ft,glyph,name,sizeof(name))==0)
    result=CFStringCreateWithCString(NULL,name,kCFStringEncodingUTF8);
   pthread_mutex_unlock(&face->lock);
   return result;
}

CGRect CTFontGetBoundingRectsForGlyphs(CTFontRef font,CTFontOrientation orientation,const CGGlyph *glyphs,CGRect *rects,CFIndex count) {
   CTFace *face=[font _ctFace];
   CGRect total=CGRectNull;
   CGFloat s;

   if(face==NULL){
    for(CFIndex i=0;rects && i<count;i++)
     rects[i]=CGRectZero;
    return CGRectZero;
   }
   s=CTFontGetSize(font)/face->unitsPerEm;
   pthread_mutex_lock(&face->lock);
   for(CFIndex i=0;i<count;i++){
    CGRect r=CGRectZero;

    if(FT_Load_Glyph(face->ft,glyphs[i],FT_LOAD_NO_SCALE|FT_LOAD_NO_HINTING|FT_LOAD_NO_BITMAP)==0){
     FT_Glyph_Metrics *m=&face->ft->glyph->metrics;

     r=CGRectMake(m->horiBearingX*s,(m->horiBearingY-m->height)*s,m->width*s,m->height*s);
    }
    if(rects)
     rects[i]=r;
    if(r.size.width>0 || r.size.height>0)
     total=CGRectIsNull(total) ? r : CGRectUnion(total,r);
   }
   pthread_mutex_unlock(&face->lock);
   return CGRectIsNull(total) ? CGRectZero : total;
}

CGRect CTFontGetOpticalBoundsForGlyphs(CTFontRef font,const CGGlyph *glyphs,CGRect *rects,CFIndex count,CFOptionFlags options) {
   CGSize advances[count>0 ? count : 1];
   CGFloat ascent=CTFontGetAscent(font),descent=CTFontGetDescent(font);
   CGRect total=CGRectZero;

   // without an 'opbd' table the optical bounds are the advance boxes
   CTFontGetAdvancesForGlyphs(font,kCTFontOrientationHorizontal,glyphs,advances,count);
   for(CFIndex i=0;i<count;i++){
    CGRect r=CGRectMake(0,-descent,advances[i].width,ascent+descent);

    if(rects)
     rects[i]=r;
    total=i==0 ? r : CGRectUnion(total,r);
   }
   return total;
}

double CTFontGetAdvancesForGlyphs(CTFontRef font,CTFontOrientation orientation,const CGGlyph *glyphs,CGSize *advances,CFIndex count) {
   CGSize local[count>0 ? count : 1];
   CGSize *out=advances ? advances : local;
   double total=0;

   [font getAdvancements:out forGlyphs:glyphs count:(unsigned)count];
   for(CFIndex i=0;i<count;i++)
    total+=out[i].width;
   return total;
}

void CTFontGetVerticalTranslationsForGlyphs(CTFontRef font,const CGGlyph *glyphs,CGSize *translations,CFIndex count) {
   CGFloat ascent=CTFontGetAscent(font);
   CGSize advances[count>0 ? count : 1];

   // the translation that centers a glyph on the vertical baseline
   CTFontGetAdvancesForGlyphs(font,kCTFontOrientationHorizontal,glyphs,advances,count);
   for(CFIndex i=0;i<count;i++)
    translations[i]=CGSizeMake(-advances[i].width/2,-ascent);
}

CGPathRef CTFontCreatePathForGlyph(CTFontRef self,CGGlyph glyph,const CGAffineTransform *xform) {
   return (CGPathRef)[self createPathForGlyph:glyph transform:(CGAffineTransform *)xform];
}

// Onyx2D has no positioned glyph call, so positions become advances from the first one
void CTFontDrawGlyphs(CTFontRef font,const CGGlyph *glyphs,const CGPoint *positions,size_t count,CGContextRef context) {
   CGFontRef graphics=[font graphicsFont];
   CGSize advances[count>0 ? count : 1];

   if(count==0 || graphics==NULL || context==NULL)
    return;
   for(size_t i=0;i+1<count;i++)
    advances[i]=CGSizeMake(positions[i+1].x-positions[i].x,positions[i+1].y-positions[i].y);
   advances[count-1]=CGSizeZero;
   // PD_CT_TRACE=1: what an app draws and how many bitmap bytes it changed, for ports that show no text
   static int trace=-1;
   uint8_t *data=NULL;
   size_t size=0;
   uint64_t before=0;

   if(trace<0)
    trace=getenv("PD_CT_TRACE")!=NULL;
   if(trace){
    data=CGBitmapContextGetData(context);
    size=data ? CGBitmapContextGetHeight(context)*CGBitmapContextGetBytesPerRow(context) : 0;
    for(size_t i=0;i<size;i++)
     before+=data[i];
   }
   CGContextSaveGState(context);
   CGContextSetFont(context,graphics);
   CGContextSetFontSize(context,CTFontGetSize(font));
   CGContextSetTextPosition(context,positions[0].x,positions[0].y);
   CGContextShowGlyphsWithAdvances(context,glyphs,advances,(unsigned)count);
   CGContextRestoreGState(context);
   if(trace){
    uint64_t after=0;
    CFStringRef name=CTFontCopyPostScriptName(font);
    char text[128]="?";

    for(size_t i=0;i<size;i++)
     after+=data[i];
    if(name){
     CFStringGetCString(name,text,sizeof(text),kCFStringEncodingUTF8);
     CFRelease(name);
    }
    fprintf(stderr,"PD-CT draw %s size %g glyphs %zu first %u at %g,%g ctx %p info 0x%x bytes %zu sum %llu -> %llu\n",text,CTFontGetSize(font),count,glyphs[0],positions[0].x,positions[0].y,context,
     data ? (unsigned)CGBitmapContextGetBitmapInfo(context) : 0,size,(unsigned long long)before,(unsigned long long)after);
    // the first few glyph bitmaps as ASCII, one char per pixel from the 32-bit pixel's channels
    static int dumped;
    if(data && dumped<6 && CGBitmapContextGetBytesPerRow(context)>=4*CGBitmapContextGetWidth(context)){
     size_t w=CGBitmapContextGetWidth(context),h=CGBitmapContextGetHeight(context),bpr=CGBitmapContextGetBytesPerRow(context);

     dumped++;
     for(size_t y=0;y<h && y<40;y++){
      char line[81];
      size_t x;

      for(x=0;x<w && x<80;x++){
       uint8_t *px=data+y*bpr+x*4;
       int lum=(px[0]+px[1]+px[2])/3;
       line[x]=lum>200 ? '#' : lum>100 ? '+' : lum>30 ? '-' : '.';
      }
      line[x]=0;
      fprintf(stderr,"PD-CT px %zux%zu a0=%02x |%s|\n",w,h,data[y*bpr+3],line);
     }
    }
   }
}

CFIndex CTFontGetLigatureCaretPositions(CTFontRef font,CGGlyph glyph,CGFloat *positions,CFIndex maxPositions) {
   hb_font_t *hb=[font _hbFont];
   CTFace *face=[font _ctFace];
   hb_position_t carets[32];
   unsigned int count=32;

   if(hb==NULL || face==NULL)
    return 0;
   hb_ot_layout_get_ligature_carets(hb,HB_DIRECTION_LTR,glyph,0,&count,carets);
   for(unsigned int i=0;positions && i<count && (CFIndex)i<maxPositions;i++)
    positions[i]=carets[i]*CTFontGetSize(font)/face->unitsPerEm;
   return count;
}

#pragma mark variations, features and tables

CFArrayRef CTFontCopyVariationAxes(CTFontRef font) {
   CTFace *face=[font _ctFace];
   FT_MM_Var *mm=NULL;
   CFMutableArrayRef axes;

   if(face==NULL || !FT_HAS_MULTIPLE_MASTERS(face->ft))
    return NULL;
   pthread_mutex_lock(&face->lock);
   if(FT_Get_MM_Var(face->ft,&mm)!=0){
    pthread_mutex_unlock(&face->lock);
    return NULL;
   }
   axes=CFArrayCreateMutable(NULL,mm->num_axis,&kCFTypeArrayCallBacks);
   for(FT_UInt i=0;i<mm->num_axis;i++){
    FT_Var_Axis *a=&mm->axis[i];
    uint32_t tag=(uint32_t)a->tag;
    double minimum=a->minimum/65536.0,maximum=a->maximum/65536.0,def=a->def/65536.0;
    CFNumberRef numbers[4]={ CFNumberCreate(NULL,kCFNumberSInt32Type,&tag),CFNumberCreate(NULL,kCFNumberDoubleType,&minimum),
                            CFNumberCreate(NULL,kCFNumberDoubleType,&maximum),CFNumberCreate(NULL,kCFNumberDoubleType,&def) };
    CFStringRef name=a->name ? CFStringCreateWithCString(NULL,a->name,kCFStringEncodingUTF8) : CFSTR("");
    const void *keys[5]={ kCTFontVariationAxisIdentifierKey,kCTFontVariationAxisMinimumValueKey,kCTFontVariationAxisMaximumValueKey,
                          kCTFontVariationAxisDefaultValueKey,kCTFontVariationAxisNameKey };
    const void *values[5]={ numbers[0],numbers[1],numbers[2],numbers[3],name };
    CFDictionaryRef axis=CFDictionaryCreate(NULL,keys,values,5,&kCFTypeDictionaryKeyCallBacks,&kCFTypeDictionaryValueCallBacks);

    CFArrayAppendValue(axes,axis);
    CFRelease(axis);
    CFRelease(name);
    for(int j=0;j<4;j++)
     CFRelease(numbers[j]);
   }
   FT_Done_MM_Var(CTFreeTypeLibrary(),mm);
   pthread_mutex_unlock(&face->lock);
   return axes;
}

CFDictionaryRef CTFontCopyVariation(CTFontRef font) {
   CFDictionaryRef variation=CTFontCopyAttribute(font,kCTFontVariationAttribute);

   return variation;
}

// the OpenType features in GSUB and GPOS, each as a feature type with one on selector
CFArrayRef CTFontCopyFeatures(CTFontRef font) {
   hb_font_t *hb=[font _hbFont];
   CFMutableArrayRef result=CFArrayCreateMutable(NULL,0,&kCFTypeArrayCallBacks);
   CFMutableSetRef seen=CFSetCreateMutable(NULL,0,&kCFTypeSetCallBacks);
   hb_tag_t tables[2]={ HB_OT_TAG_GSUB,HB_OT_TAG_GPOS };

   if(hb==NULL){
    CFRelease(seen);
    return result;
   }
   for(int t=0;t<2;t++){
    hb_tag_t tags[256];
    unsigned int count=256;

    hb_ot_layout_table_get_feature_tags(hb_font_get_face(hb),tables[t],0,&count,tags);
    for(unsigned int i=0;i<count;i++){
     char tag[5];
     CFStringRef tagString;

     hb_tag_to_string(tags[i],tag);
     tag[4]=0;
     tagString=CFStringCreateWithCString(NULL,tag,kCFStringEncodingASCII);
     if(!CFSetContainsValue(seen,tagString)){
      const void *keys[2]={ kCTFontOpenTypeFeatureTag,kCTFontFeatureTypeNameKey };
      const void *values[2]={ tagString,tagString };
      CFDictionaryRef feature=CFDictionaryCreate(NULL,keys,values,2,&kCFTypeDictionaryKeyCallBacks,&kCFTypeDictionaryValueCallBacks);

      CFSetAddValue(seen,tagString);
      CFArrayAppendValue(result,feature);
      CFRelease(feature);
     }
     CFRelease(tagString);
    }
   }
   CFRelease(seen);
   return result;
}

CFArrayRef CTFontCopyFeatureSettings(CTFontRef font) {
   CFArrayRef settings=CTFontCopyAttribute(font,kCTFontFeatureSettingsAttribute);

   return settings ? settings : CFArrayCreate(NULL,NULL,0,&kCFTypeArrayCallBacks);
}

CGFontRef CTFontCopyGraphicsFont(CTFontRef font,CTFontDescriptorRef *attributes) {
   CGFontRef graphics=[font graphicsFont];

   if(attributes)
    *attributes=NULL;
   return graphics ? CGFontRetain(graphics) : NULL;
}

CFArrayRef CTFontCopyAvailableTables(CTFontRef font,CTFontTableOptions options) {
   CTFace *face=[font _ctFace];
   CFMutableArrayRef result;
   FT_ULong length=0,tag;

   if(face==NULL || !FT_IS_SFNT(face->ft))
    return NULL;
   result=CFArrayCreateMutable(NULL,0,NULL);
   pthread_mutex_lock(&face->lock);
   for(FT_UInt i=0;FT_Sfnt_Table_Info(face->ft,i,&tag,&length)==0;i++)
    CFArrayAppendValue(result,(const void *)(uintptr_t)tag);
   pthread_mutex_unlock(&face->lock);
   return result;
}

CFDataRef CTFontCopyTable(CTFontRef font,CTFontTableTag table,CTFontTableOptions options) {
   CTFace *face=[font _ctFace];
   FT_ULong length=0;
   UInt8 *bytes;
   CFDataRef data=NULL;

   if(face==NULL || !FT_IS_SFNT(face->ft))
    return NULL;
   pthread_mutex_lock(&face->lock);
   if(FT_Load_Sfnt_Table(face->ft,table,0,NULL,&length)==0 && length>0){
    bytes=malloc(length);
    if(FT_Load_Sfnt_Table(face->ft,table,0,bytes,&length)==0)
     data=CFDataCreateWithBytesNoCopy(NULL,bytes,length,kCFAllocatorMalloc);
    else
     free(bytes);
   }
   pthread_mutex_unlock(&face->lock);
   return data;
}
