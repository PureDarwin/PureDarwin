// CTRegistry.m - type IDs, shared FreeType faces and the registry of installed fonts
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#import "CTInternal.h"
#import <CoreFoundation/CFRuntime.h>
#include <fontconfig/fontconfig.h>
#include FT_SFNT_NAMES_H
#include FT_TRUETYPE_IDS_H
#include FT_TRUETYPE_TABLES_H
#include <hb-ft.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <math.h>

CFTypeID CTRegisterTypeID(const char *name) {
    // CF keeps the class pointer, so each one lives as long as the process
    CFRuntimeClass *cls = calloc(1, sizeof(CFRuntimeClass));

    cls->version = 0;
    cls->className = strdup(name);
    return _CFRuntimeRegisterClass(cls);
}

FT_Library CTFreeTypeLibrary(void) {
    static FT_Library library;
    static dispatch_once_t once;

    dispatch_once(&once, ^{
        if (FT_Init_FreeType(&library) != 0)
            library = NULL;
    });
    return library;
}

// FreeType library calls that create or free faces must not overlap
static pthread_mutex_t libraryLock = PTHREAD_MUTEX_INITIALIZER;

static CFStringRef copyFTString(const char *s) {
    return s ? CFStringCreateWithCString(NULL, s, kCFStringEncodingUTF8) : NULL;
}

static CTFace *faceFromFT(FT_Face ft, const char *path, long index) {
    CTFace *face = calloc(1, sizeof(CTFace));
    pthread_mutexattr_t attr;

    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&face->lock, &attr);
    pthread_mutexattr_destroy(&attr);
    face->refs = 1;
    face->ft = ft;
    face->path = path ? strdup(path) : NULL;
    face->index = index;
    face->unitsPerEm = ft->units_per_EM ? ft->units_per_EM : 1000;
    FT_Select_Charmap(ft, FT_ENCODING_UNICODE);
    face->hb = hb_ft_face_create_referenced(ft);
    face->postScriptName = copyFTString(FT_Get_Postscript_Name(ft));
    face->familyName = CTFaceCopyName(face, 16);
    if (face->familyName == NULL)
        face->familyName = copyFTString(ft->family_name);
    face->styleName = CTFaceCopyName(face, 17);
    if (face->styleName == NULL)
        face->styleName = copyFTString(ft->style_name);
    face->fullName = CTFaceCopyName(face, TT_NAME_ID_FULL_NAME);
    if (face->fullName == NULL && face->familyName)
        face->fullName = CFRetain(face->familyName);
    if (face->postScriptName == NULL && face->fullName)
        face->postScriptName = CFRetain(face->fullName);
    return face;
}

// faces are cached by file and index for the life of the process
static CFMutableDictionaryRef faceCache;
static pthread_mutex_t faceCacheLock = PTHREAD_MUTEX_INITIALIZER;

CTFace *CTFaceCreateWithPath(const char *path, long index) {
    FT_Face ft = NULL;
    CTFace *face;
    CFStringRef key;

    if (path == NULL || CTFreeTypeLibrary() == NULL)
        return NULL;
    key = CFStringCreateWithFormat(NULL, NULL, CFSTR("%s#%ld"), path, index);
    pthread_mutex_lock(&faceCacheLock);
    if (faceCache == NULL)
        faceCache = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, NULL);
    face = (CTFace *)CFDictionaryGetValue(faceCache, key);
    if (face != NULL) {
        CTFaceRetain(face);
        pthread_mutex_unlock(&faceCacheLock);
        CFRelease(key);
        return face;
    }
    pthread_mutex_lock(&libraryLock);
    if (FT_New_Face(CTFreeTypeLibrary(), path, index, &ft) != 0)
        ft = NULL;
    pthread_mutex_unlock(&libraryLock);
    if (ft == NULL) {
        pthread_mutex_unlock(&faceCacheLock);
        CFRelease(key);
        return NULL;
    }
    face = faceFromFT(ft, path, index);
    // the cache holds its own reference
    CTFaceRetain(face);
    CFDictionarySetValue(faceCache, key, face);
    pthread_mutex_unlock(&faceCacheLock);
    CFRelease(key);
    return face;
}

