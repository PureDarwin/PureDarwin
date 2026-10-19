// UnicodeUtilities.c - UCKeyTranslate over 'uchr' keyboard layouts
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0

// UCKeyTranslate over Apple's documented 'uchr' keyboard layout format: modifier tables,
// key-to-character tables, dead key state records, terminators and character sequences
#include <CoreServices/SharedFileList.h>
#include <CoreServices/UnicodeUtilities.h>
#include <CoreServices/FileManager.h>
#include <mach/mach_time.h>
#include <stdlib.h>
#include <string.h>

// the on-disk layout, every offset counted from the start of the 'uchr' data
typedef struct {
    UInt32 keyboardTypeFirst;
    UInt32 keyboardTypeLast;
    UInt32 keyModifiersToTableNumOffset;
    UInt32 keyToCharTableIndexOffset;
    UInt32 keyStateRecordsIndexOffset;
    UInt32 keyStateTerminatorsOffset;
    UInt32 keySequenceDataIndexOffset;
} typeHeader;

struct UCKeyboardLayout {
    UInt16 keyLayoutHeaderFormat;
    UInt16 keyLayoutDataVersion;
    UInt32 keyLayoutFeatureInfoOffset;
    UInt32 keyboardTypeCount;
    typeHeader keyboardTypeList[1];
};

typedef struct {
    UInt16 format;
    UInt16 defaultTableNum;
    UInt32 modifiersCount;
    UInt8 tableNum[1];
} modifiersToTableNum;

typedef struct {
    UInt16 format;
    UInt16 keyToCharTableSize;
    UInt32 keyToCharTableCount;
    UInt32 keyToCharTableOffsets[1];
} keyToCharTableIndex;

typedef struct {
    UInt16 format;
    UInt16 keyStateRecordCount;
    UInt32 keyStateRecordOffsets[1];
} stateRecordsIndex;

typedef struct {
    UniChar stateZeroCharData;
    UInt16 stateZeroNextState;
    UInt16 stateEntryCount;
    UInt16 stateEntryFormat;
    // followed by the entries
} stateRecord;

typedef struct {
    UInt16 format;
    UInt16 keyStateTerminatorCount;
    UniChar keyStateTerminators[1];
} stateTerminators;

typedef struct {
    UInt16 format;
    UInt16 charSequenceCount;
    UInt16 charSequenceOffsets[1];
} sequenceDataIndex;

enum {
    kOutputTestForIndexMask = 0xC000,
    kOutputStateIndexMask = 0x4000,
    kOutputSequenceIndexMask = 0x8000,
    kOutputGetIndexMask = 0x3FFF,
    kEntryTerminalFormat = 0x0001,
    kEntryRangeFormat = 0x0002,
};

struct output {
    UniChar *chars;
    UniCharCount max, len;
    Boolean overflow;
};

static void put(struct output *o, UniChar c)
{
    if (o->len < o->max)
        o->chars[o->len] = c;
    else
        o->overflow = true;
    o->len++;
}

static const void *at(const UCKeyboardLayout *base, UInt32 offset)
{
    return offset ? (const UInt8 *)base + offset : NULL;
}

// a character value or an index into the sequence table, the two forms a key or a state can produce
static void emitCharOrSequence(const UCKeyboardLayout *base, const typeHeader *h, UInt16 value, struct output *o)
{
    if ((value & kOutputTestForIndexMask) == kOutputSequenceIndexMask) {
        const sequenceDataIndex *seq = at(base, h->keySequenceDataIndexOffset);
        UInt16 i = value & kOutputGetIndexMask;

        if (seq == NULL || i >= seq->charSequenceCount)
            return;
        const UniChar *start = (const UniChar *)((const UInt8 *)seq + seq->charSequenceOffsets[i]);
        const UniChar *end = (const UniChar *)((const UInt8 *)seq + seq->charSequenceOffsets[i + 1]);
        for (const UniChar *c = start; c < end; c++)
            put(o, *c);
        return;
    }
    // 0xFFFE and 0xFFFF mean no output
    if (value != 0xFFFE && value != 0xFFFF)
        put(o, value);
}

static void emitTerminator(const UCKeyboardLayout *base, const typeHeader *h, UInt32 state, struct output *o)
{
    const stateTerminators *t = at(base, h->keyStateTerminatorsOffset);

    if (t != NULL && state >= 1 && state <= t->keyStateTerminatorCount)
        emitCharOrSequence(base, h, t->keyStateTerminators[state - 1], o);
}

