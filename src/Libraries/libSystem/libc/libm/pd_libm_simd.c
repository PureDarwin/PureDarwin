// pd_libm_simd.c - the out-of-line half of <simd/matrix.h>, inverses and identity matrices
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// matrices are column-major structs of clang vectors, the layout the header gives them

typedef float pd_float2 __attribute__((ext_vector_type(2)));
typedef float pd_float3 __attribute__((ext_vector_type(3)));
typedef float pd_float4 __attribute__((ext_vector_type(4)));
typedef double pd_double2 __attribute__((ext_vector_type(2)));
typedef double pd_double3 __attribute__((ext_vector_type(3)));
typedef double pd_double4 __attribute__((ext_vector_type(4)));

typedef struct { pd_float2 columns[2]; } pd_float2x2;
typedef struct { pd_float3 columns[3]; } pd_float3x3;
typedef struct { pd_float4 columns[4]; } pd_float4x4;
typedef struct { pd_double2 columns[2]; } pd_double2x2;
typedef struct { pd_double3 columns[3]; } pd_double3x3;
typedef struct { pd_double4 columns[4]; } pd_double4x4;

const pd_float2x2 matrix_identity_float2x2 = { { { 1, 0 }, { 0, 1 } } };
const pd_float3x3 matrix_identity_float3x3 = { { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } } };
const pd_float4x4 matrix_identity_float4x4 = { { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 }, { 0, 0, 0, 1 } } };
const pd_double2x2 matrix_identity_double2x2 = { { { 1, 0 }, { 0, 1 } } };
const pd_double3x3 matrix_identity_double3x3 = { { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } } };
const pd_double4x4 matrix_identity_double4x4 = { { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 }, { 0, 0, 0, 1 } } };

// element (row r, column c) of an n x n column-major matrix held as doubles
#define AT(m, n, r, c) ((m)[(c) * (n) + (r)])

// partial-pivoting Gauss-Jordan, a singular matrix gives infinities as Apple's does
static void invert(double *m, int n)
{
    double inv[16];

    for (int i = 0; i < n * n; i++)
        inv[i] = (i % (n + 1)) == 0 ? 1.0 : 0.0;

    for (int col = 0; col < n; col++) {
        int pivot = col;
        for (int r = col + 1; r < n; r++) {
            if (__builtin_fabs(AT(m, n, r, col)) > __builtin_fabs(AT(m, n, pivot, col)))
                pivot = r;
        }
        if (pivot != col) {
            for (int c = 0; c < n; c++) {
                double t = AT(m, n, col, c);
                AT(m, n, col, c) = AT(m, n, pivot, c);
                AT(m, n, pivot, c) = t;
                t = AT(inv, n, col, c);
                AT(inv, n, col, c) = AT(inv, n, pivot, c);
                AT(inv, n, pivot, c) = t;
            }
        }

        double scale = 1.0 / AT(m, n, col, col);
        for (int c = 0; c < n; c++) {
            AT(m, n, col, c) *= scale;
            AT(inv, n, col, c) *= scale;
        }
        for (int r = 0; r < n; r++) {
            if (r == col)
                continue;
            double factor = AT(m, n, r, col);
            for (int c = 0; c < n; c++) {
                AT(m, n, r, c) -= factor * AT(m, n, col, c);
                AT(inv, n, r, c) -= factor * AT(inv, n, col, c);
            }
        }
    }
    for (int i = 0; i < n * n; i++)
        m[i] = inv[i];
}

#define INVERT(name, type, n)                               \
    type name(type x)                                       \
    {                                                       \
        double m[16];                                       \
        for (int c = 0; c < n; c++) {                       \
            for (int r = 0; r < n; r++)                     \
                AT(m, n, r, c) = x.columns[c][r];           \
        }                                                   \
        invert(m, n);                                       \
        for (int c = 0; c < n; c++) {                       \
            for (int r = 0; r < n; r++)                     \
                x.columns[c][r] = AT(m, n, r, c);           \
        }                                                   \
        return x;                                           \
    }

INVERT(__invert_f2, pd_float2x2, 2)
INVERT(__invert_f3, pd_float3x3, 3)
INVERT(__invert_f4, pd_float4x4, 4)
INVERT(__invert_d2, pd_double2x2, 2)
INVERT(__invert_d3, pd_double3x3, 3)
INVERT(__invert_d4, pd_double4x4, 4)
