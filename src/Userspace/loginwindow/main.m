// main.m - loginwindow: the login screen, then the person's session as that person
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// runs as root on the system compositor and starts the session in a child that has become the user

#import <AppKit/AppKit.h>
#include "pdaccounts.h"
#include <dispatch/dispatch.h>
#include <dlfcn.h>
#include <execinfo.h>
#include <sys/ucontext.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <math.h>
#include <pwd.h>
#include <signal.h>
#include <spawn.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define LOGIN_PREFS @"/Library/Preferences/org.puredarwin.loginwindow.plist"
#define USER_SESSION "/usr/libexec/pd-user-session"

extern char **environ;

static NSColor *rgba(int r, int g, int b, CGFloat a)
{
    return [NSColor colorWithCalibratedRed:r / 255.0 green:g / 255.0 blue:b / 255.0 alpha:a];
}

static NSColor *white(CGFloat a)
{
    return [NSColor colorWithCalibratedWhite:1 alpha:a];
}

static NSFont *font(NSString *name, CGFloat size)
{
    return [NSFont fontWithName:name size:size] ?: [NSFont systemFontOfSize:size];
}

// the shadow the demo puts under all text on the picture, 0 1px 3px at 25%
static NSShadow *textShadow(CGFloat blur, CGFloat alpha)
{
    NSShadow *s = [[[NSShadow alloc] init] autorelease];

    [s setShadowOffset:NSMakeSize(0, -1)];
    [s setShadowBlurRadius:blur];
    [s setShadowColor:[NSColor colorWithCalibratedWhite:0 alpha:alpha]];
    return s;
}

// macOS's AppleAccentColor numbers, the demo's swatches, blue by default
static NSColor *accentColor(id value)
{
    static const struct { int value; int r, g, b; } accents[] = {
        { 4, 0x2f, 0x6f, 0xdb }, { 5, 0x9b, 0x4f, 0xd1 }, { 6, 0xe0, 0x45, 0x7b }, { 0, 0xe5, 0x48, 0x3f },
        { 1, 0xf0, 0x8a, 0x24 }, { 2, 0xf2, 0xc2, 0x30 }, { 3, 0x4c, 0xae, 0x4f }, { -1, 0x8c, 0x8c, 0x8c },
    };
    int v = value != nil ? [value intValue] : 4;

    for (unsigned i = 0; i < sizeof(accents) / sizeof(accents[0]); i++) {
        if (accents[i].value == v)
            return rgba(accents[i].r, accents[i].g, accents[i].b, 1);
    }
    return rgba(0x2f, 0x6f, 0xdb, 1);
}

static NSString *initials(NSString *name)
{
    NSArray *words = [[name stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]]
                      componentsSeparatedByCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
    NSMutableArray *parts = [NSMutableArray array];
    NSString *first, *last;

    for (NSString *w in words) {
        if ([w length] > 0)
            [parts addObject:w];
    }
    if ([parts count] == 0)
        return @"?";
    first = [[parts[0] substringToIndex:1] uppercaseString];
    if ([parts count] == 1)
        return first;
    last = [[[parts lastObject] substringToIndex:1] uppercaseString];
    return [first stringByAppendingString:last];
}

// one account as the screen needs it
@interface Person : NSObject {
@public
    NSString *account, *realName, *home, *picture, *shell;
    uid_t uid;
    gid_t gid;
}
@end

@implementation Person
- (void)dealloc
{
    [account release];
    [realName release];
    [home release];
    [picture release];
    [shell release];
    [super dealloc];
}
@end

static NSString *attr(CFDictionaryRef rec, const char *key)
{
    char buf[1024];

    return pda_attr(rec, key, buf, sizeof(buf)) ? [NSString stringWithUTF8String:buf] : nil;
}

static NSArray *loadPeople(void)
{
    CFArrayRef names = pda_copy_user_names();
    NSMutableArray *people = [NSMutableArray array];

    for (CFIndex i = 0; i < CFArrayGetCount(names); i++) {
        char name[256];
        CFMutableDictionaryRef rec;
        Person *p;

        if (!CFStringGetCString(CFArrayGetValueAtIndex(names, i), name, sizeof(name), kCFStringEncodingUTF8))
            continue;
        rec = pda_copy_user(name);
        if (rec == NULL)
            continue;
        p = [[[Person alloc] init] autorelease];
        p->account = [[NSString alloc] initWithUTF8String:name];
        p->realName = [attr(rec, "realname") ?: p->account copy];
        p->home = [attr(rec, "home") ?: [@"/Users" stringByAppendingPathComponent:p->account] copy];
        p->picture = [attr(rec, "picture") copy];
        p->shell = [attr(rec, "shell") ?: @"/bin/zsh" copy];
        p->uid = (uid_t)[attr(rec, "uid") intValue];
        p->gid = attr(rec, "gid") ? (gid_t)[attr(rec, "gid") intValue] : PDA_STAFF_GID;
        CFRelease(rec);
        if (p->uid >= PDA_FIRST_UID)
            [people addObject:p];
    }
    CFRelease(names);
    return people;
}

// a person's own preference file, read straight from their home since loginwindow runs before them
static id personPref(Person *p, NSString *domain, NSString *key)
{
    NSString *path = [NSString stringWithFormat:@"%@/Library/Preferences/%@.plist", p ? p->home : @"/var/root", domain];
    NSDictionary *prefs = [NSDictionary dictionaryWithContentsOfFile:path];

    return prefs[key];
}

typedef enum { HitNone, HitSubmit, HitSleep, HitRestart, HitShutDown, HitPrev, HitNext, HitField } Hit;

