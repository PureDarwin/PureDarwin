/*
 * TerminalView.m - a cell grid over libvterm, fed by a pseudo-terminal.
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */

#import "TerminalView.h"
#include <utmpx.h>
#include <sys/time.h>

#include <errno.h>
#include <pwd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include <util.h>

#define SCROLLBACK_LINES 5000
#define DEFAULT_FONT_SIZE 13.0

extern char **environ;

// what a run of cells shares, cells with equal styles draw in one go
typedef struct {
    uint8_t fg[3], bg[3];
    uint8_t bold, italic, underline, strike;
} CellStyle;

static VTermColor defaultFg, defaultBg;

static NSColor *colorFromRGB(const uint8_t rgb[3])
{
    return [NSColor colorWithCalibratedRed:rgb[0] / 255.0 green:rgb[1] / 255.0
                                      blue:rgb[2] / 255.0 alpha:1.0];
}

@interface TerminalView (Callbacks)
- (void)damageRect:(VTermRect)rect;
- (void)moveCursorTo:(VTermPos)pos from:(VTermPos)old visible:(BOOL)visible;
- (void)setTermProp:(VTermProp)prop value:(VTermValue *)value;
- (void)pushLine:(const VTermScreenCell *)cells cols:(int)cols;
- (BOOL)popLine:(VTermScreenCell *)cells cols:(int)cols;
- (void)clearScrollback;
- (void)ringBell;
- (void)writeToChild:(const char *)bytes length:(size_t)length;
@end

static int onDamage(VTermRect rect, void *user)
{
    [(TerminalView *)user damageRect:rect];
    return 1;
}

static int onMoveCursor(VTermPos pos, VTermPos oldpos, int visible, void *user)
{
    [(TerminalView *)user moveCursorTo:pos from:oldpos visible:visible];
    return 1;
}

static int onSetTermProp(VTermProp prop, VTermValue *val, void *user)
{
    [(TerminalView *)user setTermProp:prop value:val];
    return 1;
}

static int onBell(void *user)
{
    [(TerminalView *)user ringBell];
    return 1;
}

static int onPushLine(int cols, const VTermScreenCell *cells, void *user)
{
    [(TerminalView *)user pushLine:cells cols:cols];
    return 1;
}

static int onPopLine(int cols, VTermScreenCell *cells, void *user)
{
    return [(TerminalView *)user popLine:cells cols:cols];
}

static int onClearScrollback(void *user)
{
    [(TerminalView *)user clearScrollback];
    return 1;
}

static void onOutput(const char *s, size_t len, void *user)
{
    [(TerminalView *)user writeToChild:s length:len];
}

static const VTermScreenCallbacks screenCallbacks = {
    .damage = onDamage,
    .movecursor = onMoveCursor,
    .settermprop = onSetTermProp,
    .bell = onBell,
    .sb_pushline = onPushLine,
    .sb_popline = onPopLine,
    .sb_clear = onClearScrollback,
};

static VTermModifier modifiersForEvent(NSEvent *event)
{
    NSUInteger flags = [event modifierFlags];
    VTermModifier mod = VTERM_MOD_NONE;

    if (flags & NSShiftKeyMask)
        mod |= VTERM_MOD_SHIFT;
    if (flags & NSControlKeyMask)
        mod |= VTERM_MOD_CTRL;
    if (flags & NSAlternateKeyMask)
        mod |= VTERM_MOD_ALT;
    return mod;
}

@implementation TerminalView

// regular, bold, italic and bold italic faces of the fixed-pitch font, and the cell they fill
- (void)useFontSize:(CGFloat)size
{
    NSFontManager *manager = [NSFontManager sharedFontManager];
    NSFont *font = [NSFont userFixedPitchFontOfSize:size];
    NSFont *faces[4];

    faces[0] = font;
    faces[1] = [manager convertFont:font toHaveTrait:NSBoldFontMask];
    faces[2] = [manager convertFont:font toHaveTrait:NSItalicFontMask];
    faces[3] = [manager convertFont:faces[1] toHaveTrait:NSItalicFontMask];
    for (int i = 0; i < 4; i++) {
        [faces[i] retain];
        [_fonts[i] release];
        _fonts[i] = faces[i] != nil ? faces[i] : [font retain];
    }
    _fontSize = size;

    // ten glyphs average out any rounding in a single advance
    NSDictionary *attrs = [NSDictionary dictionaryWithObject:font forKey:NSFontAttributeName];
    _cellWidth = [@"MMMMMMMMMM" sizeWithAttributes:attrs].width / 10.0;
    _ascent = ceil([font ascender]);
    _cellHeight = _ascent + ceil(-[font descender]) + 1.0;
}

