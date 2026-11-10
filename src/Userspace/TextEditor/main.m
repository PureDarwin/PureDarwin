// main.m - Text Editor: plain text documents, one window each
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import <AppKit/AppKit.h>
#import "PDUtility.h"

#define FIND_H 34

static CGFloat fontSize = 13;
static BOOL wrapLines = YES;
static BOOL proportional = NO;

static NSFont *editorFont(void)
{
    return proportional ? PDFont(@"Inter-Regular", fontSize) : PDMono(fontSize);
}

@class EditorController;

// one document: the toolbar and status line drawn around a text view, the find bar dropping under the toolbar
@interface EditorView : NSView <NSTextFieldDelegate> {
@public
    NSScrollView *_scroll;
    NSTextView *_text;
    NSTextField *_find;
    NSString *_path;
    NSString *_saved;
    BOOL _finding;
    NSUInteger _matches;
    NSArray *_buttons;
    EditorController *_owner;
}
- (instancetype)initWithFrame:(NSRect)frame path:(NSString *)path owner:(EditorController *)owner;
- (BOOL)edited;
- (void)applyStyle;
- (IBAction)saveDocument:(id)sender;
- (IBAction)saveDocumentAs:(id)sender;
- (IBAction)revertDocumentToSaved:(id)sender;
- (IBAction)showFind:(id)sender;
- (IBAction)findNext:(id)sender;
- (IBAction)findPrevious:(id)sender;
@end

@interface EditorController : NSObject {
    NSMutableArray *_windows;
    int _untitled;
}
- (void)openPath:(NSString *)path;
- (void)restyle;
@end

@implementation EditorView