CTFace *CTFaceCreateWithFTFace(FT_Face ft) {
    const char *path = NULL;

    if (ft == NULL)
        return NULL;
    // a face opened from a file is reopened in our own library, memory faces are shared
    if (ft->stream != NULL && ft->stream->pathname.pointer != NULL)
        path = ft->stream->pathname.pointer;
    if (path != NULL) {
        CTFace *face = CTFaceCreateWithPath(path, ft->face_index);

        if (face != NULL)
            return face;
    }
    FT_Reference_Face(ft);
    return faceFromFT(ft, NULL, ft->face_index);
}

CTFace *CTFaceRetain(CTFace *face) {
    if (face != NULL)
        __atomic_add_fetch(&face->refs, 1, __ATOMIC_RELAXED);
    return face;
}

void CTFaceRelease(CTFace *face) {
    if (face == NULL || __atomic_sub_fetch(&face->refs, 1, __ATOMIC_ACQ_REL) != 0)
        return;
    hb_face_destroy(face->hb);
    pthread_mutex_lock(&libraryLock);
    FT_Done_Face(face->ft);
    pthread_mutex_unlock(&libraryLock);
    if (face->postScriptName) CFRelease(face->postScriptName);
    if (face->familyName) CFRelease(face->familyName);
    if (face->styleName) CFRelease(face->styleName);
    if (face->fullName) CFRelease(face->fullName);
    pthread_mutex_destroy(&face->lock);
    free(face->path);
    free(face);
}

// the English name with the given ID, Windows Unicode names first, then Mac Roman ones
CFStringRef CTFaceCopyName(CTFace *face, int nameID) {
    FT_UInt count;
    int best = -1, bestScore = 0;
    FT_SfntName name;
    CFStringRef result = NULL;

    pthread_mutex_lock(&face->lock);
    count = FT_Get_Sfnt_Name_Count(face->ft);
    for (FT_UInt i = 0; i < count; i++) {
        int score = 0;

        if (FT_Get_Sfnt_Name(face->ft, i, &name) != 0 || name.name_id != nameID)
            continue;
        if (name.platform_id == TT_PLATFORM_MICROSOFT && name.language_id == TT_MS_LANGID_ENGLISH_UNITED_STATES)
            score = 4;
        else if (name.platform_id == TT_PLATFORM_MICROSOFT)
            score = 3;
        else if (name.platform_id == TT_PLATFORM_MACINTOSH && name.encoding_id == TT_MAC_ID_ROMAN && name.language_id == 0)
            score = 2;
        else if (name.platform_id == TT_PLATFORM_APPLE_UNICODE)
            score = 1;
        if (score > bestScore) {
            bestScore = score;
            best = i;
        }
    }
    if (best >= 0 && FT_Get_Sfnt_Name(face->ft, best, &name) == 0) {
        if (name.platform_id == TT_PLATFORM_MACINTOSH)
            result = CFStringCreateWithBytes(NULL, name.string, name.string_len, kCFStringEncodingMacRoman, false);
        else
            result = CFStringCreateWithBytes(NULL, name.string, name.string_len, kCFStringEncodingUTF16BE, false);
    }
    pthread_mutex_unlock(&face->lock);
    return result;
}

// OS/2 weight classes to CoreText's weights, interpolated between the standard ones
CGFloat CTWeightFromOS2Class(int weightClass) {
    static const int classes[] = { 100, 200, 300, 400, 500, 600, 700, 800, 900 };
    static const CGFloat weights[] = { -0.8, -0.6, -0.4, 0.0, 0.23, 0.3, 0.4, 0.56, 0.62 };

    if (weightClass <= 100)
        return weights[0];
    if (weightClass >= 900)
        return weights[8];
    for (int i = 0; i < 8; i++) {
        if (weightClass <= classes[i + 1]) {
            CGFloat t = (CGFloat)(weightClass - classes[i]) / (classes[i + 1] - classes[i]);

            return weights[i] + t * (weights[i + 1] - weights[i]);
        }
    }
    return 0;
}

