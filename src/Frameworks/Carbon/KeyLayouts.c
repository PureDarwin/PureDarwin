// KeyLayouts.c - the U.S., German and French Mac keyboard layouts as 'uchr' data
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#include "KeyLayouts.h"
#include <stdlib.h>
#include <string.h>

// a dead key in a layout table, the low bits are its dead state
#define DEAD(state) (0xd800 | (state))
#define IS_DEAD(c) (((c) & 0xff00) == 0xd800)

enum { kDeadGrave = 1, kDeadAcute, kDeadCircumflex, kDeadUmlaut, kDeadTilde, kDeadCount = kDeadTilde };

struct deadKey {
    UniChar terminator;
    const char *from;
    const UniChar *to;
};

static const UniChar graveTo[] = { 0xe0, 0xe8, 0xec, 0xf2, 0xf9, 0xc0, 0xc8, 0xcc, 0xd2, 0xd9 };
static const UniChar acuteTo[] = { 0xe1, 0xe9, 0xed, 0xf3, 0xfa, 0xfd, 0xc1, 0xc9, 0xcd, 0xd3, 0xda, 0xdd };
static const UniChar circumflexTo[] = { 0xe2, 0xea, 0xee, 0xf4, 0xfb, 0xc2, 0xca, 0xce, 0xd4, 0xdb };
static const UniChar umlautTo[] = { 0xe4, 0xeb, 0xef, 0xf6, 0xfc, 0xff, 0xc4, 0xcb, 0xcf, 0xd6, 0xdc };
static const UniChar tildeTo[] = { 0xe3, 0xf1, 0xf5, 0xc3, 0xd1, 0xd5 };

// what each dead key makes of the next character, space gives the accent itself
static const struct deadKey usDead[kDeadCount + 1] = {
    [kDeadGrave] = { '`', "aeiouAEIOU", graveTo },
    [kDeadAcute] = { 0xb4, "aeiouyAEIOUY", acuteTo },
    [kDeadCircumflex] = { 0x2c6, "aeiouAEIOU", circumflexTo },
    [kDeadUmlaut] = { 0xa8, "aeiouyAEIOU", umlautTo },
    [kDeadTilde] = { 0x2dc, "anoANO", tildeTo },
};

static const struct deadKey euroDead[kDeadCount + 1] = {
    [kDeadGrave] = { '`', "aeiouAEIOU", graveTo },
    [kDeadAcute] = { 0xb4, "aeiouyAEIOUY", acuteTo },
    [kDeadCircumflex] = { '^', "aeiouAEIOU", circumflexTo },
    [kDeadUmlaut] = { 0xa8, "aeiouyAEIOU", umlautTo },
    [kDeadTilde] = { '~', "anoANO", tildeTo },
};

// the keys every layout shares, by virtual key code: return, tab, space, delete, escape, keypad and navigation
static const UniChar commonKeys[128] = {
    [36] = '\r', [48] = '\t', [49] = ' ', [51] = 0x08, [53] = 0x1b,
    [65] = '.', [67] = '*', [69] = '+', [71] = 0x1b, [75] = '/', [76] = 0x03, [78] = '-',
    [81] = '=', [82] = '0', '1', '2', '3', '4', '5', '6', '7', [91] = '8', '9',
    [96] = 0x10, 0x10, 0x10, 0x10, [101] = 0x10, [103] = 0x10, [105] = 0x10, [107] = 0x10,
    [109] = 0x10, [111] = 0x10, [114] = 0x05, 0x01, 0x0b, 0x7f, 0x10, 0x04, 0x10, 0x0c,
    0x10, 0x1c, 0x1d, 0x1f, 0x1e,
};

// the typing keys of a layout: key code and its base, shift, option and shift-option characters
struct key {
    UInt8 code;
    UniChar base, shift, option, shiftOption;
};

