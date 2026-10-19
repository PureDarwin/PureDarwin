// UTType.c - the C UTType API of LaunchServices
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0

// the C UTType API of LaunchServices over the system's declared types, the same table
// UniformTypeIdentifiers declares plus the web types, tags nothing declares get a dynamic type
#include <CoreServices/UTType.h>
#include <ctype.h>
#include <string.h>
#include <strings.h>

typedef struct {
    const char *identifier;
    const char *description;
    const char *parents;    // space separated
    const char *extensions; // space separated, the preferred one first
    const char *mimes;      // space separated, the preferred one first
} declaration;

static const declaration sDeclarations[] = {
    { "public.item", "item", "", "", "" },
    { "public.content", "content", "", "", "" },
    { "public.composite-content", "composite content", "public.content", "", "" },
    { "public.data", "data", "public.item", "", "application/octet-stream" },
    { "public.directory", "directory", "public.item", "", "" },
    { "public.folder", "folder", "public.directory", "", "" },
    { "public.volume", "volume", "public.folder", "", "" },
    { "com.apple.package", "package", "public.directory", "", "" },
    { "com.apple.bundle", "bundle", "public.directory", "bundle", "" },
    { "com.apple.application-bundle", "application", "com.apple.bundle com.apple.package public.executable", "app", "" },
    { "public.executable", "executable", "public.item", "", "" },
    { "public.text", "text", "public.data public.content", "", "text/plain" },
    { "public.plain-text", "plain text", "public.text", "txt text", "text/plain" },
    { "public.utf8-plain-text", "UTF-8 plain text", "public.plain-text", "", "text/plain;charset=utf-8" },
    { "public.rtf", "rich text", "public.text", "rtf", "text/rtf application/rtf" },
    { "public.html", "HTML", "public.text", "html htm shtml", "text/html" },
    { "public.xhtml", "XHTML", "public.xml", "xhtml xht", "application/xhtml+xml" },
    { "public.xml", "XML", "public.text", "xml", "application/xml text/xml" },
    { "public.css", "CSS", "public.source-code", "css", "text/css" },
    { "com.netscape.javascript-source", "JavaScript", "public.source-code public.script", "js mjs", "text/javascript application/javascript" },
    { "public.script", "script", "public.source-code", "", "" },
    { "public.source-code", "source code", "public.plain-text", "", "" },
    { "public.json", "JSON", "public.text", "json", "application/json" },
    { "public.comma-separated-values-text", "CSV text", "public.delimited-values-text", "csv", "text/csv" },
    { "public.delimited-values-text", "delimited text", "public.text", "", "" },
    { "com.apple.property-list", "property list", "public.data", "plist", "" },
    { "com.apple.xml-property-list", "XML property list", "public.xml com.apple.property-list", "plist", "" },
    { "com.adobe.pdf", "PDF document", "public.data public.composite-content", "pdf", "application/pdf" },
    { "public.url", "URL", "public.data", "", "" },
    { "public.file-url", "file URL", "public.url", "", "" },
    { "com.apple.web-internet-location", "Web internet location", "public.data", "webloc", "" },
    { "public.image", "image", "public.data public.content", "", "" },
    { "public.png", "PNG image", "public.image", "png", "image/png" },
    { "public.jpeg", "JPEG image", "public.image", "jpeg jpg jpe", "image/jpeg" },
    { "com.compuserve.gif", "GIF image", "public.image", "gif", "image/gif" },
    { "public.tiff", "TIFF image", "public.image", "tiff tif", "image/tiff" },
    { "com.microsoft.bmp", "Windows bitmap image", "public.image", "bmp", "image/bmp" },
    { "com.microsoft.ico", "Windows icon image", "public.image", "ico", "image/vnd.microsoft.icon image/x-icon" },
    { "com.apple.icns", "Apple icon image", "public.image", "icns", "image/x-icns" },
    { "public.svg-image", "SVG image", "public.image public.xml", "svg", "image/svg+xml" },
    { "org.webmproject.webp", "WebP image", "public.image", "webp", "image/webp" },
    { "public.avif", "AVIF image", "public.image", "avif", "image/avif" },
    { "public.heic", "HEIC image", "public.image", "heic", "image/heic" },
    { "public.audiovisual-content", "audiovisual content", "public.data public.content", "", "" },
    { "public.movie", "movie", "public.audiovisual-content", "", "" },
    { "public.video", "video", "public.movie", "", "" },
    { "public.audio", "audio", "public.audiovisual-content", "", "" },
    { "com.apple.quicktime-movie", "QuickTime movie", "public.movie", "mov qt", "video/quicktime" },
    { "public.mpeg-4", "MPEG-4 movie", "public.movie", "mp4 mpeg4", "video/mp4" },
    { "public.mpeg-4-audio", "MPEG-4 audio", "public.mpeg-4 public.audio", "m4a", "audio/mp4" },
    { "public.mp3", "MP3 audio", "public.audio", "mp3", "audio/mpeg" },
    { "com.microsoft.waveform-audio", "WAVE audio", "public.audio", "wav wave", "audio/vnd.wave audio/wav" },
    { "org.webmproject.webm", "WebM", "public.movie", "webm", "video/webm audio/webm" },
    { "org.xiph.ogg", "Ogg", "public.audiovisual-content", "ogg ogv oga opus", "application/ogg audio/ogg video/ogg" },
    { "org.xiph.flac", "FLAC audio", "public.audio", "flac", "audio/flac" },
    { "public.archive", "archive", "public.data", "", "" },
    { "public.zip-archive", "ZIP archive", "public.archive", "zip", "application/zip" },
    { "org.gnu.gnu-zip-archive", "gzip archive", "public.archive", "gz gzip", "application/gzip application/x-gzip" },
    { "public.tar-archive", "tar archive", "public.archive", "tar", "application/x-tar" },
    { "com.apple.disk-image", "disk image", "public.archive", "dmg", "application/x-apple-diskimage" },
    { "public.font", "font", "public.data public.content", "", "" },
    { "public.truetype-ttf-font", "TrueType font", "public.font", "ttf", "font/ttf" },
    { "public.opentype-font", "OpenType font", "public.font", "otf", "font/otf" },
    { "org.w3.woff", "WOFF font", "public.font", "woff", "font/woff" },
    { "org.w3.woff2", "WOFF2 font", "public.font", "woff2", "font/woff2" },
    { "org.webassembly.wasm", "WebAssembly", "public.data", "wasm", "application/wasm" },
    { "public.log", "log", "public.plain-text", "log", "" },
    { "com.apple.nspboard-type", "pasteboard type", "", "", "" },
};