@interface LoginView : NSView {
@public
    NSArray *_people;
    NSUInteger _current;
    NSMutableString *_password;
    NSString *_hint;
    BOOL _hintBad, _busy, _dark;
    NSInteger _tries;
    NSDate *_lockedUntil;
    NSImage *_wall;
    NSColor *_accent;
    BOOL _clock24;
    CGFloat _shake, _fade, _spin;
    NSTimer *_animation, *_tick;
    NSDate *_shakeStart, *_fadeStart;
    NSRect _submitRect, _fieldRect, _prevRect, _nextRect, _powerRects[3];
    id _target;
    SEL _action;
}
- (void)loadLook;
- (Person *)person;
- (void)reset;
@end

@implementation LoginView

- (BOOL)isFlipped
{
    return YES;
}

- (BOOL)acceptsFirstResponder
{
    return YES;
}

- (Person *)person
{
    return [_people count] ? _people[_current] : nil;
}

// one horizontal box blur pass over premultiplied rgba rows, a running sum keeps it linear in the radius
static void boxRows(unsigned char *px, int w, int h, int stride, int r, unsigned char *tmp)
{
    for (int y = 0; y < h; y++) {
        unsigned char *row = px + (size_t)y * stride;
        int sum[4] = { 0, 0, 0, 0 }, n = 2 * r + 1;

        for (int i = -r; i <= r; i++) {
            int x = i < 0 ? 0 : i >= w ? w - 1 : i;
            for (int c = 0; c < 4; c++)
                sum[c] += row[x * 4 + c];
        }
        for (int x = 0; x < w; x++) {
            int out = x + r + 1 >= w ? w - 1 : x + r + 1, in = x - r < 0 ? 0 : x - r;

            for (int c = 0; c < 4; c++) {
                tmp[x * 4 + c] = (unsigned char)(sum[c] / n);
                sum[c] += row[out * 4 + c] - row[in * 4 + c];
            }
        }
        memcpy(row, tmp, (size_t)w * 4);
    }
}

// the same pass down the columns, through a transposed copy so it reuses the row code
static void boxColumns(unsigned char *px, int w, int h, int stride, int r)
{
    unsigned char *t = malloc((size_t)w * h * 4), *line = malloc((size_t)h * 4);

    if (t == NULL || line == NULL) {
        free(t);
        free(line);
        return;
    }
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            memcpy(t + ((size_t)x * h + y) * 4, px + (size_t)y * stride + x * 4, 4);
    boxRows(t, h, w, h * 4, r, line);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            memcpy(px + (size_t)y * stride + x * 4, t + ((size_t)x * h + y) * 4, 4);
    free(t);
    free(line);
}

