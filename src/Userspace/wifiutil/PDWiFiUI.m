/*
 * PDWiFiUI.m - the shared Wi-Fi screens, see PDWiFiUI.h.
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */
#import "PDWiFiUI.h"
#include <math.h>
#include <ctype.h>
#include <stdlib.h>
#include <dispatch/dispatch.h>
#if __has_include(<notify.h>)
#include <notify.h>
#else
// libSystem exports notify(3) but this sdk does not ship its header
uint32_t notify_post(const char *name);
uint32_t notify_register_dispatch(const char *name, int *out_token, dispatch_queue_t queue, void (^handler)(int token));
uint32_t notify_cancel(int token);
#endif
#include "pdwifi.h"

NSString *const PDWiFiChangedNotification = @"PDWiFiChangedNotification";

// every app listening refreshes when one of them changes something
static const char *const kNotifyName = "org.puredarwin.wifi.changed";

static NSColor *rgba(int r, int g, int b, CGFloat a)
{
    return [NSColor colorWithCalibratedRed:r / 255.0 green:g / 255.0 blue:b / 255.0 alpha:a];
}

static NSFont *uiFont(NSString *name, CGFloat size)
{
    return [NSFont fontWithName:name size:size] ?: [NSFont systemFontOfSize:size];
}

static NSDictionary *uiText(NSString *face, CGFloat size, NSColor *colour)
{
    return @{ NSFontAttributeName: uiFont(face, size), NSForegroundColorAttributeName: colour };
}

static BOOL uiDark(void)
{
    CFPropertyListRef style = CFPreferencesCopyAppValue(CFSTR("AppleInterfaceStyle"), kCFPreferencesAnyApplication);

    if (style == NULL)
        return NO;
    CFRelease(style);
    return YES;
}

// macOS's AppleAccentColor numbers and the demo's swatches, the table Settings uses
NSColor *PDSystemAccent(void)
{
    static const struct { int value, r, g, b; } accents[] = {
        { 4, 0x2f, 0x6f, 0xdb }, { 5, 0x9b, 0x4f, 0xd1 }, { 6, 0xe0, 0x45, 0x7b }, { 0, 0xe5, 0x48, 0x3f },
        { 1, 0xf0, 0x8a, 0x24 }, { 2, 0xf2, 0xc2, 0x30 }, { 3, 0x4c, 0xae, 0x4f }, { -1, 0x8c, 0x8c, 0x8c },
    };
    CFPropertyListRef v = CFPreferencesCopyAppValue(CFSTR("AppleAccentColor"), kCFPreferencesAnyApplication);
    int value = 4;

    if (v != NULL) {
        if (CFGetTypeID(v) == CFNumberGetTypeID())
            CFNumberGetValue((CFNumberRef)v, kCFNumberIntType, &value);
        else if (CFGetTypeID(v) == CFStringGetTypeID())
            value = [(NSString *)v intValue];
        CFRelease(v);
    }
    for (unsigned i = 0; i < sizeof(accents) / sizeof(accents[0]); i++) {
        if (accents[i].value == value)
            return rgba(accents[i].r, accents[i].g, accents[i].b, 1);
    }
    return rgba(0x2f, 0x6f, 0xdb, 1);
}

@implementation PDWiFiNetwork
- (int)bars
{
    return pdwifi_bars(_rssi);
}
- (void)dealloc
{
    [_ssid release];
    [_security release];
    [super dealloc];
}
@end

static PDWiFiNetwork *wrap(const pdwifi_network *n)
{
    PDWiFiNetwork *w = [[[PDWiFiNetwork alloc] init] autorelease];

    w.ssid = [NSString stringWithUTF8String:n->ssid];
    w.security = n->security ? [NSString stringWithUTF8String:n->security] : nil;
    w.rssi = n->rssi;
    w.channel = n->channel;
    w.known = pdwifi_is_known(n->ssid) != 0;
    return w;
}