- (instancetype)initWithFrame:(NSRect)frame path:(NSString *)path owner:(EditorController *)owner
{
    NSString *contents = @"";

    if ((self = [super initWithFrame:frame]) == nil)
        return nil;
    _owner = owner;
    _path = [path copy];
    if (path != nil && [[NSFileManager defaultManager] fileExistsAtPath:path]) {
        NSData *data = [NSData dataWithContentsOfFile:path];
        contents = [[[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding] autorelease];
        if (contents == nil)
            contents = [[[NSString alloc] initWithData:data encoding:NSISOLatin1StringEncoding] autorelease];
    }
    _saved = [contents copy];
    [self setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];

    _scroll = [[NSScrollView alloc] initWithFrame:NSZeroRect];
    [_scroll setHasVerticalScroller:YES];
    [_scroll setBorderType:NSNoBorder];
    _text = [[NSTextView alloc] initWithFrame:NSMakeRect(0, 0, 600, 400)];
    [_text setRichText:NO];
    [_text setAllowsUndo:YES];
    [_text setDelegate:self];
    [_text setTextContainerInset:NSMakeSize(12, 12)];
    [_text setString:contents];
    [_text setSelectedRange:NSMakeRange(0, 0)];
    [_scroll setDocumentView:_text];
    [self addSubview:_scroll];

    _find = [[NSTextField alloc] initWithFrame:NSZeroRect];
    [_find setFont:PDFont(@"Inter-Regular", 12)];
    [[_find cell] setPlaceholderString:NSLocalizedString(@"Find", @"")];
    [_find setDelegate:self];
    [_find setTarget:self];
    [_find setAction:@selector(findNext:)];
    [_find setHidden:YES];
    [self addSubview:_find];
    _buttons = [@[ [PDToolButton buttonWithGlyph:@"search" label:nil target:self action:@selector(showFind:)] ] retain];
    [self applyStyle];
    [self placeViews];
    return self;
}

- (void)dealloc
{
    [_scroll release];
    [_text release];
    [_find release];
    [_path release];
    [_saved release];
    [_buttons release];
    [super dealloc];
}

- (BOOL)isFlipped
{
    return YES;
}

- (NSString *)name
{
    return _path != nil ? [_path lastPathComponent] : [[self window] title];
}

- (BOOL)edited
{
    return ![[_text string] isEqualToString:_saved];
}

- (void)applyStyle
{
    NSTextContainer *tc = [_text textContainer];

    [_text setFont:editorFont()];
    [_text setTextColor:PDColor(0x1b1b1b, 0xe6e6e6)];
    [_text setBackgroundColor:PDColor(0xffffff, 0x1f1f21)];
    [_text setInsertionPointColor:PDColor(0x1b1b1b, 0xe6e6e6)];
    // wrapped lines follow the window, unwrapped ones run on and the view scrolls sideways
    [_scroll setHasHorizontalScroller:!wrapLines];
    [_text setHorizontallyResizable:!wrapLines];
    [tc setWidthTracksTextView:wrapLines];
    [tc setContainerSize:NSMakeSize(wrapLines ? NSWidth([_scroll bounds]) : 1e7, 1e7)];
    [_text setMaxSize:NSMakeSize(1e7, 1e7)];
    [self setNeedsDisplay:YES];
}

- (void)placeViews
{
    NSRect b = [self bounds];
    CGFloat top = PD_TOOLBAR_H + 1 + (_finding ? FIND_H : 0);

    [_scroll setFrame:NSMakeRect(0, top, NSWidth(b), NSHeight(b) - top - PD_STATUS_H)];
    [_find setFrame:NSMakeRect(12, PD_TOOLBAR_H + 6, 240, 22)];
    [_find setHidden:!_finding];
    if (wrapLines)
        [[_text textContainer] setContainerSize:NSMakeSize(NSWidth([_scroll bounds]), 1e7)];
}

- (void)resizeWithOldSuperviewSize:(NSSize)old
{
    [super resizeWithOldSuperviewSize:old];
    [self placeViews];
}

- (void)drawRect:(NSRect)dirty
{
    NSRect b = [self bounds];
    NSString *s = [_text string];
    NSRange sel = [_text selectedRange];
    NSString *before = [s substringToIndex:MIN(sel.location, [s length])];
    NSArray *lines = [before componentsSeparatedByString:@"\n"];
    NSUInteger words = 0;
    NSString *where = _path != nil ? [[_path stringByDeletingLastPathComponent] stringByAbbreviatingWithTildeInPath] : nil;

    for (NSString *w in [s componentsSeparatedByCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]]) {
        if ([w length] > 0)
            words++;
    }
    [[NSBezierPath bezierPathWithRoundedRect:b xRadius:8 yRadius:8] addClip];
    PDDrawToolbar(self, NSMakeRect(0, 0, NSWidth(b), PD_TOOLBAR_H));
    PDDrawTitle([self edited] ? [NSString stringWithFormat:NSLocalizedString(@"%@ — Edited", @""), [self name]] : [self name], where,
                NSMakePoint(PD_LIGHTS_W, PD_TOOLBAR_H / 2), NSWidth(b) - PD_LIGHTS_W - 60, NO);
    PDLayoutButtons(_buttons, NSWidth(b) - 10, PD_TOOLBAR_H, NO);
    if (_finding) {
        NSDictionary *a = PDText(PDFont(@"Inter-Regular", 12), PDColor(0x8a8a8a, 0x8a8a8a));
        NSString *hits = [[_find stringValue] length] == 0 ? @"" : _matches == 0 ? NSLocalizedString(@"Not found", @"")
                       : [NSString stringWithFormat:_matches == 1 ? NSLocalizedString(@"%lu match", @"") : NSLocalizedString(@"%lu matches", @""), (unsigned long)_matches];

        [PDColor(0xf4f4f4, 0x262628) setFill];
        NSRectFill(NSMakeRect(0, PD_TOOLBAR_H + 1, NSWidth(b), FIND_H));
        [PDColor(0xe2e2e2, 0x141414) setFill];
        NSRectFill(NSMakeRect(0, PD_TOOLBAR_H + FIND_H, NSWidth(b), 1));
        [hits drawAtPoint:NSMakePoint(264, PD_TOOLBAR_H + 9) withAttributes:a];
        [NSLocalizedString(@"Done", @"") drawAtPoint:NSMakePoint(NSWidth(b) - 50, PD_TOOLBAR_H + 9)
                                      withAttributes:PDText(PDFont(@"Inter-SemiBold", 12), PDColor(0x4a4a4a, 0xc4c4c4))];
    }
    PDDrawStatus(NSMakeRect(0, NSHeight(b) - PD_STATUS_H, NSWidth(b), PD_STATUS_H),
                 [NSString stringWithFormat:NSLocalizedString(@"Line %lu, Column %lu   Plain Text   UTF-8   LF", @""),
                  (unsigned long)[lines count], (unsigned long)[[lines lastObject] length] + 1],
                 [NSString stringWithFormat:NSLocalizedString(@"%lu words, %lu characters", @""), (unsigned long)words, (unsigned long)[s length]]);
}