static const struct key usKeys[] = {
    { 0, 'a', 'A', 0xe5, 0xc5 }, { 1, 's', 'S', 0xdf, 0xcd }, { 2, 'd', 'D', 0x2202, 0xce },
    { 3, 'f', 'F', 0x192, 0xcf }, { 4, 'h', 'H', 0x2d9, 0xd3 }, { 5, 'g', 'G', 0xa9, 0x2dd },
    { 6, 'z', 'Z', 0x3a9, 0xb8 }, { 7, 'x', 'X', 0x2248, 0x2db }, { 8, 'c', 'C', 0xe7, 0xc7 },
    { 9, 'v', 'V', 0x221a, 0x25ca }, { 10, 0xa7, 0xb1, 0xa7, 0xb1 }, { 11, 'b', 'B', 0x222b, 0x131 },
    { 12, 'q', 'Q', 0x153, 0x152 }, { 13, 'w', 'W', 0x2211, 0x201e }, { 14, 'e', 'E', DEAD(kDeadAcute), 0xb4 },
    { 15, 'r', 'R', 0xae, 0x2030 }, { 16, 'y', 'Y', 0xa5, 0xc1 }, { 17, 't', 'T', 0x2020, 0x2c7 },
    { 18, '1', '!', 0xa1, 0x2044 }, { 19, '2', '@', 0x2122, 0x20ac }, { 20, '3', '#', 0xa3, 0x2039 },
    { 21, '4', '$', 0xa2, 0x203a }, { 22, '6', '^', 0xa7, 0xfb02 }, { 23, '5', '%', 0x221e, 0xfb01 },
    { 24, '=', '+', 0x2260, 0xb1 }, { 25, '9', '(', 0xaa, 0xb7 }, { 26, '7', '&', 0xb6, 0x2021 },
    { 27, '-', '_', 0x2013, 0x2014 }, { 28, '8', '*', 0x2022, 0xb0 }, { 29, '0', ')', 0xba, 0x201a },
    { 30, ']', '}', 0x2018, 0x2019 }, { 31, 'o', 'O', 0xf8, 0xd8 }, { 32, 'u', 'U', DEAD(kDeadUmlaut), 0xa8 },
    { 33, '[', '{', 0x201c, 0x201d }, { 34, 'i', 'I', DEAD(kDeadCircumflex), 0x2c6 }, { 35, 'p', 'P', 0x3c0, 0x220f },
    { 37, 'l', 'L', 0xac, 0xd2 }, { 38, 'j', 'J', 0x2206, 0xd4 }, { 39, '\'', '"', 0xe6, 0xc6 },
    { 40, 'k', 'K', 0x2da, 0xf8ff }, { 41, ';', ':', 0x2026, 0xda }, { 42, '\\', '|', 0xab, 0xbb },
    { 43, ',', '<', 0x2264, 0xaf }, { 44, '/', '?', 0xf7, 0xbf }, { 45, 'n', 'N', DEAD(kDeadTilde), 0x2dc },
    { 46, 'm', 'M', 0xb5, 0xc2 }, { 47, '.', '>', 0x2265, 0x2d8 }, { 50, '`', '~', DEAD(kDeadGrave), '`' },
};

static const struct key germanKeys[] = {
    { 0, 'a', 'A', 0xe5, 0xc5 }, { 1, 's', 'S', 0x201a, 0xcd }, { 2, 'd', 'D', 0x2202, 0x2122 },
    { 3, 'f', 'F', 0x192, 0xcf }, { 4, 'h', 'H', 0xaa, 0xd3 }, { 5, 'g', 'G', 0xa9, 0xcc },
    { 6, 'y', 'Y', 0xa5, 0x2021 }, { 7, 'x', 'X', 0x2248, 0xd9 }, { 8, 'c', 'C', 0xe7, 0xc7 },
    { 9, 'v', 'V', 0x221a, 0x25ca }, { 10, DEAD(kDeadCircumflex), 0xb0, 0x201e, 0x201c }, { 11, 'b', 'B', 0x222b, 0x2039 },
    { 12, 'q', 'Q', 0xab, 0xbb }, { 13, 'w', 'W', 0x2211, 0x201e }, { 14, 'e', 'E', 0x20ac, 0x2030 },
    { 15, 'r', 'R', 0xae, 0xb8 }, { 16, 'z', 'Z', 0x3a9, 0x2dd }, { 17, 't', 'T', 0x2020, 0x2dd },
    { 18, '1', '!', 0xa1, 0xac }, { 19, '2', '"', 0x201c, 0x201d }, { 20, '3', 0xa7, 0xb6, '#' },
    { 21, '4', '$', 0xa2, 0xa3 }, { 22, '6', '&', ']', '^' }, { 23, '5', '%', '[', 0xfb01 },
    { 24, DEAD(kDeadAcute), DEAD(kDeadGrave), '\'', 0x2d9 }, { 25, '9', ')', '}', 0x2d9 }, { 26, '7', '/', '|', '\\' },
    { 27, 0xdf, '?', 0xbf, 0x2d8 }, { 28, '8', '(', '{', 0x2dc }, { 29, '0', '=', 0x2260, 0xaf },
    { 30, '+', '*', 0xb1, 0xf8ff }, { 31, 'o', 'O', 0xf8, 0xd8 }, { 32, 'u', 'U', DEAD(kDeadUmlaut), 0xdb },
    { 33, 0xfc, 0xdc, 0x2022, 0xb0 }, { 34, 'i', 'I', 0x2044, 0xdb }, { 35, 'p', 'P', 0x3c0, 0x220f },
    { 37, 'l', 'L', '@', 0x2c6 }, { 38, 'j', 'J', 0xba, 0x2c6 }, { 39, 0xe4, 0xc4, 0xe6, 0xc6 },
    { 40, 'k', 'K', 0x2206, 0x2da }, { 41, 0xf6, 0xd6, 0x153, 0x152 }, { 42, '#', '\'', 0x2018, 0x2019 },
    { 43, ',', ';', 0x221e, 0x2c7 }, { 44, '-', '_', 0x2013, 0x2014 }, { 45, 'n', 'N', DEAD(kDeadTilde), 0x2dc },
    { 46, 'm', 'M', 0xb5, 0x2dc }, { 47, '.', ':', 0x2026, 0xf7 }, { 50, '<', '>', 0x2264, 0x2265 },
};

