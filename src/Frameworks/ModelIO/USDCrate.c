// USDCrate.c - USD crate files, read the way OpenUSD's crateFile.cpp lays them out (versions 0.4.0 on)
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// a header with the table of contents' offset, then TOKENS, STRINGS, FIELDS, FIELDSETS, PATHS and SPECS
// integer tables are delta and width coded, then LZ4 compressed

#include "USDCrate.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    uint32_t pathIndex, fieldSetIndex, specType;
} Spec;

struct usdc {
    const uint8_t *bytes;
    size_t length;
    uint8_t versionMinor, versionPatch;

    char *tokenChars;
    const char **tokens;
    size_t tokenCount;

    uint32_t *strings;
    size_t stringCount;

    uint32_t *fieldTokens;
    uint64_t *fieldReps;
    size_t fieldCount;

    int32_t *fieldSets;
    size_t fieldSetCount;

    char **paths;
    size_t pathCount;

    Spec *specs;
    size_t specCount;
};

typedef struct {
    const usdc *file;
    size_t offset;
    int failed;
} Reader;

static int readBytes(Reader *r, void *out, size_t size)
{
    if (r->failed || r->offset > r->file->length || size > r->file->length - r->offset) {
        r->failed = 1;
        return 0;
    }
    memcpy(out, r->file->bytes + r->offset, size);
    r->offset += size;
    return 1;
}

static uint64_t readU64(Reader *r)
{
    uint64_t v = 0;
    readBytes(r, &v, 8);
    return v;
}

static uint32_t readU32(Reader *r)
{
    uint32_t v = 0;
    readBytes(r, &v, 4);
    return v;
}

// one LZ4 block, bounds-checked against both buffers
static long lz4Block(const uint8_t *src, size_t srcSize, uint8_t *dst, size_t dstSize)
{
    size_t s = 0, d = 0;

    while (s < srcSize) {
        uint8_t token = src[s++];
        size_t literals = token >> 4;
        if (literals == 15) {
            uint8_t b;
            do {
                if (s >= srcSize)
                    return -1;
                b = src[s++];
                literals += b;
            } while (b == 255);
        }
        if (literals > srcSize - s || literals > dstSize - d)
            return -1;
        memcpy(dst + d, src + s, literals);
        s += literals;
        d += literals;
        if (s >= srcSize)
            break;

        if (srcSize - s < 2)
            return -1;
        size_t offset = src[s] | (src[s + 1] << 8);
        s += 2;
        if (offset == 0 || offset > d)
            return -1;
        size_t match = token & 15;
        if (match == 15) {
            uint8_t b;
            do {
                if (s >= srcSize)
                    return -1;
                b = src[s++];
                match += b;
            } while (b == 255);
        }
        match += 4;
        if (match > dstSize - d)
            return -1;
        for (size_t i = 0; i < match; i++, d++)
            dst[d] = dst[d - offset];
    }
    return (long)d;
}

// TfFastCompression: a chunk count byte, then one block or that many length-prefixed blocks
static long fastDecompress(const uint8_t *src, size_t srcSize, uint8_t *dst, size_t dstSize)
{
    if (srcSize < 1)
        return -1;
    int chunks = src[0];
    if (chunks == 0)
        return lz4Block(src + 1, srcSize - 1, dst, dstSize);

    size_t s = 1, total = 0;
    for (int i = 0; i < chunks; i++) {
        int32_t chunkSize;
        if (srcSize - s < 4)
            return -1;
        memcpy(&chunkSize, src + s, 4);
        s += 4;
        if (chunkSize < 0 || (size_t)chunkSize > srcSize - s)
            return -1;
        long n = lz4Block(src + s, (size_t)chunkSize, dst + total, dstSize - total);
        if (n < 0)
            return -1;
        s += (size_t)chunkSize;
        total += (size_t)n;
    }
    return (long)total;
}

