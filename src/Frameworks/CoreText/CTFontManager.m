// CTFontManager.m - registering font files and listing installed fonts
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#include <dispatch/dispatch.h>
#include <stdatomic.h>
#import "CTInternal.h"
#include <sys/stat.h>

static CFArrayRef copyRecordStrings(bool family) {
    CFMutableArrayRef result = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    CFMutableSetRef seen = CFSetCreateMutable(NULL, 0, &kCFTypeSetCallBacks);
    CFIndex count = CTRegistryCount();

    for (CFIndex i = 0; i < count; i++) {
        const CTFontRecord *r = CTRegistryRecordAtIndex(i);
        CFStringRef s = r ? (family ? r->familyName : r->postScriptName) : NULL;

        if (s && !CFSetContainsValue(seen, s)) {
            CFSetAddValue(seen, s);
            CFArrayAppendValue(result, s);
        }
    }
    CFRelease(seen);
    if (family)
        CFArraySortValues(result, CFRangeMake(0, CFArrayGetCount(result)), CTFontManagerCompareFontFamilyNames, NULL);
    return result;
}

CFArrayRef CTFontManagerCopyAvailablePostScriptNames(void) {
    return copyRecordStrings(false);
}

CFArrayRef CTFontManagerCopyAvailableFontFamilyNames(void) {
    return copyRecordStrings(true);
}

CFArrayRef CTFontManagerCopyAvailableFontURLs(void) {
    CFMutableArrayRef result = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    CFMutableSetRef seen = CFSetCreateMutable(NULL, 0, &kCFTypeSetCallBacks);
    CFIndex count = CTRegistryCount();

    for (CFIndex i = 0; i < count; i++) {
        const CTFontRecord *r = CTRegistryRecordAtIndex(i);
        CFURLRef url;

        if (r == NULL) continue;
        url = CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8 *)r->path, strlen(r->path), false);
        if (!CFSetContainsValue(seen, url)) {
            CFSetAddValue(seen, url);
            CFArrayAppendValue(result, url);
        }
        CFRelease(url);
    }
    CFRelease(seen);
    return result;
}

CFComparisonResult CTFontManagerCompareFontFamilyNames(const void *family1, const void *family2, void *context) {
    return CFStringCompare(family1, family2, kCFCompareCaseInsensitive | kCFCompareLocalized);
}

static bool pathOfURL(CFURLRef url, char *path, size_t size) {
    return url && CFURLGetFileSystemRepresentation(url, true, (UInt8 *)path, size);
}

// every face in a file, as descriptors
CFArrayRef CTFontManagerCreateFontDescriptorsFromURL(CFURLRef fileURL) {
    char path[PATH_MAX];
    CFMutableArrayRef result;
    FT_Face probe;
    long faces;

    if (!pathOfURL(fileURL, path, sizeof(path))) return NULL;
    if (FT_New_Face(CTFreeTypeLibrary(), path, -1, &probe) != 0) return NULL;
    faces = probe->num_faces;
    FT_Done_Face(probe);
    result = CFArrayCreateMutable(NULL, faces, &kCFTypeArrayCallBacks);
    for (long i = 0; i < faces; i++) {
        CTFace *face = CTFaceCreateWithPath(path, i);
        CFMutableDictionaryRef attrs;
        CTFontDescriptorRef d;

        if (face == NULL) continue;
        attrs = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        if (face->postScriptName) CFDictionarySetValue(attrs, kCTFontNameAttribute, face->postScriptName);
        if (face->familyName) CFDictionarySetValue(attrs, kCTFontFamilyNameAttribute, face->familyName);
        if (face->styleName) CFDictionarySetValue(attrs, kCTFontStyleNameAttribute, face->styleName);
        if (face->fullName) CFDictionarySetValue(attrs, kCTFontDisplayNameAttribute, face->fullName);
        CFDictionarySetValue(attrs, kCTFontURLAttribute, fileURL);
        d = CTFontDescriptorCreateWithAttributes(attrs);
        CFArrayAppendValue(result, d);
        CFRelease(d);
        CFRelease(attrs);
        CTFaceRelease(face);
    }
    if (CFArrayGetCount(result) == 0) {
        CFRelease(result);
        return NULL;
    }
    return result;
}