static const struct key frenchKeys[] = {
    { 0, 'q', 'Q', 0x2021, 0x3a9 }, { 1, 's', 'S', 0xd2, 0x2211 }, { 2, 'd', 'D', 0x2202, 0x2206 },
    { 3, 'f', 'F', 0x192, 0xb7 }, { 4, 'h', 'H', 0xcc, 0xce }, { 5, 'g', 'G', 0xfb01, 0xfb02 },
    { 6, 'w', 'W', 0x2039, 0x203a }, { 7, 'x', 'X', 0x2248, 0x2044 }, { 8, 'c', 'C', 0xa9, 0xa2 },
    { 9, 'v', 'V', 0x25ca, 0x221a }, { 10, '@', '#', 0x2022, 0x178 }, { 11, 'b', 'B', 0xdf, 0x222b },
    { 12, 'a', 'A', 0xe6, 0xc6 }, { 13, 'z', 'Z', 0xc2, 0xc5 }, { 14, 'e', 'E', 0xea, 0xca },
    { 15, 'r', 'R', 0xae, 0x2030 }, { 16, 'y', 'Y', 0xda, 0x178 }, { 17, 't', 'T', 0x2020, 0x2122 },
    { 18, '&', '1', 0xf8ff, 0xb4 }, { 19, 0xe9, '2', 0xeb, 0xc9 }, { 20, '"', '3', 0x201c, 0x201d },
    { 21, '\'', '4', 0x2018, 0x2019 }, { 22, 0xa7, '6', 0xb6, 0xe5 }, { 23, '(', '5', '{', '[' },
    { 24, '-', '_', 0x2014, 0x2013 }, { 25, 0xe7, '9', 0xc7, 0xc1 }, { 26, 0xe8, '7', 0xab, 0xbb },
    { 27, ')', 0xb0, '}', ']' }, { 28, '!', '8', 0xa1, 0xdb }, { 29, 0xe0, '0', 0xf8, 0xd8 },
    { 30, '$', '*', 0x20ac, 0xa5 }, { 31, 'o', 'O', 0x153, 0x152 }, { 32, 'u', 'U', 0xba, 0xaa },
    { 33, DEAD(kDeadCircumflex), DEAD(kDeadUmlaut), 0xf4, 0xd4 }, { 34, 'i', 'I', 0xee, 0xef }, { 35, 'p', 'P', 0x3c0, 0x220f },
    { 37, 'l', 'L', 0xac, '|' }, { 38, 'j', 'J', 0xcf, 0xcd }, { 39, 0xf9, '%', 0xd9, 0x2030 },
    { 40, 'k', 'K', 0xc8, 0xcb }, { 41, 'm', 'M', 0xb5, 0xd3 }, { 42, DEAD(kDeadGrave), 0xa3, '@', '#' },
    { 43, ';', '.', 0x2026, 0x2219 }, { 44, '=', '+', 0x2260, 0xb1 }, { 45, 'n', 'N', DEAD(kDeadTilde), 0x131 },
    { 46, ',', '?', 0x221e, 0xbf }, { 47, ':', '/', 0xf7, '\\' }, { 50, '<', '>', 0x2264, 0x2265 },
};