#define COUNT (sizeof(sDeclarations) / sizeof(sDeclarations[0]))

const CFStringRef kUTTagClassFilenameExtension = CFSTR("public.filename-extension");
const CFStringRef kUTTagClassMIMEType = CFSTR("public.mime-type");
const CFStringRef kUTTagClassNSPboardType = CFSTR("com.apple.nspboard-type");
const CFStringRef kUTTagClassOSType = CFSTR("com.apple.ostype");

const CFStringRef kUTTypeItem = CFSTR("public.item");
const CFStringRef kUTTypeContent = CFSTR("public.content");
const CFStringRef kUTTypeData = CFSTR("public.data");
const CFStringRef kUTTypeDirectory = CFSTR("public.directory");
const CFStringRef kUTTypeFolder = CFSTR("public.folder");
const CFStringRef kUTTypeText = CFSTR("public.text");
const CFStringRef kUTTypePlainText = CFSTR("public.plain-text");
const CFStringRef kUTTypeUTF8PlainText = CFSTR("public.utf8-plain-text");
const CFStringRef kUTTypeRTF = CFSTR("public.rtf");
const CFStringRef kUTTypeHTML = CFSTR("public.html");
const CFStringRef kUTTypeXML = CFSTR("public.xml");
const CFStringRef kUTTypeURL = CFSTR("public.url");
const CFStringRef kUTTypeFileURL = CFSTR("public.file-url");
const CFStringRef kUTTypeImage = CFSTR("public.image");
const CFStringRef kUTTypePNG = CFSTR("public.png");
const CFStringRef kUTTypeJPEG = CFSTR("public.jpeg");
const CFStringRef kUTTypeGIF = CFSTR("com.compuserve.gif");
const CFStringRef kUTTypeTIFF = CFSTR("public.tiff");
const CFStringRef kUTTypePDF = CFSTR("com.adobe.pdf");
const CFStringRef kUTTypeApplicationBundle = CFSTR("com.apple.application-bundle");
const CFStringRef kUTTypeAudio = CFSTR("public.audio");
const CFStringRef kUTTypeMovie = CFSTR("public.movie");