- (void)mouseDown:(NSEvent *)event
{
    NSPoint p = [self convertPoint:[event locationInWindow] fromView:nil];

    if (PDClickButtons(_buttons, p))
        return;
    if (_finding && p.y > PD_TOOLBAR_H && p.y < PD_TOOLBAR_H + FIND_H && p.x > NSWidth([self bounds]) - 60) {
        _finding = NO;
        [self placeViews];
        [[self window] makeFirstResponder:_text];
        [self setNeedsDisplay:YES];
        return;
    }
    if (p.y < PD_TOOLBAR_H)
        [[self window] performWindowDragWithEvent:event];
}

- (void)textDidChange:(NSNotification *)note
{
    [self setNeedsDisplay:YES];
    [[self window] setDocumentEdited:[self edited]];
}

- (void)textViewDidChangeSelection:(NSNotification *)note
{
    [self setNeedsDisplayInRect:NSMakeRect(0, NSHeight([self bounds]) - PD_STATUS_H, NSWidth([self bounds]), PD_STATUS_H)];
}

- (BOOL)writeTo:(NSString *)path
{
    NSError *error = nil;

    if (![[_text string] writeToFile:path atomically:YES encoding:NSUTF8StringEncoding error:&error]) {
        [PDSheet showInView:self icon:@"texteditor" title:NSLocalizedString(@"The document couldn’t be saved.", @"")
                    message:error != nil ? [error localizedDescription] : path field:nil mono:nil
                    buttons:@[ NSLocalizedString(@"OK", @"") ] styles:@[ @"go" ] done:nil];
        return NO;
    }
    [_path release];
    _path = [path copy];
    [_saved release];
    _saved = [[_text string] copy];
    [[self window] setTitle:[path lastPathComponent]];
    [[self window] setDocumentEdited:NO];
    [self setNeedsDisplay:YES];
    return YES;
}

- (IBAction)saveDocument:(id)sender
{
    if (_path == nil)
        [self saveDocumentAs:sender];
    else
        [self writeTo:_path];
}

// a name in the document's folder, or in Documents for a new one
- (IBAction)saveDocumentAs:(id)sender
{
    NSString *dir = _path != nil ? [_path stringByDeletingLastPathComponent] : [NSHomeDirectory() stringByAppendingPathComponent:@"Documents"];
    NSString *name = _path != nil ? [_path lastPathComponent] : [[[self window] title] stringByAppendingPathExtension:@"txt"];

    [PDSheet showInView:self icon:@"texteditor" title:NSLocalizedString(@"Save As", @"")
                message:[NSString stringWithFormat:NSLocalizedString(@"Choose a name for this document in %@.", @""), [dir stringByAbbreviatingWithTildeInPath]]
                  field:name mono:nil buttons:@[ NSLocalizedString(@"Cancel", @""), NSLocalizedString(@"Save", @"") ] styles:@[ @"", @"go" ]
                   done:^(NSInteger i, NSString *field) {
                       NSString *n = [field stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];

                       if (i == 1 && [n length] > 0)
                           [self writeTo:[n isAbsolutePath] ? [n stringByExpandingTildeInPath] : [dir stringByAppendingPathComponent:n]];
                       [[self window] makeFirstResponder:_text];
                   }];
}

- (IBAction)revertDocumentToSaved:(id)sender
{
    [_text setString:_saved];
    [self textDidChange:nil];
}

- (NSUInteger)countMatches
{
    NSString *q = [_find stringValue], *s = [_text string];
    NSRange r = NSMakeRange(0, [s length]);
    NSUInteger n = 0;

    if ([q length] == 0)
        return 0;
    while ((r = [s rangeOfString:q options:NSCaseInsensitiveSearch range:r]).location != NSNotFound) {
        n++;
        r = NSMakeRange(NSMaxRange(r), [s length] - NSMaxRange(r));
    }
    return n;
}

- (void)findFrom:(BOOL)forward
{
    NSString *q = [_find stringValue], *s = [_text string];
    NSRange sel = [_text selectedRange], found;

    _matches = [self countMatches];
    if ([q length] == 0 || _matches == 0) {
        [self setNeedsDisplay:YES];
        return;
    }
    if (forward)
        found = [s rangeOfString:q options:NSCaseInsensitiveSearch range:NSMakeRange(NSMaxRange(sel), [s length] - NSMaxRange(sel))];
    else
        found = [s rangeOfString:q options:NSCaseInsensitiveSearch | NSBackwardsSearch range:NSMakeRange(0, sel.location)];
    // past the end it wraps round
    if (found.location == NSNotFound)
        found = [s rangeOfString:q options:NSCaseInsensitiveSearch | (forward ? 0 : NSBackwardsSearch)];
    [_text setSelectedRange:found];
    [_text scrollRangeToVisible:found];
    [self setNeedsDisplay:YES];
}