struct layout {
    const char *identifier;
    const struct key *keys;
    size_t count;
    const struct deadKey *dead;
};

static const struct layout layouts[] = {
    [kPDKeyLayoutUS] = { "US", usKeys, sizeof(usKeys) / sizeof(usKeys[0]), usDead },
    [kPDKeyLayoutGerman] = { "German", germanKeys, sizeof(germanKeys) / sizeof(germanKeys[0]), euroDead },
    [kPDKeyLayoutFrench] = { "French", frenchKeys, sizeof(frenchKeys) / sizeof(frenchKeys[0]), euroDead },
};

static UniChar UpperLatin(UniChar c)
{
    if (c >= 'a' && c <= 'z')
        return c - 'a' + 'A';
    if ((c >= 0xe0 && c <= 0xfe && c != 0xf7))
        return c - 0x20;
    if (c == 0x153)
        return 0x152;
    if (c == 0xe6)
        return 0xc6;
    return c;
}

static UniChar ControlChar(UniChar c)
{
    if (c >= 'a' && c <= 'z')
        return c - 'a' + 1;
    switch (c) {
    case '[': return 0x1b;
    case '\\': return 0x1c;
    case ']': return 0x1d;
    case '6': return 0x1e;
    case '-': return 0x1f;
    }
    return c;
}

enum { kTableBase, kTableShift, kTableCaps, kTableOption, kTableShiftOption, kTableControl, kTableCount };
enum { kKeys = 128, kModifierCombos = 32, kMaxStates = 128 };

// one table of characters per modifier combination, dead keys still marked
static void FillTables(const struct layout *layout, UniChar tables[kTableCount][kKeys])
{
    for (int t = 0; t < kTableCount; t++) {
        for (int k = 0; k < kKeys; k++)
            tables[t][k] = commonKeys[k] ? commonKeys[k] : 0xffff;
    }
    for (size_t i = 0; i < layout->count; i++) {
        const struct key *key = &layout->keys[i];
        tables[kTableBase][key->code] = key->base;
        tables[kTableShift][key->code] = key->shift;
        tables[kTableCaps][key->code] = IS_DEAD(key->base) ? key->base : UpperLatin(key->base);
        tables[kTableOption][key->code] = key->option;
        tables[kTableShiftOption][key->code] = key->shiftOption;
        tables[kTableControl][key->code] = IS_DEAD(key->base) ? 0xffff : ControlChar(key->base);
    }
}

// the characters a dead state turns this one into, 0 when it does not combine
static UniChar Composed(const struct deadKey *dead, int state, UniChar c)
{
    const char *hit;

    if (c == ' ')
        return dead[state].terminator;
    if (c == 0 || c >= 0x80 || (hit = strchr(dead[state].from, (int)c)) == NULL)
        return 0;
    return dead[state].to[hit - dead[state].from];
}

struct builder {
    UInt8 *bytes;
    size_t size, capacity;
};

static size_t Append(struct builder *b, const void *data, size_t length)
{
    size_t at = b->size;

    if (b->size + length > b->capacity) {
        b->capacity = (b->size + length) * 2;
        b->bytes = realloc(b->bytes, b->capacity);
    }
    if (data != NULL)
        memcpy(b->bytes + at, data, length);
    else
        memset(b->bytes + at, 0, length);
    b->size += length;
    return at;
}

#define PUT16(b, off, v) (*(UInt16 *)((b)->bytes + (off)) = (UInt16)(v))
#define PUT32(b, off, v) (*(UInt32 *)((b)->bytes + (off)) = (UInt32)(v))