@interface PDWiFi () {
    dispatch_queue_t _queue;
    int _token;
}
@property (readwrite) BOOL hardware;
@property (readwrite) BOOL powered;
@property (readwrite, retain) PDWiFiNetwork *current;
@property (readwrite, copy) NSArray *inRange;
@property (readwrite, copy) NSArray *known;
@property (readwrite, copy) NSString *busy;
@property (readwrite, copy) NSString *lastError;
@property (readwrite) BOOL scanning;
@property (readwrite, copy) NSString *interface;
@property (readwrite, copy) NSString *hardwareAddress;
- (void)join:(NSString *)ssid password:(NSString *)password remember:(BOOL)keep done:(void (^)(pdwifi_result))done;
@end

@implementation PDWiFi

+ (PDWiFi *)shared
{
    static PDWiFi *shared;
    static dispatch_once_t once;

    dispatch_once(&once, ^{
        shared = [[PDWiFi alloc] init];
    });
    return shared;
}

- (instancetype)init
{
    if ((self = [super init]) == nil)
        return nil;
    // one radio, one request at a time
    _queue = dispatch_queue_create("org.puredarwin.wifi", DISPATCH_QUEUE_SERIAL);
    _inRange = [[NSArray alloc] init];
    _known = [[NSArray alloc] init];
    notify_register_dispatch(kNotifyName, &_token, dispatch_get_main_queue(), ^(int t) {
        [self refresh];
    });
    [self refresh];
    return self;
}

- (void)changed
{
    [[NSNotificationCenter defaultCenter] postNotificationName:PDWiFiChangedNotification object:self];
}

- (void)tellOthers
{
    notify_post(kNotifyName);
}

// the cheap part: radio, link and the remembered list, no scan
- (void)refresh
{
    dispatch_async(_queue, ^{
        pdwifi_network cur;
        char names[PDWIFI_MAX_NETWORKS][33];
        uint8_t mac[6];
        const char *ifn = pdwifi_interface();
        BOOL hw = ifn != NULL, on = hw && pdwifi_power();
        BOOL linked = on && pdwifi_current(&cur);
        int n = pdwifi_known(names, PDWIFI_MAX_NETWORKS);
        NSMutableArray *known = [NSMutableArray array];
        NSString *mac_s = hw && pdwifi_hwaddr(mac) ? [NSString stringWithFormat:@"%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1],
                                                       mac[2], mac[3], mac[4], mac[5]] : nil;
        NSString *ifs = ifn ? [NSString stringWithUTF8String:ifn] : nil;
        PDWiFiNetwork *c = linked ? wrap(&cur) : nil;

        for (int i = 0; i < n; i++)
            [known addObject:[NSString stringWithUTF8String:names[i]]];
        dispatch_async(dispatch_get_main_queue(), ^{
            self.hardware = hw;
            self.powered = on;
            self.current = c;
            self.known = known;
            self.interface = ifs;
            self.hardwareAddress = mac_s;
            [self changed];
        });
    });
}

- (void)scan
{
    if (_scanning)
        return;
    self.scanning = YES;
    [self changed];
    dispatch_async(_queue, ^{
        pdwifi_network nets[PDWIFI_MAX_NETWORKS];
        int n = pdwifi_scan(nets, PDWIFI_MAX_NETWORKS);
        NSMutableArray *found = [NSMutableArray array];

        for (int i = 0; i < n; i++)
            [found addObject:wrap(&nets[i])];
        dispatch_async(dispatch_get_main_queue(), ^{
            self.inRange = found;
            self.scanning = NO;
            [self refresh];
        });
    });
}

- (void)setPower:(BOOL)on
{
    self.powered = on;
    if (!on) {
        self.current = nil;
        self.inRange = @[];
    }
    [self changed];
    dispatch_async(_queue, ^{
        pdwifi_set_power(on);
        dispatch_async(dispatch_get_main_queue(), ^{
            [self tellOthers];
            if (on)
                [self scan];
        });
        if (on)
            pdwifi_autojoin();
        dispatch_async(dispatch_get_main_queue(), ^{
            [self tellOthers];
        });
    });
}

