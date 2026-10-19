// CTInternal.h - shared pieces of PureDarwin's CoreText: type IDs, the font registry, faces and shaping
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#import <Foundation/Foundation.h>
#import <CoreFoundation/CoreFoundation.h>
#import <CoreGraphics/CoreGraphics.h>
#import <CoreText/CoreText.h>
#include <pthread.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb.h>

// every CoreText object is an Objective-C object that CF sees through -_cfTypeID
CFTypeID CTRegisterTypeID(const char *name);

// one font file face, shared by every CTFont made from it
typedef struct CTFace {
    int refs;
    pthread_mutex_t lock;
    FT_Face ft;
    hb_face_t *hb;
    char *path;
    long index;
    int unitsPerEm;
    CFStringRef postScriptName;
    CFStringRef familyName;
    CFStringRef styleName;
    CFStringRef fullName;
} CTFace;

CTFace *CTFaceCreateWithPath(const char *path, long index);
CTFace *CTFaceCreateWithFTFace(FT_Face face);
CTFace *CTFaceRetain(CTFace *face);
void CTFaceRelease(CTFace *face);
CFStringRef CTFaceCopyName(CTFace *face, int nameID);
FT_Library CTFreeTypeLibrary(void);

// what the registry knows about one installed face
typedef struct CTFontRecord {
    char *path;
    long index;
    CFStringRef postScriptName;
    CFStringRef familyName;
    CFStringRef styleName;
    CFStringRef fullName;
    CGFloat weight;
    CGFloat width;
    CGFloat slant;
    uint32_t symbolicTraits;
} CTFontRecord;

// the installed fonts, built on first use, rebuilt when fonts are registered
void CTRegistryLock(void);
void CTRegistryUnlock(void);
CFIndex CTRegistryCount(void);
const CTFontRecord *CTRegistryRecordAtIndex(CFIndex index);
const CTFontRecord *CTRegistryRecordForPostScriptName(CFStringRef name);
const CTFontRecord *CTRegistryRecordForPath(const char *path, long index);
void CTRegistryInvalidate(void);
bool CTRegistryAddFile(const char *path);
bool CTRegistryRemoveFile(const char *path);

// records matching a descriptor's attributes, best first, the caller frees the array
CFIndex CTRegistryCopyMatches(CFDictionaryRef attributes, const CTFontRecord ***matches);

// a face's attributes as a descriptor dictionary
CFDictionaryRef CTRecordCopyAttributes(const CTFontRecord *record);

// a font that covers the character, from fontconfig's fallback order
const CTFontRecord *CTRegistryFallbackForCharacter(UTF32Char c, CFStringRef language, const CTFontRecord *preferred);

// the default system face for UI text
const CTFontRecord *CTRegistryDefaultRecord(bool bold);

// trait helpers
CGFloat CTWeightFromOS2Class(int weightClass);
CFDictionaryRef CTCopyTraitsDictionary(uint32_t symbolic, CGFloat weight, CGFloat width, CGFloat slant);

#import "KTFont.h"

// internal accessors on KTFont, the object behind CTFontRef
@interface KTFont (CoreTextInternal)
- (CTFace *)_ctFace;
- (hb_font_t *)_hbFont;
- (CTFontDescriptorRef)_ctDescriptor;
- (CGAffineTransform)_ctMatrix;
@end

// fonts made by CoreText itself, from a registry record or a face
CTFontRef CTFontCreateWithRecord(const CTFontRecord *record, CGFloat size, const CGAffineTransform *matrix, CFDictionaryRef extra);

// the font in a run's attributes, accepting a CTFont or an AppKit NSFont
CTFontRef CTCopyFontFromAttributes(CFDictionaryRef attributes);

// whether the font maps the code point to a glyph
bool CTFontHasCharacter(CTFontRef font, UTF32Char c);

// UTF-16 helpers
static inline bool CTIsHighSurrogate(UniChar c) { return c >= 0xD800 && c <= 0xDBFF; }
static inline bool CTIsLowSurrogate(UniChar c) { return c >= 0xDC00 && c <= 0xDFFF; }
static inline UTF32Char CTCodePointAt(const UniChar *text, CFIndex length, CFIndex i, CFIndex *units) {
    if (CTIsHighSurrogate(text[i]) && i + 1 < length && CTIsLowSurrogate(text[i + 1])) {
        if (units) *units = 2;
        return 0x10000 + (((UTF32Char)text[i] - 0xD800) << 10) + (text[i + 1] - 0xDC00);
    }
    if (units) *units = 1;
    return text[i];
}

// the default size when a caller passes 0, as on Apple
#define CTDefaultFontSize 12.0
