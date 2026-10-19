// CTLayout.h - the objects behind CoreText's layout types
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#import "CTInternal.h"

// every paragraph style value, filled with Apple's defaults first
typedef struct CTParagraphStyleValues {
    CTTextAlignment alignment;
    CGFloat firstLineHeadIndent;
    CGFloat headIndent;
    CGFloat tailIndent;
    CFArrayRef tabStops;
    CGFloat defaultTabInterval;
    CTLineBreakMode lineBreakMode;
    CGFloat lineHeightMultiple;
    CGFloat maximumLineHeight;
    CGFloat minimumLineHeight;
    CGFloat lineSpacing;
    CGFloat paragraphSpacing;
    CGFloat paragraphSpacingBefore;
    CTWritingDirection baseWritingDirection;
    CGFloat maximumLineSpacing;
    CGFloat minimumLineSpacing;
    CGFloat lineSpacingAdjustment;
    CTLineBoundsOptions lineBoundsOptions;
} CTParagraphStyleValues;

@interface CTParagraphStyle : NSObject {
@public
    CTParagraphStyleValues _values;
    CFArrayRef _tabStops;
}
@end

@interface CTRunDelegate : NSObject {
@public
    CTRunDelegateCallbacks _callbacks;
    void *_refCon;
}
@end

void CTParagraphStyleGetDefaults(CTParagraphStyleValues *values);
void CTParagraphStyleValuesFromAttributes(CFDictionaryRef attributes, CTParagraphStyleValues *values);

// one shaped glyph run, positions relative to the line origin
@interface CTRun : NSObject {
@public
    CFDictionaryRef _attributes;
    CTFontRef _font;
    CFIndex _count;
    CGGlyph *_glyphs;
    CGPoint *_positions;
    CGSize *_advances;
    CFIndex *_indices;
    CFRange _range;
    CTRunStatus _status;
    CGFloat _ascent;
    CGFloat _descent;
    CGFloat _leading;
    double _width;
    uint8_t _level;
}
@end

@interface CTLine : NSObject {
@public
    CFMutableArrayRef _runs;
    CFRange _range;
    double _width;
    CGFloat _ascent;
    CGFloat _descent;
    CGFloat _leading;
    double _trailingWhitespace;
    CFAttributedStringRef _string;
}
@end

@interface CTTypesetter : NSObject {
@public
    CFAttributedStringRef _string;
    CFIndex _length;
    UniChar *_text;
    uint8_t *_levels;
    double *_charAdvances;
    bool _disableBidi;
    int _forcedLevel;
    CFMutableDictionaryRef _fallbacks;
}
@end

@interface CTFrame : NSObject {
@public
    CFArrayRef _lines;
    CGPoint *_origins;
    CFRange _range;
    CFRange _visible;
    CGPathRef _path;
    CFDictionaryRef _attributes;
}
@end

CTRun *CTRunCreate(CFDictionaryRef attributes, CTFontRef font, CFIndex count);
void CTLineFinish(CTLine *line);
CTLineRef CTLineCreateWithRuns(CFArrayRef runs, CFRange range, CFAttributedStringRef string);

// the font to use when a run has none, Helvetica 12 as on Apple
CTFontRef CTCopyDefaultFont(void);
