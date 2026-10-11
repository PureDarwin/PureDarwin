// USDCrate.h - a reader for USD's binary "crate" files (.usdc)
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#ifndef USDCRATE_H
#define USDCRATE_H

#include <stddef.h>
#include <stdint.h>

// spec types, as SdfSpecType numbers them
enum {
    USDC_SPEC_ATTRIBUTE = 1,
    USDC_SPEC_PRIM = 6,
    USDC_SPEC_PSEUDOROOT = 7,
    USDC_SPEC_RELATIONSHIP = 8,
};

// the crate's value type numbers for the types a mesh reader meets
enum {
    USDC_TYPE_BOOL = 1, USDC_TYPE_UCHAR = 2, USDC_TYPE_INT = 3, USDC_TYPE_UINT = 4,
    USDC_TYPE_INT64 = 5, USDC_TYPE_UINT64 = 6, USDC_TYPE_HALF = 7, USDC_TYPE_FLOAT = 8,
    USDC_TYPE_DOUBLE = 9, USDC_TYPE_STRING = 10, USDC_TYPE_TOKEN = 11, USDC_TYPE_ASSETPATH = 12,
    USDC_TYPE_MATRIX2D = 13, USDC_TYPE_MATRIX3D = 14, USDC_TYPE_MATRIX4D = 15,
    USDC_TYPE_QUATD = 16, USDC_TYPE_QUATF = 17, USDC_TYPE_QUATH = 18,
    USDC_TYPE_VEC2D = 19, USDC_TYPE_VEC2F = 20, USDC_TYPE_VEC2H = 21, USDC_TYPE_VEC2I = 22,
    USDC_TYPE_VEC3D = 23, USDC_TYPE_VEC3F = 24, USDC_TYPE_VEC3H = 25, USDC_TYPE_VEC3I = 26,
    USDC_TYPE_VEC4D = 27, USDC_TYPE_VEC4F = 28, USDC_TYPE_VEC4H = 29, USDC_TYPE_VEC4I = 30,
    USDC_TYPE_TOKENVECTOR = 41, USDC_TYPE_SPECIFIER = 42, USDC_TYPE_VARIABILITY = 44,
};

typedef struct usdc usdc;

// a decoded field: numbers as doubles in scalars, arrays as count elements of elementSize bytes in data
// strings, tokens and token vectors point into the file's own token table
typedef struct {
    int type;
    int isArray;
    size_t count;
    size_t elementSize;
    void *data;
    double scalars[16];
    int scalarCount;
    const char *string;
    const char **strings;
} usdc_value;

usdc *usdc_open(const uint8_t *bytes, size_t length, const char **error);
void usdc_close(usdc *file);

size_t usdc_spec_count(const usdc *file);
int usdc_spec_type(const usdc *file, size_t spec);
const char *usdc_spec_path(const usdc *file, size_t spec);
// fills value and returns 1 when the spec has the field, usdc_value_free releases it
int usdc_spec_field(const usdc *file, size_t spec, const char *field, usdc_value *value);
void usdc_value_free(usdc_value *value);

#endif