// a 'uchr' resource: header, modifier map, character tables, state records for dead keys and their terminators
CFDataRef PDCreateKeyLayoutData(enum PDKeyLayout which)
{
    const struct layout *layout = &layouts[which];
    UniChar tables[kTableCount][kKeys];
    // a state record per character that combines with a dead key, and one per dead key
    UniChar recordChar[kMaxStates];
    int recordCount = 0;
    struct builder b = { NULL, 0, 0 };
    size_t modifiersOffset, indexOffset, recordsIndexOffset, terminatorsOffset, tableOffsets[kTableCount];
    size_t *recordOffsets;
    CFDataRef data;

    FillTables(layout, tables);

    // every output that is a dead key, or a character some dead key combines with, becomes a state record
    for (int t = 0; t < kTableCount; t++) {
        for (int k = 0; k < kKeys; k++) {
            UniChar c = tables[t][k];
            int record = -1, combines = IS_DEAD(c);

            for (int s = 1; s <= kDeadCount && !combines; s++)
                combines = Composed(layout->dead, s, c) != 0;
            if (!combines || t == kTableControl)
                continue;
            for (int r = 0; r < recordCount; r++) {
                if (recordChar[r] == c)
                    record = r;
            }
            if (record < 0 && recordCount < kMaxStates) {
                record = recordCount++;
                recordChar[record] = c;
            }
            if (record >= 0)
                tables[t][k] = 0x4000 | (UniChar)record;
        }
    }

    Append(&b, NULL, 12 + 28);
    PUT16(&b, 0, 0x1002);
    PUT16(&b, 2, 0x0100);
    PUT32(&b, 8, 1);
    PUT32(&b, 16, 255);

    // indexed by (carbon modifiers >> 8): cmd 1, shift 2, caps 4, option 8, control 16
    modifiersOffset = Append(&b, NULL, 8 + kModifierCombos);
    PUT16(&b, modifiersOffset, 0x2001);
    PUT16(&b, modifiersOffset + 2, kTableBase);
    PUT32(&b, modifiersOffset + 4, kModifierCombos);
    for (UInt32 m = 0; m < kModifierCombos; m++) {
        UInt8 table = kTableBase;
        if (m & 16)
            table = kTableControl;
        else if ((m & 8) && (m & 2))
            table = kTableShiftOption;
        else if (m & 8)
            table = kTableOption;
        else if (m & 2)
            table = kTableShift;
        else if (m & 4)
            table = kTableCaps;
        b.bytes[modifiersOffset + 8 + m] = table;
    }

    indexOffset = Append(&b, NULL, 8 + 4 * kTableCount);
    PUT16(&b, indexOffset, 0x3001);
    PUT16(&b, indexOffset + 2, kKeys);
    PUT32(&b, indexOffset + 4, kTableCount);
    for (int t = 0; t < kTableCount; t++) {
        tableOffsets[t] = Append(&b, tables[t], sizeof(tables[t]));
        PUT32(&b, indexOffset + 8 + 4 * t, tableOffsets[t]);
    }

    // UCKeyStateRecordsIndex, then each UCKeyStateRecord with terminal-format entries
    recordsIndexOffset = Append(&b, NULL, 4 + 4 * (size_t)recordCount);
    PUT16(&b, recordsIndexOffset, 0x4001);
    PUT16(&b, recordsIndexOffset + 2, recordCount);
    recordOffsets = calloc((size_t)recordCount + 1, sizeof(size_t));
    for (int r = 0; r < recordCount; r++) {
        UniChar c = recordChar[r];
        UInt16 header[4];
        UInt16 entries[2 * kDeadCount];
        int entryCount = 0;

        if (IS_DEAD(c)) {
            header[0] = 0xffff;
            header[1] = c & 0xff;
        } else {
            header[0] = c;
            header[1] = 0;
            for (int s = 1; s <= kDeadCount; s++) {
                UniChar composed = Composed(layout->dead, s, c);
                if (composed != 0) {
                    entries[2 * entryCount] = (UInt16)s;
                    entries[2 * entryCount + 1] = composed;
                    entryCount++;
                }
            }
        }
        header[2] = (UInt16)entryCount;
        header[3] = 0x0001;
        recordOffsets[r] = Append(&b, header, sizeof(header));
        Append(&b, entries, 4 * (size_t)entryCount);
        PUT32(&b, recordsIndexOffset + 4 + 4 * r, recordOffsets[r]);
    }
    free(recordOffsets);

    // UCKeyStateTerminators: what each dead state leaves behind when the next key does not combine
    terminatorsOffset = Append(&b, NULL, 4 + 2 * kDeadCount);
    PUT16(&b, terminatorsOffset, 0x5001);
    PUT16(&b, terminatorsOffset + 2, kDeadCount);
    for (int s = 1; s <= kDeadCount; s++)
        PUT16(&b, terminatorsOffset + 4 + 2 * (s - 1), layout->dead[s].terminator);

    PUT32(&b, 20, modifiersOffset);
    PUT32(&b, 24, indexOffset);
    PUT32(&b, 28, recordsIndexOffset);
    PUT32(&b, 32, terminatorsOffset);
    PUT32(&b, 36, 0);

    data = CFDataCreate(kCFAllocatorDefault, b.bytes, (CFIndex)b.size);
    free(b.bytes);
    return data;
}

const char *PDKeyLayoutName(enum PDKeyLayout which)
{
    return layouts[which].identifier;
}