- (void)leave
{
    self.current = nil;
    [self changed];
    dispatch_async(_queue, ^{
        pdwifi_leave();
        dispatch_async(dispatch_get_main_queue(), ^{
            [self tellOthers];
            [self scan];
        });
    });
}

- (void)forget:(NSString *)ssid
{
    NSString *s = [[ssid copy] autorelease];
    dispatch_async(_queue, ^{
        if (self.current && [self.current.ssid isEqualToString:s])
            pdwifi_leave();
        pdwifi_forget([s UTF8String]);
        dispatch_async(dispatch_get_main_queue(), ^{
            [self tellOthers];
        });
    });
}

// joins on the queue, the sheet or the caller hears the result
- (void)join:(NSString *)ssid password:(NSString *)password remember:(BOOL)keep done:(void (^)(pdwifi_result))done
{
    NSString *s = [[ssid copy] autorelease], *p = [[password copy] autorelease];
    void (^finish)(pdwifi_result) = [[done copy] autorelease];

    self.busy = s;
    self.lastError = nil;
    [self changed];
    dispatch_async(_queue, ^{
        pdwifi_result r = p == nil && pdwifi_is_known([s UTF8String]) ? pdwifi_join_known([s UTF8String])
                                                                        : pdwifi_join([s UTF8String], [p UTF8String], keep);
        dispatch_async(dispatch_get_main_queue(), ^{
            self.busy = nil;
            if (r != PDWIFI_OK)
                self.lastError = [NSString stringWithUTF8String:pdwifi_message(r)];
            if (finish)
                finish(r);
            [self tellOthers];
        });
    });
}

- (void)pick:(PDWiFiNetwork *)network accent:(NSColor *)accent
{
    if (network == nil || _busy != nil || (_current && [_current.ssid isEqualToString:network.ssid]))
        return;
    if (network.security != nil && !network.known) {
        [PDWiFiJoinPanel showForNetwork:network accent:accent];
        return;
    }
    [self join:network.ssid password:nil remember:YES done:nil];
}

- (void)joinOther:(NSColor *)accent
{
    [PDWiFiJoinPanel showForNetwork:nil accent:accent];
}

- (void)dealloc
{
    notify_cancel(_token);
    [_current release];
    [_inRange release];
    [_known release];
    [_busy release];
    [_lastError release];
    [_interface release];
    [_hardwareAddress release];
    [super dealloc];
}

@end

// the wi-fi glyph from the trenta icon set (CC-BY-SA-4.0), the demo's four paths in a 256 unit box
static const char *const kWiFiPaths[4] = {
    "M109.6,199.6L109.6,199.6L128,218l18.4-18.4C136.2,189.4,119.8,189.1,109.6,199.6z",
    "M73.4,163.4l18.1,18.1c20.1-20.1,52.6-20.1,72.7,0l18.1-18.1C152.3,133.4,103.7,133.4,73.4,163.4z",
    "M37.2,127.2l18.1,18.1c40.2-40.2,105.2-40.2,145.4,0l18.1-18.1C168.7,76.8,87.3,76.8,37.2,127.2z",
    "M0.7,90.7l18.1,18.1c60.3-60,158.2-60,218.4,0l18.1-18.1C184.9,20.5,71.1,20.5,0.7,90.7z",
};

