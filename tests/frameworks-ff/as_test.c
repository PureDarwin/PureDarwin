#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <MacTypes.h>
#include <string.h>
#include <unistd.h>
#include "common.h"

typedef struct __AXTextMarker *AXTextMarkerRef;
typedef struct __AXTextMarkerRange *AXTextMarkerRangeRef;
AXTextMarkerRef AXTextMarkerCreate(CFAllocatorRef, const UInt8 *, CFIndex);
const UInt8 *AXTextMarkerGetBytePtr(AXTextMarkerRef);
CFIndex AXTextMarkerGetLength(AXTextMarkerRef);
AXTextMarkerRangeRef AXTextMarkerRangeCreate(CFAllocatorRef, AXTextMarkerRef, AXTextMarkerRef);
AXTextMarkerRef AXTextMarkerRangeCopyStartMarker(AXTextMarkerRangeRef);
CFTypeID AXTextMarkerRangeGetTypeID(void);
OSErr GetCurrentProcess(ProcessSerialNumber *);
#pragma pack(push, 2)
typedef struct { UInt32 len; StringPtr name; ProcessSerialNumber psn; UInt32 type; OSType sig; UInt32 mode; Ptr loc;
                 UInt32 size, freeMem; ProcessSerialNumber launcher; UInt32 launchDate, activeTime; void *appRef; } ProcessInfoRec;
#pragma pack(pop)
OSErr GetProcessInformation(const ProcessSerialNumber *, ProcessInfoRec *);
OSStatus TransformProcessType(const ProcessSerialNumber *, UInt32);
typedef CFTypeRef PasteboardRef;
OSStatus PasteboardCreate(CFStringRef, PasteboardRef *);
OSStatus PasteboardCopyPasteLocation(PasteboardRef, CFURLRef *);
typedef CFTypeRef PMObj;
OSStatus PMCreateSession(PMObj *);
OSStatus PMCreatePrintSettings(PMObj *);
OSStatus PMCreatePageFormat(PMObj *);
OSStatus PMSessionSetDestination(PMObj, PMObj, UInt16, CFStringRef, CFURLRef);
OSStatus PMSessionBeginCGDocumentNoDialog(PMObj, PMObj, PMObj);
OSStatus PMSessionBeginPageNoDialog(PMObj, PMObj, const void *);
OSStatus PMSessionGetCGGraphicsContext(PMObj, CGContextRef *);
OSStatus PMSessionEndPageNoDialog(PMObj);
OSStatus PMSessionEndDocumentNoDialog(PMObj);
OSStatus PMSessionGetCurrentPrinter(PMObj, PMObj *);
OSStatus PMRelease(CFTypeRef);
typedef struct { double top, left, bottom, right; } PMRect;
OSStatus PMGetAdjustedPaperRect(PMObj, PMRect *);
Boolean UAZoomEnabled(void);

int main(void)
{
    const UInt8 bytes[] = { 1, 2, 3, 4, 5 };
    AXTextMarkerRef a = AXTextMarkerCreate(NULL, bytes, 5), b = AXTextMarkerCreate(NULL, bytes, 3);
    CHECK(a && AXTextMarkerGetLength(a) == 5 && memcmp(AXTextMarkerGetBytePtr(a), bytes, 5) == 0, "AXTextMarker keeps its bytes");
    AXTextMarkerRangeRef r = AXTextMarkerRangeCreate(NULL, a, b);
    CHECK(r && CFGetTypeID(r) == AXTextMarkerRangeGetTypeID(), "AXTextMarkerRange has its own type");
    AXTextMarkerRef s = r ? AXTextMarkerRangeCopyStartMarker(r) : NULL;
    CHECK(s && CFEqual(s, a), "range start marker equals the first marker");

    ProcessSerialNumber psn;
    unsigned char name[256] = { 0 };
    ProcessInfoRec info = { sizeof(info), name };
    CHECK(GetCurrentProcess(&psn) == noErr && GetProcessInformation(&psn, &info) == noErr, "GetProcessInformation on the current process");
    printf("info process name %.*s type %08x\n", name[0], name + 1, (unsigned)info.type);
    ProcessSerialNumber other = { 0, 1234 };
    CHECK(GetProcessInformation(&other, &info) == -600, "another process is procNotFound");
    CHECK(TransformProcessType(&psn, 1) == -600, "TransformProcessType without AppKit loaded reports no app");

    PasteboardRef pb = NULL;
    CFURLRef loc = NULL;
    CHECK(PasteboardCreate(CFSTR("com.apple.pasteboard.clipboard"), &pb) == noErr && pb, "PasteboardCreate");
    CHECK(PasteboardCopyPasteLocation(pb, &loc) != noErr && loc == NULL, "no paste location without a drop");

    PMObj session, settings, format, printer = NULL;
    PMCreateSession(&session);
    PMCreatePrintSettings(&settings);
    PMCreatePageFormat(&format);
    CHECK(PMSessionGetCurrentPrinter(session, &printer) != noErr && printer == NULL, "no current printer");
    PMRect paper;
    CHECK(PMGetAdjustedPaperRect(format, &paper) == noErr && paper.top < 0 && paper.right > 500, "adjusted paper rect");
    unlink("/tmp/pdtest-print.pdf");
    CFURLRef out = CFURLCreateWithFileSystemPath(NULL, CFSTR("/tmp/pdtest-print.pdf"), kCFURLPOSIXPathStyle, false);
    CHECK(PMSessionSetDestination(session, settings, 2, NULL, out) == noErr, "file destination");
    CHECK(PMSessionBeginCGDocumentNoDialog(session, settings, format) == noErr, "begin document");
    CGContextRef ctx = NULL;
    CHECK(PMSessionBeginPageNoDialog(session, format, NULL) == noErr && PMSessionGetCGGraphicsContext(session, &ctx) == noErr && ctx,
          "page context");
    if (ctx) {
        CGContextSetRGBFillColor(ctx, 1, 0, 0, 1);
        CGContextFillRect(ctx, CGRectMake(72, 72, 200, 100));
    }
    CHECK(PMSessionEndPageNoDialog(session) == noErr && PMSessionEndDocumentNoDialog(session) == noErr, "end page and document");
    FILE *f = fopen("/tmp/pdtest-print.pdf", "rb");
    char head[5] = { 0 };
    long size = 0;
    if (f) {
        fread(head, 1, 4, f);
        fseek(f, 0, SEEK_END);
        size = ftell(f);
        fclose(f);
    }
    CHECK(strcmp(head, "%PDF") == 0 && size > 200, "printed a %ld byte PDF", size);
    CHECK(!UAZoomEnabled(), "zoom is off");
    DONE("ApplicationServices");
}
