// HIServices.c - HIServices calls apps make outside AppKit
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0

// HIServices calls apps make outside AppKit: accessibility text markers, the process
// manager for the calling process, Pasteboard handles and the zoom state
#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFRuntime.h>
#include <MacTypes.h>
#include <objc/message.h>
#include <objc/runtime.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <mach-o/dyld.h>

typedef struct FSRef { UInt8 hidden[80]; } FSRef;
extern OSStatus FSPathMakeRef(const UInt8 *path, FSRef *ref, Boolean *isDirectory);

enum { paramErr = -50 };

enum {
    procNotFound = -600,
    badPasteboardFlavorErr = -25133,
    kCurrentProcess = 2,
    kProcessTransformToForegroundApplication = 1,
    kProcessTransformToBackgroundApplication = 2,
    kProcessTransformToUIElementApplication = 4,
};

// accessibility text markers: opaque byte blobs the app hands back to itself
typedef struct __AXTextMarker {
    CFRuntimeBase base;
    CFIndex length;
    UInt8 bytes[];
} *AXTextMarkerRef;

typedef struct __AXTextMarkerRange {
    CFRuntimeBase base;
    AXTextMarkerRef start, end;
} *AXTextMarkerRangeRef;

static CFTypeID sMarkerType, sRangeType;

static Boolean markerEqual(CFTypeRef a, CFTypeRef b)
{
    AXTextMarkerRef x = (AXTextMarkerRef)a, y = (AXTextMarkerRef)b;
    return x->length == y->length && memcmp(x->bytes, y->bytes, x->length) == 0;
}

static CFHashCode markerHash(CFTypeRef cf)
{
    AXTextMarkerRef m = (AXTextMarkerRef)cf;
    CFHashCode h = (CFHashCode)m->length;
    for (CFIndex i = 0; i < m->length; i++)
        h = h * 31 + m->bytes[i];
    return h;
}

static void rangeFinalize(CFTypeRef cf)
{
    AXTextMarkerRangeRef r = (AXTextMarkerRangeRef)cf;
    CFRelease(r->start);
    CFRelease(r->end);
}

static Boolean rangeEqual(CFTypeRef a, CFTypeRef b)
{
    AXTextMarkerRangeRef x = (AXTextMarkerRangeRef)a, y = (AXTextMarkerRangeRef)b;
    return CFEqual(x->start, y->start) && CFEqual(x->end, y->end);
}

static const CFRuntimeClass sMarkerClass = { 0, "AXTextMarker", NULL, NULL, NULL, markerEqual, markerHash, NULL, NULL };
static const CFRuntimeClass sRangeClass = { 0, "AXTextMarkerRange", NULL, NULL, rangeFinalize, rangeEqual, NULL, NULL, NULL };

static void registerClasses(void)
{
    sMarkerType = _CFRuntimeRegisterClass(&sMarkerClass);
    sRangeType = _CFRuntimeRegisterClass(&sRangeClass);
}

static pthread_once_t sRegisterOnce = PTHREAD_ONCE_INIT;

CFTypeID AXTextMarkerGetTypeID(void)
{
    pthread_once(&sRegisterOnce, registerClasses);
    return sMarkerType;
}

CFTypeID AXTextMarkerRangeGetTypeID(void)
{
    pthread_once(&sRegisterOnce, registerClasses);
    return sRangeType;
}

AXTextMarkerRef AXTextMarkerCreate(CFAllocatorRef allocator, const UInt8 *bytes, CFIndex length)
{
    if (length < 0 || (bytes == NULL && length > 0))
        return NULL;
    AXTextMarkerRef m = (AXTextMarkerRef)_CFRuntimeCreateInstance(allocator, AXTextMarkerGetTypeID(),
                                                                  sizeof(*m) - sizeof(CFRuntimeBase) + length, NULL);
    if (m == NULL)
        return NULL;
    m->length = length;
    if (length)
        memcpy(m->bytes, bytes, length);
    return m;
}

const UInt8 *AXTextMarkerGetBytePtr(AXTextMarkerRef marker)
{
    return marker ? marker->bytes : NULL;
}

CFIndex AXTextMarkerGetLength(AXTextMarkerRef marker)
{
    return marker ? marker->length : 0;
}