// the most common delta, a 2-bit code per value, then the other deltas as small, medium or full width
// signed numbers (8, 16 or 32 bits, or 16, 32 or 64 for 64-bit integers)
static int decodeIntegers(const uint8_t *data, size_t size, size_t count, int wide, int64_t *out)
{
    size_t width = wide ? 8 : 4;
    size_t codesBytes = (count * 2 + 7) / 8;
    if (size < width + codesBytes)
        return 0;

    int64_t common;
    if (wide) {
        memcpy(&common, data, 8);
    } else {
        int32_t c;
        memcpy(&c, data, 4);
        common = c;
    }
    const uint8_t *codes = data + width;
    size_t v = width + codesBytes;
    int64_t previous = 0;

    for (size_t i = 0; i < count; i++) {
        int code = (codes[i / 4] >> (2 * (i % 4))) & 3;
        size_t bytes = code == 0 ? 0 : (code == 1 ? width / 4 : (code == 2 ? width / 2 : width));
        if (v + bytes > size)
            return 0;
        switch (bytes) {
        case 0: previous += common; break;
        case 1: previous += (int8_t)data[v]; break;
        case 2: { int16_t x; memcpy(&x, data + v, 2); previous += x; break; }
        case 4: { int32_t x; memcpy(&x, data + v, 4); previous += x; break; }
        case 8: { int64_t x; memcpy(&x, data + v, 8); previous += x; break; }
        }
        v += bytes;
        if (!wide)
            previous = (int32_t)previous;
        out[i] = previous;
    }
    return 1;
}

// a compressed integer table: its compressed size, then the compressed bytes
static int64_t *readCompressedInts(Reader *r, size_t count, int wide)
{
    uint64_t compressedSize = readU64(r);
    if (r->failed || compressedSize > r->file->length - r->offset) {
        r->failed = 1;
        return NULL;
    }
    size_t width = wide ? 8 : 4;
    size_t workingSize = width + (count * 2 + 7) / 8 + count * width;
    uint8_t *working = malloc(workingSize ? workingSize : 1);
    int64_t *values = malloc(sizeof(int64_t) * (count ? count : 1));
    long decompressed = fastDecompress(r->file->bytes + r->offset, (size_t)compressedSize, working, workingSize);

    r->offset += (size_t)compressedSize;
    if (decompressed < 0 || !decodeIntegers(working, (size_t)decompressed, count, wide, values)) {
        free(working);
        free(values);
        r->failed = 1;
        return NULL;
    }
    free(working);
    return values;
}

static int findSection(const usdc *file, const char *name, uint64_t *start, uint64_t *size)
{
    uint64_t toc;
    memcpy(&toc, file->bytes + 16, 8);
    Reader r = { file, (size_t)toc, 0 };
    uint64_t count = readU64(&r);

    for (uint64_t i = 0; i < count && !r.failed; i++) {
        char sectionName[17] = { 0 };
        readBytes(&r, sectionName, 16);
        uint64_t s = readU64(&r), z = readU64(&r);
        if (strcmp(sectionName, name) == 0) {
            *start = s;
            *size = z;
            return !r.failed && s <= file->length && z <= file->length - s;
        }
    }
    return 0;
}

static int readTokens(usdc *file)
{
    uint64_t start, size;
    if (!findSection(file, "TOKENS", &start, &size))
        return 0;
    Reader r = { file, (size_t)start, 0 };
    uint64_t count = readU64(&r), uncompressed = readU64(&r), compressed = readU64(&r);
    if (r.failed || compressed > file->length - r.offset || uncompressed > 256u << 20)
        return 0;

    file->tokenChars = malloc(uncompressed + 1);
    long n = fastDecompress(file->bytes + r.offset, (size_t)compressed, (uint8_t *)file->tokenChars, (size_t)uncompressed);
    if (n < 0)
        return 0;
    file->tokenChars[n] = '\0';

    file->tokens = calloc(count ? count : 1, sizeof(char *));
    file->tokenCount = (size_t)count;
    char *p = file->tokenChars, *end = file->tokenChars + n;
    for (size_t i = 0; i < count; i++) {
        file->tokens[i] = p < end ? p : "";
        p += strlen(p) + 1;
    }
    return 1;
}