CFDictionaryRef CTCopyTraitsDictionary(uint32_t symbolic, CGFloat weight, CGFloat width, CGFloat slant) {
    CFNumberRef numbers[4];
    const void *keys[4] = { kCTFontSymbolicTrait, kCTFontWeightTrait, kCTFontWidthTrait, kCTFontSlantTrait };
    CFDictionaryRef dict;

    numbers[0] = CFNumberCreate(NULL, kCFNumberSInt32Type, &symbolic);
    numbers[1] = CFNumberCreate(NULL, kCFNumberCGFloatType, &weight);
    numbers[2] = CFNumberCreate(NULL, kCFNumberCGFloatType, &width);
    numbers[3] = CFNumberCreate(NULL, kCFNumberCGFloatType, &slant);
    dict = CFDictionaryCreate(NULL, keys, (const void **)numbers, 4, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    for (int i = 0; i < 4; i++)
        CFRelease(numbers[i]);
    return dict;
}

// the registry: every face fontconfig knows plus the font folders a Mac has
static pthread_mutex_t registryLock;
static pthread_once_t registryLockOnce = PTHREAD_ONCE_INIT;
static FcConfig *registryConfig;
static CTFontRecord *records;
static CFIndex recordCount;
static bool registryBuilt;
static CFMutableDictionaryRef fallbackCache;

static void initRegistryLock(void) {
    pthread_mutexattr_t attr;

    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&registryLock, &attr);
    pthread_mutexattr_destroy(&attr);
}

void CTRegistryLock(void) {
    pthread_once(&registryLockOnce, initRegistryLock);
    pthread_mutex_lock(&registryLock);
}

void CTRegistryUnlock(void) {
    pthread_mutex_unlock(&registryLock);
}

static FcConfig *config(void) {
    if (registryConfig == NULL) {
        const char *dirs[] = { "/System/Library/Fonts", "/Library/Fonts" };
        const char *home = getenv("HOME");

        registryConfig = FcInitLoadConfigAndFonts();
        if (registryConfig == NULL)
            registryConfig = FcConfigCreate();
        for (int i = 0; i < 2; i++)
            FcConfigAppFontAddDir(registryConfig, (const FcChar8 *)dirs[i]);
        if (home != NULL) {
            char user[1024];

            snprintf(user, sizeof(user), "%s/Library/Fonts", home);
            FcConfigAppFontAddDir(registryConfig, (const FcChar8 *)user);
        }
    }
    return registryConfig;
}

static CFStringRef copyFcString(FcPattern *p, const char *object) {
    FcChar8 *s = NULL;

    if (FcPatternGetString(p, object, 0, &s) != FcResultMatch || s == NULL)
        return NULL;
    return CFStringCreateWithCString(NULL, (const char *)s, kCFStringEncodingUTF8);
}

// records handed out stay valid, so a rebuild abandons the old array instead of freeing it
static void freeRecords(void) {
    records = NULL;
    recordCount = 0;
}

static int compareRecords(const void *a, const void *b) {
    const CTFontRecord *x = a, *y = b;
    CFComparisonResult c = CFStringCompare(x->familyName ?: CFSTR(""), y->familyName ?: CFSTR(""), kCFCompareCaseInsensitive);

    if (c != kCFCompareEqualTo)
        return (int)c;
    if (x->weight != y->weight)
        return x->weight < y->weight ? -1 : 1;
    if (x->slant != y->slant)
        return x->slant < y->slant ? -1 : 1;
    return CFStringCompare(x->postScriptName ?: CFSTR(""), y->postScriptName ?: CFSTR(""), 0);
}