AXTextMarkerRangeRef AXTextMarkerRangeCreate(CFAllocatorRef allocator, AXTextMarkerRef startMarker, AXTextMarkerRef endMarker)
{
    if (startMarker == NULL || endMarker == NULL)
        return NULL;
    AXTextMarkerRangeRef r = (AXTextMarkerRangeRef)_CFRuntimeCreateInstance(allocator, AXTextMarkerRangeGetTypeID(),
                                                                            sizeof(*r) - sizeof(CFRuntimeBase), NULL);
    if (r == NULL)
        return NULL;
    r->start = (AXTextMarkerRef)CFRetain(startMarker);
    r->end = (AXTextMarkerRef)CFRetain(endMarker);
    return r;
}

AXTextMarkerRangeRef AXTextMarkerRangeCreateWithBytes(CFAllocatorRef allocator, const UInt8 *startBytes, CFIndex startLength,
                                                      const UInt8 *endBytes, CFIndex endLength)
{
    AXTextMarkerRef s = AXTextMarkerCreate(allocator, startBytes, startLength);
    AXTextMarkerRef e = AXTextMarkerCreate(allocator, endBytes, endLength);
    AXTextMarkerRangeRef r = s && e ? AXTextMarkerRangeCreate(allocator, s, e) : NULL;

    if (s)
        CFRelease(s);
    if (e)
        CFRelease(e);
    return r;
}

AXTextMarkerRef AXTextMarkerRangeCopyStartMarker(AXTextMarkerRangeRef range)
{
    return range ? (AXTextMarkerRef)CFRetain(range->start) : NULL;
}

AXTextMarkerRef AXTextMarkerRangeCopyEndMarker(AXTextMarkerRangeRef range)
{
    return range ? (AXTextMarkerRef)CFRetain(range->end) : NULL;
}

// the process manager, which on PureDarwin only knows the calling process
#pragma pack(push, 2)
typedef struct ProcessInfoRec {
    UInt32 processInfoLength;
    StringPtr processName;
    ProcessSerialNumber processNumber;
    UInt32 processType;
    OSType processSignature;
    UInt32 processMode;
    Ptr processLocation;
    UInt32 processSize;
    UInt32 processFreeMem;
    ProcessSerialNumber processLauncher;
    UInt32 processLaunchDate;
    UInt32 processActiveTime;
    FSRef *processAppRef;
} ProcessInfoRec;
#pragma pack(pop)

static Boolean isCurrent(const ProcessSerialNumber *psn)
{
    return psn && psn->highLongOfPSN == 0 && psn->lowLongOfPSN == kCurrentProcess;
}

OSErr GetCurrentProcess(ProcessSerialNumber *psn)
{
    if (psn == NULL)
        return paramErr;
    psn->highLongOfPSN = 0;
    psn->lowLongOfPSN = kCurrentProcess;
    return noErr;
}

OSErr SameProcess(const ProcessSerialNumber *a, const ProcessSerialNumber *b, Boolean *result)
{
    if (a == NULL || b == NULL || result == NULL)
        return paramErr;
    *result = a->highLongOfPSN == b->highLongOfPSN && a->lowLongOfPSN == b->lowLongOfPSN;
    return noErr;
}

static char *executablePath(void)
{
    uint32_t size = 0;
    _NSGetExecutablePath(NULL, &size);
    char *path = malloc(size + 1);
    if (path && _NSGetExecutablePath(path, &size) != 0) {
        free(path);
        return NULL;
    }
    return path;
}

OSErr GetProcessInformation(const ProcessSerialNumber *psn, ProcessInfoRec *info)
{
    if (info == NULL || info->processInfoLength < sizeof(ProcessInfoRec) - sizeof(FSRef *))
        return paramErr;
    if (!isCurrent(psn))
        return procNotFound;

    StringPtr name = info->processName;
    FSRef *appRef = info->processAppRef;
    UInt32 length = info->processInfoLength;
    memset(info, 0, length < sizeof(*info) ? length : sizeof(*info));
    info->processInfoLength = length;
    info->processName = name;
    info->processAppRef = length >= sizeof(*info) ? appRef : NULL;
    GetCurrentProcess(&info->processNumber);
    info->processType = 'APPL';
    info->processSignature = 0x3F3F3F3F;

    char *path = executablePath();
    if (path && name) {
        const char *base = strrchr(path, '/') ? strrchr(path, '/') + 1 : path;
        size_t n = strlen(base) > 255 ? 255 : strlen(base);
        name[0] = (UInt8)n;
        memcpy(name + 1, base, n);
    }
    if (path && info->processAppRef) {
        // the app reference is the enclosing bundle when the executable sits in Contents/MacOS
        char *macos = strstr(path, "/Contents/MacOS/");
        if (macos)
            *macos = 0;
        FSPathMakeRef((const UInt8 *)path, info->processAppRef, NULL);
    }
    free(path);
    return noErr;
}

