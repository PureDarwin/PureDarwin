// NSTextInputContext.m - text input for a view, from key events to its NSTextInputClient
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import <AppKit/NSTextInputContext.h>
#import <AppKit/NSApplication.h>
#import <AppKit/NSWindow.h>
#import <AppKit/NSView.h>
#import <AppKit/NSEvent.h>
#import <AppKit/NSKeyboardBindingManager.h>
#import <AppKit/NSKeyboardBinding.h>
#import <Foundation/Foundation.h>

NSString *const NSTextInputContextKeyboardSelectionDidChangeNotification = @"NSTextInputContextKeyboardSelectionDidChangeNotification";

// the layout PureDarwin's keyboard uses, in the identifier form input sources have on macOS
static NSString *const PDDefaultInputSource = @"com.apple.keylayout.US";

static NSTextInputContext *currentContext;

@implementation NSTextInputContext

@synthesize acceptsGlyphInfo = _acceptsGlyphInfo;

+ (NSTextInputContext *)currentInputContext
{
    id responder = [[NSApp keyWindow] firstResponder];

    if ([responder respondsToSelector:@selector(inputContext)]) {
        NSTextInputContext *context = [responder inputContext];
        if (context != nil)
            return context;
    }
    return currentContext;
}

+ (NSString *)localizedNameForInputSource:(NSString *)inputSourceIdentifier
{
    if ([inputSourceIdentifier isEqualToString:PDDefaultInputSource])
        return @"U.S.";
    return [[inputSourceIdentifier componentsSeparatedByString:@"."] lastObject];
}

// the client owns its context, so the context only points back at it
- (instancetype)initWithClient:(id<NSTextInputClient>)client
{
    if ((self = [super init]) == nil)
        return nil;
    _client = client;
    _selectedKeyboardInputSource = [PDDefaultInputSource copy];
    return self;
}

- (void)dealloc
{
    if (currentContext == self)
        currentContext = nil;
    [_allowedInputSourceLocales release];
    [_selectedKeyboardInputSource release];
    [super dealloc];
}

- (id<NSTextInputClient>)client
{
    return _client;
}

- (NSArray *)allowedInputSourceLocales
{
    return _allowedInputSourceLocales;
}

- (void)setAllowedInputSourceLocales:(NSArray *)locales
{
    locales = [locales copy];
    [_allowedInputSourceLocales release];
    _allowedInputSourceLocales = locales;
}

- (NSArray *)keyboardInputSources
{
    return @[ PDDefaultInputSource ];
}

- (NSString *)selectedKeyboardInputSource
{
    return _selectedKeyboardInputSource;
}

- (void)setSelectedKeyboardInputSource:(NSString *)source
{
    if (source == nil || ![[self keyboardInputSources] containsObject:source])
        return;
    source = [source copy];
    [_selectedKeyboardInputSource release];
    _selectedKeyboardInputSource = source;
    [[NSNotificationCenter defaultCenter] postNotificationName:NSTextInputContextKeyboardSelectionDidChangeNotification object:self];
}

- (void)activate
{
    currentContext = self;
}

- (void)deactivate
{
    if (currentContext == self)
        currentContext = nil;
}

// keys bound to commands go to doCommandBySelector:, printable characters are inserted as text
- (BOOL)handleEvent:(NSEvent *)event
{
    NSString *characters;
    NSKeyboardBinding *binding;
    NSArray *selectors;

    if ([event type] != NSKeyDown)
        return NO;
    binding = [[NSKeyboardBindingManager defaultKeyBindingManager] keyBindingWithString:[event charactersIgnoringModifiers]
                                                                         modifierFlags:[event modifierFlags]];
    selectors = [binding selectorNames];
    if ([selectors count] > 0) {
        for (NSString *name in selectors)
            [_client doCommandBySelector:NSSelectorFromString(name)];
        return YES;
    }
    if ([event modifierFlags] & (NSCommandKeyMask | NSControlKeyMask))
        return NO;
    characters = [event characters];
    if ([characters length] == 0)
        return NO;
    // function keys and control characters are commands, never text
    for (NSUInteger i = 0; i < [characters length]; i++) {
        unichar c = [characters characterAtIndex:i];
        if (c < ' ' || c == 0x7f || (c >= NSUpArrowFunctionKey && c <= NSModeSwitchFunctionKey))
            return NO;
    }
    if ([_client hasMarkedText])
        [_client unmarkText];
    [_client insertText:characters replacementRange:NSMakeRange(NSNotFound, 0)];
    return YES;
}

- (void)discardMarkedText
{
    if ([_client hasMarkedText])
        [_client unmarkText];
}

// there is no input method window to move, so nothing caches character positions
- (void)invalidateCharacterCoordinates
{
}

@end