static void buildRegistry(void) {
    FcPattern *pattern;
    FcObjectSet *objects;
    FcFontSet *set;
    CFIndex capacity;

    if (registryBuilt)
        return;
    registryBuilt = true;
    pattern = FcPatternCreate();
    objects = FcObjectSetBuild(FC_FILE, FC_INDEX, FC_POSTSCRIPT_NAME, FC_FAMILY, FC_STYLE, FC_FULLNAME, FC_WEIGHT,
                               FC_WIDTH, FC_SLANT, FC_SPACING, FC_COLOR, FC_VARIABLE, NULL);
    set = FcFontList(config(), pattern, objects);
    FcObjectSetDestroy(objects);
    FcPatternDestroy(pattern);
    if (set == NULL)
        return;
    capacity = set->nfont;
    records = calloc(capacity ? capacity : 1, sizeof(CTFontRecord));
    for (int i = 0; i < set->nfont; i++) {
        FcPattern *p = set->fonts[i];
        FcChar8 *file = NULL;
        int index = 0, weight = FC_WEIGHT_REGULAR, width = FC_WIDTH_NORMAL, slant = FC_SLANT_ROMAN, spacing = FC_PROPORTIONAL;
        FcBool variable = FcFalse, color = FcFalse;
        CTFontRecord *r;
        bool duplicate = false;

        if (FcPatternGetString(p, FC_FILE, 0, &file) != FcResultMatch)
            continue;
        FcPatternGetInteger(p, FC_INDEX, 0, &index);
        FcPatternGetBool(p, FC_VARIABLE, 0, &variable);
        // a variable font is listed as itself and as each named instance, the instances are the faces
        if (variable)
            continue;
        for (CFIndex j = 0; j < recordCount && !duplicate; j++)
            duplicate = records[j].index == index && strcmp(records[j].path, (const char *)file) == 0;
        if (duplicate)
            continue;
        FcPatternGetInteger(p, FC_WEIGHT, 0, &weight);
        FcPatternGetInteger(p, FC_WIDTH, 0, &width);
        FcPatternGetInteger(p, FC_SLANT, 0, &slant);
        FcPatternGetInteger(p, FC_SPACING, 0, &spacing);
        FcPatternGetBool(p, FC_COLOR, 0, &color);
        r = &records[recordCount];
        r->path = strdup((const char *)file);
        r->index = index;
        r->postScriptName = copyFcString(p, FC_POSTSCRIPT_NAME);
        r->familyName = copyFcString(p, FC_FAMILY);
        r->styleName = copyFcString(p, FC_STYLE);
        r->fullName = copyFcString(p, FC_FULLNAME);
        if (r->familyName == NULL || r->postScriptName == NULL) {
            free(r->path);
            if (r->postScriptName) CFRelease(r->postScriptName);
            if (r->familyName) CFRelease(r->familyName);
            if (r->styleName) CFRelease(r->styleName);
            if (r->fullName) CFRelease(r->fullName);
            memset(r, 0, sizeof(*r));
            continue;
        }
        if (r->fullName == NULL)
            r->fullName = CFStringCreateWithFormat(NULL, NULL, CFSTR("%@ %@"), r->familyName, r->styleName ?: CFSTR("Regular"));
        r->weight = CTWeightFromOS2Class(FcWeightToOpenType(weight));
        // fontconfig widths are percentages, 100 is normal and 50 to 200 the range
        r->width = fmax(-1.0, fmin(1.0, (width - 100) / 100.0));
        r->slant = slant == FC_SLANT_ROMAN ? 0.0 : (slant == FC_SLANT_OBLIQUE ? 0.07 : 0.06);
        if (slant != FC_SLANT_ROMAN) r->symbolicTraits |= kCTFontTraitItalic;
        if (FcWeightToOpenType(weight) >= 600) r->symbolicTraits |= kCTFontTraitBold;
        if (width < FC_WIDTH_NORMAL) r->symbolicTraits |= kCTFontTraitCondensed;
        if (width > FC_WIDTH_NORMAL) r->symbolicTraits |= kCTFontTraitExpanded;
        if (spacing == FC_MONO || spacing == FC_CHARCELL) r->symbolicTraits |= kCTFontTraitMonoSpace;
        if (color) r->symbolicTraits |= kCTFontTraitColorGlyphs;
        recordCount++;
    }
    FcFontSetDestroy(set);
    qsort(records, recordCount, sizeof(CTFontRecord), compareRecords);
}

static CFIndex CTRegistryCountUnlocked(void) {
    buildRegistry();
    return recordCount;
}

static const CTFontRecord *CTRegistryRecordAtIndexUnlocked(CFIndex index) {
    buildRegistry();
    return index >= 0 && index < recordCount ? &records[index] : NULL;
}

static const CTFontRecord *CTRegistryRecordForPostScriptNameUnlocked(CFStringRef name) {
    buildRegistry();
    if (name == NULL)
        return NULL;
    for (CFIndex i = 0; i < recordCount; i++) {
        if (CFEqual(records[i].postScriptName, name))
            return &records[i];
    }
    return NULL;
}

static const CTFontRecord *CTRegistryRecordForPathUnlocked(const char *path, long index) {
    buildRegistry();
    if (path == NULL)
        return NULL;
    for (CFIndex i = 0; i < recordCount; i++) {
        if (records[i].index == index && strcmp(records[i].path, path) == 0)
            return &records[i];
    }
    return NULL;
}

static void CTRegistryInvalidateUnlocked(void) {
    freeRecords();
    registryBuilt = false;
    if (fallbackCache != NULL)
        CFDictionaryRemoveAllValues(fallbackCache);
}