- (IBAction)showFind:(id)sender
{
    _finding = YES;
    [self placeViews];
    [[self window] makeFirstResponder:_find];
    [self setNeedsDisplay:YES];
}

- (IBAction)findNext:(id)sender
{
    if (!_finding)
        [self showFind:sender];
    else
        [self findFrom:YES];
}

- (IBAction)findPrevious:(id)sender
{
    [self findFrom:NO];
}

- (void)controlTextDidChange:(NSNotification *)note
{
    NSRange sel = [_text selectedRange];

    [_text setSelectedRange:NSMakeRange(sel.location, 0)];
    [self findFrom:YES];
}

- (BOOL)control:(NSControl *)control textView:(NSTextView *)textView doCommandBySelector:(SEL)selector
{
    if (selector == @selector(cancelOperation:)) {
        _finding = NO;
        [self placeViews];
        [[self window] makeFirstResponder:_text];
        [self setNeedsDisplay:YES];
        return YES;
    }
    return NO;
}

- (BOOL)validateMenuItem:(NSMenuItem *)item
{
    if ([item action] == @selector(revertDocumentToSaved:))
        return [self edited] && _path != nil;
    return YES;
}

@end

@implementation EditorController

- (instancetype)init
{
    if ((self = [super init]) != nil)
        _windows = [[NSMutableArray alloc] init];
    return self;
}

- (void)dealloc
{
    [_windows release];
    [super dealloc];
}

- (void)buildMenu
{
    NSMenu *windowMenu;
    NSMenu *main = PDStandardMenus(NSLocalizedString(@"Text Editor", @""), &windowMenu);
    NSMenu *file = PDAddMenu(main, NSLocalizedString(@"File", @""));
    NSMenu *edit = PDAddMenu(main, NSLocalizedString(@"Edit", @""));
    NSMenu *format = PDAddMenu(main, NSLocalizedString(@"Format", @""));
    NSUInteger shift = NSCommandKeyMask | NSShiftKeyMask;

    [PDAddItem(file, NSLocalizedString(@"New", @""), @selector(newDocument:), @"n", 0) setTarget:self];
    [PDAddItem(file, NSLocalizedString(@"Open…", @""), @selector(openDocument:), @"o", 0) setTarget:self];
    [file addItem:[NSMenuItem separatorItem]];
    PDAddItem(file, NSLocalizedString(@"Close", @""), @selector(performClose:), @"w", 0);
    PDAddItem(file, NSLocalizedString(@"Save", @""), @selector(saveDocument:), @"s", 0);
    PDAddItem(file, NSLocalizedString(@"Save As…", @""), @selector(saveDocumentAs:), @"s", shift);
    PDAddItem(file, NSLocalizedString(@"Revert to Saved", @""), @selector(revertDocumentToSaved:), @"", 0);

    PDAddItem(edit, NSLocalizedString(@"Undo", @""), @selector(undo:), @"z", 0);
    PDAddItem(edit, NSLocalizedString(@"Redo", @""), @selector(redo:), @"z", shift);
    [edit addItem:[NSMenuItem separatorItem]];
    PDAddItem(edit, NSLocalizedString(@"Cut", @""), @selector(cut:), @"x", 0);
    PDAddItem(edit, NSLocalizedString(@"Copy", @""), @selector(copy:), @"c", 0);
    PDAddItem(edit, NSLocalizedString(@"Paste", @""), @selector(paste:), @"v", 0);
    PDAddItem(edit, NSLocalizedString(@"Select All", @""), @selector(selectAll:), @"a", 0);
    [edit addItem:[NSMenuItem separatorItem]];
    PDAddItem(edit, NSLocalizedString(@"Find…", @""), @selector(showFind:), @"f", 0);
    PDAddItem(edit, NSLocalizedString(@"Find Next", @""), @selector(findNext:), @"g", 0);
    PDAddItem(edit, NSLocalizedString(@"Find Previous", @""), @selector(findPrevious:), @"g", shift);

    [PDAddItem(format, NSLocalizedString(@"Bigger", @""), @selector(bigger:), @"+", 0) setTarget:self];
    [PDAddItem(format, NSLocalizedString(@"Smaller", @""), @selector(smaller:), @"-", 0) setTarget:self];
    [format addItem:[NSMenuItem separatorItem]];
    [PDAddItem(format, NSLocalizedString(@"Wrap Lines", @""), @selector(toggleWrap:), @"", 0) setTarget:self];
    [PDAddItem(format, NSLocalizedString(@"Proportional Font", @""), @selector(toggleProportional:), @"", 0) setTarget:self];
    [PDAddItem(main, NSLocalizedString(@"Window", @""), NULL, @"", 0) setSubmenu:windowMenu];
    [NSApp setMainMenu:main];
}

