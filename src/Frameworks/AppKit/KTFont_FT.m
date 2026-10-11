/* Copyright (c) 2008 Johannes Fortmann

 Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <AppKit/NSFont.h>
#import "KTFont_FT.h"
#import <CoreText/KTFont.h>
#import <AppKit/NSRaise.h>
#import "O2Font_FT.h"
#import <AppKit/NSFontTypeface.h>
//#import <AppKit/NSGraphicsContext.h>
#import FT_TRUETYPE_TABLES_H
#import FT_TRUETYPE_TAGS_H
#include <math.h>

// the exact point size, a whole-pixel size would turn 12.5 into 12
static void KTSetSize(FT_Face face, CGFloat size) {
   FT_Set_Char_Size(face, 0, (FT_F26Dot6)llround(size * 64), 72, 72);
}

// the unhinted advance at the current size, fractional as Apple's text system reports it
static CGFloat KTAdvance(FT_Face face, CGGlyph glyph) {
   if (glyph >= face->num_glyphs || FT_Load_Glyph(face, glyph, FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP) != 0)
      return 0;
   return face->glyph->linearHoriAdvance / 65536.0;
}

#pragma mark GPOS pair kerning

// big-endian reads into the raw GPOS table, every offset checked against its length
typedef struct {
   const uint8_t *data;
   size_t length;
} KTTable;

static unsigned KTU16(KTTable t, size_t at) {
   return at + 2 <= t.length ? (unsigned)(t.data[at] << 8 | t.data[at + 1]) : 0;
}

static uint32_t KTU32(KTTable t, size_t at) {
   return at + 4 <= t.length ? (uint32_t)t.data[at] << 24 | (uint32_t)t.data[at + 1] << 16 | (uint32_t)t.data[at + 2] << 8 | t.data[at + 3] : 0;
}

// a glyph's index in a coverage table, -1 when it is not covered
static int KTCoverage(KTTable t, size_t at, unsigned glyph) {
   unsigned format = KTU16(t, at), count = KTU16(t, at + 2);

   if (format == 1) {
      unsigned lo = 0, hi = count;

      while (lo < hi) {
         unsigned mid = (lo + hi) / 2, g = KTU16(t, at + 4 + mid * 2);

         if (g == glyph)
            return (int)mid;
         if (g < glyph)
            lo = mid + 1;
         else
            hi = mid;
      }
   } else if (format == 2) {
      for (unsigned i = 0; i < count; i++) {
         size_t r = at + 4 + i * 6;
         unsigned start = KTU16(t, r), end = KTU16(t, r + 2);

         if (glyph >= start && glyph <= end)
            return (int)(KTU16(t, r + 4) + glyph - start);
      }
   }
   return -1;
}

static unsigned KTClass(KTTable t, size_t at, unsigned glyph) {
   unsigned format = KTU16(t, at);

   if (format == 1) {
      unsigned start = KTU16(t, at + 2), count = KTU16(t, at + 4);

      return glyph >= start && glyph < start + count ? KTU16(t, at + 6 + (glyph - start) * 2) : 0;
   }
   if (format == 2) {
      unsigned count = KTU16(t, at + 2);

      for (unsigned i = 0; i < count; i++) {
         size_t r = at + 4 + i * 6;

         if (glyph >= KTU16(t, r) && glyph <= KTU16(t, r + 2))
            return KTU16(t, r + 4);
      }
   }
   return 0;
}

static unsigned KTValueSize(unsigned format) {
   return __builtin_popcount(format & 0xff) * 2;
}

// the XAdvance field of a value record, zero when the format leaves it out
static int KTXAdvance(KTTable t, size_t at, unsigned format) {
   if (!(format & 0x0004))
      return 0;
   size_t skip = ((format & 0x0001) ? 2 : 0) + ((format & 0x0002) ? 2 : 0);

   return (int16_t)KTU16(t, at + skip);
}

// one PairPos subtable, YES when it covers the first glyph and so settles the pair
static BOOL KTPairPos(KTTable t, size_t at, unsigned first, unsigned second, int *value) {
   unsigned format = KTU16(t, at);
   int covered = KTCoverage(t, at + KTU16(t, at + 2), first);
   unsigned vf1 = KTU16(t, at + 4), vf2 = KTU16(t, at + 6);
   unsigned s1 = KTValueSize(vf1), s2 = KTValueSize(vf2);

   if (covered < 0)
      return NO;
   if (format == 1) {
      size_t set = at + KTU16(t, at + 10 + covered * 2);
      unsigned count = KTU16(t, set), lo = 0, hi = count;
      size_t record = 2 + s1 + s2;

      while (lo < hi) {
         unsigned mid = (lo + hi) / 2;
         size_t r = set + 2 + mid * record;
         unsigned g = KTU16(t, r);

         if (g == second) {
            *value = KTXAdvance(t, r + 2, vf1);
            return YES;
         }
         if (g < second)
            lo = mid + 1;
         else
            hi = mid;
      }
      return NO;
   }
   if (format == 2) {
      unsigned c1 = KTClass(t, at + KTU16(t, at + 8), first);
      unsigned c2 = KTClass(t, at + KTU16(t, at + 10), second);
      unsigned count1 = KTU16(t, at + 12), count2 = KTU16(t, at + 14);

      if (c1 >= count1 || c2 >= count2)
         return NO;
      *value = KTXAdvance(t, at + 16 + (c1 * count2 + c2) * (s1 + s2), vf1);
      return YES;
   }
   return NO;
}

// the kerning between two glyphs in font units, from the GPOS 'kern' feature
static int KTKernGPOS(KTTable t, unsigned first, unsigned second) {
   size_t featureList = KTU16(t, 6), lookupList = KTU16(t, 8);
   unsigned featureCount = KTU16(t, featureList);

   for (unsigned f = 0; f < featureCount; f++) {
      size_t record = featureList + 2 + f * 6;

      if (KTU32(t, record) != 0x6b65726e) // 'kern'
         continue;
      size_t feature = featureList + KTU16(t, record + 4);
      unsigned lookupCount = KTU16(t, feature + 2);

      for (unsigned l = 0; l < lookupCount; l++) {
         size_t lookup = lookupList + KTU16(t, lookupList + 2 + KTU16(t, feature + 4 + l * 2) * 2);
         unsigned type = KTU16(t, lookup), subtables = KTU16(t, lookup + 4);

         for (unsigned s = 0; s < subtables; s++) {
            size_t sub = lookup + KTU16(t, lookup + 6 + s * 2);
            unsigned subType = type;
            int value = 0;

            // an extension lookup points on to the real subtable
            if (subType == 9) {
               subType = KTU16(t, sub + 2);
               sub += KTU32(t, sub + 4);
            }
            if (subType == 2 && KTPairPos(t, sub, first, second, &value))
               return value;
         }
      }
   }
   return 0;
}

// each face's GPOS bytes, read once
static KTTable KTGPOS(FT_Face face) {
   static CFMutableDictionaryRef cache;
   CFDataRef data;
   KTTable t = { NULL, 0 };

   if (cache == NULL)
      cache = CFDictionaryCreateMutable(NULL, 0, NULL, &kCFTypeDictionaryValueCallBacks);
   data = CFDictionaryGetValue(cache, face);
   if (data == NULL) {
      FT_ULong length = 0;
      UInt8 *bytes = NULL;

      if (FT_IS_SFNT(face) && FT_Load_Sfnt_Table(face, TTAG_GPOS, 0, NULL, &length) == 0 && length > 10) {
         bytes = malloc(length);
         if (FT_Load_Sfnt_Table(face, TTAG_GPOS, 0, bytes, &length) != 0)
            length = 0;
      }
      data = CFDataCreate(NULL, bytes, bytes != NULL ? (CFIndex)length : 0);
      free(bytes);
      CFDictionarySetValue(cache, face, data);
      CFRelease(data);
   }
   t.data = CFDataGetBytePtr(data);
   t.length = (size_t)CFDataGetLength(data);
   return t;
}

// kerning in points, GPOS first and the old 'kern' table for fonts that only have that
static CGFloat KTKern(FT_Face face, CGFloat size, CGGlyph first, CGGlyph second) {
   KTTable gpos = KTGPOS(face);
   int units = gpos.length > 0 ? KTKernGPOS(gpos, first, second) : 0;

   if (units == 0 && FT_HAS_KERNING(face)) {
      FT_Vector delta;

      if (FT_Get_Kerning(face, first, second, FT_KERNING_UNSCALED, &delta) == 0)
         units = (int)delta.x;
   }
   return face->units_per_EM > 0 ? units * size / face->units_per_EM : 0;
}

@implementation KTFont(KTFont_FT)
+(id)allocWithZone:(NSZone*)zone
{
   return NSAllocateObject([KTFont_FT class], 0, NULL);
}
@end

@implementation KTFont_FT


-initWithUIFontType:(CTFontUIFontType)uiFontType size:(CGFloat)size language:(NSString *)language {
   O2Font *font=nil;

   switch(uiFontType){

    case kCTFontMenuTitleFontType:
     if(size==0)
      size=14.0;
     font=O2FontCreateWithFontName(@"Inter-Regular");
     break;
    case kCTFontMenuItemFontType:
    case kCTFontSystemFontType:
     if(size==0)
      size=12.0;
     font=O2FontCreateWithFontName(@"Inter-Regular");
     break;

    default:
     return nil;
   }

   self=[self initWithFont:font size:size];

   [font release];

   return self;
}

-(void)getGlyphs:(CGGlyph *)glyphs forCharacters:(const unichar *)characters length:(unsigned)length {
   FT_Face face=[(id)_font face];

   int i;
   if(face==NULL) {
    for(i=0;i<length;i++)
     glyphs[i]=0;
    return;
   }

   for(i=0; i<length; i++)
   {
      glyphs[i]=FT_Get_Char_Index(face, characters[i]);
   }
}

-(void)getAdvancements:(CGSize *)advancements forGlyphs:(const CGGlyph *)glyphs count:(unsigned)count {
   FT_Face face=[(id)_font face];

   int i;
   if(face==NULL) {
    for(i=0;i<count;i++)
     advancements[i]=CGSizeZero;
    return;
   }
   KTSetSize(face, _size);

   for(i=0;i<count;i++)
      advancements[i]=CGSizeMake(KTAdvance(face, glyphs[i]), 0);
}

-(NSSize)advancementForGlyph:(NSGlyph)glyph {
   CGSize adv[1];
   [self getAdvancements:&adv forGlyphs:(CGGlyph *)&glyph count:1];
   return adv[0];
}

-(CGPoint)positionOfGlyph:(CGGlyph)current precededByGlyph:(CGGlyph)previous isNominal:(BOOL *)isNominalp {
   FT_Face face=[(id)_font face];

   CGFloat kern=0;

   *isNominalp=YES;

   // as NSFont documents it, where current sits after previous: previous's advance plus any kerning between them
   if(!previous || face==NULL)
      return NSZeroPoint;

   KTSetSize(face, _size);
   if(current)
      kern=KTKern(face, _size, previous, current);
   if(kern!=0)
      *isNominalp=NO;
   return NSMakePoint(KTAdvance(face, previous)+kern, 0);
}

-(NSString *)description {
   return [NSString stringWithFormat:@"<%@ %@ %f>",[self class],_font,_size];
}

// FIXME: I feel like all of this below should be inherited from NSFont but these are
// all unrecognized selectors unless implemented here. WHY?

-(NSCharacterSet *)coveredCharacterSet {
   return [_font coveredCharacterSet];
}

// each part rounded on its own, which is how Apple's text system reaches its line heights
-(CGFloat)defaultLineHeightForFont {
   return round([self ascender]) + round(fabs([self descender])) + round([self leading]);
}

#if 0
-(void)setInContext:(NSGraphicsContext *)context {
   CGContextRef cgContext=[context graphicsPort];

   CGContextSetFont(cgContext,_font);
   CGContextSetFontSize(cgContext,_size);

   CGAffineTransform textMatrix;

// FIX, should check the focusView in the context instead of NSView's
   if([[NSGraphicsContext currentContext] isFlipped])
    textMatrix=(CGAffineTransform){1,0,0,-1,0,0};
   else
    textMatrix=CGAffineTransformIdentity;

   CGContextSetTextMatrix(cgContext,textMatrix);
}
#endif
@end
