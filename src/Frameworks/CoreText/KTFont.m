/* Copyright (c) 2006-2008 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <Onyx2D/O2Font.h>
#import <Onyx2D/O2DataProvider.h>
#import "KTFont.h"
#import "CTInternal.h"
#import <Foundation/NSArray.h>
#include FT_OUTLINE_H
#include FT_TRUETYPE_TABLES_H
#include <hb-ot.h>

static CFTypeID fontTypeID;

@implementation KTFont

+(void)initialize {
   if(self==[KTFont class])
    fontTypeID=CTRegisterTypeID("CTFont");
}

-(CFTypeID)_cfTypeID {
   return fontTypeID;
}

CFTypeID CTFontGetTypeID(void) {
   [KTFont class];
   return fontTypeID;
}

-initWithFont:(CGFontRef)font size:(CGFloat)size {
   _font=[(id)font retain];
   _unitsPerEm=CGFontGetUnitsPerEm(_font);
   _size=size;
   _matrix=CGAffineTransformIdentity;
   _fromGraphicsFont=YES;
   return self;
}

-initWithUIFontType:(CTFontUIFontType)uiFontType size:(CGFloat)size language:(NSString *)language {
   NSString *name;

   switch(uiFontType) {
    case kCTFontEmphasizedSystemFontType:
    case kCTFontSmallEmphasizedSystemFontType:
    case kCTFontMiniEmphasizedSystemFontType:
    case kCTFontMenuTitleFontType:
    case kCTFontWindowTitleFontType:
    case kCTFontUtilityWindowTitleFontType:
    case kCTFontAlertHeaderFontType:
    case kCTFontEmphasizedSystemDetailFontType:
     name=@"Inter-Bold";
     break;

    default:
     name=@"Inter-Regular";
     break;
   }

   CGFontRef font=NULL;

   // the fixed-pitch face is whichever monospace family is installed
   if(uiFontType==kCTFontUserFixedPitchFontType){
    NSString *mono[]={ @"Menlo-Regular", @"SourceCodePro-Regular", @"DejaVuSansMono" };

    for(int i=0;i<3 && font==NULL;i++)
     font=CGFontCreateWithFontName((CFStringRef)mono[i]);
   }
   if(font==NULL)
    font=CGFontCreateWithFontName((CFStringRef)name);
   if(font==NULL) {
    [self release];
    return nil;
   }

   self=[self initWithFont:font size:size];
   CGFontRelease(font);
   return self;
}

-(void)dealloc {
   if(_twoLevel!=NULL){
    for(int i=0;i<256;i++){
     free(_twoLevel[i]);
    }
    free(_twoLevel);
   }
   if(_hbFont!=NULL)
    hb_font_destroy((hb_font_t *)_hbFont);
   CTFaceRelease((CTFace *)_ctFace);
   if(_ctAttributes!=NULL)
    CFRelease(_ctAttributes);
   CGFontRelease(_font);
   [super dealloc];
}

-(BOOL)isEqual:(id)other {
   if(other==self)
    return YES;
   if(![other isKindOfClass:[KTFont class]])
    return NO;

   KTFont *font=other;

   return [self _ctFace]==[font _ctFace] && _size==font->_size && memcmp(&_matrix,&font->_matrix,sizeof(_matrix))==0;
}

-(NSUInteger)hash {
   return (NSUInteger)[self _ctFace] ^ (NSUInteger)(_size*64);
}

-(CFStringRef)copyName {
   CTFace *face=[self _ctFace];

   if(face!=NULL && face->fullName!=NULL)
    return CFRetain(face->fullName);
   return CGFontCopyFullName(_font);
}

-(CGFloat)pointSize {
	return _size;
}

-(CGFloat)fontSize {
	return _size;
}

-(CGFontRef)graphicsFont {
   return _font;
}

// the scale from font units to points
static CGFloat unitScale(KTFont *self,CTFace *face) {
   return self->_size/(face!=NULL ? face->unitsPerEm : (self->_unitsPerEm>0 ? self->_unitsPerEm : 1000));
}

-(CGRect)boundingRect {
   CTFace *face=[self _ctFace];

   if(face==NULL)
    return CGRectZero;

   CGFloat s=unitScale(self,face);
   FT_BBox b=face->ft->bbox;

   return CGRectMake(b.xMin*s,b.yMin*s,(b.xMax-b.xMin)*s,(b.yMax-b.yMin)*s);
}

// fonts AppKit made from a graphics font keep the metrics it always had, CoreText's own use the hhea table
-(CGFloat)ascender {
   CTFace *face=_fromGraphicsFont ? NULL : [self _ctFace];

   if(face!=NULL)
    return face->ft->ascender*unitScale(self,face);
   return (CGFontGetAscent(_font)/_unitsPerEm)*_size;
}

// CT descenter is the opposite of the CG one on Cocoa
-(CGFloat)descender {
   CTFace *face=_fromGraphicsFont ? NULL : [self _ctFace];

   if(face!=NULL)
    return -face->ft->descender*unitScale(self,face);
   return -(CGFontGetDescent(_font)/_unitsPerEm)*_size;
}

-(CGFloat)leading {
   CTFace *face=_fromGraphicsFont ? NULL : [self _ctFace];

   if(face!=NULL){
    // hhea's line gap, which FreeType folds into height
    int gap=face->ft->height-(face->ft->ascender-face->ft->descender);

    return (gap>0 ? gap : 0)*unitScale(self,face);
   }
   return (CGFontGetLeading(_font)/_unitsPerEm)*_size;
}

-(CGFloat)underlineThickness {
   CTFace *face=[self _ctFace];

   if(face==NULL)
    return _size/14.0;
   return face->ft->underline_thickness*unitScale(self,face);
}

-(CGFloat)underlinePosition {
   CTFace *face=[self _ctFace];

   if(face==NULL)
    return -_size/10.0;
   return face->ft->underline_position*unitScale(self,face);
}

-(CGFloat)italicAngle {
   CTFace *face=[self _ctFace];

   if(face!=NULL){
    TT_Postscript *post=FT_Get_Sfnt_Table(face->ft,FT_SFNT_POST);

    if(post!=NULL)
     return post->italicAngle/65536.0;
   }
   return CGFontGetItalicAngle(_font);
}

// OS/2 carries both heights in version 2 and later, older fonts measure H and x
static CGFloat glyphTop(CTFace *face,FT_ULong character) {
   FT_UInt glyph=FT_Get_Char_Index(face->ft,character);

   if(glyph==0 || FT_Load_Glyph(face->ft,glyph,FT_LOAD_NO_SCALE|FT_LOAD_NO_HINTING)!=0)
    return 0;
   return face->ft->glyph->metrics.horiBearingY;
}

-(CGFloat)xHeight {
   CTFace *face=[self _ctFace];

   if(face==NULL)
    return (CGFontGetXHeight(_font)/_unitsPerEm)*_size;

   TT_OS2 *os2=FT_Get_Sfnt_Table(face->ft,FT_SFNT_OS2);
   CGFloat units;

   pthread_mutex_lock(&face->lock);
   units=(os2!=NULL && os2->version>=2 && os2->sxHeight>0) ? os2->sxHeight : glyphTop(face,'x');
   pthread_mutex_unlock(&face->lock);
   return units*unitScale(self,face);
}

-(CGFloat)capHeight {
   CTFace *face=[self _ctFace];

   if(face==NULL)
    return (CGFontGetCapHeight(_font)/_unitsPerEm)*_size;

   TT_OS2 *os2=FT_Get_Sfnt_Table(face->ft,FT_SFNT_OS2);
   CGFloat units;

   pthread_mutex_lock(&face->lock);
   units=(os2!=NULL && os2->version>=2 && os2->sCapHeight>0) ? os2->sCapHeight : glyphTop(face,'H');
   pthread_mutex_unlock(&face->lock);
   return units*unitScale(self,face);
}

-(unsigned)numberOfGlyphs {
   CTFace *face=[self _ctFace];

   if(face!=NULL)
    return (unsigned)face->ft->num_glyphs;
   return CGFontGetNumberOfGlyphs(_font);
}

-(CGPoint)positionOfGlyph:(CGGlyph)current precededByGlyph:(CGGlyph)previous isNominal:(BOOL *)isNominalp {
   int advancement;

   if(previous==0)
    return CGPointMake(0,0);

   *isNominalp=YES;
   CGFontGetGlyphAdvances(_font,&previous,1,&advancement);

   return CGPointMake(((CGFloat)advancement/_unitsPerEm)*_size,0);
}

// _twoLevel caches the cmap: 256 pages of 256 glyphs, filled the first time a page is used
static CGGlyph *glyphPage(KTFont *self,uint8_t group) {
   if(self->_twoLevel==NULL)
    self->_twoLevel=calloc(256,sizeof(CGGlyph *));
   if(self->_twoLevel[group]!=NULL)
    return self->_twoLevel[group];

   uint16_t codes[256];
   CGGlyph *page=malloc(256*sizeof(CGGlyph));

   for(int i=0;i<256;i++){
    codes[i]=(group<<8)|i;
   }
   O2FontGetGlyphsForUnicode((O2FontRef)self->_font,codes,page,256);
   self->_twoLevel[group]=page;
   return page;
}

-(void)getGlyphs:(CGGlyph *)glyphs forCharacters:(const unichar *)characters length:(unsigned)length {
   CTFace *face=[self _ctFace];

   // the face's cmap reaches past the BMP, the graphics font's pages stop at it
   if(face!=NULL){
    pthread_mutex_lock(&face->lock);
    for(unsigned i=0;i<length;i++)
     glyphs[i]=FT_Get_Char_Index(face->ft,characters[i]);
    pthread_mutex_unlock(&face->lock);
    return;
   }
   for(unsigned i=0;i<length;i++){
    uint16_t code=characters[i];

    glyphs[i]=glyphPage(self,code>>8)[code&0xFF];
   }
}

-(void)getAdvancements:(CGSize *)advancements forGlyphs:(const CGGlyph *)glyphs count:(unsigned)count {
   hb_font_t *hb=[self _hbFont];

   if(hb!=NULL){
    CTFace *face=[self _ctFace];
    CGFloat s=unitScale(self,face);

    for(unsigned i=0;i<count;i++){
     advancements[i].width=hb_font_get_glyph_h_advance(hb,glyphs[i])*s;
     advancements[i].height=0;
    }
    return;
   }

   int advances[count];

   CGFontGetGlyphAdvances(_font,glyphs,count,advances);
   for(int i=0;i<count;i++){
    advancements[i].width=((CGFloat)advances[i]/(CGFloat)_unitsPerEm)*_size;
    advancements[i].height=0;
   }
}

typedef struct {
   CGMutablePathRef path;
   CGFloat scale;
   const CGAffineTransform *xform;
} KTOutline;

static CGPoint outlinePoint(KTOutline *o,const FT_Vector *v) {
   CGPoint p=CGPointMake(v->x*o->scale,v->y*o->scale);

   return o->xform!=NULL ? CGPointApplyAffineTransform(p,*o->xform) : p;
}

static int outlineMove(const FT_Vector *to,void *user) {
   KTOutline *o=user;
   CGPoint p=outlinePoint(o,to);

   CGPathCloseSubpath(o->path);
   CGPathMoveToPoint(o->path,NULL,p.x,p.y);
   return 0;
}

static int outlineLine(const FT_Vector *to,void *user) {
   KTOutline *o=user;
   CGPoint p=outlinePoint(o,to);

   CGPathAddLineToPoint(o->path,NULL,p.x,p.y);
   return 0;
}

static int outlineConic(const FT_Vector *control,const FT_Vector *to,void *user) {
   KTOutline *o=user;
   CGPoint c=outlinePoint(o,control),p=outlinePoint(o,to);

   CGPathAddQuadCurveToPoint(o->path,NULL,c.x,c.y,p.x,p.y);
   return 0;
}

static int outlineCubic(const FT_Vector *c1,const FT_Vector *c2,const FT_Vector *to,void *user) {
   KTOutline *o=user;
   CGPoint a=outlinePoint(o,c1),b=outlinePoint(o,c2),p=outlinePoint(o,to);

   CGPathAddCurveToPoint(o->path,NULL,a.x,a.y,b.x,b.y,p.x,p.y);
   return 0;
}

-(CGPathRef)createPathForGlyph:(CGGlyph)glyph transform:(CGAffineTransform *)xform {
   CTFace *face=[self _ctFace];
   FT_Outline_Funcs funcs={ outlineMove,outlineLine,outlineConic,outlineCubic,0,0 };
   KTOutline outline;

   if(face==NULL)
    return NULL;
   outline.path=CGPathCreateMutable();
   outline.scale=unitScale(self,face);
   outline.xform=xform;
   pthread_mutex_lock(&face->lock);
   if(FT_Load_Glyph(face->ft,glyph,FT_LOAD_NO_SCALE|FT_LOAD_NO_HINTING|FT_LOAD_NO_BITMAP)==0 &&
      face->ft->glyph->format==FT_GLYPH_FORMAT_OUTLINE)
    FT_Outline_Decompose(&face->ft->glyph->outline,&funcs,&outline);
   pthread_mutex_unlock(&face->lock);
   CGPathCloseSubpath(outline.path);
   return outline.path;
}

-(CGFloat)defaultLineHeightForFont {
   return round([self ascender])+round(fabs([self descender]))+round([self leading]);
}

@end

@implementation KTFont (CoreTextInternal)

// fonts AppKit made from a graphics font find their file through the graphics font's FreeType face or name
-(CTFace *)_ctFace {
   if(_ctFace!=NULL)
    return (CTFace *)_ctFace;
   if(_font==NULL)
    return NULL;

   CTFace *face=NULL;

   if([(id)_font respondsToSelector:@selector(face)]){
    FT_Face ft=(FT_Face)[(id)_font performSelector:@selector(face)];

    if(ft!=NULL)
     face=CTFaceCreateWithFTFace(ft);
   }
   if(face==NULL){
    CFStringRef name=CGFontCopyFullName(_font);
    const CTFontRecord **matches=NULL;
    const void *key=kCTFontDisplayNameAttribute;
    CFDictionaryRef query=name ? CFDictionaryCreate(NULL,&key,(const void **)&name,1,&kCFTypeDictionaryKeyCallBacks,&kCFTypeDictionaryValueCallBacks) : NULL;

    if(query!=NULL && CTRegistryCopyMatches(query,&matches)>0)
     face=CTFaceCreateWithPath(matches[0]->path,matches[0]->index);
    free(matches);
    if(query) CFRelease(query);
    if(name) CFRelease(name);
   }
   // another thread may have set it first
   if(!__sync_bool_compare_and_swap(&_ctFace,NULL,face))
    CTFaceRelease(face);
   return (CTFace *)_ctFace;
}

-(hb_font_t *)_hbFont {
   if(_hbFont!=NULL)
    return (hb_font_t *)_hbFont;

   CTFace *face=[self _ctFace];

   if(face==NULL)
    return NULL;

   // shaping works in font units, CoreText scales positions to points itself
   hb_font_t *hb=hb_font_create(face->hb);

   hb_font_set_scale(hb,face->unitsPerEm,face->unitsPerEm);
   hb_ot_font_set_funcs(hb);

   CFDictionaryRef variation=_ctAttributes ? CFDictionaryGetValue(_ctAttributes,kCTFontVariationAttribute) : NULL;

   if(variation!=NULL && CFGetTypeID(variation)==CFDictionaryGetTypeID()){
    CFIndex count=CFDictionaryGetCount(variation);
    const void *keys[count],*values[count];
    hb_variation_t vars[count];
    int n=0;

    CFDictionaryGetKeysAndValues(variation,keys,values);
    for(CFIndex i=0;i<count;i++){
     uint32_t tag=0;
     double value=0;

     if(CFGetTypeID(keys[i])!=CFNumberGetTypeID() || CFGetTypeID(values[i])!=CFNumberGetTypeID())
      continue;
     CFNumberGetValue(keys[i],kCFNumberSInt32Type,&tag);
     CFNumberGetValue(values[i],kCFNumberDoubleType,&value);
     vars[n].tag=tag;
     vars[n].value=value;
     n++;
    }
    hb_font_set_variations(hb,vars,n);
   }
   if(!__sync_bool_compare_and_swap(&_hbFont,NULL,hb))
    hb_font_destroy(hb);
   return (hb_font_t *)_hbFont;
}

-(CTFontDescriptorRef)_ctDescriptor {
   CTFace *face=[self _ctFace];
   const CTFontRecord *record=face ? CTRegistryRecordForPath(face->path,face->index) : NULL;
   CFMutableDictionaryRef dict;
   CFNumberRef size=CFNumberCreate(NULL,kCFNumberCGFloatType,&_size);
   CTFontDescriptorRef descriptor;

   if(record!=NULL){
    CFDictionaryRef base=CTRecordCopyAttributes(record);

    dict=CFDictionaryCreateMutableCopy(NULL,0,base);
    CFRelease(base);
   }
   else {
    dict=CFDictionaryCreateMutable(NULL,0,&kCFTypeDictionaryKeyCallBacks,&kCFTypeDictionaryValueCallBacks);
    if(face!=NULL && face->postScriptName) CFDictionarySetValue(dict,kCTFontNameAttribute,face->postScriptName);
    if(face!=NULL && face->familyName) CFDictionarySetValue(dict,kCTFontFamilyNameAttribute,face->familyName);
    if(face!=NULL && face->styleName) CFDictionarySetValue(dict,kCTFontStyleNameAttribute,face->styleName);
    if(face!=NULL && face->fullName) CFDictionarySetValue(dict,kCTFontDisplayNameAttribute,face->fullName);
   }
   // the caller's own attributes (features, variation, cascade list) ride along
   if(_ctAttributes!=NULL){
    CFIndex count=CFDictionaryGetCount(_ctAttributes);
    const void *keys[count],*values[count];

    CFDictionaryGetKeysAndValues(_ctAttributes,keys,values);
    for(CFIndex i=0;i<count;i++){
     if(!CFDictionaryContainsKey(dict,keys[i]))
      CFDictionarySetValue(dict,keys[i],values[i]);
    }
   }
   CFDictionarySetValue(dict,kCTFontSizeAttribute,size);
   descriptor=CTFontDescriptorCreateWithAttributes(dict);
   CFRelease(size);
   CFRelease(dict);
   return descriptor;
}

-(CGAffineTransform)_ctMatrix {
   return _matrix;
}

@end

// a font CoreText makes itself, its graphics font found by name or loaded from the file
CTFontRef CTFontCreateWithRecord(const CTFontRecord *record,CGFloat size,const CGAffineTransform *matrix,CFDictionaryRef extra) {
   CTFace *face=CTFaceCreateWithPath(record->path,record->index);
   CGFontRef graphics=NULL;

   if(face==NULL)
    return NULL;
   graphics=CGFontCreateWithFontName(record->postScriptName);
   if(graphics!=NULL){
    CFStringRef full=CGFontCopyFullName(graphics);

    // a name lookup that landed on another face is no good, load the file instead
    if(full==NULL || !CFEqual(full,record->fullName)){
     CGFontRelease(graphics);
     graphics=NULL;
    }
    if(full) CFRelease(full);
   }
   if(graphics==NULL){
    CFURLRef url=CFURLCreateFromFileSystemRepresentation(NULL,(const UInt8 *)record->path,strlen(record->path),false);
    O2DataProviderRef provider=url ? O2DataProviderCreateWithURL((NSURL *)url) : NULL;

    if(provider!=NULL){
     graphics=(CGFontRef)O2FontCreateWithDataProvider(provider);
     O2DataProviderRelease(provider);
    }
    if(url) CFRelease(url);
   }

   KTFont *font=[KTFont alloc];

   font->_font=graphics;
   font->_unitsPerEm=face->unitsPerEm;
   font->_size=size>0 ? size : CTDefaultFontSize;
   font->_matrix=matrix ? *matrix : CGAffineTransformIdentity;
   font->_ctFace=face;
   font->_ctAttributes=extra ? CFDictionaryCreateCopy(NULL,extra) : NULL;
   font->_fromGraphicsFont=NO;
   return font;
}