// the few path commands those use: M L l C c z, absolute and relative
static NSBezierPath *svgPath(const char *d, NSRect r, BOOL flipped)
{
    NSBezierPath *p = [NSBezierPath bezierPath];
    CGFloat sx = NSWidth(r) / 256, sy = NSHeight(r) / 256, x = 0, y = 0;
    char cmd = 0;
    double v[6];
#define PT(px, py) NSMakePoint(NSMinX(r) + (px) * sx, flipped ? NSMinY(r) + (py) * sy : NSMaxY(r) - (py) * sy)

    while (*d) {
        int n, need;
        BOOL rel;

        if (isalpha((unsigned char)*d))
            cmd = *d++;
        if (cmd == 'z' || cmd == 'Z') {
            [p closePath];
            cmd = 0;
            continue;
        }
        need = (cmd == 'C' || cmd == 'c') ? 6 : 2;
        for (n = 0; n < need; n++) {
            char *end;
            while (*d == ',' || *d == ' ')
                d++;
            v[n] = strtod(d, &end);
            if (end == d)
                break;
            d = end;
        }
        if (n < need)
            break;
        rel = islower((unsigned char)cmd);
        if (rel) {
            for (int k = 0; k < need; k += 2) {
                v[k] += x;
                v[k + 1] += y;
            }
        }
        if (cmd == 'M' || cmd == 'm') {
            [p moveToPoint:PT(v[0], v[1])];
            cmd = rel ? 'l' : 'L';
        } else if (cmd == 'L' || cmd == 'l') {
            [p lineToPoint:PT(v[0], v[1])];
        } else {
            [p curveToPoint:PT(v[4], v[5]) controlPoint1:PT(v[0], v[1]) controlPoint2:PT(v[2], v[3])];
        }
        x = v[need - 2];
        y = v[need - 1];
    }
#undef PT
    return p;
}

// the demo's wifiSvg: the dot and the arcs up to the level solid, the rest at a quarter, level 0 lights the first arc
void PDDrawWiFiGlyph(NSRect r, int level, NSColor *colour, BOOL flipped)
{
    CGFloat s = MIN(NSWidth(r), NSHeight(r));
    NSRect box = NSMakeRect(NSMidX(r) - s / 2, NSMidY(r) - s / 2, s, s);
    int lit = MAX(1, MIN(3, level));

    for (int k = 0; k < 4; k++) {
        [[colour colorWithAlphaComponent:[colour alphaComponent] * (k <= lit ? 1 : 0.25)] setFill];
        [svgPath(kWiFiPaths[k], box, flipped) fill];
    }
}

void PDDrawWiFiLock(NSRect r, NSColor *colour, BOOL flipped)
{
    CGFloat w = MIN(NSWidth(r), NSHeight(r) * 0.84), x = NSMidX(r) - w / 2;
    CGFloat bodyH = w * 0.72, top = flipped ? NSMaxY(r) - bodyH : NSMinY(r);
    NSBezierPath *shackle = [NSBezierPath bezierPath];
    CGFloat sy = flipped ? top : top + bodyH, rad = w * 0.28, dir = flipped ? -1 : 1;

    [colour setFill];
    [[NSBezierPath bezierPathWithRoundedRect:NSMakeRect(x, top, w, bodyH) xRadius:w * 0.16 yRadius:w * 0.16] fill];
    [shackle moveToPoint:NSMakePoint(NSMidX(r) - rad, sy)];
    [shackle lineToPoint:NSMakePoint(NSMidX(r) - rad, sy + dir * rad * 0.6)];
    for (int i = 0; i <= 12; i++) {
        CGFloat t = M_PI - M_PI * i / 12;
        [shackle lineToPoint:NSMakePoint(NSMidX(r) + rad * cos(t), sy + dir * (rad * 0.6 + rad * sin(t)))];
    }
    [shackle lineToPoint:NSMakePoint(NSMidX(r) + rad, sy)];
    [shackle setLineWidth:w * 0.15];
    [colour setStroke];
    [shackle stroke];
}

void PDDrawWiFiSpinner(NSRect r, NSColor *colour)
{
    CGFloat s = MIN(NSWidth(r), NSHeight(r)) / 2 - 1.5;
    CGFloat a = fmod([NSDate timeIntervalSinceReferenceDate] * 450, 360) * M_PI / 180;
    NSBezierPath *ring = [NSBezierPath bezierPathWithOvalInRect:NSInsetRect(r, 1.5, 1.5)];
    NSBezierPath *arc = [NSBezierPath bezierPath];

    [ring setLineWidth:2];
    [[colour colorWithAlphaComponent:0.2] setStroke];
    [ring stroke];
    for (int i = 0; i <= 10; i++) {
        CGFloat t = a + M_PI / 2 * i / 10;
        NSPoint p = NSMakePoint(NSMidX(r) + s * cos(t), NSMidY(r) + s * sin(t));
        if (i == 0)
            [arc moveToPoint:p];
        else
            [arc lineToPoint:p];
    }
    [arc setLineWidth:2];
    [arc setLineCapStyle:NSRoundLineCapStyle];
    [colour setStroke];
    [arc stroke];
}