- (instancetype)initWithRows:(int)rows cols:(int)cols
{
    [self useFontSize:DEFAULT_FONT_SIZE];

    self = [super initWithFrame:NSMakeRect(0, 0, ceil(cols * _cellWidth), rows * _cellHeight)];
    if (self == nil)
        return nil;

    _rows = rows;
    _cols = cols;
    _master = -1;
    _cursorVisible = YES;
    _cursorShape = VTERM_PROP_CURSORSHAPE_BLOCK;
    _title = [[NSMutableData alloc] init];

    _vt = vterm_new(rows, cols);
    vterm_set_utf8(_vt, 1);
    vterm_output_set_callback(_vt, onOutput, self);

    vterm_color_rgb(&defaultFg, 0xd8, 0xd8, 0xd8);
    vterm_color_rgb(&defaultBg, 0x1d, 0x1f, 0x21);

    // the 16 ANSI colours, with blues light enough to read on the dark background
    static const uint32_t ansi[16] = {
        0x000000, 0xcd3131, 0x0dbc79, 0xe5e510, 0x2472c8, 0xbc3fbc, 0x11a8cd, 0xe5e5e5,
        0x666666, 0xf14c4c, 0x23d18b, 0xf5f543, 0x3b8eea, 0xd670d6, 0x29b8db, 0xffffff,
    };
    VTermState *state = vterm_obtain_state(_vt);
    for (int i = 0; i < 16; i++) {
        VTermColor color;
        vterm_color_rgb(&color, ansi[i] >> 16, (ansi[i] >> 8) & 0xff, ansi[i] & 0xff);
        vterm_state_set_palette_color(state, i, &color);
    }
    // bold text in the first eight colours uses their bright forms, as Terminal does by default
    vterm_state_set_bold_highbright(state, 1);

    _screen = vterm_obtain_screen(_vt);
    vterm_screen_set_callbacks(_screen, &screenCallbacks, self);
    vterm_screen_set_default_colors(_screen, &defaultFg, &defaultBg);
    vterm_screen_set_damage_merge(_screen, VTERM_DAMAGE_ROW);
    vterm_screen_enable_altscreen(_screen, 1);
    vterm_screen_enable_reflow(_screen, true);
    vterm_screen_reset(_screen, 1);

    defaultFg.type |= VTERM_COLOR_DEFAULT_FG;
    defaultBg.type |= VTERM_COLOR_DEFAULT_BG;

    _sbCapacity = SCROLLBACK_LINES;
    _sbLines = calloc(_sbCapacity, sizeof(*_sbLines));
    _sbCols = calloc(_sbCapacity, sizeof(*_sbCols));
    _sbWraps = calloc(_sbCapacity, sizeof(*_sbWraps));

    [self setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    return self;
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    if (_readSource != NULL) {
        dispatch_source_cancel(_readSource);
        dispatch_release(_readSource);
    }
    if (_master >= 0)
        close(_master);

    [self clearScrollback];
    free(_sbLines);
    free(_sbCols);
    free(_sbWraps);
    vterm_free(_vt);
    for (int i = 0; i < 4; i++)
        [_fonts[i] release];
    [_title release];
    [_titleString release];
    [super dealloc];
}

- (NSSize)sizeForRows:(int)rows cols:(int)cols
{
    return NSMakeSize(ceil(cols * _cellWidth), rows * _cellHeight);
}

- (BOOL)isFlipped
{
    return YES;
}

- (BOOL)isOpaque
{
    return YES;
}

- (BOOL)acceptsFirstResponder
{
    return YES;
}

- (void)viewDidMoveToWindow
{
    NSNotificationCenter *center = [NSNotificationCenter defaultCenter];

    [center removeObserver:self];
    if ([self window] == nil)
        return;

    // the cursor is drawn hollow while the window is not key
    [center addObserver:self selector:@selector(keyStateChanged:)
                   name:NSWindowDidBecomeKeyNotification object:[self window]];
    [center addObserver:self selector:@selector(keyStateChanged:)
                   name:NSWindowDidResignKeyNotification object:[self window]];
}

- (void)keyStateChanged:(NSNotification *)note
{
    [self setNeedsDisplay:YES];
}

// an I-beam over text, an arrow while the program takes the mouse
- (void)resetCursorRects
{
    NSCursor *cursor = _mouseMode != VTERM_PROP_MOUSE_NONE ? [NSCursor arrowCursor] : [NSCursor IBeamCursor];

    [self addCursorRect:[self visibleRect] cursor:cursor];
}

// the child process

- (BOOL)startShell
{
    struct winsize ws = { (unsigned short)_rows, (unsigned short)_cols, 0, 0 };
    int master, slave;

    if (openpty(&master, &slave, NULL, NULL, &ws) < 0) {
        NSLog(@"Terminal: openpty: %s", strerror(errno));
        return NO;
    }
    const char *tty = ttyname(slave);
    strlcpy(_line, tty != NULL && strncmp(tty, "/dev/", 5) == 0 ? tty + 5 : "ttys000", sizeof(_line));
    // read before this session is written, or the last login would be this one
    [self showLastLogin];

    struct passwd *pw = getpwuid(getuid());
    const char *shell = getenv("SHELL");
    if (shell == NULL || *shell == '\0')
        shell = (pw != NULL && pw->pw_shell != NULL && *pw->pw_shell) ? pw->pw_shell : "/bin/zsh";
    const char *home = getenv("HOME");
    if (home == NULL && pw != NULL)
        home = pw->pw_dir;

    // a leading dash makes it a login shell
    const char *base = strrchr(shell, '/');
    char arg0[256];
    snprintf(arg0, sizeof(arg0), "-%s", base != NULL ? base + 1 : shell);

    // everything the child needs is built here: after fork only exec-safe calls remain
    static const char *overrides[] = {
        "TERM=xterm-256color", "COLORTERM=truecolor", "TERM_PROGRAM=Terminal",
    };
    int count = 0;
    while (environ[count] != NULL)
        count++;

    const int extra = sizeof(overrides) / sizeof(overrides[0]);
    char **envp = calloc(count + extra + 3, sizeof(char *));
    int n = 0;
    for (int i = 0; i < count; i++) {
        if (strncmp(environ[i], "TERM=", 5) == 0 || strncmp(environ[i], "COLORTERM=", 10) == 0 ||
            strncmp(environ[i], "TERM_PROGRAM=", 13) == 0 || strncmp(environ[i], "SHELL=", 6) == 0)
            continue;
        envp[n++] = environ[i];
    }
    for (int i = 0; i < extra; i++)
        envp[n++] = (char *)overrides[i];

    char shellVar[1024];
    snprintf(shellVar, sizeof(shellVar), "SHELL=%s", shell);
    envp[n++] = shellVar;
    if (getenv("HOME") == NULL && home != NULL) {
        static char homeVar[1024];
        snprintf(homeVar, sizeof(homeVar), "HOME=%s", home);
        envp[n++] = homeVar;
    }

    pid_t pid = fork();
    if (pid < 0) {
        NSLog(@"Terminal: fork: %s", strerror(errno));
        free(envp);
        close(master);
        close(slave);
        return NO;
    }

    if (pid == 0) {
        close(master);
        if (login_tty(slave) < 0)
            _exit(126);

        // the shell starts with every signal at its default, whatever this process inherited
        for (int sig = 1; sig < NSIG; sig++) {
            if (sig != SIGKILL && sig != SIGSTOP)
                signal(sig, SIG_DFL);
        }
        sigset_t none;
        sigemptyset(&none);
        sigprocmask(SIG_SETMASK, &none, NULL);

        if (home != NULL)
            chdir(home);
        char *argv[] = { arg0, NULL };
        execve(shell, argv, envp);
        _exit(127);
    }

    free(envp);
    close(slave);
    _master = master;
    _child = pid;
    [self recordLoginForPid:pid];

    // the main queue is drained by the AppKit event loop, so output lands on the main thread
    _readSource = dispatch_source_create(DISPATCH_SOURCE_TYPE_READ, master, 0,
                                         dispatch_get_main_queue());
    dispatch_source_set_event_handler(_readSource, ^{
        [self readFromChild];
    });
    dispatch_resume(_readSource);
    return YES;
}

- (void)readFromChild
{
    char buf[65536];
    ssize_t n = read(_master, buf, sizeof(buf));

    if (n < 0 && (errno == EINTR || errno == EAGAIN))
        return;
    if (n <= 0) {
        [self childExited];
        return;
    }

    vterm_input_write(_vt, buf, n);
    vterm_screen_flush_damage(_screen);
}

- (void)childExited
{
    int status;

    dispatch_source_cancel(_readSource);
    waitpid(_child, &status, 0);
    [self recordLogout];
    if (_delegate != nil)
        [_delegate terminalViewDidExit:self];
    else
        [NSApp terminate:self];
}

- (void)hangUp
{
    if (_child > 0)
        kill(_child, SIGHUP);
    if (_readSource != NULL)
        dispatch_source_cancel(_readSource);
    [self recordLogout];
}

- (void)setDelegate:(id<TerminalViewDelegate>)delegate
{
    _delegate = delegate;
}

- (NSString *)title
{
    return _titleString;
}

// a utmpx record like login(1) writes, which also files the session as this user's last login
- (void)recordLoginForPid:(pid_t)pid
{
    struct utmpx ux;
    const char *user = getpwuid(getuid()) != NULL ? getpwuid(getuid())->pw_name : "root";

    memset(&ux, 0, sizeof(ux));
    strlcpy(ux.ut_user, user, sizeof(ux.ut_user));
    strlcpy(ux.ut_line, _line, sizeof(ux.ut_line));
    // the id is the tty's last four characters, s000 for ttys000, as Darwin's login uses
    strncpy(ux.ut_id, strlen(_line) > 4 ? _line + strlen(_line) - 4 : _line, sizeof(ux.ut_id));
    ux.ut_pid = pid;
    ux.ut_type = USER_PROCESS;
    gettimeofday(&ux.ut_tv, NULL);
    setutxent();
    pututxline(&ux);
    endutxent();
    _loggedIn = YES;
}

- (void)recordLogout
{
    struct utmpx ux;

    if (!_loggedIn)
        return;
    _loggedIn = NO;
    memset(&ux, 0, sizeof(ux));
    // the user stays on the dead record, so the session still counts as a past login
    if (getpwuid(getuid()) != NULL)
        strlcpy(ux.ut_user, getpwuid(getuid())->pw_name, sizeof(ux.ut_user));
    strlcpy(ux.ut_line, _line, sizeof(ux.ut_line));
    strncpy(ux.ut_id, strlen(_line) > 4 ? _line + strlen(_line) - 4 : _line, sizeof(ux.ut_id));
    ux.ut_pid = _child;
    ux.ut_type = DEAD_PROCESS;
    gettimeofday(&ux.ut_tv, NULL);
    setutxent();
    pututxline(&ux);
    endutxent();
}

// macOS's login prints the previous login before the shell starts, in ctime's style without the year
- (void)showLastLogin
{
    struct passwd *pw = getpwuid(getuid());
    struct lastlogx last;
    char when[64], text[160];
    time_t t;
    int n;

    if (pw == NULL)
        return;
    memset(&last, 0, sizeof(last));
    // the ASL store keeps lastlog on macOS, utmpx still has this boot's sessions when it doesn't
    if (getlastlogxbyname(pw->pw_name, &last) == NULL || last.ll_tv.tv_sec == 0) {
        struct utmpx *u;

        memset(&last, 0, sizeof(last));
        setutxent();
        while ((u = getutxent()) != NULL) {
            if ((u->ut_type == USER_PROCESS || u->ut_type == DEAD_PROCESS) &&
                strncmp(u->ut_user, pw->pw_name, sizeof(u->ut_user)) == 0 && u->ut_tv.tv_sec > last.ll_tv.tv_sec) {
                last.ll_tv = u->ut_tv;
                strlcpy(last.ll_line, u->ut_line, sizeof(last.ll_line));
            }
        }
        endutxent();
        if (last.ll_tv.tv_sec == 0)
            return;
    }
    t = last.ll_tv.tv_sec;
    strftime(when, sizeof(when), "%a %b %e %H:%M:%S", localtime(&t));
    n = snprintf(text, sizeof(text), "\033[90mLast login: %s on %s\033[0m\r\n", when, last.ll_line[0] ? last.ll_line : "console");
    vterm_input_write(_vt, text, MIN(n, (int)sizeof(text) - 1));
    vterm_screen_flush_damage(_screen);
}

- (void)writeToChild:(const char *)bytes length:(size_t)length
{
    while (length > 0 && _master >= 0) {
        ssize_t n = write(_master, bytes, length);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0)
            return;

        bytes += n;
        length -= n;
    }
}