// the desktop picture at half the screen's size, three box passes approach the demo's 28px gaussian
- (NSImage *)frosted:(NSImage *)picture
{
    NSSize size = [picture size];
    int w = MAX(64, (int)NSWidth([self bounds]) / 2), h = MAX(40, (int)NSHeight([self bounds]) / 2);
    NSBitmapImageRep *rep;
    NSImage *image;
    unsigned char *tmp;
    CGFloat scale;
    NSRect from;

    if (size.width <= 0 || size.height <= 0)
        return nil;
    rep = [[[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL pixelsWide:w pixelsHigh:h bitsPerSample:8 samplesPerPixel:4
                                                     hasAlpha:YES isPlanar:NO colorSpaceName:NSDeviceRGBColorSpace
                                                  bytesPerRow:w * 4 bitsPerPixel:32] autorelease];
    scale = MAX(w / size.width, h / size.height);
    from.size = NSMakeSize(w / scale, h / scale);
    from.origin = NSMakePoint((size.width - from.size.width) / 2, (size.height - from.size.height) / 2);
    [NSGraphicsContext saveGraphicsState];
    [NSGraphicsContext setCurrentContext:[NSGraphicsContext graphicsContextWithBitmapImageRep:rep]];
    [picture drawInRect:NSMakeRect(0, 0, w, h) fromRect:from operation:NSCompositeCopy fraction:1];
    [[NSGraphicsContext currentContext] flushGraphics];
    [NSGraphicsContext restoreGraphicsState];

    tmp = malloc((size_t)MAX(w, h) * 4);
    if (tmp != NULL) {
        for (int pass = 0; pass < 3; pass++) {
            boxRows([rep bitmapData], w, h, (int)[rep bytesPerRow], 8, tmp);
            boxColumns([rep bitmapData], w, h, (int)[rep bytesPerRow], 8);
        }
        free(tmp);
    }
    image = [[[NSImage alloc] initWithSize:NSMakeSize(w, h)] autorelease];
    [image addRepresentation:rep];
    return image;
}

- (void)loadLook
{
    Person *p = [self person];
    NSString *name = personPref(p, @"org.puredarwin.Dock", @"DesktopPicture") ?: @"Sunset";
    NSString *path = [name isAbsolutePath] ? name :
                     [[@"/System/Library/Desktop Pictures" stringByAppendingPathComponent:name] stringByAppendingPathExtension:@"jpg"];
    NSImage *picture = [[[NSImage alloc] initWithContentsOfFile:path] autorelease];

    [_wall release];
    _wall = [[self frosted:picture] retain];
    [_accent release];
    _accent = [accentColor(personPref(p, @".GlobalPreferences", @"AppleAccentColor")) retain];
    _dark = personPref(p, @".GlobalPreferences", @"AppleInterfaceStyle") != nil;
    _clock24 = [personPref(p, @"org.puredarwin.MenuBar", @"Clock24") boolValue];
    [self setNeedsDisplay:YES];
}

- (void)reset
{
    [_password setString:@""];
    [_hint release];
    _hint = nil;
    _hintBad = NO;
    _busy = NO;
    _fade = 0;
    [self setNeedsDisplay:YES];
}

- (void)say:(NSString *)hint bad:(BOOL)bad
{
    [_hint release];
    _hint = [hint copy];
    _hintBad = bad;
    [self setNeedsDisplay:YES];
}

// the shake, the spinner and the fade all run off one timer
- (void)animate:(NSTimer *)timer
{
    BOOL running = NO;

    if (_shakeStart != nil) {
        // the demo's keyframes: left at 15% and 55%, right at 35% and 75% of 0.4 s
        CGFloat t = -[_shakeStart timeIntervalSinceNow] / 0.4;
        static const CGFloat keys[][2] = { { 0, 0 }, { .15, -9 }, { .35, 9 }, { .55, -9 }, { .75, 9 }, { 1, 0 } };

        _shake = 0;
        if (t >= 1) {
            [_shakeStart release];
            _shakeStart = nil;
        } else {
            for (int i = 0; i < 5; i++) {
                if (t >= keys[i][0] && t <= keys[i + 1][0])
                    _shake = keys[i][1] + (keys[i + 1][1] - keys[i][1]) * (t - keys[i][0]) / (keys[i + 1][0] - keys[i][0]);
            }
            running = YES;
        }
    }
    if (_busy) {
        _spin = fmod(_spin + 0.08 * 2 * M_PI, 2 * M_PI);
        running = YES;
    }
    if (_fadeStart != nil) {
        _fade = MIN(1, -[_fadeStart timeIntervalSinceNow] / 0.5);
        running = YES;
    }
    if (_lockedUntil != nil) {
        NSTimeInterval left = [_lockedUntil timeIntervalSinceNow];

        if (left <= 0) {
            [_lockedUntil release];
            _lockedUntil = nil;
            [self say:nil bad:NO];
        } else {
            [self say:[NSString stringWithFormat:NSLocalizedString(@"Too many attempts. Try again in %d s.", @""), (int)ceil(left)] bad:YES];
            running = YES;
        }
    }
    [self setNeedsDisplay:YES];
    if (!running) {
        [_animation invalidate];
        _animation = nil;
    }
}

- (void)startAnimating
{
    if (_animation == nil)
        _animation = [NSTimer scheduledTimerWithTimeInterval:1.0 / 60 target:self selector:@selector(animate:) userInfo:nil repeats:YES];
}

- (void)shake
{
    [_shakeStart release];
    _shakeStart = [[NSDate date] retain];
    [self startAnimating];
}

- (void)tick:(NSTimer *)timer
{
    [self setNeedsDisplay:YES];
}

- (void)viewDidMoveToWindow
{
    if ([self window] != nil && _tick == nil)
        _tick = [NSTimer scheduledTimerWithTimeInterval:10 target:self selector:@selector(tick:) userInfo:nil repeats:YES];
}

- (void)drawCentered:(NSString *)text at:(CGFloat)y attributes:(NSDictionary *)attrs
{
    NSSize size = [text sizeWithAttributes:attrs];

    [text drawAtPoint:NSMakePoint(floor(NSMidX([self bounds]) - size.width / 2), y) withAttributes:attrs];
}

// frosted round control, the demo's glass with its half-pixel edge
- (void)glass:(NSBezierPath *)path
{
    [(_dark ? white(.13) : white(.2)) setFill];
    [path fill];
    [(_dark ? white(.2) : white(.32)) setStroke];
    [path setLineWidth:.5];
    [path stroke];
}

- (void)drawWall
{
    NSRect b = [self bounds];

    [rgba(0xd9, 0x77, 0x5a, 1) setFill];
    NSRectFill(b);
    if (_wall != nil) {
        [[NSGraphicsContext currentContext] setImageInterpolation:NSImageInterpolationHigh];
        // drawn past the edges like the demo's inset:-40px, so the soft edge never shows
        [_wall drawInRect:NSInsetRect(b, -20, -20) fromRect:NSZeroRect operation:NSCompositeCopy fraction:1 respectFlipped:YES hints:nil];
    }
    [(_dark ? rgba(10, 6, 8, .42) : rgba(40, 18, 12, .12)) setFill];
    NSRectFillUsingOperation(b, NSCompositeSourceOver);
}

// wi-fi and sound in the top corner, as the menu bar would have them
- (void)drawStatus
{
    NSRect b = [self bounds];
    CGFloat x = NSMaxX(b) - 14 - 16, y = 13;
    NSBezierPath *p = [NSBezierPath bezierPath];

    [NSGraphicsContext saveGraphicsState];
    [textShadow(2, .25) set];
    [white(1) set];
    // sound: a speaker and two waves
    [p moveToPoint:NSMakePoint(x, y - 3)];
    [p lineToPoint:NSMakePoint(x + 3, y - 3)];
    [p lineToPoint:NSMakePoint(x + 7, y - 6.5)];
    [p lineToPoint:NSMakePoint(x + 7, y + 6.5)];
    [p lineToPoint:NSMakePoint(x + 3, y + 3)];
    [p lineToPoint:NSMakePoint(x, y + 3)];
    [p closePath];
    [p fill];
    for (int i = 0; i < 2; i++) {
        NSBezierPath *w = [NSBezierPath bezierPath];

        [w appendBezierPathWithArcWithCenter:NSMakePoint(x + 7, y) radius:4 + 4 * i startAngle:-45 endAngle:45];
        [w setLineWidth:1.5];
        [w stroke];
    }
    // wi-fi: three arcs over a dot
    x -= 32;
    for (int i = 0; i < 3; i++) {
        NSBezierPath *w = [NSBezierPath bezierPath];

        [w appendBezierPathWithArcWithCenter:NSMakePoint(x + 8, y + 5) radius:3 + 3.5 * i startAngle:225 endAngle:315];
        [w setLineWidth:1.6];
        [w stroke];
    }
    [[NSBezierPath bezierPathWithOvalInRect:NSMakeRect(x + 6.6, y + 3.6, 2.8, 2.8)] fill];
    [NSGraphicsContext restoreGraphicsState];
}

- (void)drawClock
{
    NSRect b = [self bounds];
    NSDate *now = [NSDate date];
    NSDateFormatter *df = [[[NSDateFormatter alloc] init] autorelease];
    CGFloat size = MIN(104, MAX(64, NSWidth(b) * 0.11)), top = floor(NSHeight(b) * (NSHeight(b) <= 560 ? .04 : .09));
    CGFloat alpha = 1 - _fade;
    NSString *date, *time;
    NSDictionary *dateAttrs, *timeAttrs;

    [df setDateFormat:@"EEEE, MMMM d"];
    date = [df stringFromDate:now];
    [df setDateFormat:_clock24 ? @"H:mm" : @"h:mm"];
    time = [df stringFromDate:now];

    dateAttrs = @{ NSFontAttributeName: font(@"Inter-Medium", 19), NSForegroundColorAttributeName: white(.92 * alpha),
                   NSShadowAttributeName: textShadow(12, .18) };
    timeAttrs = @{ NSFontAttributeName: font(@"Inter-SemiBold", size), NSForegroundColorAttributeName: white(alpha),
                   NSShadowAttributeName: textShadow(12, .18), NSKernAttributeName: @(-0.02 * size) };
    [self drawCentered:date at:top attributes:dateAttrs];
    [self drawCentered:time at:top + 24 attributes:timeAttrs];
}

- (void)drawFace:(Person *)p in:(NSRect)r alpha:(CGFloat)alpha
{
    NSImage *photo = p->picture ? [[[NSImage alloc] initWithContentsOfFile:p->picture] autorelease] : nil;
    NSBezierPath *circle = [NSBezierPath bezierPathWithOvalInRect:r];
    NSShadow *s = [[[NSShadow alloc] init] autorelease];

    // 0 6px 22px at 25% under the photo, then a 2px ring at 35% white
    [NSGraphicsContext saveGraphicsState];
    [s setShadowOffset:NSMakeSize(0, -6)];
    [s setShadowBlurRadius:22];
    [s setShadowColor:[NSColor colorWithCalibratedWhite:0 alpha:.25 * alpha]];
    [s set];
    [rgba(0xd9, 0x77, 0x5a, alpha) setFill];
    [circle fill];
    [NSGraphicsContext restoreGraphicsState];

    [NSGraphicsContext saveGraphicsState];
    [circle addClip];
    if (photo != nil) {
        NSSize size = [photo size];
        CGFloat scale = MAX(NSWidth(r) / size.width, NSHeight(r) / size.height);
        NSRect from = NSMakeRect(0, 0, NSWidth(r) / scale, NSHeight(r) / scale);

        from.origin = NSMakePoint((size.width - from.size.width) / 2, (size.height - from.size.height) / 2);
        [photo drawInRect:r fromRect:from operation:NSCompositeSourceOver fraction:alpha respectFlipped:YES hints:nil];
    } else {
        NSDictionary *a = @{ NSFontAttributeName: font(@"Inter-SemiBold", 34), NSForegroundColorAttributeName: white(alpha) };
        NSString *letters = initials(p->realName);
        NSSize ls = [letters sizeWithAttributes:a];

        [letters drawAtPoint:NSMakePoint(floor(NSMidX(r) - ls.width / 2), floor(NSMidY(r) - ls.height / 2)) withAttributes:a];
    }
    [NSGraphicsContext restoreGraphicsState];
    [white(.35 * alpha) setStroke];
    [circle setLineWidth:2];
    [circle stroke];
}

// a round glass button with an arrow or the spinner, at the right end of the field
- (void)drawSubmit:(NSRect)r
{
    NSBezierPath *circle = [NSBezierPath bezierPathWithOvalInRect:r];

    if (_busy) {
        NSBezierPath *ring = [NSBezierPath bezierPathWithOvalInRect:NSInsetRect(r, 5, 5)];
        NSBezierPath *arc = [NSBezierPath bezierPath];
        CGFloat deg = _spin * 180 / M_PI;

        [white(.35) setStroke];
        [ring setLineWidth:2];
        [ring stroke];
        [arc appendBezierPathWithArcWithCenter:NSMakePoint(NSMidX(r), NSMidY(r)) radius:NSWidth(r) / 2 - 5 startAngle:deg endAngle:deg + 90];
        [white(1) setStroke];
        [arc setLineWidth:2];
        [arc stroke];
        return;
    }
    if ([_password length] == 0)
        return;
    [white(.28) setFill];
    [circle fill];
    NSBezierPath *arrow = [NSBezierPath bezierPath];
    CGFloat cx = NSMidX(r), cy = NSMidY(r);

    [arrow moveToPoint:NSMakePoint(cx - 5, cy)];
    [arrow lineToPoint:NSMakePoint(cx + 5, cy)];
    [arrow moveToPoint:NSMakePoint(cx + 1, cy - 4)];
    [arrow lineToPoint:NSMakePoint(cx + 5, cy)];
    [arrow lineToPoint:NSMakePoint(cx + 1, cy + 4)];
    [arrow setLineWidth:1.8];
    [arrow setLineCapStyle:NSRoundLineCapStyle];
    [arrow setLineJoinStyle:NSRoundLineJoinStyle];
    [white(1) setStroke];
    [arrow stroke];
}

- (void)drawPerson
{
    NSRect b = [self bounds];
    Person *p = [self person];
    CGFloat alpha = 1 - _fade, bottom = NSHeight(b) * (NSHeight(b) <= 560 ? .18 : .21);
    // the demo's grid: face, name, field, hint with 12px between
    CGFloat block = 88 + 12 + 21 + 12 + 32 + 12 + 18, top = floor(NSHeight(b) - bottom - block);
    CGFloat cx = floor(NSMidX(b) + _shake);
    NSDictionary *nameAttrs = @{ NSFontAttributeName: font(@"Inter-SemiBold", 17), NSForegroundColorAttributeName: white(alpha),
                                 NSShadowAttributeName: textShadow(3, .25) };
    NSRect face = NSMakeRect(cx - 44, top, 88, 88), field;
    NSString *name;
    NSSize ns;

    if (p == nil) {
        [self drawCentered:NSLocalizedString(@"No accounts yet", @"") at:top + 40 attributes:nameAttrs];
        return;
    }
    [self drawFace:p in:face alpha:alpha];

    name = p->realName;
    ns = [name sizeWithAttributes:nameAttrs];
    [name drawAtPoint:NSMakePoint(floor(cx - ns.width / 2), top + 88 + 12) withAttributes:nameAttrs];

    // other people are a step away on either side of the photo
    _prevRect = _nextRect = NSZeroRect;
    if ([_people count] > 1) {
        NSDictionary *chev = @{ NSFontAttributeName: font(@"Inter-SemiBold", 22), NSForegroundColorAttributeName: white(.7 * alpha),
                                NSShadowAttributeName: textShadow(3, .25) };

        _prevRect = NSMakeRect(cx - 44 - 52, top + 28, 32, 32);
        _nextRect = NSMakeRect(cx + 44 + 20, top + 28, 32, 32);
        [@"‹" drawAtPoint:NSMakePoint(NSMinX(_prevRect) + 11, NSMinY(_prevRect) + 2) withAttributes:chev];
        [@"›" drawAtPoint:NSMakePoint(NSMinX(_nextRect) + 11, NSMinY(_nextRect) + 2) withAttributes:chev];
    }

    field = NSMakeRect(cx - 110, top + 88 + 12 + 21 + 12, 220, 32);
    _fieldRect = field;
    NSBezierPath *pill = [NSBezierPath bezierPathWithRoundedRect:field xRadius:16 yRadius:16];

    [self glass:pill];
    // the accent ring the field gets while it has focus, 3px at 55% just outside the pill
    NSBezierPath *ring = [NSBezierPath bezierPathWithRoundedRect:NSInsetRect(field, -1.5, -1.5) xRadius:17.5 yRadius:17.5];
    [[_accent colorWithAlphaComponent:.55 * alpha] setStroke];
    [ring setLineWidth:3];
    [ring stroke];

    NSString *shown = [_password length] ? [@"" stringByPaddingToLength:[_password length] withString:@"•" startingAtIndex:0] :
                                           NSLocalizedString(@"Enter Password", @"");
    NSDictionary *fieldAttrs = @{ NSFontAttributeName: font(@"Inter-Regular", 14),
                                  NSForegroundColorAttributeName: [_password length] ? white(alpha) : white(.78 * alpha),
                                  NSKernAttributeName: @([_password length] ? 1.1 : 0) };
    NSSize fs = [shown sizeWithAttributes:fieldAttrs];
    NSRect clip = NSMakeRect(NSMinX(field) + 14, NSMinY(field), 220 - 14 - 32, 32);

    [NSGraphicsContext saveGraphicsState];
    NSRectClip(clip);
    // a long password scrolls, the newest characters stay in view
    [shown drawAtPoint:NSMakePoint(MIN(NSMinX(clip), NSMaxX(clip) - fs.width), floor(NSMidY(field) - fs.height / 2)) withAttributes:fieldAttrs];
    [NSGraphicsContext restoreGraphicsState];

    _submitRect = NSMakeRect(NSMaxX(field) - 4 - 24, NSMinY(field) + 4, 24, 24);
    [self drawSubmit:_submitRect];

    if (_hint != nil) {
        NSDictionary *hintAttrs = @{ NSFontAttributeName: font(@"Inter-Regular", 12),
                                     NSForegroundColorAttributeName: _hintBad ? rgba(0xff, 0xd0, 0xcc, alpha) : white(.78 * alpha),
                                     NSShadowAttributeName: textShadow(2, .25) };

        [self drawCentered:_hint at:NSMaxY(field) + 12 attributes:hintAttrs];
    }
}

- (void)drawPowerSymbol:(int)which in:(NSRect)r
{
    NSBezierPath *p = [NSBezierPath bezierPath];
    NSPoint c = NSMakePoint(NSMidX(r), NSMidY(r));

    [p setLineWidth:1.6];
    [p setLineCapStyle:NSRoundLineCapStyle];
    if (which == 0) {
        // a crescent: the outer arc of one circle back along a smaller one
        NSBezierPath *moon = [NSBezierPath bezierPath];

        [moon appendBezierPathWithArcWithCenter:c radius:6.5 startAngle:-60 endAngle:240 clockwise:NO];
        [moon appendBezierPathWithArcWithCenter:NSMakePoint(c.x + 3.2, c.y - 2.6) radius:4.6 startAngle:200 endAngle:-20 clockwise:YES];
        [moon closePath];
        [white(1) setFill];
        [moon fill];
        return;
    }
    if (which == 1) {
        // restart: most of a circle with an arrowhead
        [p appendBezierPathWithArcWithCenter:c radius:6.5 startAngle:-60 endAngle:240 clockwise:NO];
        [p moveToPoint:NSMakePoint(c.x + 6.5, c.y - 9)];
        [p lineToPoint:NSMakePoint(c.x + 3.5, c.y - 5.6)];
        [p lineToPoint:NSMakePoint(c.x + 7.5, c.y - 4)];
    } else {
        // shut down: the power symbol
        [p appendBezierPathWithArcWithCenter:c radius:6.5 startAngle:-55 endAngle:235 clockwise:NO];
        [p moveToPoint:NSMakePoint(c.x, c.y - 8.5)];
        [p lineToPoint:NSMakePoint(c.x, c.y - 1)];
    }
    [white(1) setStroke];
    [p stroke];
}

- (void)drawPower
{
    NSRect b = [self bounds];
    NSArray *labels = @[ NSLocalizedString(@"Sleep", @""), NSLocalizedString(@"Restart", @""), NSLocalizedString(@"Shut Down", @"") ];
    NSDictionary *attrs = @{ NSFontAttributeName: font(@"Inter-Regular", 12), NSForegroundColorAttributeName: white(1),
                             NSShadowAttributeName: textShadow(2, .25) };
    CGFloat widths[3], total = 2 * 34, x, y = floor(NSHeight(b) * .95 - 36 - 6 - 15);

    // the demo's flex row: each button as wide as its label, 34px between them
    for (int i = 0; i < 3; i++) {
        widths[i] = MAX(36, ceil([labels[i] sizeWithAttributes:attrs].width));
        total += widths[i];
    }
    x = floor(NSMidX(b) - total / 2);
    for (int i = 0; i < 3; i++) {
        NSRect circle = NSMakeRect(floor(x + widths[i] / 2 - 18), y, 36, 36);
        NSSize ls = [labels[i] sizeWithAttributes:attrs];

        x += widths[i] + 34;

        _powerRects[i] = NSUnionRect(circle, NSMakeRect(NSMidX(circle) - ls.width / 2, NSMaxY(circle) + 6, ls.width, ls.height));
        [self glass:[NSBezierPath bezierPathWithOvalInRect:circle]];
        [self drawPowerSymbol:i in:circle];
        [labels[i] drawAtPoint:NSMakePoint(floor(NSMidX(circle) - ls.width / 2), NSMaxY(circle) + 6) withAttributes:attrs];
    }
}

- (void)drawHost
{
    NSDictionary *sv = [NSDictionary dictionaryWithContentsOfFile:@"/System/Library/CoreServices/SystemVersion.plist"];
    NSString *host = [[[NSProcessInfo processInfo] hostName] componentsSeparatedByString:@"."][0];
    NSString *text = [NSString stringWithFormat:@"%@ %@ · %@", sv[@"ProductName"] ?: @"PureDarwin",
                                                sv[@"ProductVersion"] ?: @"", host ?: @"puredarwin"];
    NSDictionary *attrs = @{ NSFontAttributeName: font(@"Inter-Regular", 11), NSForegroundColorAttributeName: white(.78),
                             NSShadowAttributeName: textShadow(2, .25) };

    [text drawAtPoint:NSMakePoint(14, NSHeight([self bounds]) - 12 - 13) withAttributes:attrs];
}

- (void)drawRect:(NSRect)dirty
{
    [self drawWall];
    [self drawStatus];
    [self drawClock];
    [self drawPerson];
    [self drawPower];
    [self drawHost];
}

- (Hit)hitAt:(NSPoint)p
{
    if (NSPointInRect(p, NSInsetRect(_submitRect, -4, -4)) && [_password length] > 0)
        return HitSubmit;
    if (NSPointInRect(p, _prevRect))
        return HitPrev;
    if (NSPointInRect(p, _nextRect))
        return HitNext;
    if (NSPointInRect(p, _fieldRect))
        return HitField;
    for (int i = 0; i < 3; i++) {
        if (NSPointInRect(p, _powerRects[i]))
            return (Hit)(HitSleep + i);
    }
    return HitNone;
}

- (void)choose:(NSInteger)step
{
    if ([_people count] < 2 || _busy)
        return;
    _current = (_current + [_people count] + step) % [_people count];
    _tries = 0;
    [self reset];
    [self loadLook];
}

- (void)mouseDown:(NSEvent *)event
{
    NSPoint p = [self convertPoint:[event locationInWindow] fromView:nil];

    switch ([self hitAt:p]) {
    case HitSubmit: [_target performSelector:_action withObject:self]; break;
    case HitPrev: [self choose:-1]; break;
    case HitNext: [self choose:1]; break;
    case HitSleep: [_target performSelector:@selector(sleep:) withObject:self]; break;
    case HitRestart: [_target performSelector:@selector(restart:) withObject:self]; break;
    case HitShutDown: [_target performSelector:@selector(shutDown:) withObject:self]; break;
    default: break;
    }
}

- (void)keyDown:(NSEvent *)event
{
    NSString *chars = [event characters];
    unichar c = [chars length] ? [chars characterAtIndex:0] : 0;

    if (_busy || _lockedUntil != nil)
        return;
    if (c == '\r' || c == 3) {
        if ([_password length])
            [_target performSelector:_action withObject:self];
    } else if (c == 127 || c == 8) {
        if ([_password length])
            [_password deleteCharactersInRange:NSMakeRange([_password length] - 1, 1)];
    } else if (c == 27) {
        [_password setString:@""];
    } else if (c == NSLeftArrowFunctionKey && [_password length] == 0) {
        [self choose:-1];
        return;
    } else if (c == NSRightArrowFunctionKey && [_password length] == 0) {
        [self choose:1];
        return;
    } else if (c >= 0x20 && c < 0xf700 && [_password length] < 256) {
        [_password appendString:chars];
    } else {
        return;
    }
    if (!_hintBad || _lockedUntil == nil)
        [self say:nil bad:NO];
    [self setNeedsDisplay:YES];
}

@end

// a borderless window never becomes key on its own, the login screen has to take the keyboard
@interface LoginWindow : NSWindow
@end

@implementation LoginWindow
- (BOOL)canBecomeKeyWindow
{
    return YES;
}

- (BOOL)canBecomeMainWindow
{
    return YES;
}
@end

@interface LoginController : NSObject {
    NSWindow *_window;
    LoginView *_view;
    pid_t _session;
    dispatch_source_t _watch, _usersWatch;
    uid_t _sessionUid;
}
@end

@implementation LoginController

- (NSString *)socketPath
{
    NSString *dir = [[[NSProcessInfo processInfo] environment] objectForKey:@"XDG_RUNTIME_DIR"] ?: @"/var/run/user/0";
    NSString *display = [[[NSProcessInfo processInfo] environment] objectForKey:@"WAYLAND_DISPLAY"] ?: @"wayland-0";

    return [display isAbsolutePath] ? display : [dir stringByAppendingPathComponent:display];
}

// the compositor's socket is lent to the person for the length of their session
- (void)lendDisplayTo:(uid_t)uid gid:(gid_t)gid
{
    NSString *socket = [self socketPath];

    chmod([[socket stringByDeletingLastPathComponent] fileSystemRepresentation], uid == 0 ? 0700 : 0711);
    chown([socket fileSystemRepresentation], uid, gid);
}

- (void)rememberPerson:(Person *)p
{
    NSDictionary *prefs = @{ @"lastUserName": p->account };

    [prefs writeToFile:LOGIN_PREFS atomically:YES];
}

- (void)sessionEnded
{
    int status;

    if (_watch != NULL) {
        dispatch_source_cancel(_watch);
        dispatch_release(_watch);
        _watch = NULL;
    }
    waitpid(_session, &status, WNOHANG);
    _session = 0;
    [self lendDisplayTo:0 gid:0];
    [_view reset];
    [_view loadLook];
    [_window orderFront:nil];
    [_window makeKeyWindow];
    [_window makeFirstResponder:_view];
}

// the child becomes the person and runs their session, nothing of root's state comes along
- (BOOL)startSessionFor:(Person *)p
{
    NSString *runtime = [NSString stringWithFormat:@"/var/run/user/%u", (unsigned)p->uid];
    NSString *display = [self socketPath];
    const char *user = [p->account UTF8String], *home = [p->home fileSystemRepresentation];
    char envHome[1100], envUser[300], envLogname[300], envShell[1100], envRuntime[200], envDisplay[1100];
    char *envp[10], *argv[] = { (char *)USER_SESSION, NULL };
    pid_t pid;

    mkdir("/var/run/user", 0755);
    mkdir([runtime fileSystemRepresentation], 0700);
    chown([runtime fileSystemRepresentation], p->uid, p->gid);
    chmod([runtime fileSystemRepresentation], 0700);
    [self lendDisplayTo:p->uid gid:p->gid];

    snprintf(envHome, sizeof(envHome), "HOME=%s", home);
    snprintf(envUser, sizeof(envUser), "USER=%s", user);
    snprintf(envLogname, sizeof(envLogname), "LOGNAME=%s", user);
    snprintf(envShell, sizeof(envShell), "SHELL=%s", [p->shell fileSystemRepresentation]);
    snprintf(envRuntime, sizeof(envRuntime), "XDG_RUNTIME_DIR=%s", [runtime fileSystemRepresentation]);
    snprintf(envDisplay, sizeof(envDisplay), "WAYLAND_DISPLAY=%s", [display fileSystemRepresentation]);
    envp[0] = envHome;
    envp[1] = envUser;
    envp[2] = envLogname;
    envp[3] = envShell;
    envp[4] = (char *)"PATH=/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin";
    envp[5] = envRuntime;
    envp[6] = envDisplay;
    envp[7] = (char *)"LANG=en_US.UTF-8";
    envp[8] = NULL;

    pid = fork();
    if (pid < 0) {
        [self lendDisplayTo:0 gid:0];
        return NO;
    }
    if (pid == 0) {
        // its own session and process group, so Log Out can end everything it started
        for (int fd = 3; fd < 1024; fd++)
            close(fd);
        setsid();
        if (setgid(p->gid) != 0 || initgroups(user, (int)p->gid) != 0 || setuid(p->uid) != 0 || getuid() != p->uid ||
            setuid(0) == 0)
            _exit(126);
        if (chdir(home) != 0)
            chdir("/");
        execve(USER_SESSION, argv, envp);
        _exit(127);
    }
    _session = pid;
    _sessionUid = p->uid;
    _watch = dispatch_source_create(DISPATCH_SOURCE_TYPE_PROC, (uintptr_t)pid, DISPATCH_PROC_EXIT, dispatch_get_main_queue());
    dispatch_source_set_event_handler(_watch, ^{ [self sessionEnded]; });
    dispatch_resume(_watch);
    return YES;
}

- (void)finishLogin:(Person *)p
{
    [_window orderOut:nil];
    [_view->_fadeStart release];
    _view->_fadeStart = nil;
    // nothing animates on the hidden screen while the person works
    _view->_busy = NO;
    [_view->_animation invalidate];
    _view->_animation = nil;
    if (![self startSessionFor:p]) {
        [self sessionEnded];
        [_view say:NSLocalizedString(@"The session couldn't start.", @"") bad:YES];
    }
}

- (void)checkPassword:(Person *)p
{
    CFMutableDictionaryRef rec = pda_copy_user([p->account UTF8String]);
    BOOL ok = rec != NULL && pda_check_password(rec, [_view->_password UTF8String]);

    if (rec)
        CFRelease(rec);
    [_view->_password setString:@""];
    _view->_busy = NO;
    if (ok) {
        _view->_tries = 0;
        [self rememberPerson:p];
        _view->_busy = YES;
        _view->_fadeStart = [[NSDate date] retain];
        [_view startAnimating];
        [self performSelector:@selector(finishLogin:) withObject:p afterDelay:0.55];
        return;
    }
    _view->_tries++;
    [_view shake];
    if (_view->_tries >= 3)
        [_view say:[NSString stringWithFormat:NSLocalizedString(@"Incorrect password. It's the one you chose for %@.", @""), p->account] bad:YES];
    else
        [_view say:NSLocalizedString(@"Incorrect password", @"") bad:YES];
    // past three tries each further one waits longer, up to half a minute
    if (_view->_tries > 3) {
        NSTimeInterval wait = MIN(30, pow(2, _view->_tries - 3));

        _view->_lockedUntil = [[NSDate dateWithTimeIntervalSinceNow:wait] retain];
        [_view startAnimating];
    }
}

- (void)submit:(LoginView *)view
{
    Person *p = [view person];

    if (p == nil || view->_busy || view->_lockedUntil != nil || [view->_password length] == 0)
        return;
    view->_busy = YES;
    [view startAnimating];
    [view setNeedsDisplay:YES];
    // the spinner draws once before the key derivation takes the main thread
    [self performSelector:@selector(checkPassword:) withObject:p afterDelay:0.05];
}

- (void)power:(const char *)flag
{
    char *argv[] = { (char *)"/bin/shutdown", (char *)flag, NULL };
    pid_t pid;

    if (posix_spawn(&pid, argv[0], NULL, NULL, argv, environ) != 0)
        [_view say:NSLocalizedString(@"That isn't available right now.", @"") bad:YES];
}

- (void)sleep:(id)sender
{
    [_view say:NSLocalizedString(@"Sleep isn't available on this computer yet.", @"") bad:NO];
}

- (void)restart:(id)sender
{
    [self power:"-r"];
}

- (void)shutDown:(id)sender
{
    [self power:"-h"];
}

// people added or removed while the screen is up show at once, the selection stays on the same account
- (void)reloadPeople
{
    Person *cur = [_view person];
    NSString *was = cur != nil ? [[cur->account retain] autorelease] : nil;
    NSArray *people = loadPeople();
    NSUInteger keep = 0;

    for (NSUInteger i = 0; i < [people count]; i++) {
        if ([((Person *)people[i])->account isEqualToString:was])
            keep = i;
    }
    [_view->_people release];
    _view->_people = [people retain];
    if (keep != _view->_current && !_view->_busy) {
        _view->_current = keep;
        [_view reset];
        [_view loadLook];
    }
    _view->_current = MIN(_view->_current, [people count] ? [people count] - 1 : 0);
    [_view setNeedsDisplay:YES];
}

// every add, delete and rewrite renames a plist into the users folder, which writes the folder
- (void)watchUsers
{
    int fd = open(PDA_USERS, O_EVTONLY);

    if (fd < 0)
        return;
    _usersWatch = dispatch_source_create(DISPATCH_SOURCE_TYPE_VNODE, fd,
                                         DISPATCH_VNODE_WRITE | DISPATCH_VNODE_EXTEND | DISPATCH_VNODE_LINK, dispatch_get_main_queue());
    dispatch_source_set_event_handler(_usersWatch, ^{
        [self reloadPeople];
    });
    dispatch_source_set_cancel_handler(_usersWatch, ^{
        close(fd);
    });
    dispatch_resume(_usersWatch);
}

- (void)applicationDidFinishLaunching:(NSNotification *)note
{
    NSRect screen = [[NSScreen mainScreen] frame];
    NSString *last = [NSDictionary dictionaryWithContentsOfFile:LOGIN_PREFS][@"lastUserName"];
    NSArray *people = loadPeople();

    if (NSWidth(screen) < 100)
        screen = NSMakeRect(0, 0, 1280, 800);
    _view = [[LoginView alloc] initWithFrame:NSMakeRect(0, 0, NSWidth(screen), NSHeight(screen))];
    _view->_people = [people retain];
    _view->_password = [[NSMutableString alloc] init];
    _view->_target = self;
    _view->_action = @selector(submit:);
    for (NSUInteger i = 0; i < [people count]; i++) {
        if ([((Person *)people[i])->account isEqualToString:last])
            _view->_current = i;
    }
    [_view loadLook];

    _window = [[LoginWindow alloc] initWithContentRect:screen styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO];
    [_window setLevel:NSScreenSaverWindowLevel];
    [_window setContentView:_view];
    [_window makeFirstResponder:_view];
    [_window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];
    [self watchUsers];
}

@end

// a crash prints where it happened, the session log otherwise only sees loginwindow vanish
static void crashed(int sig, siginfo_t *info, void *context)
{
    void *frames[48];
    int count = backtrace(frames, 48);

    ucontext_t *uc = context;
    void *pc = (void *)uc->uc_mcontext->__ss.__rip;
    Dl_info at;

    fprintf(stderr, "loginwindow: signal %d at %p\n", sig, info->si_addr);
    // the faulting instruction first, the backtrace below may not survive a corrupted stack
    if (dladdr(pc, &at) && at.dli_sname != NULL)
        fprintf(stderr, "  pc %s + %ld (%s)\n", at.dli_sname, (long)((char *)pc - (char *)at.dli_saddr), at.dli_fname);
    else
        fprintf(stderr, "  pc %p\n", pc);
    // callers from the saved frame pointers
    void **fp = (void **)uc->uc_mcontext->__ss.__rbp;

    for (int i = 0; i < 16 && fp != NULL && ((uintptr_t)fp & 7) == 0; i++) {
        void *ret = fp[1];

        if (dladdr(ret, &at) && at.dli_sname != NULL)
            fprintf(stderr, "  fp%-2d %s + %ld\n", i, at.dli_sname, (long)((char *)ret - (char *)at.dli_saddr));
        else
            fprintf(stderr, "  fp%-2d %p\n", i, ret);
        if ((void **)fp[0] <= fp)
            break;
        fp = (void **)fp[0];
    }
    fflush(stderr);
    for (int i = 0; i < count; i++) {
        Dl_info where;

        if (dladdr(frames[i], &where) && where.dli_sname != NULL)
            fprintf(stderr, "  %2d %s + %ld (%s)\n", i, where.dli_sname, (long)((char *)frames[i] - (char *)where.dli_saddr),
                    where.dli_fname ? where.dli_fname : "?");
        else
            fprintf(stderr, "  %2d %p\n", i, frames[i]);
    }
    signal(sig, SIG_DFL);
    raise(sig);
}


int main(int argc, const char *argv[])
{
    struct sigaction action = { 0 };
    stack_t altstack = { .ss_sp = malloc(1 << 16), .ss_size = 1 << 16 };

    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    NSApplication *app = [NSApplication sharedApplication];
    LoginController *controller = [[LoginController alloc] init];

    sigaltstack(&altstack, NULL);
    action.sa_sigaction = crashed;
    action.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigaction(SIGSEGV, &action, NULL);
    sigaction(SIGBUS, &action, NULL);
    sigaction(SIGABRT, &action, NULL);
    sigaction(SIGILL, &action, NULL);
    sigaction(SIGTRAP, &action, NULL);
    signal(SIGPIPE, SIG_IGN);
    [app setDelegate:controller];
    [app run];
    [controller release];
    [pool release];
    return 0;
}