// the sheet draws its own fields and takes typing itself
@interface PDWiFiJoinView : NSView {
@public
    PDWiFiNetwork *_network;
    NSColor *_accent;
    NSMutableString *_name, *_password;
    int _focus;
    BOOL _secure, _show, _keep, _joining;
    NSString *_error;
    NSTimer *_spin;
    NSRect _nameBox, _passBox, _secRects[2], _showBox, _keepBox, _cancelRect, _joinRect;
}
@end

@implementation PDWiFiJoinView

- (BOOL)isFlipped
{
    return YES;
}

- (BOOL)acceptsFirstResponder
{
    return YES;
}

- (BOOL)other
{
    return _network == nil;
}

- (BOOL)needsPassword
{
    return [self other] ? _secure : YES;
}

- (BOOL)canJoin
{
    if (_joining)
        return NO;
    if ([self other] && [[_name stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]] length] == 0)
        return NO;
    return ![self needsPassword] || ([_password length] >= 8 && [_password length] <= 63);
}

- (NSColor *)fg
{
    return uiDark() ? rgba(0xee, 0xee, 0xee, 1) : rgba(0x1b, 0x1b, 0x1b, 1);
}

- (NSColor *)muted
{
    return uiDark() ? rgba(0x9a, 0x93, 0x95, 1) : rgba(0x6e, 0x6e, 0x6e, 1);
}

- (void)field:(NSRect)r text:(NSString *)t focused:(BOOL)f
{
    NSBezierPath *p = [NSBezierPath bezierPathWithRoundedRect:r xRadius:6 yRadius:6];
    NSDictionary *a = uiText(@"Inter-Regular", 12.5, [self fg]);

    [(uiDark() ? rgba(0x1f, 0x1d, 0x1f, 1) : [NSColor whiteColor]) setFill];
    [p fill];
    [p setLineWidth:f ? 2 : 1];
    [(f ? [_accent colorWithAlphaComponent:0.7] : rgba(0, 0, 0, uiDark() ? 0.5 : 0.18)) setStroke];
    [p stroke];
    [t drawAtPoint:NSMakePoint(NSMinX(r) + 8, NSMinY(r) + 5) withAttributes:a];
    if (f) {
        CGFloat x = NSMinX(r) + 9 + [t sizeWithAttributes:a].width;
        [[self fg] setFill];
        NSRectFill(NSMakeRect(x, NSMinY(r) + 6, 1, NSHeight(r) - 12));
    }
}

- (void)check:(NSRect)r label:(NSString *)l on:(BOOL)on
{
    NSRect box = NSMakeRect(NSMinX(r), NSMinY(r) + 2, 14, 14);
    NSBezierPath *p = [NSBezierPath bezierPathWithRoundedRect:box xRadius:3.5 yRadius:3.5];

    [(on ? _accent : (uiDark() ? rgba(0x1f, 0x1d, 0x1f, 1) : [NSColor whiteColor])) setFill];
    [p fill];
    if (!on) {
        [rgba(0, 0, 0, uiDark() ? 0.5 : 0.25) setStroke];
        [p stroke];
    } else {
        NSBezierPath *m = [NSBezierPath bezierPath];
        [m moveToPoint:NSMakePoint(NSMinX(box) + 3.5, NSMinY(box) + 7.5)];
        [m lineToPoint:NSMakePoint(NSMinX(box) + 6, NSMinY(box) + 10)];
        [m lineToPoint:NSMakePoint(NSMinX(box) + 10.5, NSMinY(box) + 4)];
        [m setLineWidth:1.6];
        [[NSColor whiteColor] setStroke];
        [m stroke];
    }
    [l drawAtPoint:NSMakePoint(NSMinX(r) + 22, NSMinY(r) + 1) withAttributes:uiText(@"Inter-Regular", 12, [self fg])];
}