// data fonts go through a private file so the registry, FreeType and fontconfig all see a path
static CFURLRef copyURLForData(CFDataRef data) {
    char path[] = "/tmp/CoreText.XXXXXX";
    int fd = mkstemp(path);
    CFURLRef url;

    if (fd < 0) return NULL;
    if (write(fd, CFDataGetBytePtr(data), CFDataGetLength(data)) != CFDataGetLength(data)) {
        close(fd);
        unlink(path);
        return NULL;
    }
    close(fd);
    url = CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8 *)path, strlen(path), false);
    return url;
}

CFArrayRef CTFontManagerCreateFontDescriptorsFromData(CFDataRef data) {
    CFURLRef url = data ? copyURLForData(data) : NULL;
    CFArrayRef result;

    if (url == NULL) return NULL;
    CTFontManagerRegisterFontsForURL(url, kCTFontManagerScopeProcess, NULL);
    result = CTFontManagerCreateFontDescriptorsFromURL(url);
    CFRelease(url);
    return result;
}

CTFontDescriptorRef CTFontManagerCreateFontDescriptorFromData(CFDataRef data) {
    CFArrayRef all = CTFontManagerCreateFontDescriptorsFromData(data);
    CTFontDescriptorRef d = NULL;

    if (all && CFArrayGetCount(all) > 0) d = CFRetain(CFArrayGetValueAtIndex(all, 0));
    if (all) CFRelease(all);
    return d;
}

static CFErrorRef createError(CFIndex code, CFURLRef url) {
    const void *key = kCTFontManagerErrorFontURLsKey;
    CFArrayRef urls = url ? CFArrayCreate(NULL, (const void **)&url, 1, &kCFTypeArrayCallBacks) : NULL;
    CFDictionaryRef info = urls ? CFDictionaryCreate(NULL, &key, (const void **)&urls, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks) : NULL;
    CFErrorRef error = CFErrorCreate(NULL, kCTFontManagerErrorDomain, code, info);

    if (info) CFRelease(info);
    if (urls) CFRelease(urls);
    return error;
}

// delivered later from the main queue as on macOS, observers often register fonts while building
// the very list the notification asks them to rebuild, and a burst of registrations posts once
static void postChanged(void) {
    static _Atomic int pending;

    if (atomic_exchange(&pending, 1))
        return;
    dispatch_async(dispatch_get_main_queue(), ^{
        atomic_store(&pending, 0);
        CFNotificationCenterPostNotification(CFNotificationCenterGetLocalCenter(), kCTFontManagerRegisteredFontsChangedNotification, NULL, NULL, true);
    });
}

bool CTFontManagerRegisterFontsForURL(CFURLRef fontURL, CTFontManagerScope scope, CFErrorRef *error) {
    char path[PATH_MAX];
    struct stat st;
    CFIndex code = 0;

    if (!pathOfURL(fontURL, path, sizeof(path)) || stat(path, &st) != 0)
        code = kCTFontManagerErrorFileNotFound;
    else if (!CTFontManagerIsSupportedFont(fontURL))
        code = kCTFontManagerErrorInvalidFontData;
    else if (!CTRegistryAddFile(path))
        code = kCTFontManagerErrorAlreadyRegistered;
    if (code) {
        if (error) *error = createError(code, fontURL);
        return false;
    }
    postChanged();
    return true;
}

bool CTFontManagerUnregisterFontsForURL(CFURLRef fontURL, CTFontManagerScope scope, CFErrorRef *error) {
    char path[PATH_MAX];

    if (!pathOfURL(fontURL, path, sizeof(path)) || !CTRegistryRemoveFile(path)) {
        if (error) *error = createError(kCTFontManagerErrorNotRegistered, fontURL);
        return false;
    }
    postChanged();
    return true;
}