// libvterm callbacks

- (NSRect)rectForRow:(int)row col:(int)col rows:(int)rows cols:(int)cols
{
    CGFloat x0 = floor(col * _cellWidth), x1 = floor((col + cols) * _cellWidth);
    return NSMakeRect(x0, row * _cellHeight, x1 - x0, rows * _cellHeight);
}

- (void)damageRect:(VTermRect)rect
{
    if (_scrollOffset != 0) {
        [self setNeedsDisplay:YES];
        return;
    }
    [self setNeedsDisplayInRect:[self rectForRow:rect.start_row col:rect.start_col
                                            rows:rect.end_row - rect.start_row
                                            cols:rect.end_col - rect.start_col + 1]];
}

- (void)moveCursorTo:(VTermPos)pos from:(VTermPos)old visible:(BOOL)visible
{
    [self setNeedsDisplayInRect:[self rectForRow:old.row + _scrollOffset col:old.col rows:1 cols:2]];
    [self setNeedsDisplayInRect:[self rectForRow:pos.row + _scrollOffset col:pos.col rows:1 cols:2]];
    _cursor = pos;
}

- (void)setTermProp:(VTermProp)prop value:(VTermValue *)value
{
    switch (prop) {
    case VTERM_PROP_CURSORVISIBLE:
        _cursorVisible = value->boolean;
        [self setNeedsDisplayInRect:[self rectForRow:_cursor.row col:_cursor.col rows:1 cols:2]];
        break;

    case VTERM_PROP_CURSORSHAPE:
        _cursorShape = value->number;
        [self setNeedsDisplayInRect:[self rectForRow:_cursor.row col:_cursor.col rows:1 cols:2]];
        break;

    case VTERM_PROP_ALTSCREEN:
        _altScreen = value->boolean;
        break;

    case VTERM_PROP_MOUSE:
        _mouseMode = value->number;
        [[self window] invalidateCursorRectsForView:self];
        break;

    case VTERM_PROP_TITLE:
        if (value->string.initial)
            [_title setLength:0];
        [_title appendBytes:value->string.str length:value->string.len];
        if (value->string.final) {
            NSString *title = [[NSString alloc] initWithData:_title encoding:NSUTF8StringEncoding];
            if (title != nil) {
                [_titleString release];
                _titleString = [title retain];
                if (_delegate != nil)
                    [_delegate terminalViewTitleChanged:self];
                else
                    [[self window] setTitle:title];
            }
            [title release];
        }
        break;

    default:
        break;
    }
}