static int readStrings(usdc *file)
{
    uint64_t start, size;
    if (!findSection(file, "STRINGS", &start, &size))
        return 1;
    Reader r = { file, (size_t)start, 0 };
    uint64_t count = readU64(&r);
    if (r.failed || count > size)
        return 0;
    file->strings = malloc(sizeof(uint32_t) * (count ? count : 1));
    file->stringCount = (size_t)count;
    for (size_t i = 0; i < count; i++)
        file->strings[i] = readU32(&r);
    return !r.failed;
}

static int readFields(usdc *file)
{
    uint64_t start, size;
    if (!findSection(file, "FIELDS", &start, &size))
        return 0;
    Reader r = { file, (size_t)start, 0 };
    uint64_t count = readU64(&r);
    if (r.failed || count > file->length)
        return 0;
    int64_t *tokens = readCompressedInts(&r, (size_t)count, 0);
    if (tokens == NULL)
        return 0;

    file->fieldCount = (size_t)count;
    file->fieldTokens = malloc(sizeof(uint32_t) * (count ? count : 1));
    for (size_t i = 0; i < count; i++)
        file->fieldTokens[i] = (uint32_t)tokens[i];
    free(tokens);

    uint64_t repsSize = readU64(&r);
    if (r.failed || repsSize > file->length - r.offset)
        return 0;
    file->fieldReps = calloc(count ? count : 1, sizeof(uint64_t));
    long n = fastDecompress(file->bytes + r.offset, (size_t)repsSize, (uint8_t *)file->fieldReps, (size_t)count * 8);
    return n == (long)(count * 8);
}

static int readFieldSets(usdc *file)
{
    uint64_t start, size;
    if (!findSection(file, "FIELDSETS", &start, &size))
        return 0;
    Reader r = { file, (size_t)start, 0 };
    uint64_t count = readU64(&r);
    if (r.failed || count > file->length)
        return 0;
    int64_t *sets = readCompressedInts(&r, (size_t)count, 0);
    if (sets == NULL)
        return 0;
    file->fieldSets = malloc(sizeof(int32_t) * (count ? count : 1));
    file->fieldSetCount = (size_t)count;
    for (size_t i = 0; i < count; i++)
        file->fieldSets[i] = (int32_t)sets[i];
    free(sets);
    return 1;
}

static char *joinPath(const char *parent, const char *element, int property)
{
    size_t parentLength = strlen(parent), elementLength = strlen(element);
    char *path = malloc(parentLength + elementLength + 2);
    int root = strcmp(parent, "/") == 0;

    memcpy(path, parent, parentLength);
    size_t n = root && !property ? parentLength : parentLength + 1;
    if (!(root && !property))
        path[parentLength] = property ? '.' : '/';
    memcpy(path + n, element, elementLength + 1);
    return path;
}