static id sharedApp(void)
{
    Class cls = objc_getClass("NSApplication");
    return cls ? ((id (*)(id, SEL))objc_msgSend)((id)cls, sel_registerName("sharedApplication")) : nil;
}

OSStatus SetFrontProcess(const ProcessSerialNumber *psn)
{
    if (!isCurrent(psn))
        return procNotFound;
    // without AppKit there is no window to bring forward
    id app = objc_getClass("NSApplication") ? sharedApp() : nil;
    if (app == nil)
        return procNotFound;
    ((void (*)(id, SEL, BOOL))objc_msgSend)(app, sel_registerName("activateIgnoringOtherApps:"), YES);
    return noErr;
}

OSStatus TransformProcessType(const ProcessSerialNumber *psn, UInt32 transformState)
{
    long policy;

    if (!isCurrent(psn))
        return procNotFound;
    switch (transformState) {
    case kProcessTransformToForegroundApplication: policy = 0; break;
    case kProcessTransformToUIElementApplication: policy = 1; break;
    case kProcessTransformToBackgroundApplication: policy = 2; break;
    default: return paramErr;
    }
    id app = objc_getClass("NSApplication") ? sharedApp() : nil;
    if (app == nil)
        return procNotFound;
    BOOL ok = ((BOOL (*)(id, SEL, long))objc_msgSend)(app, sel_registerName("setActivationPolicy:"), policy);
    return ok ? noErr : paramErr;
}

// Pasteboard handles: the clipboard itself is NSPasteboard's, these only carry the name
typedef CFMutableDictionaryRef PasteboardRef;
typedef OptionBits PasteboardSyncFlags;

OSStatus PasteboardCreate(CFStringRef inName, PasteboardRef *outPasteboard)
{
    if (outPasteboard == NULL)
        return paramErr;
    PasteboardRef pb = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
                                                 &kCFTypeDictionaryValueCallBacks);
    CFStringRef name = inName ? CFRetain(inName) : CFStringCreateWithFormat(NULL, NULL, CFSTR("com.puredarwin.pasteboard.%p"), pb);
    CFDictionarySetValue(pb, CFSTR("name"), name);
    CFRelease(name);
    *outPasteboard = pb;
    return noErr;
}

PasteboardSyncFlags PasteboardSynchronize(PasteboardRef inPasteboard)
{
    (void)inPasteboard;
    return 0;
}

// the paste location is set by the Finder for promised-file drops, no such drop exists here
OSStatus PasteboardCopyPasteLocation(PasteboardRef inPasteboard, CFURLRef *outPasteLocation)
{
    if (inPasteboard == NULL || outPasteLocation == NULL)
        return paramErr;
    CFURLRef url = CFDictionaryGetValue(inPasteboard, CFSTR("pasteLocation"));
    *outPasteLocation = url ? CFRetain(url) : NULL;
    return url ? noErr : badPasteboardFlavorErr;
}

OSStatus PasteboardSetPasteLocation(PasteboardRef inPasteboard, CFURLRef inPasteLocation)
{
    if (inPasteboard == NULL || inPasteLocation == NULL)
        return paramErr;
    CFDictionarySetValue(inPasteboard, CFSTR("pasteLocation"), inPasteLocation);
    return noErr;
}

// screen zoom, which PureDarwin does not provide
Boolean UAZoomEnabled(void)
{
    return false;
}

OSStatus UAZoomChangeFocus(const void *inRect, const void *inHighlightRect, UInt32 inType)
{
    (void)inHighlightRect;
    (void)inType;
    return inRect ? noErr : paramErr;
}