static bool CTRegistryAddFileUnlocked(const char *path) {
    bool ok = FcConfigAppFontAddFile(config(), (const FcChar8 *)path) == FcTrue;

    if (ok)
        CTRegistryInvalidateUnlocked();
    return ok;
}

static bool CTRegistryRemoveFileUnlocked(const char *path) {
    // fontconfig can't drop one application font, so rebuild the set without it
    FcFontSet *app = FcConfigGetFonts(config(), FcSetApplication);
    bool found = false;
    FcStrSet *keep = FcStrSetCreate();

    if (app != NULL) {
        for (int i = 0; i < app->nfont; i++) {
            FcChar8 *file = NULL;

            if (FcPatternGetString(app->fonts[i], FC_FILE, 0, &file) != FcResultMatch)
                continue;
            if (strcmp((const char *)file, path) == 0)
                found = true;
            else
                FcStrSetAdd(keep, file);
        }
    }
    if (found) {
        FcStrList *list = FcStrListCreate(keep);
        FcChar8 *file;

        FcConfigAppFontClear(config());
        while ((file = FcStrListNext(list)) != NULL)
            FcConfigAppFontAddFile(config(), file);
        FcStrListDone(list);
        CTRegistryInvalidateUnlocked();
    }
    FcStrSetDestroy(keep);
    return found;
}

CFDictionaryRef CTRecordCopyAttributes(const CTFontRecord *record) {
    CFMutableDictionaryRef dict = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionaryRef traits = CTCopyTraitsDictionary(record->symbolicTraits, record->weight, record->width, record->slant);
    CFURLRef url = CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8 *)record->path, strlen(record->path), false);

    CFDictionarySetValue(dict, kCTFontNameAttribute, record->postScriptName);
    CFDictionarySetValue(dict, kCTFontFamilyNameAttribute, record->familyName);
    if (record->styleName)
        CFDictionarySetValue(dict, kCTFontStyleNameAttribute, record->styleName);
    CFDictionarySetValue(dict, kCTFontDisplayNameAttribute, record->fullName);
    CFDictionarySetValue(dict, kCTFontTraitsAttribute, traits);
    if (url != NULL) {
        CFDictionarySetValue(dict, kCTFontURLAttribute, url);
        CFRelease(url);
    }
    CFRelease(traits);
    return dict;
}

static bool equalCaseless(CFStringRef a, CFStringRef b) {
    return a != NULL && b != NULL && CFStringCompare(a, b, kCFCompareCaseInsensitive) == kCFCompareEqualTo;
}

static CGFloat numberValue(CFDictionaryRef dict, CFStringRef key, CGFloat fallback, bool *present) {
    CFNumberRef n = dict ? CFDictionaryGetValue(dict, key) : NULL;
    CGFloat v = fallback;

    if (n != NULL && CFGetTypeID(n) == CFNumberGetTypeID()) {
        CFNumberGetValue(n, kCFNumberCGFloatType, &v);
        if (present) *present = true;
    }
    return v;
}

typedef struct {
    const CTFontRecord *record;
    double score;
} Match;

static int compareMatches(const void *a, const void *b) {
    const Match *x = a, *y = b;

    if (x->score != y->score)
        return x->score < y->score ? -1 : 1;
    return x->record < y->record ? -1 : (x->record > y->record);
}