// a visual bell: the view flashes briefly, and bells during the flash fold into it
- (void)ringBell
{
    if (_bellShowing)
        return;

    _bellShowing = YES;
    [self setNeedsDisplay:YES];
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 120 * NSEC_PER_MSEC), dispatch_get_main_queue(), ^{
        [self endBell];
    });
}

- (void)endBell
{
    _bellShowing = NO;
    [self setNeedsDisplay:YES];
}

- (void)pushLine:(const VTermScreenCell *)cells cols:(int)cols
{
    // a full buffer drops its oldest line, which shifts every virtual line up by one
    if (_sbCount == _sbCapacity) {
        free(_sbLines[0]);
        memmove(_sbLines, _sbLines + 1, (_sbCapacity - 1) * sizeof(*_sbLines));
        memmove(_sbCols, _sbCols + 1, (_sbCapacity - 1) * sizeof(*_sbCols));
        memmove(_sbWraps, _sbWraps + 1, (_sbCapacity - 1) * sizeof(*_sbWraps));
        _sbCount--;
        _selAnchor.row--;
        _selEnd.row--;
    }

    _sbLines[_sbCount] = malloc(cols * sizeof(VTermScreenCell));
    memcpy(_sbLines[_sbCount], cells, cols * sizeof(VTermScreenCell));
    _sbCols[_sbCount] = cols;

    // libvterm has already scrolled its line flags: the new top row says whether it continues this line
    const VTermLineInfo *next = vterm_state_get_lineinfo(vterm_obtain_state(_vt), 0);
    _sbWraps[_sbCount] = next != NULL && next->continuation;
    _sbCount++;

    // keep a scrolled-back view on the same lines while output arrives
    if (_scrollOffset > 0) {
        _scrollOffset++;
        if (_scrollOffset > _sbCount)
            _scrollOffset = _sbCount;
    }
}

- (BOOL)popLine:(VTermScreenCell *)cells cols:(int)cols
{
    if (_sbCount == 0)
        return NO;

    _sbCount--;
    VTermScreenCell *line = _sbLines[_sbCount];
    int have = _sbCols[_sbCount];

    for (int c = 0; c < cols; c++) {
        if (c < have) {
            cells[c] = line[c];
            continue;
        }

        memset(&cells[c], 0, sizeof(cells[c]));
        cells[c].width = 1;
        cells[c].fg = defaultFg;
        cells[c].bg = defaultBg;
    }
    free(line);

    if (_scrollOffset > _sbCount)
        _scrollOffset = _sbCount;
    return YES;
}

- (void)clearScrollback
{
    for (int i = 0; i < _sbCount; i++)
        free(_sbLines[i]);

    _sbCount = 0;
    _scrollOffset = 0;
    _hasSelection = NO;
    [self setNeedsDisplay:YES];
}

// cell access over scrollback plus screen

- (int)topLine
{
    return _sbCount - _scrollOffset;
}

- (int)lineCount
{
    return _sbCount + _rows;
}