- (NSRect)button:(NSString *)t right:(CGFloat)right y:(CGFloat)y go:(BOOL)go enabled:(BOOL)en
{
    NSDictionary *a = uiText(@"Inter-Regular", 13, go ? [NSColor whiteColor] : [self fg]);
    NSSize sz = [t sizeWithAttributes:a];
    NSRect r = NSMakeRect(right - MAX(80, sz.width + 28), y, MAX(80, sz.width + 28), 26);
    NSBezierPath *p = [NSBezierPath bezierPathWithRoundedRect:r xRadius:6 yRadius:6];
    NSColor *fill = go ? _accent : (uiDark() ? rgba(0x3a, 0x35, 0x37, 1) : [NSColor whiteColor]);

    [(en ? fill : [fill colorWithAlphaComponent:0.45]) setFill];
    [p fill];
    if (!go) {
        [rgba(0, 0, 0, 0.15) setStroke];
        [p stroke];
    }
    [t drawAtPoint:NSMakePoint(floor(NSMidX(r) - sz.width / 2), floor(NSMidY(r) - sz.height / 2))
     withAttributes:en ? a : uiText(@"Inter-Regular", 13, [a[NSForegroundColorAttributeName] colorWithAlphaComponent:0.6])];
    return r;
}

- (void)drawRect:(NSRect)dirty
{
    NSRect b = [self bounds];
    CGFloat y, lx = 22, fx = 120, fw = NSWidth(b) - fx - 22;
    NSDictionary *label = uiText(@"Inter-Regular", 12, [self fg]);
    NSString *title = [self other] ? @"Find and join a Wi-Fi network."
                                   : [NSString stringWithFormat:@"The Wi-Fi network “%@” requires a %@ password.", _network.ssid,
                                                                _network.security ?: @"WPA2"];
    NSString *sub = [self other] ? @"Enter the name and security type of the network you want to join."
                                 : @"Enter the password to join this network.";

    [(uiDark() ? rgba(0x2c, 0x29, 0x2b, 1) : rgba(0xf6, 0xf2, 0xef, 1)) setFill];
    NSRectFill(b);

    NSRect badge = NSMakeRect(22, 34, 46, 46);
    [_accent setFill];
    [[NSBezierPath bezierPathWithRoundedRect:badge xRadius:12 yRadius:12] fill];
    PDDrawWiFiGlyph(NSInsetRect(badge, 10, 10), 3, [NSColor whiteColor], YES);
    // the title wraps at word breaks onto a second line when a long network name needs it
    NSDictionary *ta = uiText(@"Inter-SemiBold", 13, [self fg]);
    NSMutableString *line = [NSMutableString string];
    CGFloat ty = 34, tw = NSWidth(b) - 82 - 22;
    for (NSString *word in [title componentsSeparatedByString:@" "]) {
        NSString *next = [line length] ? [NSString stringWithFormat:@"%@ %@", line, word] : word;
        if ([line length] && [next sizeWithAttributes:ta].width > tw) {
            [line drawAtPoint:NSMakePoint(82, ty) withAttributes:ta];
            ty += 18;
            [line setString:word];
        } else {
            [line setString:next];
        }
    }
    [line drawAtPoint:NSMakePoint(82, ty) withAttributes:ta];
    // the line under it wraps the same way
    NSDictionary *sa = uiText(@"Inter-Regular", 12, [self muted]);
    [line setString:@""];
    ty += 20;
    for (NSString *word in [sub componentsSeparatedByString:@" "]) {
        NSString *next = [line length] ? [NSString stringWithFormat:@"%@ %@", line, word] : word;
        if ([line length] && [next sizeWithAttributes:sa].width > tw) {
            [line drawAtPoint:NSMakePoint(82, ty) withAttributes:sa];
            ty += 16;
            [line setString:word];
        } else {
            [line setString:next];
        }
    }
    [line drawAtPoint:NSMakePoint(82, ty) withAttributes:sa];
    y = MAX(96, ty + 30);

    if ([self other]) {
        [@"Network Name" drawAtPoint:NSMakePoint(lx, y + 5) withAttributes:label];
        _nameBox = NSMakeRect(fx, y, fw, 26);
        [self field:_nameBox text:_name focused:_focus == 0];
        y += 36;
        [@"Security" drawAtPoint:NSMakePoint(lx, y + 4) withAttributes:label];
        NSString *opts[2] = { @"None", @"WPA2 Personal" };
        CGFloat x = fx;
        for (int i = 0; i < 2; i++) {
            NSDictionary *a = uiText(@"Inter-Regular", 12, [self fg]);
            CGFloat w = [opts[i] sizeWithAttributes:a].width + 24;
            _secRects[i] = NSMakeRect(x, y, w, 24);
            if ((i == 1) == _secure) {
                [(uiDark() ? rgba(0x57, 0x51, 0x53, 1) : [NSColor whiteColor]) setFill];
                [[NSBezierPath bezierPathWithRoundedRect:_secRects[i] xRadius:6 yRadius:6] fill];
            }
            [opts[i] drawAtPoint:NSMakePoint(x + 12, y + 4) withAttributes:a];
            x += w + 4;
        }
        y += 34;
    }
    _passBox = NSZeroRect;
    if ([self needsPassword]) {
        NSString *shown = _show ? _password : [@"" stringByPaddingToLength:[_password length] withString:@"•" startingAtIndex:0];
        [@"Password" drawAtPoint:NSMakePoint(lx, y + 5) withAttributes:label];
        _passBox = NSMakeRect(fx, y, fw, 26);
        [self field:_passBox text:shown focused:_focus == 1 || ![self other]];
        y += 34;
        _showBox = NSMakeRect(fx, y, 160, 20);
        [self check:_showBox label:@"Show password" on:_show];
        y += 24;
    }
    _keepBox = NSMakeRect(fx, y, 200, 20);
    [self check:_keepBox label:@"Remember this network" on:_keep];
    y += 26;
    if (_error)
        [_error drawAtPoint:NSMakePoint(fx, y) withAttributes:uiText(@"Inter-Regular", 12, rgba(0xd9, 0x4f, 0x45, 1))];

    CGFloat by = NSHeight(b) - 44;
    _joinRect = [self button:@"Join" right:NSWidth(b) - 22 y:by go:YES enabled:[self canJoin]];
    _cancelRect = [self button:@"Cancel" right:NSMinX(_joinRect) - 8 y:by go:NO enabled:YES];
    if (_joining) {
        PDDrawWiFiSpinner(NSMakeRect(22, by + 6, 14, 14), _accent);
        [@"Joining…" drawAtPoint:NSMakePoint(42, by + 5) withAttributes:uiText(@"Inter-Regular", 12, [self muted])];
    }
}

