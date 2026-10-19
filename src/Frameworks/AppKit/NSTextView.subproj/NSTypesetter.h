#import <AppKit/NSText.h>
#import <AppKit/NSFont.h>
#import <AppKit/NSLayoutManager.h>
#import <AppKit/NSGlyphGenerator.h>

@class NSLayoutManager, NSParagraphStyle, NSTextTab;

typedef enum {
    NSTypesetterLatestBehavior,
} NSTypesetterBehavior;

typedef enum {
    NSTypesetterZeroAdvancementAction,
    NSTypesetterWhitespaceAction,
    NSTypesetterHorizontalTabAction,
    NSTypesetterLineBreakAction,
    NSTypesetterParagraphBreakAction,
    NSTypesetterContainerBreakAction,
} NSTypesetterControlCharacterAction;

@interface NSTypesetter : NSObject <NSGlyphStorage> {
    NSTypesetterBehavior _behavior;
    float _hyphenationFactor;
    float _lineFragmentPadding;
    BOOL _usesFontLeading;
    BOOL _bidiProcessingEnabled;

    NSLayoutManager *_layoutManager;
    NSArray *_textContainers;
    NSAttributedString *_attributedString;
    NSString *_string;

    NSTextContainer *_currentTextContainer;
    NSParagraphStyle *_currentParagraphStyle;
}

+ (NSTypesetterBehavior)defaultTypesetterBehavior;

+ (NSSize)printingAdjustmentInLayoutManager:(NSLayoutManager *)layoutManager forNominallySpacedGlyphRange:(NSRange)glyphRange packedGlyphs:(const unsigned char *)packedGlyphs count:(unsigned)count;

+ sharedSystemTypesetterForBehavior:(NSTypesetterBehavior)behavior;

+ sharedSystemTypesetter;

- (NSTypesetterBehavior)typesetterBehavior;
- (CGFloat)hyphenationFactor;
- (CGFloat)lineFragmentPadding;
- (BOOL)usesFontLeading;
- (BOOL)bidiProcessingEnabled;

- (void)setTypesetterBehavior:(NSTypesetterBehavior)behavior;
- (void)setHyphenationFactor:(CGFloat)factor;
- (void)setLineFragmentPadding:(CGFloat)padding;
- (void)setUsesFontLeading:(BOOL)flag;
- (void)setBidiProcessingEnabled:(BOOL)flag;

// glyph storage

- (NSRange)characterRangeForGlyphRange:(NSRange)glyphRange actualGlyphRange:(NSRange *)actualGlyphRange;
- (NSRange)glyphRangeForCharacterRange:(NSRange)characterRange actualCharacterRange:(NSRange *)actualCharacterRange;
- (unsigned)getGlyphsInRange:(NSRange)glyphRange glyphs:(NSGlyph *)glyphs characterIndexes:(unsigned *)characterIndexes glyphInscriptions:(NSGlyphInscription *)glyphInscriptions elasticBits:(BOOL *)elasticBits bidiLevels:(unsigned char *)bidiLevels;
- (void)getLineFragmentRect:(NSRect *)fragmentRect usedRect:(NSRect *)usedRect remainingRect:(NSRect *)remainingRect forStartingGlyphAtIndex:(unsigned)startingGlyphIndex proposedRect:(NSRect)proposedRect lineSpacing:(CGFloat)lineSpacing paragraphSpacingBefore:(CGFloat)paragraphSpacingBefore paragraphSpacingAfter:(CGFloat)paragraphSpacingAfter;
- (void)setLineFragmentRect:(NSRect)fragmentRect forGlyphRange:(NSRange)glyphRange usedRect:(NSRect)usedRect baselineOffset:(CGFloat)baselineOffset;
- (void)substituteGlyphsInRange:(NSRange)glyphRange withGlyphs:(NSGlyph *)glyphs;
- (void)insertGlyph:(NSGlyph)glyph atGlyphIndex:(unsigned)glyphIndex characterIndex:(unsigned)characterIndex;

- (void)deleteGlyphsInRange:(NSRange)glyphRange;
- (void)setNotShownAttribute:(BOOL)flag forGlyphRange:(NSRange)range;

