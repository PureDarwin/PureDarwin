// PrintCore.c - PrintCore sessions, settings and page formats
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0

// PrintCore sessions, settings and page formats. There is no print system, so every
// session prints to a PDF file and a printer destination fails when the job is sent
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <MacTypes.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef CFMutableDictionaryRef PMPrintSession, PMPrintSettings, PMPageFormat, PMPrinter, PMPaper;
typedef UInt16 PMDestinationType;
typedef UInt16 PMOrientation;
typedef struct { double top, left, bottom, right; } PMRect;

enum { paramErr = -50 };

enum {
    kPMNoError = 0,
    kPMGeneralError = -30870,
    kPMOutOfScope = -30871,
    kPMInvalidParameter = paramErr,
    kPMNoDefaultPrinter = -30872,
    kPMInvalidPrinter = -30882,
    kPMInvalidPrintSettings = -30876,
    kPMInvalidPageFormat = -30877,
    kPMValueOutOfRange = -30878,
};

enum {
    kPMDestinationInvalid = 0,
    kPMDestinationPrinter = 1,
    kPMDestinationFile = 2,
    kPMDestinationFax = 3,
    kPMDestinationPreview = 4,
    kPMDestinationProcessPDF = 5,
};

enum {
    kPMPortrait = 1,
    kPMLandscape = 2,
    kPMReversePortrait = 3,
    kPMReverseLandscape = 4,
};

#define kPMPrintAllPages ((UInt32)-1)

// US Letter with the quarter-inch margins most printers report
static const double kPaperWidth = 612, kPaperHeight = 792, kMargin = 18;

static CFMutableDictionaryRef createObject(CFStringRef kind)
{
    CFMutableDictionaryRef o = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
                                                         &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(o, CFSTR("kind"), kind);
    return o;
}

static Boolean isKind(CFTypeRef o, CFStringRef kind)
{
    CFTypeRef k = o && CFGetTypeID(o) == CFDictionaryGetTypeID() ? CFDictionaryGetValue(o, CFSTR("kind")) : NULL;
    return k != NULL && CFEqual(k, kind);
}

static void setInt(CFMutableDictionaryRef o, CFStringRef key, SInt64 value)
{
    CFNumberRef n = CFNumberCreate(NULL, kCFNumberSInt64Type, &value);
    CFDictionarySetValue(o, key, n);
    CFRelease(n);
}

static SInt64 getInt(CFDictionaryRef o, CFStringRef key, SInt64 fallback)
{
    CFNumberRef n = CFDictionaryGetValue(o, key);
    SInt64 value = fallback;
    if (n && CFGetTypeID(n) == CFNumberGetTypeID())
        CFNumberGetValue(n, kCFNumberSInt64Type, &value);
    return value;
}

OSStatus PMRetain(CFTypeRef object)
{
    if (object == NULL)
        return kPMInvalidParameter;
    CFRetain(object);
    return kPMNoError;
}

OSStatus PMRelease(CFTypeRef object)
{
    if (object)
        CFRelease(object);
    return kPMNoError;
}

OSStatus PMCreateSession(PMPrintSession *printSession)
{
    if (printSession == NULL)
        return kPMInvalidParameter;
    *printSession = createObject(CFSTR("session"));
    setInt(*printSession, CFSTR("destination"), kPMDestinationFile);
    return kPMNoError;
}

OSStatus PMCreatePrintSettings(PMPrintSettings *printSettings)
{
    if (printSettings == NULL)
        return kPMInvalidParameter;
    *printSettings = createObject(CFSTR("settings"));
    setInt(*printSettings, CFSTR("first"), 1);
    setInt(*printSettings, CFSTR("last"), kPMPrintAllPages);
    setInt(*printSettings, CFSTR("copies"), 1);
    return kPMNoError;
}

OSStatus PMCreatePageFormat(PMPageFormat *pageFormat)
{
    if (pageFormat == NULL)
        return kPMInvalidParameter;
    *pageFormat = createObject(CFSTR("format"));
    setInt(*pageFormat, CFSTR("orientation"), kPMPortrait);
    return kPMNoError;
}

OSStatus PMSessionDefaultPrintSettings(PMPrintSession session, PMPrintSettings settings)
{
    return isKind(session, CFSTR("session")) && isKind(settings, CFSTR("settings")) ? kPMNoError : kPMInvalidParameter;
}

