/*
 * TerminalView.h - a cell grid over libvterm, fed by a pseudo-terminal.
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */

#import <AppKit/AppKit.h>
#include <sys/types.h>
#include <vterm.h>

@class TerminalView;

// the window that holds a session hears when its shell ends and when its title changes
@protocol TerminalViewDelegate
- (void)terminalViewDidExit:(TerminalView *)view;
- (void)terminalViewTitleChanged:(TerminalView *)view;
- (void)terminalView:(TerminalView *)view wantsSize:(NSSize)size;
@end

@interface TerminalView : NSView {
    VTerm *_vt;
    VTermScreen *_screen;
    int _rows, _cols;

    int _master;
    pid_t _child;
    dispatch_source_t _readSource;

    // regular, bold, italic and bold italic
    NSFont *_fonts[4];
    CGFloat _fontSize;
    CGFloat _cellWidth, _cellHeight, _ascent;

    VTermPos _cursor;
    BOOL _cursorVisible;
    int _cursorShape;

    // scrollback, oldest line first, a set wrap flag means the next line continues this one
    VTermScreenCell **_sbLines;
    int *_sbCols;
    BOOL *_sbWraps;
    int _sbCount, _sbCapacity;
    int _scrollOffset;

    // selection in virtual lines: scrollback lines first, then the screen
    BOOL _selecting, _hasSelection;
    VTermPos _selAnchor, _selEnd;

    // what the program asked for: mouse reports, the alternate screen
    int _mouseMode;
    BOOL _altScreen;
    BOOL _reportingClick;

    BOOL _bellShowing;
    NSMutableData *_title;
    NSString *_titleString;

    id<TerminalViewDelegate> _delegate;
    // the session's utmpx record, ttys000 and the like
    char _line[32];
    BOOL _loggedIn;
}

- (instancetype)initWithRows:(int)rows cols:(int)cols;
- (NSSize)sizeForRows:(int)rows cols:(int)cols;
- (BOOL)startShell;
- (void)setDelegate:(id<TerminalViewDelegate>)delegate;
- (NSString *)title;
// ends the session as closing its tab does, the shell gets SIGHUP
- (void)hangUp;

- (void)copy:(id)sender;
- (void)paste:(id)sender;
- (void)selectAll:(id)sender;
- (void)clearScrollback:(id)sender;
- (void)biggerFont:(id)sender;
- (void)smallerFont:(id)sender;
- (void)defaultFontSize:(id)sender;

@end
