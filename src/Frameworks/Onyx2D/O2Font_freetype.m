#import <Onyx2D/O2Font_freetype.h>
#ifdef FREETYPE_PRESENT
#import <Onyx2D/O2Encoding.h>
#include FT_SFNT_NAMES_H
#include FT_TRUETYPE_IDS_H
#include FT_TRUETYPE_TABLES_H
#include FT_MULTIPLE_MASTERS_H

// the sfnt full name (name id 4), what Apple's CGFontCopyFullName returns
static NSString *copyFullNameOfFace(FT_Face face) {
   FT_UInt count=FT_Get_Sfnt_Name_Count(face);
   NSString *mac=nil;

   for(FT_UInt i=0;i<count;i++){
    FT_SfntName name;

    if(FT_Get_Sfnt_Name(face,i,&name)!=0 || name.name_id!=TT_NAME_ID_FULL_NAME) continue;

    if(name.platform_id==TT_PLATFORM_MICROSOFT || name.platform_id==TT_PLATFORM_APPLE_UNICODE){
     unichar chars[name.string_len/2+1];

     for(FT_UInt j=0;j<name.string_len/2;j++){
      chars[j]=(name.string[2*j]<<8)|name.string[2*j+1];
     }
     [mac release];
     return [[NSString alloc] initWithCharacters:chars length:name.string_len/2];
    }
    if(name.platform_id==TT_PLATFORM_MACINTOSH && mac==nil)
     mac=[[NSString alloc] initWithBytes:name.string length:name.string_len encoding:NSMacOSRomanStringEncoding];
   }
   if(mac!=nil) return mac;

   if(face->family_name==NULL) return nil;
   if(face->style_name==NULL || !strcmp(face->style_name,"Regular") || !strcmp(face->style_name,"Book"))
    return [[NSString alloc] initWithUTF8String:face->family_name];
   return [[NSString alloc] initWithFormat:@"%s %s",face->family_name,face->style_name];
}

@implementation O2Font_freetype

FT_Library freeTypeLibrary;

+(void)initialize {
  if(self==[O2Font_freetype class]){
   int error = FT_Init_FreeType(&freeTypeLibrary);
  
   if ( error ) {
     NSLog(@"error initializing FreeType %d",error);
   }
  }
}


-initWithDataProvider:(O2DataProviderRef)provider {
   if([super initWithDataProvider:provider]==nil)
    return nil;

   _platformType=O2FontPlatformTypeFreeType;
   
   int error;

   if([provider isDirectAccess]) {
    const void *bytes=[provider bytes];
    size_t length=[provider length];

    error=FT_New_Memory_Face(freeTypeLibrary,bytes,length,0,&_face);
   }
   else {
    error=FT_New_Face(freeTypeLibrary,
                     [[provider path] fileSystemRepresentation],0,&_face);
   }
   
   if(error!=0){
    [self dealloc];
    return nil;
   }
   
   int i,numberOfCharMaps=_face->num_charmaps;
   bool hasUnicode=FALSE;
   bool hasMacRoman=FALSE;
   bool hasSymbol=FALSE;

   // a face without any character map can't map text, so it is no font here
   if(numberOfCharMaps<=0){
    FT_Done_Face(_face);
    _face=NULL;
    [self dealloc];
    return nil;
   }

   for(i=0;i<numberOfCharMaps;i++){
    if(_face->charmaps[i]->encoding==FT_ENCODING_UNICODE)
     hasUnicode=TRUE;
    if(_face->charmaps[i]->encoding==FT_ENCODING_APPLE_ROMAN)
     hasMacRoman=TRUE;
    if(_face->charmaps[i]->encoding==FT_ENCODING_MS_SYMBOL)
     hasSymbol=TRUE;
   }

   if(hasUnicode)
    _ftEncoding=FT_ENCODING_UNICODE;
   else if(hasMacRoman)
    _ftEncoding=FT_ENCODING_APPLE_ROMAN;
   else if(hasSymbol)
    _ftEncoding=FT_ENCODING_MS_SYMBOL;
   else
    _ftEncoding=_face->charmaps[0]->encoding;
   
   if(FT_Select_Charmap(_face,_ftEncoding)!=0)
    NSLog(@"FT_Select_Charmap(%d) failed",_ftEncoding);
   
   if(!(_face->face_flags&FT_FACE_FLAG_SCALABLE))
    NSLog(@"FreeType font face is not scalable");
    
   _unitsPerEm=(float)_face->units_per_EM;
   _ascent=_face->ascender;
   _descent=_face->descender;
   _leading=0;
   _capHeight=_face->height;
   _xHeight=_face->height;
   _italicAngle=0;
   _stemV=0;
   _bbox.origin.x=_face->bbox.xMin;
   _bbox.origin.y=_face->bbox.yMin;
   _bbox.size.width=_face->bbox.xMax-_face->bbox.xMin;
   _bbox.size.height=_face->bbox.yMax-_face->bbox.yMin;
   _numberOfGlyphs=_face->num_glyphs;
   if(_name==nil)
    _name=copyFullNameOfFace(_face);
   _advances=NULL;
   return self;
}