OSStatus PMSessionDefaultPageFormat(PMPrintSession session, PMPageFormat format)
{
    return isKind(session, CFSTR("session")) && isKind(format, CFSTR("format")) ? kPMNoError : kPMInvalidParameter;
}

OSStatus PMSessionValidatePrintSettings(PMPrintSession session, PMPrintSettings settings, Boolean *changed)
{
    if (changed)
        *changed = false;
    return PMSessionDefaultPrintSettings(session, settings);
}

OSStatus PMSessionValidatePageFormat(PMPrintSession session, PMPageFormat format, Boolean *changed)
{
    if (changed)
        *changed = false;
    return PMSessionDefaultPageFormat(session, format);
}

OSStatus PMSessionError(PMPrintSession session)
{
    return isKind(session, CFSTR("session")) ? (OSStatus)getInt(session, CFSTR("error"), kPMNoError) : kPMInvalidParameter;
}

OSStatus PMSessionSetError(PMPrintSession session, OSStatus error)
{
    if (!isKind(session, CFSTR("session")))
        return kPMInvalidParameter;
    setInt(session, CFSTR("error"), error);
    return kPMNoError;
}

// libcups reports no queues, so there is never a current printer
OSStatus PMSessionGetCurrentPrinter(PMPrintSession session, PMPrinter *printer)
{
    if (!isKind(session, CFSTR("session")) || printer == NULL)
        return kPMInvalidParameter;
    *printer = NULL;
    return kPMNoDefaultPrinter;
}

OSStatus PMSessionGetDestinationType(PMPrintSession session, PMPrintSettings settings, PMDestinationType *type)
{
    (void)settings;
    if (!isKind(session, CFSTR("session")) || type == NULL)
        return kPMInvalidParameter;
    *type = (PMDestinationType)getInt(session, CFSTR("destination"), kPMDestinationFile);
    return kPMNoError;
}

OSStatus PMSessionSetDestination(PMPrintSession session, PMPrintSettings settings, PMDestinationType type,
                                 CFStringRef format, CFURLRef location)
{
    (void)settings;
    (void)format;
    if (!isKind(session, CFSTR("session")) || type == kPMDestinationInvalid || type > kPMDestinationProcessPDF)
        return kPMInvalidParameter;
    setInt(session, CFSTR("destination"), type);
    if (location)
        CFDictionarySetValue(session, CFSTR("location"), location);
    else
        CFDictionaryRemoveValue(session, CFSTR("location"));
    return kPMNoError;
}

OSStatus PMSessionCopyDestinationLocation(PMPrintSession session, PMPrintSettings settings, CFURLRef *location)
{
    (void)settings;
    if (!isKind(session, CFSTR("session")) || location == NULL)
        return kPMInvalidParameter;
    CFURLRef url = CFDictionaryGetValue(session, CFSTR("location"));
    *location = url ? CFRetain(url) : NULL;
    return kPMNoError;
}

OSStatus PMSetFirstPage(PMPrintSettings settings, UInt32 first, Boolean lock)
{
    (void)lock;
    if (!isKind(settings, CFSTR("settings")) || first == 0)
        return kPMInvalidParameter;
    setInt(settings, CFSTR("first"), first);
    return kPMNoError;
}

OSStatus PMSetLastPage(PMPrintSettings settings, UInt32 last, Boolean lock)
{
    (void)lock;
    if (!isKind(settings, CFSTR("settings")) || last == 0)
        return kPMInvalidParameter;
    setInt(settings, CFSTR("last"), last);
    return kPMNoError;
}

OSStatus PMGetFirstPage(PMPrintSettings settings, UInt32 *first)
{
    if (!isKind(settings, CFSTR("settings")) || first == NULL)
        return kPMInvalidParameter;
    *first = (UInt32)getInt(settings, CFSTR("first"), 1);
    return kPMNoError;
}

OSStatus PMGetLastPage(PMPrintSettings settings, UInt32 *last)
{
    if (!isKind(settings, CFSTR("settings")) || last == NULL)
        return kPMInvalidParameter;
    *last = (UInt32)getInt(settings, CFSTR("last"), kPMPrintAllPages);
    return kPMNoError;
}

OSStatus PMSetCopies(PMPrintSettings settings, UInt32 copies, Boolean lock)
{
    (void)lock;
    if (!isKind(settings, CFSTR("settings")) || copies == 0)
        return kPMInvalidParameter;
    setInt(settings, CFSTR("copies"), copies);
    return kPMNoError;
}