static const typeHeader *headerFor(const UCKeyboardLayout *layout, UInt32 keyboardType)
{
    for (UInt32 i = 0; i < layout->keyboardTypeCount; i++) {
        const typeHeader *h = &layout->keyboardTypeList[i];
        if (keyboardType >= h->keyboardTypeFirst && keyboardType <= h->keyboardTypeLast)
            return h;
    }
    return layout->keyboardTypeCount ? &layout->keyboardTypeList[0] : NULL;
}

OSStatus UCKeyTranslate(const UCKeyboardLayout *keyLayoutPtr, UInt16 virtualKeyCode, UInt16 keyAction,
                        UInt32 modifierKeyState, UInt32 keyboardType, OptionBits keyTranslateOptions,
                        UInt32 *deadKeyState, UniCharCount maxStringLength, UniCharCount *actualStringLength,
                        UniChar unicodeString[])
{
    struct output o = { unicodeString, maxStringLength, 0, false };
    const typeHeader *h;

    if (keyLayoutPtr == NULL || deadKeyState == NULL || actualStringLength == NULL ||
        (unicodeString == NULL && maxStringLength > 0))
        return paramErr;
    *actualStringLength = 0;
    if ((h = headerFor(keyLayoutPtr, keyboardType)) == NULL)
        return paramErr;
    if (keyAction == kUCKeyActionUp)
        return noErr;

    const modifiersToTableNum *mods = at(keyLayoutPtr, h->keyModifiersToTableNumOffset);
    const keyToCharTableIndex *tables = at(keyLayoutPtr, h->keyToCharTableIndexOffset);
    if (mods == NULL || tables == NULL)
        return paramErr;

    UInt32 modifier = modifierKeyState & 0xFF;
    UInt32 table = modifier < mods->modifiersCount ? mods->tableNum[modifier] : mods->defaultTableNum;
    if (table >= tables->keyToCharTableCount)
        table = mods->defaultTableNum;
    if (table >= tables->keyToCharTableCount || virtualKeyCode >= tables->keyToCharTableSize)
        return noErr;

    const UInt16 *row = (const UInt16 *)((const UInt8 *)keyLayoutPtr + tables->keyToCharTableOffsets[table]);
    UInt16 value = row[virtualKeyCode];
    // display translation never starts or advances a dead key sequence
    Boolean noDeadKeys = (keyTranslateOptions & kUCKeyTranslateNoDeadKeysMask) || keyAction == kUCKeyActionDisplay;
    UInt32 state = *deadKeyState;

    if ((value & kOutputTestForIndexMask) == kOutputStateIndexMask) {
        const stateRecordsIndex *records = at(keyLayoutPtr, h->keyStateRecordsIndexOffset);
        UInt16 index = value & kOutputGetIndexMask;

        if (records == NULL || index >= records->keyStateRecordCount)
            return noErr;
        const stateRecord *r = (const stateRecord *)((const UInt8 *)keyLayoutPtr + records->keyStateRecordOffsets[index]);

        if (state == 0 || noDeadKeys) {
            if (r->stateZeroNextState != 0 && !noDeadKeys) {
                *deadKeyState = r->stateZeroNextState;
            } else {
                emitCharOrSequence(keyLayoutPtr, h, r->stateZeroCharData, &o);
            }
        } else {
            const UInt8 *entries = (const UInt8 *)(r + 1);
            Boolean matched = false;
            UInt16 out = 0xFFFF, next = 0;

            for (UInt16 i = 0; i < r->stateEntryCount && !matched; i++) {
                if (r->stateEntryFormat == kEntryTerminalFormat) {
                    const UInt16 *e = (const UInt16 *)(entries + i * 4);
                    if (e[0] == state) {
                        out = e[1];
                        matched = true;
                    }
                } else if (r->stateEntryFormat == kEntryRangeFormat) {
                    const UInt8 *e = entries + i * 8;
                    UInt16 start = *(const UInt16 *)e;
                    UInt8 range = e[2], delta = e[3];
                    UInt16 charData = *(const UInt16 *)(e + 4), nextState = *(const UInt16 *)(e + 6);
                    if (state >= start && state <= (UInt32)start + range) {
                        UInt32 step = (state - start) * delta;
                        out = charData == 0xFFFF ? 0xFFFF : (UInt16)(charData + step);
                        next = nextState ? (UInt16)(nextState + step) : 0;
                        matched = true;
                    }
                }
            }
            if (matched && next != 0) {
                *deadKeyState = next;
            } else if (matched) {
                emitCharOrSequence(keyLayoutPtr, h, out, &o);
                *deadKeyState = 0;
            } else {
                // a key the dead state has no entry for: the dead key's own character, then this key's
                emitTerminator(keyLayoutPtr, h, state, &o);
                emitCharOrSequence(keyLayoutPtr, h, r->stateZeroCharData, &o);
                *deadKeyState = r->stateZeroNextState;
            }
        }
    } else {
        if (state != 0 && !noDeadKeys) {
            emitTerminator(keyLayoutPtr, h, state, &o);
            *deadKeyState = 0;
        }
        emitCharOrSequence(keyLayoutPtr, h, value, &o);
    }

    *actualStringLength = o.len < maxStringLength ? o.len : maxStringLength;
    return o.overflow ? kUCOutputBufferTooSmall : noErr;
}