-(void)dealloc {
   FT_Done_Face(_face);
   [_macRomanEncoding release];
   [_macExpertEncoding release];
   [_winAnsiEncoding release];
   [super dealloc];
}

FT_Face O2FontFreeTypeFace(O2Font_freetype *self) {
   return self->_face;
}

-(FT_Face)face {
   return _face;
}

-(void)fetchAdvances {
   O2Glyph glyph;

   FT_Set_Char_Size(_face,0,_unitsPerEm*64,72,72);

   _advances=NSZoneMalloc(NULL,sizeof(int)*_numberOfGlyphs);

   for(glyph=0;glyph<_numberOfGlyphs;glyph++){
    FT_Load_Glyph(_face, glyph, FT_LOAD_DEFAULT);
    
    _advances[glyph]=_face->glyph->advance.x/(float)(2<<5);
   }
}

-(NSData *)copyTableForTag:(uint32_t)tag {
   FT_ULong length=0;

   if(FT_Load_Sfnt_Table(_face,tag,0,NULL,&length)!=0 || length==0)
    return nil;

   NSMutableData *data=[[NSMutableData alloc] initWithLength:length];

   if(FT_Load_Sfnt_Table(_face,tag,0,[data mutableBytes],&length)!=0){
    [data release];
    return nil;
   }
   return data;
}

-(NSString *)copyPostScriptName {
   const char *name=FT_Get_Postscript_Name(_face);

   return name!=NULL ? [[NSString alloc] initWithUTF8String:name] : nil;
}

// the tags are NSNumbers here, CoreGraphics turns them into bare values
-(NSArray *)copyTableTags {
   NSMutableArray *result=[[NSMutableArray alloc] init];
   FT_ULong count=0;

   if(FT_Sfnt_Table_Info(_face,0,NULL,&count)!=0)
    return result;

   for(FT_UInt i=0;i<count;i++){
    FT_ULong tag=0,length=0;

    if(FT_Sfnt_Table_Info(_face,i,&tag,&length)==0)
     [result addObject:[NSNumber numberWithUnsignedInt:(uint32_t)tag]];
   }
   return result;
}

static double fixedToDouble(FT_Fixed value) {
   return value/65536.0;
}

-(NSArray *)copyVariationAxes {
   FT_MM_Var *mm=NULL;

   if(!FT_HAS_MULTIPLE_MASTERS(_face) || FT_Get_MM_Var(_face,&mm)!=0)
    return nil;

   NSMutableArray *result=[[NSMutableArray alloc] init];

   for(FT_UInt i=0;i<mm->num_axis;i++){
    FT_Var_Axis *axis=mm->axis+i;
    NSString *name=axis->name!=NULL ? [NSString stringWithUTF8String:axis->name] : @"";
    NSDictionary *entry=[NSDictionary dictionaryWithObjectsAndKeys:
     name,O2FontVariationAxisName,
     [NSNumber numberWithDouble:fixedToDouble(axis->minimum)],O2FontVariationAxisMinValue,
     [NSNumber numberWithDouble:fixedToDouble(axis->maximum)],O2FontVariationAxisMaxValue,
     [NSNumber numberWithDouble:fixedToDouble(axis->def)],O2FontVariationAxisDefaultValue,
     nil];

    [result addObject:entry];
   }
   FT_Done_MM_Var(freeTypeLibrary,mm);
   return result;
}

// keyed by axis name, as Apple's CGFontCopyVariations is
-(NSDictionary *)copyVariations {
   FT_MM_Var *mm=NULL;

   if(!FT_HAS_MULTIPLE_MASTERS(_face) || FT_Get_MM_Var(_face,&mm)!=0)
    return nil;

   FT_Fixed coords[mm->num_axis];
   NSMutableDictionary *result=nil;

   if(FT_Get_Var_Design_Coordinates(_face,mm->num_axis,coords)==0){
    result=[[NSMutableDictionary alloc] init];
    for(FT_UInt i=0;i<mm->num_axis;i++){
     NSString *name=mm->axis[i].name!=NULL ? [NSString stringWithUTF8String:mm->axis[i].name] : @"";

     [result setObject:[NSNumber numberWithDouble:fixedToDouble(coords[i])] forKey:name];
    }
   }
   FT_Done_MM_Var(freeTypeLibrary,mm);
   return result;
}