- (void)tick:(NSTimer *)t
{
    [self setNeedsDisplay:YES];
}

- (void)close
{
    [_spin invalidate];
    _spin = nil;
    [[self window] orderOut:nil];
    [[self window] autorelease];
}

- (void)join
{
    NSString *ssid = [self other] ? [_name stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]] : _network.ssid;

    if (![self canJoin])
        return;
    _joining = YES;
    [_error release];
    _error = nil;
    _spin = [NSTimer scheduledTimerWithTimeInterval:0.05 target:self selector:@selector(tick:) userInfo:nil repeats:YES];
    [[PDWiFi shared] join:ssid password:[self needsPassword] ? _password : nil remember:_keep done:^(pdwifi_result r) {
        [_spin invalidate];
        _spin = nil;
        _joining = NO;
        if (r == PDWIFI_OK) {
            [self close];
            return;
        }
        _error = [[NSString stringWithUTF8String:pdwifi_message(r)] retain];
        // a wrong password is typed again from scratch
        if (r == PDWIFI_BAD_PASSWORD)
            [_password setString:@""];
        _focus = 1;
        [self setNeedsDisplay:YES];
    }];
    [self setNeedsDisplay:YES];
}

- (void)mouseDown:(NSEvent *)e
{
    NSPoint p = [self convertPoint:[e locationInWindow] fromView:nil];

    if (_joining)
        return;
    if (NSPointInRect(p, _cancelRect))
        [self close];
    else if (NSPointInRect(p, _joinRect))
        [self join];
    else if (NSPointInRect(p, _nameBox))
        _focus = 0;
    else if (NSPointInRect(p, _passBox))
        _focus = 1;
    else if ([self other] && NSPointInRect(p, _secRects[0]))
        _secure = NO;
    else if ([self other] && NSPointInRect(p, _secRects[1]))
        _secure = YES;
    else if (NSPointInRect(p, _showBox) && [self needsPassword])
        _show = !_show;
    else if (NSPointInRect(p, _keepBox))
        _keep = !_keep;
    [self setNeedsDisplay:YES];
}