// a depth-first tree where each entry's jump says whether a child follows (-1 or above 0)
// and where its next sibling is (0 or above, relative), siblings wait on a stack
static int readPaths(usdc *file)
{
    uint64_t start, size;
    if (!findSection(file, "PATHS", &start, &size))
        return 0;
    Reader r = { file, (size_t)start, 0 };
    uint64_t pathCount = readU64(&r), encoded = readU64(&r);
    if (r.failed || pathCount > file->length || encoded > file->length)
        return 0;

    int64_t *indexes = readCompressedInts(&r, (size_t)encoded, 0);
    int64_t *elements = indexes ? readCompressedInts(&r, (size_t)encoded, 0) : NULL;
    int64_t *jumps = elements ? readCompressedInts(&r, (size_t)encoded, 0) : NULL;
    if (jumps == NULL) {
        free(indexes);
        free(elements);
        return 0;
    }

    file->pathCount = (size_t)pathCount;
    file->paths = calloc(pathCount ? pathCount : 1, sizeof(char *));

    typedef struct { size_t index; char *parent; } Pending;
    Pending *stack = malloc(sizeof(Pending) * (encoded ? encoded : 1));
    size_t depth = 0;
    int ok = 1;

    stack[depth++] = (Pending){ 0, NULL };
    while (depth > 0 && ok) {
        Pending item = stack[--depth];
        size_t current = item.index;
        char *parent = item.parent;
        int more;

        do {
            if (current >= encoded || (uint64_t)indexes[current] >= pathCount || file->paths[indexes[current]] != NULL) {
                ok = 0;
                break;
            }
            size_t self = current++;
            char *path;
            if (parent == NULL) {
                path = strdup("/");
            } else {
                int64_t token = elements[self];
                int property = token < 0;
                uint64_t tokenIndex = (uint64_t)(token < 0 ? -token : token);
                if (tokenIndex >= file->tokenCount) {
                    ok = 0;
                    break;
                }
                path = joinPath(parent, file->tokens[tokenIndex], property);
            }
            file->paths[indexes[self]] = path;

            int hasChild = jumps[self] > 0 || jumps[self] == -1;
            int hasSibling = jumps[self] >= 0;
            if (hasChild && hasSibling)
                stack[depth++] = (Pending){ self + (size_t)jumps[self], parent };
            if (hasChild)
                parent = path;
            more = hasChild || hasSibling;
        } while (more);
    }

    free(stack);
    free(indexes);
    free(elements);
    free(jumps);
    return ok;
}

static int readSpecs(usdc *file)
{
    uint64_t start, size;
    if (!findSection(file, "SPECS", &start, &size))
        return 0;
    Reader r = { file, (size_t)start, 0 };
    uint64_t count = readU64(&r);
    if (r.failed || count > file->length)
        return 0;

    int64_t *paths = readCompressedInts(&r, (size_t)count, 0);
    int64_t *sets = paths ? readCompressedInts(&r, (size_t)count, 0) : NULL;
    int64_t *types = sets ? readCompressedInts(&r, (size_t)count, 0) : NULL;
    if (types != NULL) {
        file->specs = malloc(sizeof(Spec) * (count ? count : 1));
        file->specCount = (size_t)count;
        for (size_t i = 0; i < count; i++)
            file->specs[i] = (Spec){ (uint32_t)paths[i], (uint32_t)sets[i], (uint32_t)types[i] };
    }
    free(paths);
    free(sets);
    free(types);
    return types != NULL;
}

usdc *usdc_open(const uint8_t *bytes, size_t length, const char **error)
{
    if (length < 24 || memcmp(bytes, "PXR-USDC", 8) != 0) {
        if (error)
            *error = "not a USD crate file";
        return NULL;
    }
    usdc *file = calloc(1, sizeof(usdc));
    file->bytes = bytes;
    file->length = length;
    file->versionMinor = bytes[9];
    file->versionPatch = bytes[10];

    if (bytes[8] != 0 || file->versionMinor < 4) {
        if (error)
            *error = "crate version older than 0.4.0";
        usdc_close(file);
        return NULL;
    }
    if (!readTokens(file) || !readStrings(file) || !readFields(file) || !readFieldSets(file) ||
        !readPaths(file) || !readSpecs(file)) {
        if (error)
            *error = "corrupt crate file";
        usdc_close(file);
        return NULL;
    }
    return file;
}

void usdc_close(usdc *file)
{
    if (file == NULL)
        return;
    free(file->tokenChars);
    free(file->tokens);
    free(file->strings);
    free(file->fieldTokens);
    free(file->fieldReps);
    free(file->fieldSets);
    for (size_t i = 0; i < file->pathCount; i++)
        free(file->paths[i]);
    free(file->paths);
    free(file->specs);
    free(file);
}

size_t usdc_spec_count(const usdc *file)
{
    return file->specCount;
}

int usdc_spec_type(const usdc *file, size_t spec)
{
    return spec < file->specCount ? (int)file->specs[spec].specType : 0;
}

const char *usdc_spec_path(const usdc *file, size_t spec)
{
    if (spec >= file->specCount || file->specs[spec].pathIndex >= file->pathCount)
        return NULL;
    return file->paths[file->specs[spec].pathIndex];
}