// keys are axis names or axis tags as numbers, unnamed axes keep their current value
-(O2Font *)createCopyWithVariations:(NSDictionary *)variations {
   FT_MM_Var *mm=NULL;

   if(!FT_HAS_MULTIPLE_MASTERS(_face) || FT_Get_MM_Var(_face,&mm)!=0)
    return nil;

   O2Font_freetype *copy=[[O2Font_freetype alloc] initWithDataProvider:_provider];
   FT_Fixed coords[mm->num_axis];

   if(copy==nil || FT_Get_Var_Design_Coordinates(_face,mm->num_axis,coords)!=0){
    FT_Done_MM_Var(freeTypeLibrary,mm);
    return copy;
   }

   for(id key in variations){
    id value=[variations objectForKey:key];

    if(![value isKindOfClass:[NSNumber class]]) continue;

    for(FT_UInt i=0;i<mm->num_axis;i++){
     FT_Var_Axis *axis=mm->axis+i;
     BOOL match=NO;

     if([key isKindOfClass:[NSNumber class]])
      match=([key unsignedIntValue]==(uint32_t)axis->tag);
     else if([key isKindOfClass:[NSString class]] && axis->name!=NULL)
      match=[key isEqualToString:[NSString stringWithUTF8String:axis->name]];
     if(!match) continue;

     double v=[value doubleValue];

     if(v<fixedToDouble(axis->minimum)) v=fixedToDouble(axis->minimum);
     if(v>fixedToDouble(axis->maximum)) v=fixedToDouble(axis->maximum);
     coords[i]=(FT_Fixed)(v*65536.0);
    }
   }
   FT_Set_Var_Design_Coordinates(copy->_face,mm->num_axis,coords);
   FT_Done_MM_Var(freeTypeLibrary,mm);
   return copy;
}

// boxes in font units, as CGFontGetGlyphBBoxes reports them
-(BOOL)getGlyphBBoxes:(const O2Glyph *)glyphs count:(size_t)count bboxes:(O2Rect *)bboxes {
   BOOL ok=YES;

   for(size_t i=0;i<count;i++){
    if(FT_Load_Glyph(_face,glyphs[i],FT_LOAD_NO_SCALE|FT_LOAD_NO_HINTING)!=0){
     bboxes[i]=O2RectZero;
     ok=NO;
     continue;
    }

    FT_Glyph_Metrics *m=&_face->glyph->metrics;

    bboxes[i]=O2RectMake(m->horiBearingX,m->horiBearingY-m->height,m->width,m->height);
   }
   return ok;
}

-(O2Glyph)glyphWithGlyphName:(NSString *)name {
   return FT_Get_Name_Index(_face,(char *)[name cString]);
}

-(void)getGlyphsForCodePoints:(uint16_t *)codes:(O2Glyph *)glyphs:(int)length {
   int i;
   
   for(i=0;i<length;i++)
    glyphs[i]=FT_Get_Char_Index(_face,codes[i]);
}

-(O2Encoding *)unicode_createEncodingForTextEncoding:(O2TextEncoding)encoding {
   unichar unicode[256];
   O2Glyph glyphs[256];
   
   switch(encoding){
    case kO2EncodingFontSpecific:
    case kO2EncodingMacRoman:
     if(_macRomanEncoding==nil){
      O2EncodingGetMacRomanUnicode(unicode);
      [self getGlyphsForCodePoints:unicode:glyphs:256];
      _macRomanEncoding=[[O2Encoding alloc] initWithGlyphs:glyphs unicode:unicode];
     }
     return [_macRomanEncoding retain];
     
    case kO2EncodingMacExpert:
     if(_macExpertEncoding==nil){
      O2EncodingGetMacExpertUnicode(unicode);
      [self getGlyphsForCodePoints:unicode:glyphs:256];
      _macExpertEncoding=[[O2Encoding alloc] initWithGlyphs:glyphs unicode:unicode];
     }
     return [_macExpertEncoding retain];

    case kO2EncodingWinAnsi:
     if(_winAnsiEncoding==nil){
      O2EncodingGetWinAnsiUnicode(unicode);
      [self getGlyphsForCodePoints:unicode:glyphs:256];
      _winAnsiEncoding=[[O2Encoding alloc] initWithGlyphs:glyphs unicode:unicode];
     }
     return [_winAnsiEncoding retain];
    
    default:
     return nil;
   }
   return nil;
}

-(O2Encoding *)MacRoman_createEncodingForTextEncoding:(O2TextEncoding)encoding {
   uint16_t codes[256];
   O2Glyph glyphs[256];
   unichar unicode[256];

   if(_macRomanEncoding==nil){
    int i;
   
    if(encoding!=kO2EncodingMacRoman && encoding!=kO2EncodingFontSpecific){
     NSLog(@"font encoding is MacRoman, requesting encoding %d failed",encoding);
    }
   
    for(i=0;i<256;i++)
     codes[i]=i;
    
    [self getGlyphsForCodePoints:codes:glyphs:256];
    
    O2EncodingGetMacExpertUnicode(unicode);

    _macRomanEncoding=[[O2Encoding alloc] initWithGlyphs:glyphs unicode:unicode];
   }
   
   return [_macRomanEncoding retain];
}

-(O2Encoding *)createEncodingForTextEncoding:(O2TextEncoding)encoding {
   if(_ftEncoding==FT_ENCODING_APPLE_ROMAN)
    return [self MacRoman_createEncodingForTextEncoding:encoding];
    
   return [self unicode_createEncodingForTextEncoding:encoding];
}

@end
#endif