// a PostScript-style name split at the dash, its style words picked up as weight and slant
static const CTFontRecord *aliasRecordForName(CFStringRef name) {
    // the core macOS families map to the metric-compatible or closest free faces
    static const char *const aliases[][2] = {
        { "Helvetica", "Nimbus Sans" }, { "HelveticaNeue", "Nimbus Sans" }, { "Helvetica Neue", "Nimbus Sans" },
        { "Arial", "Nimbus Sans" }, { "ArialMT", "Nimbus Sans" }, { "Times", "Nimbus Roman" }, { "Times New Roman", "Nimbus Roman" },
        { "TimesNewRomanPSMT", "Nimbus Roman" }, { "Courier", "Nimbus Mono PS" }, { "Courier New", "Nimbus Mono PS" },
        { "CourierNewPSMT", "Nimbus Mono PS" }, { "Menlo", "DejaVu Sans Mono" }, { "Monaco", "DejaVu Sans Mono" },
        { "SF Mono", "DejaVu Sans Mono" }, { "Lucida Grande", "Inter" }, { "LucidaGrande", "Inter" }, { ".AppleSystemUIFont", "Inter" },
        { "SF Pro", "Inter" }, { "SF Pro Text", "Inter" }, { "SF NS", "Inter" }, { "system-ui", "Inter" }, { "Geneva", "DejaVu Sans" },
        { "Verdana", "DejaVu Sans" }, { "Georgia", "DejaVu Serif" }, { "Palatino", "P052" }, { "Bookman", "URW Bookman" },
    };
    char buffer[256], *dash;
    FcPattern *pattern, *match;
    FcResult result;
    FcChar8 *file = NULL;
    int index = 0;
    const CTFontRecord *record = NULL;

    if (!CFStringGetCString(name, buffer, sizeof(buffer), kCFStringEncodingUTF8)) return NULL;
    dash = buffer[0] == '.' ? NULL : strchr(buffer, '-');
    if (dash) *dash++ = 0;
    pattern = FcPatternCreate();
    for (size_t i = 0; i < sizeof(aliases) / sizeof(aliases[0]); i++) {
        if (strcasecmp(buffer, aliases[i][0]) == 0) {
            FcPatternAddString(pattern, FC_FAMILY, (const FcChar8 *)aliases[i][1]);
            break;
        }
    }
    FcPatternAddString(pattern, FC_FAMILY, (const FcChar8 *)buffer);
    if (dash && (strstr(dash, "Bold") || strstr(dash, "Black") || strstr(dash, "Heavy")))
        FcPatternAddInteger(pattern, FC_WEIGHT, FC_WEIGHT_BOLD);
    if (dash && (strstr(dash, "Italic") || strstr(dash, "Oblique")))
        FcPatternAddInteger(pattern, FC_SLANT, FC_SLANT_ITALIC);
    FcConfigSubstitute(config(), pattern, FcMatchPattern);
    FcDefaultSubstitute(pattern);
    match = FcFontMatch(config(), pattern, &result);
    if (match && FcPatternGetString(match, FC_FILE, 0, &file) == FcResultMatch) {
        FcPatternGetInteger(match, FC_INDEX, 0, &index);
        record = CTRegistryRecordForPathUnlocked((const char *)file, index & 0xffff);
    }
    if (match) FcPatternDestroy(match);
    FcPatternDestroy(pattern);
    return record;
}