// bytes per element of the types this reader decodes, 0 for the rest
static size_t elementSize(int type)
{
    switch (type) {
    case USDC_TYPE_BOOL: case USDC_TYPE_UCHAR: return 1;
    case USDC_TYPE_HALF: return 2;
    case USDC_TYPE_INT: case USDC_TYPE_UINT: case USDC_TYPE_FLOAT: case USDC_TYPE_TOKEN:
    case USDC_TYPE_STRING: case USDC_TYPE_ASSETPATH: return 4;
    case USDC_TYPE_INT64: case USDC_TYPE_UINT64: case USDC_TYPE_DOUBLE: return 8;
    case USDC_TYPE_VEC2H: return 4;
    case USDC_TYPE_VEC3H: return 6;
    case USDC_TYPE_VEC4H: case USDC_TYPE_QUATH: return 8;
    case USDC_TYPE_VEC2F: case USDC_TYPE_VEC2I: return 8;
    case USDC_TYPE_VEC3F: case USDC_TYPE_VEC3I: return 12;
    case USDC_TYPE_VEC4F: case USDC_TYPE_VEC4I: case USDC_TYPE_QUATF: return 16;
    case USDC_TYPE_VEC2D: return 16;
    case USDC_TYPE_VEC3D: return 24;
    case USDC_TYPE_VEC4D: case USDC_TYPE_QUATD: return 32;
    case USDC_TYPE_MATRIX2D: return 32;
    case USDC_TYPE_MATRIX3D: return 72;
    case USDC_TYPE_MATRIX4D: return 128;
    }
    return 0;
}

// the number of components, and whether each is a float, double or int
static int components(int type, int *isFloat, int *isDouble)
{
    *isFloat = *isDouble = 0;
    switch (type) {
    case USDC_TYPE_FLOAT: *isFloat = 1; return 1;
    case USDC_TYPE_DOUBLE: *isDouble = 1; return 1;
    case USDC_TYPE_VEC2F: *isFloat = 1; return 2;
    case USDC_TYPE_VEC3F: *isFloat = 1; return 3;
    case USDC_TYPE_VEC4F: case USDC_TYPE_QUATF: *isFloat = 1; return 4;
    case USDC_TYPE_VEC2D: *isDouble = 1; return 2;
    case USDC_TYPE_VEC3D: *isDouble = 1; return 3;
    case USDC_TYPE_VEC4D: case USDC_TYPE_QUATD: *isDouble = 1; return 4;
    case USDC_TYPE_MATRIX2D: *isDouble = 1; return 4;
    case USDC_TYPE_MATRIX3D: *isDouble = 1; return 9;
    case USDC_TYPE_MATRIX4D: *isDouble = 1; return 16;
    case USDC_TYPE_VEC2I: return 2;
    case USDC_TYPE_VEC3I: return 3;
    case USDC_TYPE_VEC4I: return 4;
    }
    return 0;
}

static const char *stringAt(const usdc *file, uint64_t index)
{
    if (index >= file->stringCount || file->strings[index] >= file->tokenCount)
        return "";
    return file->tokens[file->strings[index]];
}

static const char *tokenAt(const usdc *file, uint64_t index)
{
    return index < file->tokenCount ? file->tokens[index] : "";
}