static CFStringRef copyString(const char *s)
{
    return CFStringCreateWithCString(kCFAllocatorDefault, s, kCFStringEncodingUTF8);
}

// whether the space separated list contains word, ignoring case
static Boolean listContains(const char *list, const char *word)
{
    size_t n = strlen(word);

    if (n == 0)
        return false;
    for (const char *p = list; *p;) {
        const char *end = strchr(p, ' ');
        size_t len = end ? (size_t)(end - p) : strlen(p);

        if (len == n && strncasecmp(p, word, n) == 0)
            return true;
        if (end == NULL)
            break;
        p = end + 1;
    }
    return false;
}

static const declaration *findIdentifier(CFStringRef uti)
{
    char buf[256];

    if (uti == NULL || !CFStringGetCString(uti, buf, sizeof(buf), kCFStringEncodingUTF8))
        return NULL;
    for (size_t i = 0; i < COUNT; i++) {
        if (strcasecmp(sDeclarations[i].identifier, buf) == 0)
            return &sDeclarations[i];
    }
    return NULL;
}

static Boolean conformsTo(const char *identifier, const char *to, int depth)
{
    if (strcasecmp(identifier, to) == 0)
        return true;
    if (depth > 16)
        return false;
    for (size_t i = 0; i < COUNT; i++) {
        if (strcasecmp(sDeclarations[i].identifier, identifier) != 0)
            continue;
        char parents[256];
        strlcpy(parents, sDeclarations[i].parents, sizeof(parents));
        for (char *save = NULL, *p = strtok_r(parents, " ", &save); p; p = strtok_r(NULL, " ", &save)) {
            if (conformsTo(p, to, depth + 1))
                return true;
        }
    }
    return false;
}

Boolean UTTypeConformsTo(CFStringRef inUTI, CFStringRef inConformsToUTI)
{
    char a[256], b[256];

    if (inUTI == NULL || inConformsToUTI == NULL)
        return false;
    if (!CFStringGetCString(inUTI, a, sizeof(a), kCFStringEncodingUTF8) ||
        !CFStringGetCString(inConformsToUTI, b, sizeof(b), kCFStringEncodingUTF8))
        return false;
    return conformsTo(a, b, 0);
}

Boolean UTTypeEqual(CFStringRef inUTI1, CFStringRef inUTI2)
{
    if (inUTI1 == NULL || inUTI2 == NULL)
        return false;
    return CFStringCompare(inUTI1, inUTI2, kCFCompareCaseInsensitive) == kCFCompareEqualTo;
}

Boolean UTTypeIsDynamic(CFStringRef inUTI)
{
    return inUTI != NULL && CFStringHasPrefix(inUTI, CFSTR("dyn."));
}

Boolean UTTypeIsDeclared(CFStringRef inUTI)
{
    return findIdentifier(inUTI) != NULL;
}

// lowercase, MIME parameters dropped, extensions without a leading dot
static Boolean normalisedTag(CFStringRef tagClass, CFStringRef tag, char *out, size_t size)
{
    if (!CFStringGetCString(tag, out, size, kCFStringEncodingUTF8))
        return false;
    if (CFEqual(tagClass, kUTTagClassMIMEType)) {
        char *semi = strchr(out, ';');
        if (semi)
            *semi = '\0';
        while (strlen(out) && out[strlen(out) - 1] == ' ')
            out[strlen(out) - 1] = '\0';
    } else if (CFEqual(tagClass, kUTTagClassFilenameExtension) && out[0] == '.') {
        memmove(out, out + 1, strlen(out));
    }
    for (char *p = out; *p; p++)
        *p = (char)tolower((unsigned char)*p);
    return out[0] != '\0';
}