- (void)keyDown:(NSEvent *)e
{
    NSString *k = [e characters];
    unichar c = [k length] ? [k characterAtIndex:0] : 0;
    NSMutableString *f = (_focus == 0 && [self other]) ? _name : _password;

    if (_joining)
        return;
    if (c == '\r' || c == '\n' || c == 3)
        [self join];
    else if (c == 27)
        [self close];
    else if (c == '\t' && [self other])
        _focus = [self needsPassword] ? !_focus : 0;
    else if (c == 0x7f || c == 8) {
        if ([f length])
            [f deleteCharactersInRange:NSMakeRange([f length] - 1, 1)];
    } else if (c >= 0x20 && c < 0xF700 && [f length] < (f == _name ? 32 : 63))
        [f appendString:k];
    [self setNeedsDisplay:YES];
}

- (void)dealloc
{
    [_network release];
    [_accent release];
    [_name release];
    [_password release];
    [_error release];
    [super dealloc];
}

@end

@implementation PDWiFiJoinPanel

+ (void)showForNetwork:(PDWiFiNetwork *)network accent:(NSColor *)accent
{
    // room under the checkboxes for an error line above the buttons
    CGFloat h = network == nil ? 362 : 276;
    NSRect screen = [[NSScreen mainScreen] visibleFrame];
    NSRect frame = NSMakeRect(NSMidX(screen) - 220, NSMaxY(screen) - NSHeight(screen) * 0.22 - h, 440, h);
    NSWindow *w = [[NSWindow alloc] initWithContentRect:frame
                                              styleMask:NSTitledWindowMask | NSFullSizeContentViewWindowMask
                                                backing:NSBackingStoreBuffered
                                                  defer:NO];
    PDWiFiJoinView *v = [[PDWiFiJoinView alloc] initWithFrame:NSMakeRect(0, 0, 440, h)];

    v->_network = [network retain];
    v->_accent = [accent retain];
    v->_name = [[NSMutableString alloc] init];
    v->_password = [[NSMutableString alloc] init];
    v->_focus = network == nil ? 0 : 1;
    v->_secure = YES;
    v->_keep = YES;
    [w setTitle:@"Join a Wi-Fi Network"];
    [w setTitlebarAppearsTransparent:YES];
    [w setTitleVisibility:NSWindowTitleHidden];
    [w setLevel:NSFloatingWindowLevel];
    // a full-size content view covers the title bar, so the frame is h and the lights sit on the sheet
    NSRect f = [w frame];
    f.origin.y += NSHeight(f) - h;
    f.size.height = h;
    [w setFrame:f display:NO];
    [w setContentView:v];
    [v setFrame:NSMakeRect(0, 0, 440, h)];
    [w makeFirstResponder:v];
    [v release];
    [NSApp activateIgnoringOtherApps:YES];
    [w makeKeyAndOrderFront:nil];
}

@end