// inlined values sit in the 48-bit payload, as an index, a small integer or a float's bits
// vectors and matrices keep int8 components there, a matrix only its diagonal
static int decodeInlined(const usdc *file, int type, uint64_t payload, usdc_value *value)
{
    switch (type) {
    case USDC_TYPE_TOKEN: value->string = tokenAt(file, payload); return 1;
    case USDC_TYPE_STRING: case USDC_TYPE_ASSETPATH: value->string = stringAt(file, payload); return 1;
    case USDC_TYPE_BOOL: case USDC_TYPE_UCHAR: case USDC_TYPE_UINT: case USDC_TYPE_SPECIFIER:
    case USDC_TYPE_VARIABILITY: case USDC_TYPE_UINT64:
        value->scalars[0] = (double)(uint32_t)payload;
        value->scalarCount = 1;
        return 1;
    case USDC_TYPE_INT: case USDC_TYPE_INT64:
        value->scalars[0] = (double)(int32_t)(uint32_t)payload;
        value->scalarCount = 1;
        return 1;
    case USDC_TYPE_FLOAT: case USDC_TYPE_DOUBLE: {
        uint32_t bits = (uint32_t)payload;
        float f;
        memcpy(&f, &bits, 4);
        value->scalars[0] = f;
        value->scalarCount = 1;
        return 1;
    }
    case USDC_TYPE_MATRIX2D: case USDC_TYPE_MATRIX3D: case USDC_TYPE_MATRIX4D: {
        int isFloat, isDouble, n = components(type, &isFloat, &isDouble);
        int side = n == 4 ? 2 : (n == 9 ? 3 : 4);
        for (int i = 0; i < n; i++)
            value->scalars[i] = 0;
        for (int i = 0; i < side; i++)
            value->scalars[i * side + i] = (int8_t)(payload >> (8 * i));
        value->scalarCount = n;
        return 1;
    }
    default: {
        int isFloat, isDouble, n = components(type, &isFloat, &isDouble);
        if (n == 0)
            return 0;
        for (int i = 0; i < n; i++)
            value->scalars[i] = (int8_t)(payload >> (8 * i));
        value->scalarCount = n;
        return 1;
    }
    }
}

static void readScalarNumbers(Reader *r, int type, usdc_value *value)
{
    int isFloat, isDouble, n = components(type, &isFloat, &isDouble);

    for (int i = 0; i < n; i++) {
        if (isDouble) {
            double d = 0;
            readBytes(r, &d, 8);
            value->scalars[i] = d;
        } else if (isFloat) {
            float f = 0;
            readBytes(r, &f, 4);
            value->scalars[i] = f;
        } else {
            int32_t x = 0;
            readBytes(r, &x, 4);
            value->scalars[i] = x;
        }
    }
    value->scalarCount = n;
}

static int decodeArray(const usdc *file, int type, int compressed, uint64_t payload, usdc_value *value)
{
    value->isArray = 1;
    value->elementSize = elementSize(type);
    if (value->elementSize == 0)
        return 0;
    if (payload == 0)
        return 1;

    Reader r = { file, (size_t)payload, 0 };
    if (file->versionMinor < 5)
        readU32(&r);
    uint64_t count = file->versionMinor < 7 ? readU32(&r) : readU64(&r);
    if (r.failed || count > file->length)
        return 0;
    value->count = (size_t)count;

    int isInt = type == USDC_TYPE_INT || type == USDC_TYPE_UINT || type == USDC_TYPE_INT64 || type == USDC_TYPE_UINT64;
    int isReal = type == USDC_TYPE_FLOAT || type == USDC_TYPE_DOUBLE || type == USDC_TYPE_HALF;

    // integer arrays are compressed from 0.5.0 and real ones from 0.6.0, either only from 16 elements
    if (compressed && count >= 16 && ((isInt && file->versionMinor >= 5) || (isReal && file->versionMinor >= 6))) {
        int wide = type == USDC_TYPE_INT64 || type == USDC_TYPE_UINT64;
        value->data = malloc((size_t)count * value->elementSize);

        if (isInt) {
            int64_t *ints = readCompressedInts(&r, (size_t)count, wide);
            if (ints == NULL)
                return 0;
            for (size_t i = 0; i < count; i++) {
                if (wide)
                    ((int64_t *)value->data)[i] = ints[i];
                else
                    ((int32_t *)value->data)[i] = (int32_t)ints[i];
            }
            free(ints);
            return 1;
        }

        char code = 0;
        readBytes(&r, &code, 1);
        if (code == 'i') {
            // whole numbers stored as compressed ints
            int64_t *ints = readCompressedInts(&r, (size_t)count, 0);
            if (ints == NULL)
                return 0;
            for (size_t i = 0; i < count; i++) {
                if (type == USDC_TYPE_FLOAT)
                    ((float *)value->data)[i] = (float)ints[i];
                else if (type == USDC_TYPE_DOUBLE)
                    ((double *)value->data)[i] = (double)ints[i];
                else
                    return free(ints), 0;
            }
            free(ints);
            return 1;
        }
        if (code == 't') {
            // a table of distinct values and compressed indices into it
            uint32_t tableSize = readU32(&r);
            if (r.failed || tableSize > file->length / value->elementSize)
                return 0;
            uint8_t *table = malloc((size_t)tableSize * value->elementSize + 1);
            readBytes(&r, table, (size_t)tableSize * value->elementSize);
            int64_t *indexes = readCompressedInts(&r, (size_t)count, 0);
            if (indexes == NULL || r.failed) {
                free(table);
                free(indexes);
                return 0;
            }
            for (size_t i = 0; i < count; i++) {
                if ((uint64_t)indexes[i] >= tableSize) {
                    free(table);
                    free(indexes);
                    return 0;
                }
                memcpy((uint8_t *)value->data + i * value->elementSize, table + (size_t)indexes[i] * value->elementSize, value->elementSize);
            }
            free(table);
            free(indexes);
            return 1;
        }
        return 0;
    }

    value->data = malloc((size_t)count * value->elementSize + 1);
    if (!readBytes(&r, value->data, (size_t)count * value->elementSize))
        return 0;

    // token and string arrays hold indices, which become strings here
    if (type == USDC_TYPE_TOKEN || type == USDC_TYPE_STRING || type == USDC_TYPE_ASSETPATH) {
        value->strings = calloc((size_t)count + 1, sizeof(char *));
        for (size_t i = 0; i < count; i++) {
            uint32_t index = ((uint32_t *)value->data)[i];
            value->strings[i] = type == USDC_TYPE_TOKEN ? tokenAt(file, index) : stringAt(file, index);
        }
    }
    return 1;
}