- (void)setDrawsOutsideLineFragment:(BOOL)flag forGlyphRange:(NSRange)range;
- (void)setLocation:(NSPoint)location withAdvancements:(const CGFloat *)nominalAdvancements forStartOfGlyphRange:(NSRange)glyphRange;
- (void)setAttachmentSize:(NSSize)size forGlyphRange:(NSRange)glyphRange;
- (void)setBidiLevels:(const unsigned char *)bidiLevels forGlyphRange:(NSRange)glyphRange;

// layout

- (void)willSetLineFragmentRect:(NSRect *)fragmentRect forGlyphRange:(NSRange)glyphRange usedRect:(NSRect *)usedRect baselineOffset:(CGFloat *)baselineOffset;
- (BOOL)shouldBreakLineByHyphenatingBeforeCharacterAtIndex:(unsigned)characterIndex;

- (BOOL)shouldBreakLineByWordBeforeCharacterAtIndex:(unsigned)characterIndex;

- (CGFloat)hyphenationFactorForGlyphAtIndex:(unsigned)glyphIndex;

- (unichar)hyphenCharacterForGlyphAtIndex:(unsigned)glyphIndex;

- (NSRect)boundingBoxForControlGlyphAtIndex:(unsigned)glyphIndex forTextContainer:(NSTextContainer *)textContainer proposedLineFragment:(NSRect)proposedRect glyphPosition:(NSPoint)glyphPosition characterIndex:(unsigned)characterIndex;
//--

- (NSAttributedString *)attributedString;
- (NSDictionary *)attributesForExtraLineFragment;

- (NSLayoutManager *)layoutManager;

- (NSArray *)textContainers;
- (NSTextContainer *)currentTextContainer;

- (NSParagraphStyle *)currentParagraphStyle;
- (NSRange)paragraphCharacterRange;
- (NSRange)paragraphGlyphRange;
- (NSRange)paragraphSeparatorCharacterRange;
- (NSRange)paragraphSeparatorGlyphRange;
- (NSTypesetterControlCharacterAction)actionForControlCharacterAtIndex:(unsigned)characterIndex;
- (NSFont *)substituteFontForFont:(NSFont *)font;

- (void)setAttributedString:(NSAttributedString *)text;
- (void)setHardInvalidation:(BOOL)invalidate forGlyphRange:(NSRange)glyphRange;
- (void)setParagraphGlyphRange:(NSRange)glyphRange separatorGlyphRange:(NSRange)separatorGlyphRange;

- (void)beginLineWithGlyphAtIndex:(unsigned)glyphIndex;
- (void)endLineWithGlyphRange:(NSRange)glyphRange;

- (void)beginParagraph;
- (void)endParagraph;

- (CGFloat)baselineOffsetInLayoutManager:(NSLayoutManager *)layoutManager glyphIndex:(unsigned)glyphIndex;
- (NSTextTab *)textTabForGlyphLocation:(CGFloat)location writingDirection:(NSWritingDirection)direction maxLocation:(CGFloat)maxLocation;

- (void)getLineFragmentRect:(NSRect *)fragmentRect usedRect:(NSRect *)usedRect forParagraphSeparatorGlyphRange:(NSRange)glyphRange atProposedOrigin:(NSPoint)proposedOrigin;

- (CGFloat)lineSpacingAfterGlyphAtIndex:(unsigned)glyphIndex withProposedLineFragmentRect:(NSRect)rect;
- (CGFloat)paragraphSpacingAfterGlyphAtIndex:(unsigned)glyphIndex withProposedLineFragmentRect:(NSRect)rect;
- (CGFloat)paragraphSpacingBeforeGlyphAtIndex:(unsigned)glyphIndex withProposedLineFragmentRect:(NSRect)rect;

- (unsigned)layoutParagraphAtPoint:(NSPoint *)point;

- (void)layoutGlyphsInLayoutManager:(NSLayoutManager *)layoutManager startingAtGlyphIndex:(unsigned)startGlyphIndex maxNumberOfLineFragments:(unsigned)maxFragments nextGlyphIndex:(unsigned *)nextGlyph;

@end