- (void)openPath:(NSString *)path
{
    NSWindow *w = PDMakeWindow(NSMakeSize(640, 470), NSMakeSize(360, 220), 24.0 * [_windows count], self);
    EditorView *v = [[[EditorView alloc] initWithFrame:[[w contentView] bounds] path:path owner:self] autorelease];

    if (path == nil)
        [w setTitle:_untitled++ == 0 ? NSLocalizedString(@"Untitled", @"") : [NSString stringWithFormat:NSLocalizedString(@"Untitled %d", @""), _untitled]];
    else
        [w setTitle:[path lastPathComponent]];
    [w setContentView:v];
    [_windows addObject:w];
    [w makeKeyAndOrderFront:nil];
    [w makeFirstResponder:v->_text];
}

- (void)restyle
{
    for (NSWindow *w in _windows)
        [(EditorView *)[w contentView] applyStyle];
}

- (IBAction)newDocument:(id)sender
{
    [self openPath:nil];
}

// there is no open panel yet, Files is where documents are picked
- (IBAction)openDocument:(id)sender
{
    [[NSWorkspace sharedWorkspace] launchApplication:@"/System/Library/CoreServices/Finder.app"];
}

- (IBAction)bigger:(id)sender
{
    fontSize = MIN(28, fontSize + 1);
    [self restyle];
}

- (IBAction)smaller:(id)sender
{
    fontSize = MAX(9, fontSize - 1);
    [self restyle];
}

- (IBAction)toggleWrap:(id)sender
{
    wrapLines = !wrapLines;
    [self restyle];
}

- (IBAction)toggleProportional:(id)sender
{
    proportional = !proportional;
    [self restyle];
}

- (BOOL)validateMenuItem:(NSMenuItem *)item
{
    if ([item action] == @selector(toggleWrap:))
        [item setState:wrapLines ? NSOnState : NSOffState];
    if ([item action] == @selector(toggleProportional:))
        [item setState:proportional ? NSOnState : NSOffState];
    return YES;
}

- (BOOL)windowShouldClose:(NSWindow *)w
{
    EditorView *v = (EditorView *)[w contentView];

    if (![v edited])
        return YES;
    [PDSheet showInView:v icon:@"texteditor" title:[NSString stringWithFormat:NSLocalizedString(@"Save changes to “%@”?", @""), [v name]]
                message:NSLocalizedString(@"Your changes will be lost if you don’t save them.", @"")
                  field:nil mono:nil buttons:@[ NSLocalizedString(@"Don’t Save", @""), NSLocalizedString(@"Cancel", @""), NSLocalizedString(@"Save", @"") ]
                 styles:@[ @"", @"", @"go" ]
                   done:^(NSInteger i, NSString *field) {
                       if (i == 0) {
                           [v->_text setString:v->_saved];
                           [w close];
                       } else if (i == 2 && v->_path != nil && [v writeTo:v->_path]) {
                           [w close];
                       } else if (i == 2) {
                           [v saveDocumentAs:nil];
                       }
                   }];
    return NO;
}

- (void)windowWillClose:(NSNotification *)note
{
    [[[note object] retain] autorelease];
    [_windows removeObject:[note object]];
    if ([_windows count] == 0)
        [NSApp terminate:self];
}

- (void)applicationDidFinishLaunching:(NSNotification *)note
{
    NSArray *args = [[NSProcessInfo processInfo] arguments];
    NSUInteger opened = 0;

    [self buildMenu];
    // Files and LaunchServices pass the documents to open as arguments
    for (NSUInteger i = 1; i < [args count]; i++) {
        if ([args[i] hasPrefix:@"-"])
            continue;
        [self openPath:args[i]];
        opened++;
    }
    if (opened == 0)
        [self openPath:nil];
}

@end

int main(int argc, const char *argv[])
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    NSApplication *app = [NSApplication sharedApplication];
    EditorController *controller = [[EditorController alloc] init];

    [app setDelegate:controller];
    [app run];
    [controller release];
    [pool release];
    return 0;
}