OSStatus PMGetCopies(PMPrintSettings settings, UInt32 *copies)
{
    if (!isKind(settings, CFSTR("settings")) || copies == NULL)
        return kPMInvalidParameter;
    *copies = (UInt32)getInt(settings, CFSTR("copies"), 1);
    return kPMNoError;
}

OSStatus PMPrintSettingsSetJobName(PMPrintSettings settings, CFStringRef name)
{
    if (!isKind(settings, CFSTR("settings")) || name == NULL)
        return kPMInvalidParameter;
    CFDictionarySetValue(settings, CFSTR("jobName"), name);
    return kPMNoError;
}

OSStatus PMPrintSettingsGetJobName(PMPrintSettings settings, CFStringRef *name)
{
    if (!isKind(settings, CFSTR("settings")) || name == NULL)
        return kPMInvalidParameter;
    *name = CFDictionaryGetValue(settings, CFSTR("jobName"));
    return kPMNoError;
}

OSStatus PMSetOrientation(PMPageFormat format, PMOrientation orientation, Boolean lock)
{
    (void)lock;
    if (!isKind(format, CFSTR("format")) || orientation < kPMPortrait || orientation > kPMReverseLandscape)
        return kPMInvalidParameter;
    setInt(format, CFSTR("orientation"), orientation);
    return kPMNoError;
}

OSStatus PMGetOrientation(PMPageFormat format, PMOrientation *orientation)
{
    if (!isKind(format, CFSTR("format")) || orientation == NULL)
        return kPMInvalidParameter;
    *orientation = (PMOrientation)getInt(format, CFSTR("orientation"), kPMPortrait);
    return kPMNoError;
}

static Boolean isLandscape(PMPageFormat format)
{
    SInt64 o = getInt(format, CFSTR("orientation"), kPMPortrait);
    return o == kPMLandscape || o == kPMReverseLandscape;
}

// the paper relative to the imageable area's origin, so its top left is negative
OSStatus PMGetAdjustedPaperRect(PMPageFormat format, PMRect *rect)
{
    if (!isKind(format, CFSTR("format")) || rect == NULL)
        return kPMInvalidParameter;
    double w = isLandscape(format) ? kPaperHeight : kPaperWidth, h = isLandscape(format) ? kPaperWidth : kPaperHeight;
    *rect = (PMRect){ -kMargin, -kMargin, h - kMargin, w - kMargin };
    return kPMNoError;
}

OSStatus PMGetAdjustedPageRect(PMPageFormat format, PMRect *rect)
{
    if (!isKind(format, CFSTR("format")) || rect == NULL)
        return kPMInvalidParameter;
    double w = isLandscape(format) ? kPaperHeight : kPaperWidth, h = isLandscape(format) ? kPaperWidth : kPaperHeight;
    *rect = (PMRect){ 0, 0, h - 2 * kMargin, w - 2 * kMargin };
    return kPMNoError;
}

OSStatus PMGetUnadjustedPaperRect(PMPageFormat format, PMRect *rect)
{
    if (!isKind(format, CFSTR("format")) || rect == NULL)
        return kPMInvalidParameter;
    double w = isLandscape(format) ? kPaperHeight : kPaperWidth, h = isLandscape(format) ? kPaperWidth : kPaperHeight;
    *rect = (PMRect){ 0, 0, h, w };
    return kPMNoError;
}