// collators compare through CFString, with the collate options mapped onto CF's compare flags
struct OpaqueLocaleRef {
    CFLocaleRef locale;
};

struct OpaqueCollatorRef {
    CFLocaleRef locale;
    CFStringCompareFlags flags;
};

OSStatus LocaleRefFromLocaleString(const char *localeString, LocaleRef *locale)
{
    if (!localeString || !locale) return paramErr;

    CFStringRef ident = CFStringCreateWithCString(kCFAllocatorDefault, localeString, kCFStringEncodingUTF8);
    if (!ident) return paramErr;
    struct OpaqueLocaleRef *ref = calloc(1, sizeof(*ref));
    if (!ref) {
        CFRelease(ident);
        return memFullErr;
    }
    ref->locale = CFLocaleCreate(kCFAllocatorDefault, ident);
    CFRelease(ident);
    *locale = ref;
    return noErr;
}

OSStatus UCCreateCollator(LocaleRef locale, LocaleOperationVariant opVariant, UCCollateOptions options,
                          CollatorRef *collatorRef)
{
    (void)opVariant;
    if (!collatorRef) return paramErr;

    struct OpaqueCollatorRef *c = calloc(1, sizeof(*c));
    if (!c) return memFullErr;
    if (locale && locale->locale) c->locale = (CFLocaleRef)CFRetain(locale->locale);
    c->flags = kCFCompareLocalized;
    if (options & kUCCollateCaseInsensitiveMask) c->flags |= kCFCompareCaseInsensitive;
    if (options & kUCCollateDiacritInsensitiveMask) c->flags |= kCFCompareDiacriticInsensitive;
    if (options & kUCCollateWidthInsensitiveMask) c->flags |= kCFCompareWidthInsensitive;
    if (options & kUCCollateComposeInsensitiveMask) c->flags |= kCFCompareNonliteral;
    if (options & kUCCollateDigitsAsNumberMask) c->flags |= kCFCompareNumerically;
    *collatorRef = c;
    return noErr;
}

OSStatus UCDisposeCollator(CollatorRef *collatorRef)
{
    if (!collatorRef) return paramErr;
    if (*collatorRef) {
        if ((*collatorRef)->locale) CFRelease((*collatorRef)->locale);
        free(*collatorRef);
        *collatorRef = NULL;
    }
    return noErr;
}

OSStatus UCCompareText(CollatorRef collatorRef, const UniChar *text1Ptr, UniCharCount text1Length,
                       const UniChar *text2Ptr, UniCharCount text2Length, Boolean *equivalent, SInt32 *order)
{
    if (!collatorRef || (!text1Ptr && text1Length) || (!text2Ptr && text2Length)) return paramErr;

    CFStringRef a = CFStringCreateWithCharactersNoCopy(kCFAllocatorDefault, text1Ptr, text1Length, kCFAllocatorNull);
    CFStringRef b = CFStringCreateWithCharactersNoCopy(kCFAllocatorDefault, text2Ptr, text2Length, kCFAllocatorNull);
    if (!a || !b) {
        if (a) CFRelease(a);
        if (b) CFRelease(b);
        return memFullErr;
    }
    CFComparisonResult r = CFStringCompareWithOptionsAndLocale(a, b, CFRangeMake(0, text1Length), collatorRef->flags,
                                                               collatorRef->locale);
    CFRelease(a);
    CFRelease(b);
    if (equivalent) *equivalent = (r == kCFCompareEqualTo);
    if (order) *order = (SInt32)r;
    return noErr;
}

// user activity resets the idle time that IOKit's HIDIdleTime and CoreGraphics read
extern uint32_t notify_register_check(const char *name, int *out_token);
extern uint32_t notify_set_state(int token, uint64_t state);

OSErr UpdateSystemActivity(UInt8 activity)
{
    static int token = -1;
    static mach_timebase_info_data_t timebase;

    if (activity != UsrActivity) return noErr;
    if (timebase.denom == 0) mach_timebase_info(&timebase);
    if (token < 0 && notify_register_check("org.puredarwin.hid.lastinput", &token) != 0) return noErr;
    notify_set_state(token, mach_absolute_time() * timebase.numer / timebase.denom);
    return noErr;
}
