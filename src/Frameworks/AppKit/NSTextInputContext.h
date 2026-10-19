// NSTextInputContext.h - text input for a view, from key events to its NSTextInputClient
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import <Foundation/NSObject.h>
#import <Foundation/NSRange.h>
#import <Foundation/NSGeometry.h>
#import <AppKit/AppKitExport.h>

@class NSArray, NSString, NSEvent, NSAttributedString;

@protocol NSTextInputClient
@required
- (void)insertText:(id)string replacementRange:(NSRange)replacementRange;
- (void)doCommandBySelector:(SEL)selector;
- (void)setMarkedText:(id)string selectedRange:(NSRange)selectedRange replacementRange:(NSRange)replacementRange;
- (void)unmarkText;
- (NSRange)selectedRange;
- (NSRange)markedRange;
- (BOOL)hasMarkedText;
- (NSAttributedString *)attributedSubstringForProposedRange:(NSRange)range actualRange:(NSRangePointer)actualRange;
- (NSArray *)validAttributesForMarkedText;
- (NSRect)firstRectForCharacterRange:(NSRange)range actualRange:(NSRangePointer)actualRange;
- (NSUInteger)characterIndexForPoint:(NSPoint)point;
@optional
- (NSAttributedString *)attributedString;
- (CGFloat)fractionOfDistanceThroughGlyphForPoint:(NSPoint)point;
- (CGFloat)baselineDeltaForCharacterAtIndex:(NSUInteger)index;
- (NSInteger)windowLevel;
- (BOOL)drawsVerticallyForCharacterAtIndex:(NSUInteger)index;
@end

APPKIT_EXPORT NSString *const NSTextInputContextKeyboardSelectionDidChangeNotification;

@interface NSTextInputContext : NSObject {
    id _client;
    BOOL _acceptsGlyphInfo;
    NSArray *_allowedInputSourceLocales;
    NSString *_selectedKeyboardInputSource;
}

+ (NSTextInputContext *)currentInputContext;
+ (NSString *)localizedNameForInputSource:(NSString *)inputSourceIdentifier;

- (instancetype)initWithClient:(id<NSTextInputClient>)client;

@property(readonly) id<NSTextInputClient> client;
@property BOOL acceptsGlyphInfo;
@property(copy) NSArray *allowedInputSourceLocales;
@property(readonly) NSArray *keyboardInputSources;
@property(copy) NSString *selectedKeyboardInputSource;

- (void)activate;
- (void)deactivate;
- (BOOL)handleEvent:(NSEvent *)event;
- (void)discardMarkedText;
- (void)invalidateCharacterCoordinates;

@end