static const char *tagList(const declaration *d, CFStringRef tagClass)
{
    if (CFEqual(tagClass, kUTTagClassFilenameExtension))
        return d->extensions;
    if (CFEqual(tagClass, kUTTagClassMIMEType))
        return d->mimes;
    return NULL;
}

// a type nothing declares: Apple encodes the tag in a dyn. identifier, this keeps tag and class readable
static CFStringRef createDynamic(CFStringRef tagClass, CFStringRef tag, CFStringRef conformingTo)
{
    return CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("dyn.%@:%@%@%@"), tagClass, tag,
                                    conformingTo ? CFSTR(":") : CFSTR(""), conformingTo ? conformingTo : CFSTR(""));
}

CFArrayRef UTTypeCreateAllIdentifiersForTag(CFStringRef inTagClass, CFStringRef inTag, CFStringRef inConformingToUTI)
{
    char tag[256];
    CFMutableArrayRef result;

    if (inTagClass == NULL || inTag == NULL || !normalisedTag(inTagClass, inTag, tag, sizeof(tag)))
        return NULL;
    result = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    for (size_t i = 0; i < COUNT; i++) {
        const char *list = tagList(&sDeclarations[i], inTagClass);
        if (list == NULL || !listContains(list, tag))
            continue;
        CFStringRef uti = copyString(sDeclarations[i].identifier);
        if (inConformingToUTI == NULL || UTTypeConformsTo(uti, inConformingToUTI))
            CFArrayAppendValue(result, uti);
        CFRelease(uti);
    }
    if (CFArrayGetCount(result) == 0) {
        CFStringRef dyn = createDynamic(inTagClass, inTag, inConformingToUTI);
        CFArrayAppendValue(result, dyn);
        CFRelease(dyn);
    }
    return result;
}

CFStringRef UTTypeCreatePreferredIdentifierForTag(CFStringRef inTagClass, CFStringRef inTag, CFStringRef inConformingToUTI)
{
    CFArrayRef all = UTTypeCreateAllIdentifiersForTag(inTagClass, inTag, inConformingToUTI);
    CFStringRef uti = NULL;

    if (all != NULL) {
        if (CFArrayGetCount(all) > 0)
            uti = CFRetain(CFArrayGetValueAtIndex(all, 0));
        CFRelease(all);
    }
    return uti;
}

CFStringRef UTTypeCopyPreferredTagWithClass(CFStringRef inUTI, CFStringRef inTagClass)
{
    const declaration *d = findIdentifier(inUTI);
    const char *list;
    char first[128];

    if (d == NULL || inTagClass == NULL || (list = tagList(d, inTagClass)) == NULL || list[0] == '\0')
        return NULL;
    strlcpy(first, list, sizeof(first));
    char *space = strchr(first, ' ');
    if (space)
        *space = '\0';
    return copyString(first);
}

CFStringRef UTTypeCopyDescription(CFStringRef inUTI)
{
    const declaration *d = findIdentifier(inUTI);

    return d != NULL ? copyString(d->description) : NULL;
}

CFDictionaryRef UTTypeCopyDeclaration(CFStringRef inUTI)
{
    const declaration *d = findIdentifier(inUTI);

    if (d == NULL)
        return NULL;
    CFMutableDictionaryRef decl = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
                                                            &kCFTypeDictionaryValueCallBacks);
    CFStringRef s = copyString(d->identifier);
    CFDictionarySetValue(decl, CFSTR("UTTypeIdentifier"), s);
    CFRelease(s);
    s = copyString(d->description);
    CFDictionarySetValue(decl, CFSTR("UTTypeDescription"), s);
    CFRelease(s);
    return decl;
}