static bool forEachURL(CFArrayRef urls, CTFontManagerScope scope, CFArrayRef *errors, bool (*fn)(CFURLRef, CTFontManagerScope, CFErrorRef *)) {
    CFMutableArrayRef list = errors ? CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks) : NULL;
    bool all = true;

    for (CFIndex i = 0; urls && i < CFArrayGetCount(urls); i++) {
        CFErrorRef e = NULL;

        if (!fn(CFArrayGetValueAtIndex(urls, i), scope, &e)) {
            all = false;
            if (list && e) CFArrayAppendValue(list, e);
        }
        if (e) CFRelease(e);
    }
    if (errors) *errors = list;
    return all;
}

bool CTFontManagerRegisterFontsForURLs(CFArrayRef fontURLs, CTFontManagerScope scope, CFArrayRef *errors) {
    return forEachURL(fontURLs, scope, errors, CTFontManagerRegisterFontsForURL);
}

bool CTFontManagerUnregisterFontsForURLs(CFArrayRef fontURLs, CTFontManagerScope scope, CFArrayRef *errors) {
    return forEachURL(fontURLs, scope, errors, CTFontManagerUnregisterFontsForURL);
}

// graphics fonts already come from files the registry knows, so registering them only checks they exist
bool CTFontManagerRegisterGraphicsFont(CGFontRef font, CFErrorRef *error) {
    if (font == NULL) {
        if (error) *error = createError(kCTFontManagerErrorInvalidFontData, NULL);
        return false;
    }
    return true;
}

bool CTFontManagerUnregisterGraphicsFont(CGFontRef font, CFErrorRef *error) {
    return font != NULL;
}

CTFontManagerScope CTFontManagerGetScopeForURL(CFURLRef fontURL) {
    char path[PATH_MAX];
    CFIndex count = CTRegistryCount();

    if (!pathOfURL(fontURL, path, sizeof(path))) return kCTFontManagerScopeNone;
    for (CFIndex i = 0; i < count; i++) {
        const CTFontRecord *r = CTRegistryRecordAtIndex(i);

        if (r && strcmp(r->path, path) == 0)
            return kCTFontManagerScopeProcess;
    }
    return kCTFontManagerScopeNone;
}

bool CTFontManagerIsSupportedFont(CFURLRef fontURL) {
    char path[PATH_MAX];
    FT_Face face;

    if (!pathOfURL(fontURL, path, sizeof(path))) return false;
    if (FT_New_Face(CTFreeTypeLibrary(), path, -1, &face) != 0) return false;
    FT_Done_Face(face);
    return true;
}

// the block form calls back once with any errors, then reports completion
void CTFontManagerRegisterFontURLs(CFArrayRef fontURLs, CTFontManagerScope scope, bool enabled, bool (^registrationHandler)(CFArrayRef errors, bool done)) {
    CFArrayRef errors = NULL;

    CTFontManagerRegisterFontsForURLs(fontURLs, scope, &errors);
    if (registrationHandler) registrationHandler(errors ? errors : (CFArrayRef)CFAutorelease(CFArrayCreate(NULL, NULL, 0, &kCFTypeArrayCallBacks)), true);
    if (errors) CFRelease(errors);
}

void CTFontManagerUnregisterFontURLs(CFArrayRef fontURLs, CTFontManagerScope scope, bool (^registrationHandler)(CFArrayRef errors, bool done)) {
    CFArrayRef errors = NULL;

    CTFontManagerUnregisterFontsForURLs(fontURLs, scope, &errors);
    if (registrationHandler) registrationHandler(errors ? errors : (CFArrayRef)CFAutorelease(CFArrayCreate(NULL, NULL, 0, &kCFTypeArrayCallBacks)), true);
    if (errors) CFRelease(errors);
}
