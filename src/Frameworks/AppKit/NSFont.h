/* Copyright (c) 2006-2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <Foundation/Foundation.h>
#import <ApplicationServices/ApplicationServices.h>
#import <AppKit/NSCell.h>

@class NSFontDescriptor;

// Provides an extendable translation scheme for apps that use non-standard fonts in UI elements
@interface NSNibFontNameTranslator : NSObject {
}

- (NSString *)translateToNibFontName:(NSString *)fontName;
- (NSString *)translateFromNibFontName:(NSString *)fontName;

@end

typedef NSUInteger NSGlyph;

enum {
    NSNullGlyph = 0,
    NSControlGlyph = 0xFFFFFF,
};

typedef enum {
    NSNativeShortGlyphPacking,
} NSMultibyteGlyphPacking;

typedef enum {
    NSFontDefaultRenderingMode,
    NSFontAntialiasedRenderingMode,
    NSFontIntegerAdvancementsRenderingMode,
    NSFontAntialiasedIntegerAdvancementsRenderingMode,
} NSFontRenderingMode;

@interface NSFont : NSObject <NSCopying> {
    NSString *_name;
    CGFloat _pointSize;
    CGFloat _matrix[6];
    NSStringEncoding _encoding;

    CGFontRef _cgFont;
    CTFontRef _ctFont;
}

+ (CGFloat)systemFontSize;
+ (CGFloat)smallSystemFontSize;
+ (CGFloat)labelFontSize;
+ (CGFloat)systemFontSizeForControlSize:(NSControlSize)size;

+ (NSFont *)boldSystemFontOfSize:(CGFloat)size;
+ (NSFont *)controlContentFontOfSize:(CGFloat)size;

+ (NSFont *)labelFontOfSize:(CGFloat)size;
+ (NSFont *)menuFontOfSize:(CGFloat)size;
+ (NSFont *)menuBarFontOfSize:(CGFloat)size;

+ (NSFont *)messageFontOfSize:(CGFloat)size;
+ (NSFont *)paletteFontOfSize:(CGFloat)size;
+ (NSFont *)systemFontOfSize:(CGFloat)size;
+ (NSFont *)systemFontOfSize:(CGFloat)size weight:(CGFloat)weight;
+ (NSFont *)monospacedSystemFontOfSize:(CGFloat)size weight:(CGFloat)weight;
+ (NSFont *)titleBarFontOfSize:(CGFloat)size;
+ (NSFont *)toolTipsFontOfSize:(CGFloat)size;
+ (NSFont *)userFontOfSize:(CGFloat)size;
+ (NSFont *)userFixedPitchFontOfSize:(CGFloat)size;

+ (void)setUserFont:(NSFont *)value;
+ (void)setUserFixedPitchFont:(NSFont *)value;

+ (NSFont *)fontWithName:(NSString *)name size:(CGFloat)size;
+ (NSFont *)fontWithName:(NSString *)name matrix:(const CGFloat *)matrix;
+ (NSFont *)fontWithDescriptor:(NSFontDescriptor *)descriptor size:(CGFloat)size;
+ (NSFont *)fontWithDescriptor:(NSFontDescriptor *)descriptor size:(CGFloat)size textTransform:(NSAffineTransform *)transform;

+ (NSArray *)preferredFontNames;
+ (void)setPreferredFontNames:(NSArray *)fontNames;

- (CGFloat)pointSize;
- (NSString *)fontName;
- (const CGFloat *)matrix;
- (NSAffineTransform *)textTransform;
- (NSFontRenderingMode)renderingMode;
- (NSCharacterSet *)coveredCharacterSet;
- (NSStringEncoding)mostCompatibleStringEncoding;
- (NSString *)familyName;
- (NSString *)displayName;
- (NSFontDescriptor *)fontDescriptor;

- (NSFont *)printerFont;
- (NSFont *)screenFont;
- (NSFont *)screenFontWithRenderingMode:(NSFontRenderingMode)mode;

- (NSRect)boundingRectForFont;
- (NSRect)boundingRectForGlyph:(NSGlyph)glyph;

- (NSMultibyteGlyphPacking)glyphPacking;
- (unsigned)numberOfGlyphs;
- (NSGlyph)glyphWithName:(NSString *)name;
- (BOOL)glyphIsEncoded:(NSGlyph)glyph;
- (NSSize)advancementForGlyph:(NSGlyph)glyph;

- (NSSize)maximumAdvancement;
- (CGFloat)underlinePosition;
- (CGFloat)underlineThickness;
- (CGFloat)ascender;
- (CGFloat)descender;
- (CGFloat)leading;
- (CGFloat)defaultLineHeightForFont;
- (BOOL)isFixedPitch;
- (CGFloat)italicAngle;
- (CGFloat)xHeight;
- (CGFloat)capHeight;

- (void)setInContext:(NSGraphicsContext *)context;
- (void)set;

- (NSPoint)positionOfGlyph:(NSGlyph)current precededByGlyph:(NSGlyph)previous isNominal:(BOOL *)isNominalp;

- (void)getAdvancements:(NSSize *)advancements forGlyphs:(const NSGlyph *)glyphs count:(unsigned)count;
- (void)getAdvancements:(NSSize *)advancements forPackedGlyphs:(const void *)packed length:(unsigned)length;
- (void)getBoundingRects:(NSRect *)rects forGlyphs:(const NSGlyph *)glyphs count:(unsigned)count;

// private

- (unsigned)getGlyphs:(NSGlyph *)glyphs forCharacters:(unichar *)characters length:(unsigned)length;


- (CGFloat)widthOfString:(NSString *)string;

@end

@interface NSFont (PortatibilityAdditions)

+ (void)setNibFontTranslator:(NSNibFontNameTranslator *)fontTranslator;
+ (NSNibFontNameTranslator *)nibFontTranslator;

@end

int NSConvertGlyphsToPackedGlyphs(NSGlyph *glyphs, int length, NSMultibyteGlyphPacking packing, char *output);
