/* Copyright (c) 2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files(the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#ifndef __CORETEXT_CTFONT__
#define __CORETEXT_CTFONT__

#import <CoreFoundation/CoreFoundation.h>
#import <CoreText/CoreTextExport.h>
#import <CoreGraphics/CGAffineTransform.h>
#import <CoreGraphics/CGFont.h>
#import <CoreGraphics/CGPath.h>
#import <CoreGraphics/CGContext.h>
#import <CoreText/CTFontDescriptor.h>

// Objective-C code sees the KTFont object AppKit subclasses, everything else the opaque pointer Apple declares
#ifdef __OBJC__
@class KTFont;
typedef KTFont *CTFontRef;
#else
typedef const struct __CTFont *CTFontRef;
#endif

CORETEXT_EXPORT CFTypeID CTFontGetTypeID(void);

typedef enum {
    kCTFontUserFontType = 0,
    kCTFontUserFixedPitchFontType = 1,
    kCTFontSystemFontType = 2,
    kCTFontEmphasizedSystemFontType = 3,
    kCTFontSmallSystemFontType = 4,
    kCTFontSmallEmphasizedSystemFontType = 5,
    kCTFontMiniSystemFontType = 6,
    kCTFontMiniEmphasizedSystemFontType = 7,
    kCTFontViewsFontType = 8,
    kCTFontApplicationFontType = 9,
    kCTFontLabelFontType = 10,
    kCTFontMenuTitleFontType = 11,
    kCTFontMenuItemFontType = 12,
    kCTFontMenuItemMarkFontType = 13,
    kCTFontMenuItemCmdKeyFontType = 14,
    kCTFontWindowTitleFontType = 15,
    kCTFontPushButtonFontType = 16,
    kCTFontUtilityWindowTitleFontType = 17,
    kCTFontAlertHeaderFontType = 18,
    kCTFontSystemDetailFontType = 19,
    kCTFontEmphasizedSystemDetailFontType = 20,
    kCTFontToolbarFontType = 21,
    kCTFontSmallToolbarFontType = 22,
    kCTFontMessageFontType = 23,
    kCTFontPaletteFontType = 24,
    kCTFontToolTipFontType = 25,
    kCTFontControlContentFontType = 26,

    kCTFontUIFontNone = (uint32_t)-1,
    kCTFontUIFontUser = 0,
    kCTFontUIFontUserFixedPitch = 1,
    kCTFontUIFontSystem = 2,
    kCTFontUIFontEmphasizedSystem = 3,
    kCTFontUIFontSmallSystem = 4,
    kCTFontUIFontSmallEmphasizedSystem = 5,
    kCTFontUIFontMiniSystem = 6,
    kCTFontUIFontMiniEmphasizedSystem = 7,
    kCTFontUIFontViews = 8,
    kCTFontUIFontApplication = 9,
    kCTFontUIFontLabel = 10,
    kCTFontUIFontMenuTitle = 11,
    kCTFontUIFontMenuItem = 12,
    kCTFontUIFontMenuItemMark = 13,
    kCTFontUIFontMenuItemCmdKey = 14,
    kCTFontUIFontWindowTitle = 15,
    kCTFontUIFontPushButton = 16,
    kCTFontUIFontUtilityWindowTitle = 17,
    kCTFontUIFontAlertHeader = 18,
    kCTFontUIFontSystemDetail = 19,
    kCTFontUIFontEmphasizedSystemDetail = 20,
    kCTFontUIFontToolbar = 21,
    kCTFontUIFontSmallToolbar = 22,
    kCTFontUIFontMessage = 23,
    kCTFontUIFontPalette = 24,
    kCTFontUIFontToolTip = 25,
    kCTFontUIFontControlContent = 26
} CTFontUIFontType;

typedef CFOptionFlags CTFontOptions;
enum {
    kCTFontOptionsDefault = 0,
    kCTFontOptionsPreventAutoActivation = 1 << 0,
    kCTFontOptionsPreventAutoDownload = 1 << 1,
    kCTFontOptionsPreferSystemFont = 1 << 2
};

// name keys for CTFontCopyName and CTFontCopyLocalizedName
CORETEXT_EXPORT const CFStringRef kCTFontCopyrightNameKey;
CORETEXT_EXPORT const CFStringRef kCTFontFamilyNameKey;
CORETEXT_EXPORT const CFStringRef kCTFontSubFamilyNameKey;
CORETEXT_EXPORT const CFStringRef kCTFontStyleNameKey;
CORETEXT_EXPORT const CFStringRef kCTFontUniqueNameKey;
CORETEXT_EXPORT const CFStringRef kCTFontFullNameKey;
CORETEXT_EXPORT const CFStringRef kCTFontVersionNameKey;
CORETEXT_EXPORT const CFStringRef kCTFontPostScriptNameKey;
CORETEXT_EXPORT const CFStringRef kCTFontTrademarkNameKey;
CORETEXT_EXPORT const CFStringRef kCTFontManufacturerNameKey;
CORETEXT_EXPORT const CFStringRef kCTFontDesignerNameKey;
CORETEXT_EXPORT const CFStringRef kCTFontDescriptionNameKey;
CORETEXT_EXPORT const CFStringRef kCTFontVendorURLNameKey;
CORETEXT_EXPORT const CFStringRef kCTFontDesignerURLNameKey;
CORETEXT_EXPORT const CFStringRef kCTFontLicenseNameKey;
CORETEXT_EXPORT const CFStringRef kCTFontLicenseURLNameKey;
CORETEXT_EXPORT const CFStringRef kCTFontSampleTextNameKey;
CORETEXT_EXPORT const CFStringRef kCTFontPostScriptCIDNameKey;

// variation axis dictionary keys
CORETEXT_EXPORT const CFStringRef kCTFontVariationAxisIdentifierKey;
CORETEXT_EXPORT const CFStringRef kCTFontVariationAxisMinimumValueKey;
CORETEXT_EXPORT const CFStringRef kCTFontVariationAxisMaximumValueKey;
CORETEXT_EXPORT const CFStringRef kCTFontVariationAxisDefaultValueKey;
CORETEXT_EXPORT const CFStringRef kCTFontVariationAxisNameKey;
CORETEXT_EXPORT const CFStringRef kCTFontVariationAxisHiddenKey;

// feature dictionary keys
CORETEXT_EXPORT const CFStringRef kCTFontOpenTypeFeatureTag;
CORETEXT_EXPORT const CFStringRef kCTFontOpenTypeFeatureValue;
CORETEXT_EXPORT const CFStringRef kCTFontFeatureTypeIdentifierKey;
CORETEXT_EXPORT const CFStringRef kCTFontFeatureTypeNameKey;
CORETEXT_EXPORT const CFStringRef kCTFontFeatureTypeExclusiveKey;
CORETEXT_EXPORT const CFStringRef kCTFontFeatureTypeSelectorsKey;
CORETEXT_EXPORT const CFStringRef kCTFontFeatureSelectorIdentifierKey;
CORETEXT_EXPORT const CFStringRef kCTFontFeatureSelectorNameKey;
CORETEXT_EXPORT const CFStringRef kCTFontFeatureSelectorDefaultKey;
CORETEXT_EXPORT const CFStringRef kCTFontFeatureSelectorSettingKey;
CORETEXT_EXPORT const CFStringRef kCTFontFeatureSampleTextKey;
CORETEXT_EXPORT const CFStringRef kCTFontFeatureTooltipTextKey;

typedef FourCharCode CTFontTableTag;
enum {
    kCTFontTableBASE = 'BASE',
    kCTFontTableCFF = 'CFF ',
    kCTFontTableCFF2 = 'CFF2',
    kCTFontTableCOLR = 'COLR',
    kCTFontTableCPAL = 'CPAL',
    kCTFontTableDSIG = 'DSIG',
    kCTFontTableGDEF = 'GDEF',
    kCTFontTableGPOS = 'GPOS',
    kCTFontTableGSUB = 'GSUB',
    kCTFontTableHVAR = 'HVAR',
    kCTFontTableJSTF = 'JSTF',
    kCTFontTableMATH = 'MATH',
    kCTFontTableMVAR = 'MVAR',
    kCTFontTableOS2 = 'OS/2',
    kCTFontTableSVG = 'SVG ',
    kCTFontTableSTAT = 'STAT',
    kCTFontTableVORG = 'VORG',
    kCTFontTableVVAR = 'VVAR',
    kCTFontTableAvar = 'avar',
    kCTFontTableCmap = 'cmap',
    kCTFontTableCvar = 'cvar',
    kCTFontTableCvt = 'cvt ',
    kCTFontTableFpgm = 'fpgm',
    kCTFontTableFvar = 'fvar',
    kCTFontTableGasp = 'gasp',
    kCTFontTableGlyf = 'glyf',
    kCTFontTableGvar = 'gvar',
    kCTFontTableHdmx = 'hdmx',
    kCTFontTableHead = 'head',
    kCTFontTableHhea = 'hhea',
    kCTFontTableHmtx = 'hmtx',
    kCTFontTableKern = 'kern',
    kCTFontTableLoca = 'loca',
    kCTFontTableMaxp = 'maxp',
    kCTFontTableMorx = 'morx',
    kCTFontTableName = 'name',
    kCTFontTablePost = 'post',
    kCTFontTablePrep = 'prep',
    kCTFontTableSbix = 'sbix',
    kCTFontTableTrak = 'trak',
    kCTFontTableVhea = 'vhea',
    kCTFontTableVmtx = 'vmtx'
};

typedef uint32_t CTFontTableOptions;
enum {
    kCTFontTableOptionNoOptions = 0,
    kCTFontTableOptionExcludeSynthetic = (1 << 0)
};

// creation
CORETEXT_EXPORT CTFontRef CTFontCreateWithName(CFStringRef name, CGFloat size, const CGAffineTransform *matrix);
CORETEXT_EXPORT CTFontRef CTFontCreateWithNameAndOptions(CFStringRef name, CGFloat size, const CGAffineTransform *matrix, CTFontOptions options);
CORETEXT_EXPORT CTFontRef CTFontCreateWithFontDescriptor(CTFontDescriptorRef descriptor, CGFloat size, const CGAffineTransform *matrix);
CORETEXT_EXPORT CTFontRef CTFontCreateWithFontDescriptorAndOptions(CTFontDescriptorRef descriptor, CGFloat size, const CGAffineTransform *matrix, CTFontOptions options);
CORETEXT_EXPORT CTFontRef CTFontCreateWithGraphicsFont(CGFontRef graphicsFont, CGFloat size, const CGAffineTransform *matrix, CTFontDescriptorRef attributes);
CORETEXT_EXPORT CTFontRef CTFontCreateUIFontForLanguage(CTFontUIFontType uiType, CGFloat size, CFStringRef language);
CORETEXT_EXPORT CTFontRef CTFontCreateCopyWithAttributes(CTFontRef font, CGFloat size, const CGAffineTransform *matrix, CTFontDescriptorRef attributes);
CORETEXT_EXPORT CTFontRef CTFontCreateCopyWithSymbolicTraits(CTFontRef font, CGFloat size, const CGAffineTransform *matrix, CTFontSymbolicTraits symTraitValue, CTFontSymbolicTraits symTraitMask);
CORETEXT_EXPORT CTFontRef CTFontCreateCopyWithFamily(CTFontRef font, CGFloat size, const CGAffineTransform *matrix, CFStringRef family);
CORETEXT_EXPORT CTFontRef CTFontCreateForString(CTFontRef currentFont, CFStringRef string, CFRange range);
CORETEXT_EXPORT CTFontRef CTFontCreateForStringWithLanguage(CTFontRef currentFont, CFStringRef string, CFRange range, CFStringRef language);

// descriptors and attributes
CORETEXT_EXPORT CTFontDescriptorRef CTFontCopyFontDescriptor(CTFontRef font);
CORETEXT_EXPORT CFTypeRef CTFontCopyAttribute(CTFontRef font, CFStringRef attribute);
CORETEXT_EXPORT CGFloat CTFontGetSize(CTFontRef font);
CORETEXT_EXPORT CGAffineTransform CTFontGetMatrix(CTFontRef font);
CORETEXT_EXPORT CTFontSymbolicTraits CTFontGetSymbolicTraits(CTFontRef font);
CORETEXT_EXPORT CFDictionaryRef CTFontCopyTraits(CTFontRef font);
CORETEXT_EXPORT CFArrayRef CTFontCopyDefaultCascadeListForLanguages(CTFontRef font, CFArrayRef languagePrefList);

// names
CORETEXT_EXPORT CFStringRef CTFontCopyPostScriptName(CTFontRef font);
CORETEXT_EXPORT CFStringRef CTFontCopyFamilyName(CTFontRef font);
CORETEXT_EXPORT CFStringRef CTFontCopyFullName(CTFontRef font);
CORETEXT_EXPORT CFStringRef CTFontCopyDisplayName(CTFontRef font);
CORETEXT_EXPORT CFStringRef CTFontCopyName(CTFontRef font, CFStringRef nameKey);
CORETEXT_EXPORT CFStringRef CTFontCopyLocalizedName(CTFontRef font, CFStringRef nameKey, CFStringRef *actualLanguage);

// encoding and coverage
CORETEXT_EXPORT CFCharacterSetRef CTFontCopyCharacterSet(CTFontRef font);
CORETEXT_EXPORT CFStringEncoding CTFontGetStringEncoding(CTFontRef font);
CORETEXT_EXPORT CFArrayRef CTFontCopySupportedLanguages(CTFontRef font);
CORETEXT_EXPORT bool CTFontGetGlyphsForCharacters(CTFontRef font, const UniChar *characters, CGGlyph *glyphs, CFIndex count);

// metrics
CORETEXT_EXPORT CGFloat CTFontGetAscent(CTFontRef font);
CORETEXT_EXPORT CGFloat CTFontGetDescent(CTFontRef font);
CORETEXT_EXPORT CGFloat CTFontGetLeading(CTFontRef font);
CORETEXT_EXPORT unsigned CTFontGetUnitsPerEm(CTFontRef font);
CORETEXT_EXPORT CFIndex CTFontGetGlyphCount(CTFontRef font);
CORETEXT_EXPORT CGRect CTFontGetBoundingBox(CTFontRef font);
CORETEXT_EXPORT CGFloat CTFontGetUnderlinePosition(CTFontRef font);
CORETEXT_EXPORT CGFloat CTFontGetUnderlineThickness(CTFontRef font);
CORETEXT_EXPORT CGFloat CTFontGetSlantAngle(CTFontRef font);
CORETEXT_EXPORT CGFloat CTFontGetCapHeight(CTFontRef font);
CORETEXT_EXPORT CGFloat CTFontGetXHeight(CTFontRef font);

// glyphs
CORETEXT_EXPORT CGGlyph CTFontGetGlyphWithName(CTFontRef font, CFStringRef glyphName);
CORETEXT_EXPORT CFStringRef CTFontCopyNameForGlyph(CTFontRef font, CGGlyph glyph);
CORETEXT_EXPORT CGRect CTFontGetBoundingRectsForGlyphs(CTFontRef font, CTFontOrientation orientation, const CGGlyph *glyphs, CGRect *boundingRects, CFIndex count);
CORETEXT_EXPORT CGRect CTFontGetOpticalBoundsForGlyphs(CTFontRef font, const CGGlyph *glyphs, CGRect *boundingRects, CFIndex count, CFOptionFlags options);
CORETEXT_EXPORT double CTFontGetAdvancesForGlyphs(CTFontRef font, CTFontOrientation orientation, const CGGlyph *glyphs, CGSize *advances, CFIndex count);
CORETEXT_EXPORT void CTFontGetVerticalTranslationsForGlyphs(CTFontRef font, const CGGlyph *glyphs, CGSize *translations, CFIndex count);
CORETEXT_EXPORT CGPathRef CTFontCreatePathForGlyph(CTFontRef font, CGGlyph glyph, const CGAffineTransform *matrix);
CORETEXT_EXPORT void CTFontDrawGlyphs(CTFontRef font, const CGGlyph *glyphs, const CGPoint *positions, size_t count, CGContextRef context);
CORETEXT_EXPORT CFIndex CTFontGetLigatureCaretPositions(CTFontRef font, CGGlyph glyph, CGFloat *positions, CFIndex maxPositions);

// variations, features and tables
CORETEXT_EXPORT CFArrayRef CTFontCopyVariationAxes(CTFontRef font);
CORETEXT_EXPORT CFDictionaryRef CTFontCopyVariation(CTFontRef font);
CORETEXT_EXPORT CFArrayRef CTFontCopyFeatures(CTFontRef font);
CORETEXT_EXPORT CFArrayRef CTFontCopyFeatureSettings(CTFontRef font);
CORETEXT_EXPORT CGFontRef CTFontCopyGraphicsFont(CTFontRef font, CTFontDescriptorRef *attributes);
CORETEXT_EXPORT CFArrayRef CTFontCopyAvailableTables(CTFontRef font, CTFontTableOptions options);
CORETEXT_EXPORT CFDataRef CTFontCopyTable(CTFontRef font, CTFontTableTag table, CTFontTableOptions options);

#endif