- (void)getCell:(VTermScreenCell *)cell line:(int)line col:(int)col
{
    memset(cell, 0, sizeof(*cell));
    cell->width = 1;
    cell->fg = defaultFg;
    cell->bg = defaultBg;

    if (line < _sbCount) {
        if (col < _sbCols[line])
            *cell = _sbLines[line][col];
    } else {
        vterm_screen_get_cell(_screen, (VTermPos){ .row = line - _sbCount, .col = col }, cell);
    }

    // reflow can leave never-written cells zeroed, which read as black on black unless drawn as blank
    static const VTermColor zero;
    if (cell->chars[0] == 0 && memcmp(&cell->fg, &zero, sizeof(zero)) == 0 &&
        memcmp(&cell->bg, &zero, sizeof(zero)) == 0) {
        cell->fg = defaultFg;
        cell->bg = defaultBg;
    }
}

// whether the line after this one is its soft-wrapped continuation
- (BOOL)lineWraps:(int)line
{
    if (line < _sbCount)
        return _sbWraps[line];

    int next = line - _sbCount + 1;
    if (next >= _rows)
        return NO;

    const VTermLineInfo *info = vterm_state_get_lineinfo(vterm_obtain_state(_vt), next);
    return info != NULL && info->continuation;
}

- (BOOL)isSelectedLine:(int)line col:(int)col
{
    if (!_hasSelection)
        return NO;

    long a = (long)_selAnchor.row * _cols + _selAnchor.col;
    long b = (long)_selEnd.row * _cols + _selEnd.col;
    long p = (long)line * _cols + col;
    return p >= MIN(a, b) && p < MAX(a, b);
}

- (CellStyle)styleForCell:(VTermScreenCell *)cell line:(int)line col:(int)col
{
    CellStyle style;
    VTermColor fg = cell->fg, bg = cell->bg;

    memset(&style, 0, sizeof(style));
    vterm_screen_convert_color_to_rgb(_screen, &fg);
    vterm_screen_convert_color_to_rgb(_screen, &bg);

    BOOL reverse = cell->attrs.reverse;
    if ([self isSelectedLine:line col:col])
        reverse = !reverse;
    BOOL atCursor = _scrollOffset == 0 && _cursorVisible && line - _sbCount == _cursor.row &&
        col == _cursor.col && _cursorShape == VTERM_PROP_CURSORSHAPE_BLOCK && [[self window] isKeyWindow];
    if (atCursor)
        reverse = !reverse;

    VTermColor *f = reverse ? &bg : &fg, *b = reverse ? &fg : &bg;
    style.fg[0] = f->rgb.red, style.fg[1] = f->rgb.green, style.fg[2] = f->rgb.blue;
    style.bg[0] = b->rgb.red, style.bg[1] = b->rgb.green, style.bg[2] = b->rgb.blue;
    if (cell->attrs.conceal)
        memcpy(style.fg, style.bg, 3);

    style.bold = cell->attrs.bold;
    style.italic = cell->attrs.italic;
    style.underline = cell->attrs.underline != 0;
    style.strike = cell->attrs.strike;
    return style;
}

// drawing

static void appendCodepoint(unichar *buf, int *len, uint32_t c)
{
    if (c == 0 || c == (uint32_t)-1)
        c = ' ';
    if (c >= 0x10000) {
        c -= 0x10000;
        buf[(*len)++] = 0xd800 + (c >> 10);
        buf[(*len)++] = 0xdc00 + (c & 0x3ff);
        return;
    }
    buf[(*len)++] = c;
}

- (void)drawRun:(unichar *)text length:(int)length style:(CellStyle *)style
            row:(int)row col:(int)col cols:(int)cols
{
    NSRect rect = [self rectForRow:row col:col rows:1 cols:cols];

    [colorFromRGB(style->bg) set];
    NSRectFill(rect);

    BOOL blank = YES;
    for (int i = 0; i < length && blank; i++)
        blank = text[i] == ' ';

    NSColor *fg = colorFromRGB(style->fg);
    if (!blank) {
        NSString *string = [[NSString alloc] initWithCharacters:text length:length];
        NSDictionary *attrs = [NSDictionary dictionaryWithObjectsAndKeys:
            _fonts[(style->bold ? 1 : 0) | (style->italic ? 2 : 0)], NSFontAttributeName,
            fg, NSForegroundColorAttributeName, nil];

        [string drawAtPoint:NSMakePoint(col * _cellWidth, row * _cellHeight) withAttributes:attrs];
        [string release];
    }

    [fg set];
    if (style->underline)
        NSRectFill(NSMakeRect(rect.origin.x, rect.origin.y + _ascent + 1, rect.size.width, 1));
    if (style->strike)
        NSRectFill(NSMakeRect(rect.origin.x, rect.origin.y + floor(_cellHeight / 2), rect.size.width, 1));
}

- (void)drawRow:(int)row
{
    int line = [self topLine] + row;
    VTermScreenCell cells[_cols];
    unichar text[_cols * 2 * VTERM_MAX_CHARS_PER_CELL];

    for (int c = 0; c < _cols; c++)
        [self getCell:&cells[c] line:line col:c];

    for (int c = 0; c < _cols;) {
        int start = c, length = 0;
        CellStyle style = [self styleForCell:&cells[c] line:line col:c];

        // a wide glyph is a run of its own so the grid stays aligned after it
        if (cells[c].width == 2 && c + 1 < _cols) {
            for (int i = 0; i < VTERM_MAX_CHARS_PER_CELL && cells[c].chars[i]; i++)
                appendCodepoint(text, &length, cells[c].chars[i]);
            [self drawRun:text length:length style:&style row:row col:c cols:2];
            c += 2;
            continue;
        }

        while (c < _cols) {
            CellStyle next = [self styleForCell:&cells[c] line:line col:c];

            if (c > start && (cells[c].width == 2 || memcmp(&next, &style, sizeof(style)) != 0))
                break;

            if (cells[c].chars[0] == 0)
                appendCodepoint(text, &length, ' ');
            for (int i = 0; i < VTERM_MAX_CHARS_PER_CELL && cells[c].chars[i]; i++)
                appendCodepoint(text, &length, cells[c].chars[i]);
            c++;
        }
        [self drawRun:text length:length style:&style row:row col:start cols:c - start];
    }
}