static CFIndex CTRegistryCopyMatchesUnlocked(CFDictionaryRef attributes, const CTFontRecord ***result) {
    CFStringRef name = attributes ? CFDictionaryGetValue(attributes, kCTFontNameAttribute) : NULL;
    CFStringRef family = attributes ? CFDictionaryGetValue(attributes, kCTFontFamilyNameAttribute) : NULL;
    CFStringRef style = attributes ? CFDictionaryGetValue(attributes, kCTFontStyleNameAttribute) : NULL;
    CFStringRef display = attributes ? CFDictionaryGetValue(attributes, kCTFontDisplayNameAttribute) : NULL;
    CFURLRef url = attributes ? CFDictionaryGetValue(attributes, kCTFontURLAttribute) : NULL;
    CFDictionaryRef traits = attributes ? CFDictionaryGetValue(attributes, kCTFontTraitsAttribute) : NULL;
    bool hasWeight = false, hasWidth = false, hasSlant = false, hasSymbolic = false;
    CGFloat weight = numberValue(traits, kCTFontWeightTrait, 0, &hasWeight);
    CGFloat width = numberValue(traits, kCTFontWidthTrait, 0, &hasWidth);
    CGFloat slant = numberValue(traits, kCTFontSlantTrait, 0, &hasSlant);
    uint32_t symbolic = (uint32_t)numberValue(traits, kCTFontSymbolicTrait, 0, &hasSymbolic);
    char path[4096];
    bool hasPath = false;
    Match *matches;
    CFIndex count = 0;

    if (name != NULL && CFGetTypeID(name) != CFStringGetTypeID()) name = NULL;
    if (family != NULL && CFGetTypeID(family) != CFStringGetTypeID()) family = NULL;
    if (style != NULL && CFGetTypeID(style) != CFStringGetTypeID()) style = NULL;
    if (display != NULL && CFGetTypeID(display) != CFStringGetTypeID()) display = NULL;
    if (url != NULL && CFGetTypeID(url) == CFURLGetTypeID())
        hasPath = CFURLGetFileSystemRepresentation(url, true, (UInt8 *)path, sizeof(path));
    buildRegistry();
    matches = calloc(recordCount ? recordCount : 1, sizeof(Match));
    for (CFIndex i = 0; i < recordCount; i++) {
        const CTFontRecord *r = &records[i];
        double score = 0;

        if (hasPath && strcmp(r->path, path) != 0)
            continue;
        if (name != NULL) {
            // a name matches the PostScript name exactly, else the full name, else a family's regular face
            if (CFEqual(r->postScriptName, name))
                score -= 100;
            else if (equalCaseless(r->fullName, name))
                score -= 90;
            else if (equalCaseless(r->familyName, name))
                score -= 50 - (r->weight == 0 && r->slant == 0 ? 1 : 0);
            else
                continue;
        }
        if (family != NULL && !equalCaseless(r->familyName, family))
            continue;
        if (display != NULL && !equalCaseless(r->fullName, display))
            continue;
        if (style != NULL)
            score += equalCaseless(r->styleName, style) ? -10 : 5;
        if (hasSymbolic) {
            uint32_t wanted = symbolic & (kCTFontTraitItalic | kCTFontTraitBold | kCTFontTraitCondensed | kCTFontTraitExpanded | kCTFontTraitMonoSpace);

            if (wanted & kCTFontTraitMonoSpace && !(r->symbolicTraits & kCTFontTraitMonoSpace))
                continue;
            score += __builtin_popcount((r->symbolicTraits ^ wanted) & (kCTFontTraitItalic | kCTFontTraitBold)) * 3;
            if ((wanted & kCTFontTraitBold) == 0 && (r->symbolicTraits & kCTFontTraitBold) == 0)
                score += fabs(r->weight) * 2;
        }
        if (hasWeight) score += fabs(r->weight - weight) * 4;
        if (hasWidth) score += fabs(r->width - width) * 2;
        if (hasSlant) score += fabs(r->slant - slant) * 2;
        if (!hasWeight && !hasSymbolic && name == NULL)
            score += fabs(r->weight) + r->slant;
        matches[count].record = r;
        matches[count].score = score;
        count++;
    }
    // names no installed face carries go through fontconfig's aliases, Helvetica to Nimbus Sans and so on
    if (count == 0 && name != NULL && family == NULL && !hasPath) {
        const CTFontRecord *alias = aliasRecordForName(name);

        if (alias != NULL) {
            matches[0].record = alias;
            matches[0].score = 0;
            count = 1;
        }
    }
    qsort(matches, count, sizeof(Match), compareMatches);
    *result = calloc(count ? count : 1, sizeof(CTFontRecord *));
    for (CFIndex i = 0; i < count; i++)
        (*result)[i] = matches[i].record;
    free(matches);
    return count;
}

static const CTFontRecord *CTRegistryDefaultRecordUnlocked(bool bold) {
    const CTFontRecord *r = CTRegistryRecordForPostScriptNameUnlocked(bold ? CFSTR("Inter-Bold") : CFSTR("Inter-Regular"));

    if (r == NULL)
        r = CTRegistryRecordForPostScriptNameUnlocked(bold ? CFSTR("NimbusSans-Bold") : CFSTR("NimbusSans-Regular"));
    if (r == NULL && recordCount > 0)
        r = &records[0];
    return r;
}