int usdc_spec_field(const usdc *file, size_t spec, const char *field, usdc_value *value)
{
    memset(value, 0, sizeof(*value));
    if (spec >= file->specCount)
        return 0;

    for (size_t set = file->specs[spec].fieldSetIndex; set < file->fieldSetCount && file->fieldSets[set] >= 0; set++) {
        uint32_t index = (uint32_t)file->fieldSets[set];
        if (index >= file->fieldCount || strcmp(tokenAt(file, file->fieldTokens[index]), field) != 0)
            continue;

        uint64_t rep = file->fieldReps[index];
        int isArray = (rep >> 63) & 1, isInlined = (rep >> 62) & 1, isCompressed = (rep >> 61) & 1;
        int type = (int)((rep >> 48) & 0xff);
        uint64_t payload = rep & ((1ull << 48) - 1);

        value->type = type;
        if (isArray)
            return decodeArray(file, type, isCompressed, payload, value) ? 1 : (usdc_value_free(value), 0);
        if (isInlined)
            return decodeInlined(file, type, payload, value);

        Reader r = { file, (size_t)payload, 0 };
        switch (type) {
        case USDC_TYPE_TOKEN: value->string = tokenAt(file, readU32(&r)); break;
        case USDC_TYPE_STRING: case USDC_TYPE_ASSETPATH: value->string = stringAt(file, readU32(&r)); break;
        case USDC_TYPE_TOKENVECTOR: {
            uint64_t count = readU64(&r);
            if (r.failed || count > file->length)
                return 0;
            value->count = (size_t)count;
            value->strings = calloc((size_t)count + 1, sizeof(char *));
            for (size_t i = 0; i < count; i++)
                value->strings[i] = tokenAt(file, readU32(&r));
            break;
        }
        default:
            if (components(type, &(int){ 0 }, &(int){ 0 }) == 0)
                return 0;
            readScalarNumbers(&r, type, value);
            break;
        }
        return !r.failed;
    }
    return 0;
}

void usdc_value_free(usdc_value *value)
{
    free(value->data);
    free(value->strings);
    value->data = NULL;
    value->strings = NULL;
}