// a document is a PDF context on the destination file, or a temporary file a printer would take
OSStatus PMSessionBeginCGDocumentNoDialog(PMPrintSession session, PMPrintSettings settings, PMPageFormat format)
{
    if (!isKind(session, CFSTR("session")) || !isKind(settings, CFSTR("settings")) || !isKind(format, CFSTR("format")))
        return kPMInvalidParameter;
    if (CFDictionaryContainsKey(session, CFSTR("context")))
        return kPMOutOfScope;

    CFURLRef url = CFDictionaryGetValue(session, CFSTR("location"));
    if (url == NULL) {
        char path[] = "/tmp/pdprint.XXXXXX";
        int fd = mkstemp(path);
        if (fd < 0) {
            PMSessionSetError(session, kPMGeneralError);
            return kPMGeneralError;
        }
        close(fd);
        url = CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8 *)path, strlen(path), false);
        CFDictionarySetValue(session, CFSTR("spool"), url);
        CFRelease(url);
    }

    PMRect paper;
    PMGetUnadjustedPaperRect(format, &paper);
    CGRect box = CGRectMake(0, 0, paper.right, paper.bottom);
    CFMutableDictionaryRef info = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFStringRef job = CFDictionaryGetValue(settings, CFSTR("jobName"));
    if (job)
        CFDictionarySetValue(info, kCGPDFContextTitle, job);
    CGDataConsumerRef consumer = CGDataConsumerCreateWithURL(url);
    CGContextRef ctx = consumer ? CGPDFContextCreate(consumer, &box, info) : NULL;
    CFRelease(info);
    if (consumer)
        CGDataConsumerRelease(consumer);
    if (ctx == NULL) {
        PMSessionSetError(session, kPMGeneralError);
        return kPMGeneralError;
    }
    CFDictionarySetValue(session, CFSTR("context"), ctx);
    CFDictionarySetValue(session, CFSTR("format"), format);
    CGContextRelease(ctx);
    PMSessionSetError(session, kPMNoError);
    return kPMNoError;
}

OSStatus PMSessionBeginPageNoDialog(PMPrintSession session, PMPageFormat format, const PMRect *pageFrame)
{
    (void)pageFrame;
    CGContextRef ctx = isKind(session, CFSTR("session")) ? (CGContextRef)CFDictionaryGetValue(session, CFSTR("context")) : NULL;

    if (ctx == NULL)
        return kPMOutOfScope;
    PMRect paper;
    PMGetUnadjustedPaperRect(format ? format : (PMPageFormat)CFDictionaryGetValue(session, CFSTR("format")), &paper);
    CGRect box = CGRectMake(0, 0, paper.right, paper.bottom);
    CGContextBeginPage(ctx, &box);
    return kPMNoError;
}

OSStatus PMSessionEndPageNoDialog(PMPrintSession session)
{
    CGContextRef ctx = isKind(session, CFSTR("session")) ? (CGContextRef)CFDictionaryGetValue(session, CFSTR("context")) : NULL;

    if (ctx == NULL)
        return kPMOutOfScope;
    CGContextEndPage(ctx);
    return kPMNoError;
}

OSStatus PMSessionGetCGGraphicsContext(PMPrintSession session, CGContextRef *context)
{
    if (!isKind(session, CFSTR("session")) || context == NULL)
        return kPMInvalidParameter;
    *context = (CGContextRef)CFDictionaryGetValue(session, CFSTR("context"));
    return *context ? kPMNoError : kPMOutOfScope;
}

OSStatus PMSessionEndDocumentNoDialog(PMPrintSession session)
{
    CGContextRef ctx = isKind(session, CFSTR("session")) ? (CGContextRef)CFDictionaryGetValue(session, CFSTR("context")) : NULL;

    if (ctx == NULL)
        return kPMOutOfScope;
    CGPDFContextClose(ctx);
    CFDictionaryRemoveValue(session, CFSTR("context"));

    // the PDF is written, a printer destination has nowhere to send it
    CFURLRef spool = CFDictionaryGetValue(session, CFSTR("spool"));
    if (spool) {
        UInt8 path[1024];
        if (CFURLGetFileSystemRepresentation(spool, true, path, sizeof(path)))
            unlink((const char *)path);
        CFDictionaryRemoveValue(session, CFSTR("spool"));
    }
    if (getInt(session, CFSTR("destination"), kPMDestinationFile) != kPMDestinationFile) {
        PMSessionSetError(session, kPMNoDefaultPrinter);
        return kPMNoDefaultPrinter;
    }
    return kPMNoError;
}

OSStatus PMPrinterPrintWithFile(PMPrinter printer, PMPrintSettings settings, PMPageFormat format, CFStringRef mimeType,
                                CFURLRef fileURL)
{
    (void)settings;
    (void)format;
    (void)mimeType;
    (void)fileURL;
    return printer ? kPMInvalidPrinter : kPMInvalidParameter;
}

OSStatus PMServerCreatePrinterList(void *server, CFArrayRef *printerList)
{
    (void)server;
    if (printerList == NULL)
        return kPMInvalidParameter;
    *printerList = CFArrayCreate(NULL, NULL, 0, &kCFTypeArrayCallBacks);
    return kPMNoError;
}
