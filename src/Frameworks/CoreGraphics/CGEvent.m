/*
 * Quartz Event Services
 * Copyright (C) 2024 Zoe Knox <zoe@ravynsoft.com>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#import <CoreGraphics/CGEvent.h>
#import <CoreGraphics/CGDirectDisplay.h>
#import <objc/runtime.h>
#import <objc/message.h>
#include <mach/mach_time.h>
#include <stdarg.h>
#include <dlfcn.h>

// an event, and an event source, is a mutable dictionary of its values, so CFRetain, CFRelease and CFAutorelease work on it
// local, remote mouse and remote keyboard events
#define PDFilterMaskPermitAllEvents 7

#define PD_KEY_TYPE CFSTR("type")
#define PD_KEY_LOCATION_X CFSTR("x")
#define PD_KEY_LOCATION_Y CFSTR("y")
#define PD_KEY_FLAGS CFSTR("flags")
#define PD_KEY_TIMESTAMP CFSTR("timestamp")
#define PD_KEY_STRING CFSTR("string")
#define PD_KEY_INTEGERS CFSTR("integers")
#define PD_KEY_DOUBLES CFSTR("doubles")
#define PD_KEY_SOURCE_STATE CFSTR("sourceState")
#define PD_KEY_KEYBOARD CFSTR("keyboardType")
#define PD_KEY_PIXELS_PER_LINE CFSTR("pixelsPerLine")
#define PD_KEY_SUPPRESSION CFSTR("suppression")

static CFMutableDictionaryRef PDDict(const void *ref)
{
    return (CFMutableDictionaryRef)ref;
}

static int64_t PDGetInt(CFDictionaryRef dict, CFStringRef key, int64_t fallback)
{
    CFNumberRef value = dict ? CFDictionaryGetValue(dict, key) : NULL;
    int64_t result = fallback;

    if (value != NULL)
        CFNumberGetValue(value, kCFNumberSInt64Type, &result);
    return result;
}

static void PDSetInt(CFMutableDictionaryRef dict, CFStringRef key, int64_t value)
{
    CFNumberRef number = CFNumberCreate(NULL, kCFNumberSInt64Type, &value);

    CFDictionarySetValue(dict, key, number);
    CFRelease(number);
}

static double PDGetDouble(CFDictionaryRef dict, CFStringRef key, double fallback)
{
    CFNumberRef value = dict ? CFDictionaryGetValue(dict, key) : NULL;
    double result = fallback;

    if (value != NULL)
        CFNumberGetValue(value, kCFNumberDoubleType, &result);
    return result;
}

static void PDSetDouble(CFMutableDictionaryRef dict, CFStringRef key, double value)
{
    CFNumberRef number = CFNumberCreate(NULL, kCFNumberDoubleType, &value);

    CFDictionarySetValue(dict, key, number);
    CFRelease(number);
}

static CFMutableDictionaryRef PDCreateDict(void)
{
    return CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
}

// a field table inside the event, created on first use
static CFMutableDictionaryRef PDFieldTable(CGEventRef event, CFStringRef which, bool create)
{
    CFMutableDictionaryRef table = (CFMutableDictionaryRef)CFDictionaryGetValue(PDDict(event), which);

    if (table == NULL && create) {
        table = PDCreateDict();
        CFDictionarySetValue(PDDict(event), which, table);
        CFRelease(table);
    }
    return table;
}

static CFNumberRef PDFieldKey(CGEventField field)
{
    int32_t value = (int32_t)field;

    return CFNumberCreate(NULL, kCFNumberSInt32Type, &value);
}

// the AppKit event state, when AppKit is loaded, is the session's input state
static uint64_t PDAppKitUnsigned(const char *selector)
{
    Class event = objc_getClass("NSEvent");

    if (event == Nil || !class_respondsToSelector(object_getClass((id)event), sel_registerName(selector)))
        return 0;
    return ((unsigned long (*)(id, SEL))objc_msgSend)((id)event, sel_registerName(selector));
}

static CGPoint PDCurrentLocation(void)
{
    Class event = objc_getClass("NSEvent");

    if (event == Nil)
        return CGPointZero;
    return ((CGPoint (*)(id, SEL))objc_msgSend)((id)event, sel_registerName("mouseLocation"));
}

CFTypeID CGEventGetTypeID(void) {
    return CFDictionaryGetTypeID();
}

CGEventRef CGEventCreate(CGEventSourceRef source) {
    CFMutableDictionaryRef event = PDCreateDict();
    CGPoint location = PDCurrentLocation();

    PDSetInt(event, PD_KEY_TYPE, kCGEventNull);
    PDSetInt(event, PD_KEY_TIMESTAMP, (int64_t)mach_absolute_time());
    PDSetDouble(event, PD_KEY_LOCATION_X, location.x);
    PDSetDouble(event, PD_KEY_LOCATION_Y, location.y);
    PDSetInt(event, PD_KEY_FLAGS, (int64_t)CGEventSourceFlagsState(kCGEventSourceStateCombinedSessionState));
    if (source != NULL)
        CGEventSetIntegerValueField((CGEventRef)event, kCGEventSourceUserData, CGEventSourceGetUserData(source));
    return (CGEventRef)event;
}

// the serialized form is the dictionary as a binary property list
CFDataRef CGEventCreateData(CFAllocatorRef allocator, CGEventRef event) {
    if (event == NULL)
        return NULL;
    return CFPropertyListCreateData(allocator, PDDict(event), kCFPropertyListBinaryFormat_v1_0, 0, NULL);
}

CGEventRef CGEventCreateFromData(CFAllocatorRef allocator, CFDataRef data) {
    CFPropertyListRef plist;

    if (data == NULL)
        return NULL;
    plist = CFPropertyListCreateWithData(allocator, data, kCFPropertyListMutableContainers, NULL, NULL);
    if (plist != NULL && CFGetTypeID(plist) != CFDictionaryGetTypeID()) {
        CFRelease(plist);
        return NULL;
    }
    return (CGEventRef)plist;
}

CGEventRef CGEventCreateMouseEvent(CGEventSourceRef source, CGEventType mouseType, CGPoint mouseCursorPosition, CGMouseButton mouseButton) {
    CGEventRef event = CGEventCreate(source);

    CGEventSetType(event, mouseType);
    CGEventSetLocation(event, mouseCursorPosition);
    CGEventSetIntegerValueField(event, kCGMouseEventButtonNumber, mouseButton);
    if (mouseType != kCGEventMouseMoved)
        CGEventSetIntegerValueField(event, kCGMouseEventClickState, 1);
    return event;
}

CGEventRef CGEventCreateKeyboardEvent(CGEventSourceRef source, CGKeyCode virtualKey, bool keyDown) {
    CGEventRef event = CGEventCreate(source);

    CGEventSetType(event, keyDown ? kCGEventKeyDown : kCGEventKeyUp);
    CGEventSetIntegerValueField(event, kCGKeyboardEventKeycode, virtualKey);
    return event;
}

// line deltas are given, the pixel and fixed-point deltas follow from them at the source's line height
CGEventRef CGEventCreateScrollWheelEvent(CGEventSourceRef source, CGScrollEventUnit units, uint32_t wheelCount, int32_t wheel1, ...) {
    static const CGEventField deltas[3] = { kCGScrollWheelEventDeltaAxis1, kCGScrollWheelEventDeltaAxis2, kCGScrollWheelEventDeltaAxis3 };
    static const CGEventField points[3] = { kCGScrollWheelEventPointDeltaAxis1, kCGScrollWheelEventPointDeltaAxis2, kCGScrollWheelEventPointDeltaAxis3 };
    static const CGEventField fixed[3] = { kCGScrollWheelEventFixedPtDeltaAxis1, kCGScrollWheelEventFixedPtDeltaAxis2, kCGScrollWheelEventFixedPtDeltaAxis3 };
    double lineHeight = source ? CGEventSourceGetPixelsPerLine(source) : 10.0;
    CGEventRef event = CGEventCreate(source);
    int32_t wheel[3] = { wheel1, 0, 0 };
    va_list args;

    va_start(args, wheel1);
    for (uint32_t i = 1; i < wheelCount && i < 3; i++)
        wheel[i] = va_arg(args, int32_t);
    va_end(args);

    CGEventSetType(event, kCGEventScrollWheel);
    CGEventSetIntegerValueField(event, kCGScrollWheelEventIsContinuous, units == kCGScrollEventUnitPixel);
    for (uint32_t i = 0; i < 3; i++) {
        double lines = units == kCGScrollEventUnitPixel ? wheel[i] / lineHeight : wheel[i];
        double pixels = units == kCGScrollEventUnitPixel ? wheel[i] : wheel[i] * lineHeight;
        CGEventSetIntegerValueField(event, deltas[i], (int64_t)lines);
        CGEventSetIntegerValueField(event, points[i], (int64_t)pixels);
        CGEventSetDoubleValueField(event, fixed[i], lines);
    }
    return event;
}

CGEventRef CGEventCreateCopy(CGEventRef event) {
    if (event == NULL)
        return NULL;
    return (CGEventRef)CFPropertyListCreateDeepCopy(NULL, PDDict(event), kCFPropertyListMutableContainers);
}

CGEventSourceRef CGEventCreateSourceFromEvent(CGEventRef event) {
    CGEventSourceRef source = CGEventSourceCreate(kCGEventSourceStateHIDSystemState);

    if (event != NULL)
        CGEventSourceSetUserData(source, CGEventGetIntegerValueField(event, kCGEventSourceUserData));
    return source;
}

void CGEventSetSource(CGEventRef event, CGEventSourceRef source) {
    if (event != NULL && source != NULL)
        CGEventSetIntegerValueField(event, kCGEventSourceUserData, CGEventSourceGetUserData(source));
}

CGEventType CGEventGetType(CGEventRef event) {
    return (CGEventType)PDGetInt(PDDict(event), PD_KEY_TYPE, kCGEventNull);
}

void CGEventSetType(CGEventRef event, CGEventType type) {
    if (event != NULL)
        PDSetInt(PDDict(event), PD_KEY_TYPE, type);
}

CGEventTimestamp CGEventGetTimestamp(CGEventRef event) {
    return (CGEventTimestamp)PDGetInt(PDDict(event), PD_KEY_TIMESTAMP, 0);
}

void CGEventSetTimestamp(CGEventRef event, CGEventTimestamp timestamp) {
    if (event != NULL)
        PDSetInt(PDDict(event), PD_KEY_TIMESTAMP, (int64_t)timestamp);
}

// locations are global display coordinates, origin top-left
CGPoint CGEventGetLocation(CGEventRef event) {
    return CGPointMake(PDGetDouble(PDDict(event), PD_KEY_LOCATION_X, 0), PDGetDouble(PDDict(event), PD_KEY_LOCATION_Y, 0));
}

// the unflipped location has its origin at the bottom-left of the main display
CGPoint CGEventGetUnflippedLocation(CGEventRef event) {
    CGPoint location = CGEventGetLocation(event);
    CGRect main = CGDisplayBounds(CGMainDisplayID());

    return CGPointMake(location.x, CGRectGetHeight(main) - location.y);
}

void CGEventSetLocation(CGEventRef event, CGPoint location) {
    if (event == NULL)
        return;
    PDSetDouble(PDDict(event), PD_KEY_LOCATION_X, location.x);
    PDSetDouble(PDDict(event), PD_KEY_LOCATION_Y, location.y);
}

CGEventFlags CGEventGetFlags(CGEventRef event) {
    return (CGEventFlags)PDGetInt(PDDict(event), PD_KEY_FLAGS, 0);
}

void CGEventSetFlags(CGEventRef event, CGEventFlags flags) {
    if (event != NULL)
        PDSetInt(PDDict(event), PD_KEY_FLAGS, (int64_t)flags);
}

void CGEventKeyboardGetUnicodeString(CGEventRef event, UniCharCount maxStringLength, UniCharCount *actualStringLength, UniChar *unicodeString) {
    CFStringRef string = event ? CFDictionaryGetValue(PDDict(event), PD_KEY_STRING) : NULL;
    CFIndex length = string ? CFStringGetLength(string) : 0;

    if (unicodeString != NULL && length > 0)
        CFStringGetCharacters(string, CFRangeMake(0, length < (CFIndex)maxStringLength ? length : (CFIndex)maxStringLength), unicodeString);
    if (actualStringLength != NULL)
        *actualStringLength = (UniCharCount)length;
}

void CGEventKeyboardSetUnicodeString(CGEventRef event, UniCharCount stringLength, const UniChar *unicodeString) {
    CFStringRef string;

    if (event == NULL)
        return;
    string = CFStringCreateWithCharacters(NULL, unicodeString, (CFIndex)stringLength);
    CFDictionarySetValue(PDDict(event), PD_KEY_STRING, string);
    CFRelease(string);
}

// a field set as a double reads back as an integer, and the other way round, as on macOS
int64_t CGEventGetIntegerValueField(CGEventRef event, CGEventField field) {
    CFNumberRef key;
    CFNumberRef value = NULL;
    int64_t result = 0;

    if (event == NULL)
        return 0;
    key = PDFieldKey(field);
    if (PDFieldTable(event, PD_KEY_INTEGERS, false) != NULL)
        value = CFDictionaryGetValue(PDFieldTable(event, PD_KEY_INTEGERS, false), key);
    if (value == NULL && PDFieldTable(event, PD_KEY_DOUBLES, false) != NULL)
        value = CFDictionaryGetValue(PDFieldTable(event, PD_KEY_DOUBLES, false), key);
    if (value != NULL)
        CFNumberGetValue(value, kCFNumberSInt64Type, &result);
    CFRelease(key);
    return result;
}

void CGEventSetIntegerValueField(CGEventRef event, CGEventField field, int64_t value) {
    CFNumberRef key, number;

    if (event == NULL)
        return;
    key = PDFieldKey(field);
    number = CFNumberCreate(NULL, kCFNumberSInt64Type, &value);
    CFDictionarySetValue(PDFieldTable(event, PD_KEY_INTEGERS, true), key, number);
    if (PDFieldTable(event, PD_KEY_DOUBLES, false) != NULL)
        CFDictionaryRemoveValue(PDFieldTable(event, PD_KEY_DOUBLES, false), key);
    CFRelease(number);
    CFRelease(key);
}

double CGEventGetDoubleValueField(CGEventRef event, CGEventField field) {
    CFNumberRef key;
    CFNumberRef value = NULL;
    double result = 0;

    if (event == NULL)
        return 0;
    key = PDFieldKey(field);
    if (PDFieldTable(event, PD_KEY_DOUBLES, false) != NULL)
        value = CFDictionaryGetValue(PDFieldTable(event, PD_KEY_DOUBLES, false), key);
    if (value == NULL && PDFieldTable(event, PD_KEY_INTEGERS, false) != NULL)
        value = CFDictionaryGetValue(PDFieldTable(event, PD_KEY_INTEGERS, false), key);
    if (value != NULL)
        CFNumberGetValue(value, kCFNumberDoubleType, &result);
    CFRelease(key);
    return result;
}

void CGEventSetDoubleValueField(CGEventRef event, CGEventField field, double value) {
    CFNumberRef key, number;

    if (event == NULL)
        return;
    key = PDFieldKey(field);
    number = CFNumberCreate(NULL, kCFNumberDoubleType, &value);
    CFDictionarySetValue(PDFieldTable(event, PD_KEY_DOUBLES, true), key, number);
    if (PDFieldTable(event, PD_KEY_INTEGERS, false) != NULL)
        CFDictionaryRemoveValue(PDFieldTable(event, PD_KEY_INTEGERS, false), key);
    CFRelease(number);
    CFRelease(key);
}

// the compositor owns global input, so no tap sees other processes' events
CFMachPortRef CGEventTapCreate(CGEventTapLocation tap, CGEventTapPlacement place, CGEventTapOptions options, CGEventMask eventsOfInterest, CGEventTapCallBack callback, void *userInfo) {
    return NULL;
}

CFMachPortRef CGEventTapCreateForPSN(void *processSerialNumber, CGEventTapPlacement place, CGEventTapOptions options, CGEventMask eventsOfInterest, CGEventTapCallBack callback, void *userInfo) {
    return NULL;
}

void CGEventTapEnable(CFMachPortRef tap, bool enable) {
}

bool CGEventTapIsEnabled(CFMachPortRef tap) {
    return false;
}

void CGEventTapPostEvent(CGEventTapProxy proxy, CGEventRef event) {
}

// posting into the global stream needs the compositor's virtual input, which is not wired up
void CGEventPost(CGEventTapLocation tap, CGEventRef event) {
}

void CGEventPostToPSN(void *processSerialNumber, CGEventRef event) {
}

CGError CGGetEventTapList(uint32_t maxNumberOfTaps, CGEventTapInformation *tapList, uint32_t *eventTapCount) {
    if (eventTapCount != NULL)
        *eventTapCount = 0;
    return kCGErrorSuccess;
}

CFTypeID CGEventSourceGetTypeID(void) {
    return CFDictionaryGetTypeID();
}

CGEventSourceRef CGEventSourceCreate(CGEventSourceStateID stateID) {
    CFMutableDictionaryRef source = PDCreateDict();

    PDSetInt(source, PD_KEY_SOURCE_STATE, stateID);
    PDSetInt(source, PD_KEY_KEYBOARD, 40);
    PDSetDouble(source, PD_KEY_PIXELS_PER_LINE, 10.0);
    PDSetDouble(source, PD_KEY_SUPPRESSION, 0.25);
    return (CGEventSourceRef)source;
}

CGEventSourceKeyboardType CGEventSourceGetKeyboardType(CGEventSourceRef source) {
    return (CGEventSourceKeyboardType)PDGetInt(PDDict(source), PD_KEY_KEYBOARD, 40);
}

void CGEventSourceSetKeyboardType(CGEventSourceRef source, CGEventSourceKeyboardType keyboardType) {
    if (source != NULL)
        PDSetInt(PDDict(source), PD_KEY_KEYBOARD, keyboardType);
}

CGEventSourceStateID CGEventSourceGetSourceStateID(CGEventSourceRef source) {
    return (CGEventSourceStateID)PDGetInt(PDDict(source), PD_KEY_SOURCE_STATE, kCGEventSourceStatePrivate);
}

bool CGEventSourceButtonState(CGEventSourceStateID stateID, CGMouseButton button) {
    return (PDAppKitUnsigned("pressedMouseButtons") >> button) & 1;
}

// key state per key code is not tracked, only the modifier keys are known
bool CGEventSourceKeyState(CGEventSourceStateID stateID, CGKeyCode key) {
    CGEventFlags flags = CGEventSourceFlagsState(stateID);

    switch (key) {
    case 0x37: case 0x36: return (flags & kCGEventFlagMaskCommand) != 0;
    case 0x38: case 0x3c: return (flags & kCGEventFlagMaskShift) != 0;
    case 0x3a: case 0x3d: return (flags & kCGEventFlagMaskAlternate) != 0;
    case 0x3b: case 0x3e: return (flags & kCGEventFlagMaskControl) != 0;
    case 0x39: return (flags & kCGEventFlagMaskAlphaShift) != 0;
    }
    return false;
}

// NSEvent's modifier flags use the same bits as CGEventFlags
CGEventFlags CGEventSourceFlagsState(CGEventSourceStateID stateID) {
    return (CGEventFlags)PDAppKitUnsigned("modifierFlags");
}

// libnotify ships in libSystem without its header in this SDK
extern uint32_t notify_register_check(const char *name, int *out_token);
extern uint32_t notify_get_state(int token, uint64_t *state);

// the latest input any app's WindowServer connection saw (published through notify state) or this process's own
CFTimeInterval CGEventSourceSecondsSinceLastEventType(CGEventSourceStateID stateID, CGEventType eventType) {
    static uint64_t (*ownLastInput)(void);
    static int token = -1;
    static mach_timebase_info_data_t timebase;
    uint64_t last = 0, shared = 0, now;

    if (timebase.denom == 0) {
        mach_timebase_info(&timebase);
        ownLastInput = dlsym(RTLD_DEFAULT, "_windowServerLastInputTime");
    }
    now = mach_absolute_time() * timebase.numer / timebase.denom;
    if (stateID != kCGEventSourceStatePrivate) {
        if (token < 0)
            notify_register_check("org.puredarwin.hid.lastinput", &token);
        if (token >= 0)
            notify_get_state(token, &shared);
    }
    if (ownLastInput != NULL)
        last = ownLastInput();
    if (shared > last)
        last = shared;
    // no input seen yet counts from boot, as the HID system does
    return (now - (last < now ? last : now)) / 1e9;
}

uint32_t CGEventSourceCounterForEventType(CGEventSourceStateID stateID, CGEventType eventType) {
    return 0;
}

int64_t CGEventSourceGetUserData(CGEventSourceRef source) {
    return PDGetInt(PDDict(source), CFSTR("userData"), 0);
}

void CGEventSourceSetUserData(CGEventSourceRef source, int64_t userData) {
    if (source != NULL)
        PDSetInt(PDDict(source), CFSTR("userData"), userData);
}

CGEventFilterMask CGEventSourceGetLocalEventsFilterDuringSuppressionState(CGEventSourceRef source, CGEventSuppressionState state) {
    return (CGEventFilterMask)PDGetInt(PDDict(source), state ? CFSTR("filterRemote") : CFSTR("filterInterval"),
                                       PDFilterMaskPermitAllEvents);
}

void CGEventSourceSetLocalEventsFilterDuringSuppressionState(CGEventSourceRef source, CGEventFilterMask filter, CGEventSuppressionState state) {
    if (source != NULL)
        PDSetInt(PDDict(source), state ? CFSTR("filterRemote") : CFSTR("filterInterval"), filter);
}

CFTimeInterval CGEventSourceGetLocalEventsSuppressionInterval(CGEventSourceRef source) {
    return PDGetDouble(PDDict(source), PD_KEY_SUPPRESSION, 0.25);
}

void CGEventSourceSetLocalEventsSuppressionInterval(CGEventSourceRef source, CFTimeInterval seconds) {
    if (source != NULL)
        PDSetDouble(PDDict(source), PD_KEY_SUPPRESSION, seconds);
}

double CGEventSourceGetPixelsPerLine(CGEventSourceRef source) {
    return PDGetDouble(PDDict(source), PD_KEY_PIXELS_PER_LINE, 10.0);
}

void CGEventSourceSetPixelsPerLine(CGEventSourceRef source, double pixelsPerLine) {
    if (source != NULL)
        PDSetDouble(PDDict(source), PD_KEY_PIXELS_PER_LINE, pixelsPerLine);
}