- (void)drawCursor
{
    if (!_cursorVisible || _scrollOffset != 0)
        return;

    VTermScreenCell cell;
    [self getCell:&cell line:_sbCount + _cursor.row col:_cursor.col];
    CellStyle style = [self styleForCell:&cell line:_sbCount + _cursor.row col:_cursor.col];
    NSRect rect = [self rectForRow:_cursor.row col:_cursor.col rows:1 cols:1];

    [colorFromRGB(style.fg) set];
    // a hollow block while another window has the keyboard, a key window's block is drawn reversed
    if (![[self window] isKeyWindow])
        NSFrameRect(rect);
    else if (_cursorShape == VTERM_PROP_CURSORSHAPE_UNDERLINE)
        NSRectFill(NSMakeRect(rect.origin.x, NSMaxY(rect) - 2, rect.size.width, 2));
    else if (_cursorShape == VTERM_PROP_CURSORSHAPE_BAR_LEFT)
        NSRectFill(NSMakeRect(rect.origin.x, rect.origin.y, 2, rect.size.height));
}

- (void)drawRect:(NSRect)dirty
{
    VTermColor bg = defaultBg;
    uint8_t rgb[3] = { bg.rgb.red, bg.rgb.green, bg.rgb.blue };

    // the margin past the last whole cell
    [colorFromRGB(rgb) set];
    NSRectFill(dirty);

    int first = MAX(0, (int)floor(NSMinY(dirty) / _cellHeight));
    int last = MIN(_rows - 1, (int)ceil(NSMaxY(dirty) / _cellHeight));
    for (int row = first; row <= last; row++)
        [self drawRow:row];

    [self drawCursor];

    if (_bellShowing) {
        [[NSColor colorWithCalibratedWhite:1.0 alpha:0.2] set];
        NSRectFillUsingOperation(dirty, NSCompositeSourceOver);
    }
}

// geometry

- (void)setFrameSize:(NSSize)size
{
    [super setFrameSize:size];
    if (_vt == NULL)
        return;

    int cols = MAX(2, (int)floor(size.width / _cellWidth));
    int rows = MAX(1, (int)floor(size.height / _cellHeight));
    if (cols == _cols && rows == _rows)
        return;

    _cols = cols;
    _rows = rows;
    _hasSelection = NO;
    vterm_set_size(_vt, rows, cols);
    vterm_screen_flush_damage(_screen);

    // the kernel sends the foreground job SIGWINCH
    struct winsize ws = { (unsigned short)rows, (unsigned short)cols, 0, 0 };
    if (_master >= 0)
        ioctl(_master, TIOCSWINSZ, &ws);
    [self setNeedsDisplay:YES];
}

- (void)scrollBy:(int)lines
{
    int offset = MAX(0, MIN(_sbCount, _scrollOffset + lines));

    if (offset == _scrollOffset)
        return;
    _scrollOffset = offset;
    [self setNeedsDisplay:YES];
}

// a normal window keeps its rows and columns, a zoomed or full-screen one its size
- (void)setFontSize:(CGFloat)size
{
    NSWindow *window = [self window];

    size = MAX(7.0, MIN(72.0, size));
    if (size == _fontSize)
        return;

    int rows = _rows, cols = _cols;
    [self useFontSize:size];
    if (window != nil && !([window styleMask] & NSFullScreenWindowMask) && ![window isZoomed]) {
        if (_delegate != nil)
            [_delegate terminalView:self wantsSize:[self sizeForRows:rows cols:cols]];
        else
            [window setContentSize:[self sizeForRows:rows cols:cols]];
    }

    [self setFrameSize:[self frame].size];
    [self setNeedsDisplay:YES];
}

- (void)biggerFont:(id)sender
{
    [self setFontSize:_fontSize + 1.0];
}

- (void)smallerFont:(id)sender
{
    [self setFontSize:_fontSize - 1.0];
}

- (void)defaultFontSize:(id)sender
{
    [self setFontSize:DEFAULT_FONT_SIZE];
}

// keyboard

static VTermKey keyForFunctionKey(unichar c)
{
    switch (c) {
    case NSUpArrowFunctionKey: return VTERM_KEY_UP;
    case NSDownArrowFunctionKey: return VTERM_KEY_DOWN;
    case NSLeftArrowFunctionKey: return VTERM_KEY_LEFT;
    case NSRightArrowFunctionKey: return VTERM_KEY_RIGHT;
    case NSInsertFunctionKey: return VTERM_KEY_INS;
    case NSDeleteFunctionKey: return VTERM_KEY_DEL;
    case NSHomeFunctionKey: return VTERM_KEY_HOME;
    case NSEndFunctionKey: return VTERM_KEY_END;
    case NSPageUpFunctionKey: return VTERM_KEY_PAGEUP;
    case NSPageDownFunctionKey: return VTERM_KEY_PAGEDOWN;
    case '\r': case 0x03: return VTERM_KEY_ENTER;
    case '\t': case 0x19: return VTERM_KEY_TAB;
    case 0x7f: case '\b': return VTERM_KEY_BACKSPACE;
    case 0x1b: return VTERM_KEY_ESCAPE;
    }
    if (c >= NSF1FunctionKey && c <= NSF35FunctionKey)
        return VTERM_KEY_FUNCTION(c - NSF1FunctionKey + 1);
    return VTERM_KEY_NONE;
}