static const CTFontRecord *CTRegistryFallbackForCharacterUnlocked(UTF32Char c, CFStringRef language, const CTFontRecord *preferred) {
    FcPattern *pattern;
    FcCharSet *charset;
    FcResult result;
    FcFontSet *sorted;
    const CTFontRecord *found = NULL;
    CFStringRef key;

    buildRegistry();
    key = CFStringCreateWithFormat(NULL, NULL, CFSTR("%u|%@|%@"), (unsigned)c, language ?: CFSTR(""),
                                   preferred ? preferred->postScriptName : CFSTR(""));
    if (fallbackCache == NULL)
        fallbackCache = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, NULL);
    if (CFDictionaryGetValueIfPresent(fallbackCache, key, (const void **)&found)) {
        CFRelease(key);
        return found;
    }
    pattern = FcPatternCreate();
    charset = FcCharSetCreate();
    FcCharSetAddChar(charset, c);
    FcPatternAddCharSet(pattern, FC_CHARSET, charset);
    if (preferred != NULL) {
        char family[256];

        if (CFStringGetCString(preferred->familyName, family, sizeof(family), kCFStringEncodingUTF8))
            FcPatternAddString(pattern, FC_FAMILY, (const FcChar8 *)family);
        FcPatternAddInteger(pattern, FC_WEIGHT, FcWeightFromOpenType(preferred->symbolicTraits & kCTFontTraitBold ? 700 : 400));
        FcPatternAddInteger(pattern, FC_SLANT, preferred->symbolicTraits & kCTFontTraitItalic ? FC_SLANT_ITALIC : FC_SLANT_ROMAN);
    }
    if (language != NULL) {
        char lang[64];

        if (CFStringGetCString(language, lang, sizeof(lang), kCFStringEncodingUTF8))
            FcPatternAddString(pattern, FC_LANG, (const FcChar8 *)lang);
    }
    FcConfigSubstitute(config(), pattern, FcMatchPattern);
    FcDefaultSubstitute(pattern);
    sorted = FcFontSort(config(), pattern, FcFalse, NULL, &result);
    if (sorted != NULL) {
        for (int i = 0; i < sorted->nfont && found == NULL; i++) {
            FcCharSet *fontSet = NULL;
            FcChar8 *file = NULL;
            int index = 0;

            if (FcPatternGetCharSet(sorted->fonts[i], FC_CHARSET, 0, &fontSet) != FcResultMatch || !FcCharSetHasChar(fontSet, c))
                continue;
            if (FcPatternGetString(sorted->fonts[i], FC_FILE, 0, &file) != FcResultMatch)
                continue;
            FcPatternGetInteger(sorted->fonts[i], FC_INDEX, 0, &index);
            found = CTRegistryRecordForPathUnlocked((const char *)file, index);
        }
        FcFontSetDestroy(sorted);
    }
    FcCharSetDestroy(charset);
    FcPatternDestroy(pattern);
    CFDictionarySetValue(fallbackCache, key, found);
    CFRelease(key);
    return found;
}

// locked entry points, the registry is shared by every thread
CFIndex CTRegistryCount(void) {
    CFIndex n;

    CTRegistryLock();
    n = CTRegistryCountUnlocked();
    CTRegistryUnlock();
    return n;
}

const CTFontRecord *CTRegistryRecordAtIndex(CFIndex index) {
    const CTFontRecord *r;

    CTRegistryLock();
    r = CTRegistryRecordAtIndexUnlocked(index);
    CTRegistryUnlock();
    return r;
}

const CTFontRecord *CTRegistryRecordForPostScriptName(CFStringRef name) {
    const CTFontRecord *r;

    CTRegistryLock();
    r = CTRegistryRecordForPostScriptNameUnlocked(name);
    CTRegistryUnlock();
    return r;
}

const CTFontRecord *CTRegistryRecordForPath(const char *path, long index) {
    const CTFontRecord *r;

    CTRegistryLock();
    r = CTRegistryRecordForPathUnlocked(path, index);
    CTRegistryUnlock();
    return r;
}

void CTRegistryInvalidate(void) {
    CTRegistryLock();
    CTRegistryInvalidateUnlocked();
    CTRegistryUnlock();
}

bool CTRegistryAddFile(const char *path) {
    bool ok;

    CTRegistryLock();
    ok = CTRegistryAddFileUnlocked(path);
    CTRegistryUnlock();
    return ok;
}

bool CTRegistryRemoveFile(const char *path) {
    bool ok;

    CTRegistryLock();
    ok = CTRegistryRemoveFileUnlocked(path);
    CTRegistryUnlock();
    return ok;
}

CFIndex CTRegistryCopyMatches(CFDictionaryRef attributes, const CTFontRecord ***result) {
    CFIndex n;

    CTRegistryLock();
    n = CTRegistryCopyMatchesUnlocked(attributes, result);
    CTRegistryUnlock();
    return n;
}

const CTFontRecord *CTRegistryDefaultRecord(bool bold) {
    const CTFontRecord *r;

    CTRegistryLock();
    r = CTRegistryDefaultRecordUnlocked(bold);
    CTRegistryUnlock();
    return r;
}

const CTFontRecord *CTRegistryFallbackForCharacter(UTF32Char c, CFStringRef language, const CTFontRecord *preferred) {
    const CTFontRecord *r;

    CTRegistryLock();
    r = CTRegistryFallbackForCharacterUnlocked(c, language, preferred);
    CTRegistryUnlock();
    return r;
}