// command shortcuts that work with or without a menu bar
- (BOOL)performKeyEquivalent:(NSEvent *)event
{
    NSUInteger flags = [event modifierFlags];
    NSString *key = [event charactersIgnoringModifiers];

    if (!(flags & NSCommandKeyMask) || [key length] != 1 || [[self window] firstResponder] != self)
        return [super performKeyEquivalent:event];

    switch ([key characterAtIndex:0]) {
    case 'f': case 'F':
        if (!(flags & NSControlKeyMask))
            return NO;
        [[self window] toggleFullScreen:self];
        return YES;
    case '+': case '=':
        [self biggerFont:self];
        return YES;
    case '-': case '_':
        [self smallerFont:self];
        return YES;
    case '0':
        [self defaultFontSize:self];
        return YES;
    case 'k':
        [self clearScrollback:self];
        return YES;
    case 'a':
        [self selectAll:self];
        return YES;
    case 'c':
        [self copy:self];
        return YES;
    case 'v':
        [self paste:self];
        return YES;
    }
    return [super performKeyEquivalent:event];
}

- (void)keyDown:(NSEvent *)event
{
    NSString *chars = [event characters];
    NSString *plain = [event charactersIgnoringModifiers];
    NSUInteger flags = [event modifierFlags];

    // command keys are shortcuts, never input
    if (flags & NSCommandKeyMask)
        return;

    VTermModifier mod = modifiersForEvent(event);
    unichar key = [plain length] > 0 ? [plain characterAtIndex:0] : 0;

    // shift-page keys page through the scrollback and never reach the program
    if ((flags & NSShiftKeyMask) && (key == NSPageUpFunctionKey || key == NSPageDownFunctionKey)) {
        [self scrollBy:(key == NSPageUpFunctionKey ? 1 : -1) * MAX(1, _rows - 1)];
        return;
    }

    [self scrollBy:-_scrollOffset];

    VTermKey vkey = keyForFunctionKey(key);
    if (vkey != VTERM_KEY_NONE) {
        // the window server reports shift-tab as its own character
        if (key == 0x19)
            mod |= VTERM_MOD_SHIFT;
        vterm_keyboard_key(_vt, vkey, mod);
        return;
    }

    // control combinations go by the unmodified key, the way libvterm encodes them
    if ((mod & (VTERM_MOD_CTRL | VTERM_MOD_ALT)) && [plain length] == 1) {
        vterm_keyboard_unichar(_vt, key, mod & ~VTERM_MOD_SHIFT);
        return;
    }

    NSUInteger length = [chars length];
    for (NSUInteger i = 0; i < length; i++) {
        uint32_t c = [chars characterAtIndex:i];

        if (c >= 0xd800 && c < 0xdc00 && i + 1 < length) {
            uint32_t low = [chars characterAtIndex:++i];
            c = 0x10000 + ((c - 0xd800) << 10) + (low - 0xdc00);
        }
        if (c >= 0xf700 && c < 0xf900)
            continue;
        vterm_keyboard_unichar(_vt, c, VTERM_MOD_NONE);
    }
}

// mouse: reports to programs that ask, otherwise selection and scrolling

- (VTermPos)cellForEvent:(NSEvent *)event
{
    NSPoint p = [self convertPoint:[event locationInWindow] fromView:nil];
    int row = MAX(0, MIN(_rows - 1, (int)floor(p.y / _cellHeight)));
    int col = MAX(0, MIN(_cols - 1, (int)floor(p.x / _cellWidth)));

    return (VTermPos){ .row = row, .col = col };
}

// a program that turned on mouse reporting gets the mouse, shift keeps it for selection
- (BOOL)reportsMouse:(NSEvent *)event
{
    return _mouseMode != VTERM_PROP_MOUSE_NONE && _scrollOffset == 0 &&
        !([event modifierFlags] & NSShiftKeyMask);
}

- (void)reportMouse:(NSEvent *)event button:(int)button pressed:(BOOL)pressed
{
    VTermPos cell = [self cellForEvent:event];
    VTermModifier mod = modifiersForEvent(event);

    vterm_mouse_move(_vt, cell.row, cell.col, mod);
    if (button != 0)
        vterm_mouse_button(_vt, button, pressed, mod);
}

- (VTermPos)lineAndColumnForEvent:(NSEvent *)event
{
    NSPoint p = [self convertPoint:[event locationInWindow] fromView:nil];
    int row = MAX(0, MIN(_rows - 1, (int)floor(p.y / _cellHeight)));
    int col = MAX(0, MIN(_cols, (int)floor(p.x / _cellWidth + 0.5)));

    return (VTermPos){ .row = [self topLine] + row, .col = col };
}

static BOOL isWordCharacter(uint32_t c)
{
    return c != 0 && c != ' ' && c != (uint32_t)-1 && strchr("()[]{}<>'\"`,;|&", (int)(c < 128 ? c : 'a')) == NULL;
}

// a double click takes the word under the pointer, a triple click the whole line
- (void)selectUnitAt:(VTermPos)pos clicks:(NSInteger)clicks
{
    int line = pos.row, start = 0, end = _cols;

    if (clicks == 2) {
        VTermScreenCell cell;
        int col = MIN(pos.col, _cols - 1);

        [self getCell:&cell line:line col:col];
        if (!isWordCharacter(cell.chars[0]))
            return;
        for (start = col; start > 0; start--) {
            [self getCell:&cell line:line col:start - 1];
            if (!isWordCharacter(cell.chars[0]))
                break;
        }
        for (end = col + 1; end < _cols; end++) {
            [self getCell:&cell line:line col:end];
            if (!isWordCharacter(cell.chars[0]))
                break;
        }
    }
    _selAnchor = (VTermPos){ .row = line, .col = start };
    _selEnd = (VTermPos){ .row = line, .col = end };
    _hasSelection = YES;
}

- (void)mouseDown:(NSEvent *)event
{
    [[self window] makeFirstResponder:self];
    if ([self reportsMouse:event]) {
        _reportingClick = YES;
        [self reportMouse:event button:1 pressed:YES];
        return;
    }

    _selAnchor = _selEnd = [self lineAndColumnForEvent:event];
    _hasSelection = NO;
    _selecting = YES;
    if ([event clickCount] >= 2) {
        [self selectUnitAt:_selAnchor clicks:[event clickCount] >= 3 ? 3 : 2];
        _selecting = NO;
    }
    [self setNeedsDisplay:YES];
}

- (void)mouseDragged:(NSEvent *)event
{
    if (_reportingClick) {
        [self reportMouse:event button:0 pressed:NO];
        return;
    }
    if (!_selecting)
        return;

    _selEnd = [self lineAndColumnForEvent:event];
    _hasSelection = vterm_pos_cmp(_selAnchor, _selEnd) != 0;
    [self setNeedsDisplay:YES];
}

- (void)mouseUp:(NSEvent *)event
{
    if (_reportingClick) {
        _reportingClick = NO;
        [self reportMouse:event button:1 pressed:NO];
        return;
    }
    [self mouseDragged:event];
    _selecting = NO;
}

- (void)rightMouseDown:(NSEvent *)event
{
    if ([self reportsMouse:event])
        [self reportMouse:event button:3 pressed:YES];
}

- (void)rightMouseUp:(NSEvent *)event
{
    if ([self reportsMouse:event])
        [self reportMouse:event button:3 pressed:NO];
}

- (void)otherMouseDown:(NSEvent *)event
{
    if ([self reportsMouse:event])
        [self reportMouse:event button:2 pressed:YES];
    else
        [self paste:self];
}

- (void)otherMouseUp:(NSEvent *)event
{
    if ([self reportsMouse:event])
        [self reportMouse:event button:2 pressed:NO];
}

- (void)mouseMoved:(NSEvent *)event
{
    if (_mouseMode == VTERM_PROP_MOUSE_MOVE && [self reportsMouse:event])
        [self reportMouse:event button:0 pressed:NO];
}

- (void)scrollWheel:(NSEvent *)event
{
    CGFloat dy = [event deltaY];

    if (dy == 0)
        return;
    int lines = (int)(dy > 0 ? ceil(dy) : floor(dy));

    // wheel buttons 4 and 5 for a program reading the mouse
    if ([self reportsMouse:event]) {
        for (int i = 0; i < abs(lines); i++)
            [self reportMouse:event button:lines > 0 ? 4 : 5 pressed:YES];
        return;
    }

    // the alternate screen has no scrollback, so the wheel moves the program's view with arrow keys
    if (_altScreen) {
        for (int i = 0; i < abs(lines) * 3; i++)
            vterm_keyboard_key(_vt, lines > 0 ? VTERM_KEY_UP : VTERM_KEY_DOWN, VTERM_MOD_NONE);
        return;
    }
    [self scrollBy:lines * 3];
}

// the pasteboard

- (NSString *)selectedText
{
    if (!_hasSelection)
        return nil;

    VTermPos a = _selAnchor, b = _selEnd;
    if (vterm_pos_cmp(a, b) > 0) {
        VTermPos t = a;
        a = b;
        b = t;
    }

    NSMutableString *text = [NSMutableString string];
    for (int line = a.row; line <= b.row; line++) {
        int from = line == a.row ? a.col : 0;
        int to = line == b.row ? b.col : _cols;
        unichar buf[_cols * 2 * VTERM_MAX_CHARS_PER_CELL + 1];
        int length = 0;

        for (int col = from; col < to; col++) {
            VTermScreenCell cell;
            [self getCell:&cell line:line col:col];
            if (cell.chars[0] == (uint32_t)-1)
                continue;
            if (cell.chars[0] == 0)
                appendCodepoint(buf, &length, ' ');
            for (int i = 0; i < VTERM_MAX_CHARS_PER_CELL && cell.chars[i]; i++)
                appendCodepoint(buf, &length, cell.chars[i]);
        }

        // a soft-wrapped line runs on into the next, elsewhere trailing blanks are padding
        BOOL wraps = line != b.row && [self lineWraps:line];
        while (!wraps && length > 0 && buf[length - 1] == ' ')
            length--;
        [text appendString:[NSString stringWithCharacters:buf length:length]];
        if (line != b.row && !wraps)
            [text appendString:@"\n"];
    }
    return text;
}

- (void)copy:(id)sender
{
    NSString *text = [self selectedText];
    if (text == nil)
        return;

    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    [pasteboard clearContents];
    [pasteboard setString:text forType:NSPasteboardTypeString];
}

- (void)paste:(id)sender
{
    NSString *text = [[NSPasteboard generalPasteboard] stringForType:NSPasteboardTypeString];
    if ([text length] == 0)
        return;

    [self scrollBy:-_scrollOffset];
    vterm_keyboard_start_paste(_vt);
    NSUInteger length = [text length];
    for (NSUInteger i = 0; i < length; i++) {
        uint32_t c = [text characterAtIndex:i];

        if (c >= 0xd800 && c < 0xdc00 && i + 1 < length) {
            uint32_t low = [text characterAtIndex:++i];
            c = 0x10000 + ((c - 0xd800) << 10) + (low - 0xdc00);
        }
        // line ends go in as returns, a CRLF pair as one
        if (c == '\r' && i + 1 < length && [text characterAtIndex:i + 1] == '\n')
            continue;
        vterm_keyboard_unichar(_vt, c == '\n' ? '\r' : c, VTERM_MOD_NONE);
    }
    vterm_keyboard_end_paste(_vt);
}

- (void)selectAll:(id)sender
{
    _selAnchor = (VTermPos){ .row = 0, .col = 0 };
    _selEnd = (VTermPos){ .row = [self lineCount] - 1, .col = _cols };
    _hasSelection = YES;
    [self setNeedsDisplay:YES];
}

- (void)clearScrollback:(id)sender
{
    [self clearScrollback];
}

- (BOOL)validateMenuItem:(NSMenuItem *)item
{
    if ([item action] == @selector(copy:))
        return _hasSelection;
    return YES;
}

@end
