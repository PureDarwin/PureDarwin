// PD's <simd/simd.h>: the vector and matrix types Apple's sample code shares with Metal shaders, as clang
// ext_vector_types with Metal's layout (float3 takes 16 bytes), and the common functions on them, overloaded
// for C and Objective-C like Apple's spelling. Written from the documented API; covers what samples use
#ifndef PD_SIMD_SIMD_H
#define PD_SIMD_SIMD_H

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#define PD_SIMD static inline __attribute__((__overloadable__, __always_inline__))
#define PD_SIMD_C static inline __attribute__((__always_inline__))

typedef float simd_float2 __attribute__((ext_vector_type(2)));
typedef float simd_float3 __attribute__((ext_vector_type(3)));
typedef float simd_float4 __attribute__((ext_vector_type(4)));
// packed types: float alignment, so they sit in packed structs like Metal's packed_floatN
typedef float simd_packed_float2 __attribute__((ext_vector_type(2), aligned(4)));
typedef float simd_packed_float4 __attribute__((ext_vector_type(4), aligned(4)));
typedef int32_t simd_packed_int2 __attribute__((ext_vector_type(2), aligned(4)));
typedef int32_t simd_packed_int4 __attribute__((ext_vector_type(4), aligned(4)));
typedef uint32_t simd_packed_uint2 __attribute__((ext_vector_type(2), aligned(4)));
typedef uint32_t simd_packed_uint4 __attribute__((ext_vector_type(4), aligned(4)));
typedef simd_packed_float2 packed_float2;
typedef simd_packed_float4 packed_float4;
typedef simd_packed_int2 packed_int2;
typedef simd_packed_int4 packed_int4;
typedef simd_packed_uint2 packed_uint2;
typedef simd_packed_uint4 packed_uint4;
typedef double simd_double2 __attribute__((ext_vector_type(2)));
typedef double simd_double3 __attribute__((ext_vector_type(3)));
typedef double simd_double4 __attribute__((ext_vector_type(4)));
typedef int32_t simd_int2 __attribute__((ext_vector_type(2)));
typedef int32_t simd_int3 __attribute__((ext_vector_type(3)));
typedef int32_t simd_int4 __attribute__((ext_vector_type(4)));
typedef uint32_t simd_uint2 __attribute__((ext_vector_type(2)));
typedef uint32_t simd_uint3 __attribute__((ext_vector_type(3)));
typedef uint32_t simd_uint4 __attribute__((ext_vector_type(4)));
typedef int16_t simd_short2 __attribute__((ext_vector_type(2)));
typedef int16_t simd_short4 __attribute__((ext_vector_type(4)));
typedef uint16_t simd_ushort2 __attribute__((ext_vector_type(2)));
typedef uint16_t simd_ushort3 __attribute__((ext_vector_type(3)));
typedef uint16_t simd_ushort4 __attribute__((ext_vector_type(4)));
typedef int8_t simd_char4 __attribute__((ext_vector_type(4)));
typedef uint8_t simd_uchar2 __attribute__((ext_vector_type(2)));
typedef uint8_t simd_uchar3 __attribute__((ext_vector_type(3)));
typedef uint8_t simd_uchar4 __attribute__((ext_vector_type(4)));
typedef _Float16 simd_half2 __attribute__((ext_vector_type(2)));
typedef _Float16 simd_half3 __attribute__((ext_vector_type(3)));
typedef _Float16 simd_half4 __attribute__((ext_vector_type(4)));

typedef simd_float2 vector_float2;
typedef simd_float3 vector_float3;
typedef simd_float4 vector_float4;
typedef simd_double2 vector_double2;
typedef simd_double3 vector_double3;
typedef simd_double4 vector_double4;
typedef simd_int2 vector_int2;
typedef simd_int3 vector_int3;
typedef simd_int4 vector_int4;
typedef simd_uint2 vector_uint2;
typedef simd_uint3 vector_uint3;
typedef simd_uint4 vector_uint4;
typedef simd_short2 vector_short2;
typedef simd_short4 vector_short4;
typedef simd_ushort2 vector_ushort2;
typedef simd_ushort3 vector_ushort3;
typedef simd_ushort4 vector_ushort4;
typedef simd_char4 vector_char4;
typedef simd_uchar2 vector_uchar2;
typedef simd_uchar3 vector_uchar3;
typedef simd_uchar4 vector_uchar4;

typedef struct { simd_float2 columns[2]; } simd_float2x2;
typedef struct { simd_float3 columns[3]; } simd_float3x3;
typedef struct { simd_float4 columns[3]; } simd_float3x4;
typedef struct { simd_float3 columns[4]; } simd_float4x3;
typedef struct { simd_float4 columns[4]; } simd_float4x4;
typedef simd_float2x2 matrix_float2x2;
typedef simd_float3x3 matrix_float3x3;
typedef simd_float3x4 matrix_float3x4;
typedef simd_float4x3 matrix_float4x3;
typedef simd_float4x4 matrix_float4x4;

typedef struct { simd_float4 vector; } simd_quatf;

static const simd_float3x3 matrix_identity_float3x3 = { { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } } };
static const simd_float4x4 matrix_identity_float4x4 = { { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 }, { 0, 0, 0, 1 } } };

// constructors
PD_SIMD simd_float2 simd_make_float2(float x, float y) { return (simd_float2){ x, y }; }
PD_SIMD simd_float3 simd_make_float3(float x, float y, float z) { return (simd_float3){ x, y, z }; }
PD_SIMD simd_float3 simd_make_float3(simd_float2 xy, float z) { return (simd_float3){ xy.x, xy.y, z }; }
PD_SIMD simd_float3 simd_make_float3(simd_float4 v) { return v.xyz; }
PD_SIMD simd_float4 simd_make_float4(float x, float y, float z, float w) { return (simd_float4){ x, y, z, w }; }
PD_SIMD simd_float4 simd_make_float4(simd_float3 xyz, float w) { return (simd_float4){ xyz.x, xyz.y, xyz.z, w }; }
PD_SIMD simd_float4 simd_make_float4(simd_float2 xy, simd_float2 zw) { return (simd_float4){ xy.x, xy.y, zw.x, zw.y }; }
PD_SIMD simd_float4 simd_make_float4(simd_float3 xyz) { return (simd_float4){ xyz.x, xyz.y, xyz.z, 0 }; }
PD_SIMD simd_int2 simd_make_int2(int32_t x, int32_t y) { return (simd_int2){ x, y }; }
PD_SIMD simd_uint2 simd_make_uint2(uint32_t x, uint32_t y) { return (simd_uint2){ x, y }; }

PD_SIMD simd_float2x2 simd_matrix(simd_float2 c0, simd_float2 c1) { return (simd_float2x2){ { c0, c1 } }; }
PD_SIMD simd_float3x3 simd_matrix(simd_float3 c0, simd_float3 c1, simd_float3 c2) { return (simd_float3x3){ { c0, c1, c2 } }; }
PD_SIMD simd_float4x4 simd_matrix(simd_float4 c0, simd_float4 c1, simd_float4 c2, simd_float4 c3) {
  return (simd_float4x4){ { c0, c1, c2, c3 } };
}
PD_SIMD simd_float3x3 simd_matrix_from_rows(simd_float3 r0, simd_float3 r1, simd_float3 r2) {
  return (simd_float3x3){ { { r0.x, r1.x, r2.x }, { r0.y, r1.y, r2.y }, { r0.z, r1.z, r2.z } } };
}
PD_SIMD simd_float4x4 simd_matrix_from_rows(simd_float4 r0, simd_float4 r1, simd_float4 r2, simd_float4 r3) {
  return (simd_float4x4){ { { r0.x, r1.x, r2.x, r3.x }, { r0.y, r1.y, r2.y, r3.y }, { r0.z, r1.z, r2.z, r3.z },
                            { r0.w, r1.w, r2.w, r3.w } } };
}
PD_SIMD simd_float3x3 simd_diagonal_matrix(simd_float3 d) {
  return (simd_float3x3){ { { d.x, 0, 0 }, { 0, d.y, 0 }, { 0, 0, d.z } } };
}
PD_SIMD simd_float4x4 simd_diagonal_matrix(simd_float4 d) {
  return (simd_float4x4){ { { d.x, 0, 0, 0 }, { 0, d.y, 0, 0 }, { 0, 0, d.z, 0 }, { 0, 0, 0, d.w } } };
}

// geometry
PD_SIMD float simd_dot(simd_float2 a, simd_float2 b) { return a.x * b.x + a.y * b.y; }
PD_SIMD float simd_dot(simd_float3 a, simd_float3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
PD_SIMD float simd_dot(simd_float4 a, simd_float4 b) { return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w; }
PD_SIMD simd_float3 simd_cross(simd_float3 a, simd_float3 b) {
  return (simd_float3){ a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
}
PD_SIMD float simd_length_squared(simd_float2 v) { return simd_dot(v, v); }
PD_SIMD float simd_length_squared(simd_float3 v) { return simd_dot(v, v); }
PD_SIMD float simd_length_squared(simd_float4 v) { return simd_dot(v, v); }
PD_SIMD float simd_length(simd_float2 v) { return sqrtf(simd_dot(v, v)); }
PD_SIMD float simd_length(simd_float3 v) { return sqrtf(simd_dot(v, v)); }
PD_SIMD float simd_length(simd_float4 v) { return sqrtf(simd_dot(v, v)); }
PD_SIMD float simd_distance(simd_float3 a, simd_float3 b) { return simd_length(a - b); }
PD_SIMD simd_float2 simd_normalize(simd_float2 v) { return v / simd_length(v); }
PD_SIMD simd_float3 simd_normalize(simd_float3 v) { return v / simd_length(v); }
PD_SIMD simd_float4 simd_normalize(simd_float4 v) { return v / simd_length(v); }

// common
PD_SIMD float simd_mix(float a, float b, float t) { return a + (b - a) * t; }
PD_SIMD simd_float2 simd_mix(simd_float2 a, simd_float2 b, simd_float2 t) { return a + (b - a) * t; }
PD_SIMD simd_float3 simd_mix(simd_float3 a, simd_float3 b, simd_float3 t) { return a + (b - a) * t; }
PD_SIMD simd_float4 simd_mix(simd_float4 a, simd_float4 b, simd_float4 t) { return a + (b - a) * t; }
PD_SIMD float simd_min(float a, float b) { return a < b ? a : b; }
PD_SIMD float simd_max(float a, float b) { return a > b ? a : b; }
PD_SIMD simd_float2 simd_min(simd_float2 a, simd_float2 b) { return (simd_float2){ simd_min(a.x, b.x), simd_min(a.y, b.y) }; }
PD_SIMD simd_float2 simd_max(simd_float2 a, simd_float2 b) { return (simd_float2){ simd_max(a.x, b.x), simd_max(a.y, b.y) }; }
PD_SIMD simd_float3 simd_min(simd_float3 a, simd_float3 b) {
  return (simd_float3){ simd_min(a.x, b.x), simd_min(a.y, b.y), simd_min(a.z, b.z) };
}
PD_SIMD simd_float3 simd_max(simd_float3 a, simd_float3 b) {
  return (simd_float3){ simd_max(a.x, b.x), simd_max(a.y, b.y), simd_max(a.z, b.z) };
}
PD_SIMD simd_float4 simd_min(simd_float4 a, simd_float4 b) {
  return (simd_float4){ simd_min(a.x, b.x), simd_min(a.y, b.y), simd_min(a.z, b.z), simd_min(a.w, b.w) };
}
PD_SIMD simd_float4 simd_max(simd_float4 a, simd_float4 b) {
  return (simd_float4){ simd_max(a.x, b.x), simd_max(a.y, b.y), simd_max(a.z, b.z), simd_max(a.w, b.w) };
}
PD_SIMD float simd_clamp(float x, float lo, float hi) { return simd_min(simd_max(x, lo), hi); }
PD_SIMD simd_float2 simd_clamp(simd_float2 x, simd_float2 lo, simd_float2 hi) { return simd_min(simd_max(x, lo), hi); }
PD_SIMD simd_float3 simd_clamp(simd_float3 x, simd_float3 lo, simd_float3 hi) { return simd_min(simd_max(x, lo), hi); }
PD_SIMD simd_float4 simd_clamp(simd_float4 x, simd_float4 lo, simd_float4 hi) { return simd_min(simd_max(x, lo), hi); }
PD_SIMD simd_float2 simd_abs(simd_float2 v) { return (simd_float2){ fabsf(v.x), fabsf(v.y) }; }
PD_SIMD simd_float3 simd_abs(simd_float3 v) { return (simd_float3){ fabsf(v.x), fabsf(v.y), fabsf(v.z) }; }
PD_SIMD simd_float4 simd_abs(simd_float4 v) { return (simd_float4){ fabsf(v.x), fabsf(v.y), fabsf(v.z), fabsf(v.w) }; }

// matrices
PD_SIMD simd_float2 simd_mul(simd_float2x2 m, simd_float2 v) { return m.columns[0] * v.x + m.columns[1] * v.y; }
PD_SIMD simd_float3 simd_mul(simd_float3x3 m, simd_float3 v) {
  return m.columns[0] * v.x + m.columns[1] * v.y + m.columns[2] * v.z;
}
PD_SIMD simd_float4 simd_mul(simd_float4x4 m, simd_float4 v) {
  return m.columns[0] * v.x + m.columns[1] * v.y + m.columns[2] * v.z + m.columns[3] * v.w;
}
PD_SIMD simd_float3x3 simd_mul(simd_float3x3 a, simd_float3x3 b) {
  return (simd_float3x3){ { simd_mul(a, b.columns[0]), simd_mul(a, b.columns[1]), simd_mul(a, b.columns[2]) } };
}
PD_SIMD simd_float4x4 simd_mul(simd_float4x4 a, simd_float4x4 b) {
  return (simd_float4x4){ { simd_mul(a, b.columns[0]), simd_mul(a, b.columns[1]), simd_mul(a, b.columns[2]),
                            simd_mul(a, b.columns[3]) } };
}
PD_SIMD simd_float3x3 simd_mul(float s, simd_float3x3 m) {
  return (simd_float3x3){ { m.columns[0] * s, m.columns[1] * s, m.columns[2] * s } };
}
PD_SIMD simd_float4x4 simd_mul(float s, simd_float4x4 m) {
  return (simd_float4x4){ { m.columns[0] * s, m.columns[1] * s, m.columns[2] * s, m.columns[3] * s } };
}
PD_SIMD simd_float3x3 simd_add(simd_float3x3 a, simd_float3x3 b) {
  return (simd_float3x3){ { a.columns[0] + b.columns[0], a.columns[1] + b.columns[1], a.columns[2] + b.columns[2] } };
}
PD_SIMD simd_float4x4 simd_add(simd_float4x4 a, simd_float4x4 b) {
  return (simd_float4x4){ { a.columns[0] + b.columns[0], a.columns[1] + b.columns[1], a.columns[2] + b.columns[2],
                            a.columns[3] + b.columns[3] } };
}
PD_SIMD simd_float3x3 simd_transpose(simd_float3x3 m) {
  return simd_matrix_from_rows(m.columns[0], m.columns[1], m.columns[2]);
}
PD_SIMD simd_float4x4 simd_transpose(simd_float4x4 m) {
  return simd_matrix_from_rows(m.columns[0], m.columns[1], m.columns[2], m.columns[3]);
}
PD_SIMD float simd_determinant(simd_float3x3 m) {
  return simd_dot(m.columns[0], simd_cross(m.columns[1], m.columns[2]));
}
PD_SIMD simd_float3x3 simd_inverse(simd_float3x3 m) {
  simd_float3 a = m.columns[0], b = m.columns[1], c = m.columns[2];
  simd_float3 r0 = simd_cross(b, c), r1 = simd_cross(c, a), r2 = simd_cross(a, b);
  float inv = 1.0f / simd_dot(a, r0);

  return simd_matrix_from_rows(r0 * inv, r1 * inv, r2 * inv);
}
PD_SIMD float simd_determinant(simd_float4x4 m) {
  const float *a = (const float *)&m;
#define PD_M(c, r) a[(c) * 4 + (r)]
  float s0 = PD_M(0, 0) * PD_M(1, 1) - PD_M(1, 0) * PD_M(0, 1), s1 = PD_M(0, 0) * PD_M(1, 2) - PD_M(1, 0) * PD_M(0, 2);
  float s2 = PD_M(0, 0) * PD_M(1, 3) - PD_M(1, 0) * PD_M(0, 3), s3 = PD_M(0, 1) * PD_M(1, 2) - PD_M(1, 1) * PD_M(0, 2);
  float s4 = PD_M(0, 1) * PD_M(1, 3) - PD_M(1, 1) * PD_M(0, 3), s5 = PD_M(0, 2) * PD_M(1, 3) - PD_M(1, 2) * PD_M(0, 3);
  float c5 = PD_M(2, 2) * PD_M(3, 3) - PD_M(3, 2) * PD_M(2, 3), c4 = PD_M(2, 1) * PD_M(3, 3) - PD_M(3, 1) * PD_M(2, 3);
  float c3 = PD_M(2, 1) * PD_M(3, 2) - PD_M(3, 1) * PD_M(2, 2), c2 = PD_M(2, 0) * PD_M(3, 3) - PD_M(3, 0) * PD_M(2, 3);
  float c1 = PD_M(2, 0) * PD_M(3, 2) - PD_M(3, 0) * PD_M(2, 2), c0 = PD_M(2, 0) * PD_M(3, 1) - PD_M(3, 0) * PD_M(2, 1);
#undef PD_M
  return s0 * c5 - s1 * c4 + s2 * c3 + s3 * c2 - s4 * c1 + s5 * c0;
}
// general 4x4 inverse by cofactors (rows of the 2x2 sub-determinants)
PD_SIMD simd_float4x4 simd_inverse(simd_float4x4 m) {
  const float *a = (const float *)&m;
  float o[16], det;
#define PD_M(c, r) a[(c) * 4 + (r)]
  float s0 = PD_M(0, 0) * PD_M(1, 1) - PD_M(1, 0) * PD_M(0, 1), s1 = PD_M(0, 0) * PD_M(1, 2) - PD_M(1, 0) * PD_M(0, 2);
  float s2 = PD_M(0, 0) * PD_M(1, 3) - PD_M(1, 0) * PD_M(0, 3), s3 = PD_M(0, 1) * PD_M(1, 2) - PD_M(1, 1) * PD_M(0, 2);
  float s4 = PD_M(0, 1) * PD_M(1, 3) - PD_M(1, 1) * PD_M(0, 3), s5 = PD_M(0, 2) * PD_M(1, 3) - PD_M(1, 2) * PD_M(0, 3);
  float c5 = PD_M(2, 2) * PD_M(3, 3) - PD_M(3, 2) * PD_M(2, 3), c4 = PD_M(2, 1) * PD_M(3, 3) - PD_M(3, 1) * PD_M(2, 3);
  float c3 = PD_M(2, 1) * PD_M(3, 2) - PD_M(3, 1) * PD_M(2, 2), c2 = PD_M(2, 0) * PD_M(3, 3) - PD_M(3, 0) * PD_M(2, 3);
  float c1 = PD_M(2, 0) * PD_M(3, 2) - PD_M(3, 0) * PD_M(2, 2), c0 = PD_M(2, 0) * PD_M(3, 1) - PD_M(3, 0) * PD_M(2, 1);

  det = s0 * c5 - s1 * c4 + s2 * c3 + s3 * c2 - s4 * c1 + s5 * c0;
  det = 1.0f / det;
  // o is column-major like the input: o[c * 4 + r]
  o[0] = (PD_M(1, 1) * c5 - PD_M(1, 2) * c4 + PD_M(1, 3) * c3) * det;
  o[4] = (-PD_M(1, 0) * c5 + PD_M(1, 2) * c2 - PD_M(1, 3) * c1) * det;
  o[8] = (PD_M(1, 0) * c4 - PD_M(1, 1) * c2 + PD_M(1, 3) * c0) * det;
  o[12] = (-PD_M(1, 0) * c3 + PD_M(1, 1) * c1 - PD_M(1, 2) * c0) * det;
  o[1] = (-PD_M(0, 1) * c5 + PD_M(0, 2) * c4 - PD_M(0, 3) * c3) * det;
  o[5] = (PD_M(0, 0) * c5 - PD_M(0, 2) * c2 + PD_M(0, 3) * c1) * det;
  o[9] = (-PD_M(0, 0) * c4 + PD_M(0, 1) * c2 - PD_M(0, 3) * c0) * det;
  o[13] = (PD_M(0, 0) * c3 - PD_M(0, 1) * c1 + PD_M(0, 2) * c0) * det;
  o[2] = (PD_M(3, 1) * s5 - PD_M(3, 2) * s4 + PD_M(3, 3) * s3) * det;
  o[6] = (-PD_M(3, 0) * s5 + PD_M(3, 2) * s2 - PD_M(3, 3) * s1) * det;
  o[10] = (PD_M(3, 0) * s4 - PD_M(3, 1) * s2 + PD_M(3, 3) * s0) * det;
  o[14] = (-PD_M(3, 0) * s3 + PD_M(3, 1) * s1 - PD_M(3, 2) * s0) * det;
  o[3] = (-PD_M(2, 1) * s5 + PD_M(2, 2) * s4 - PD_M(2, 3) * s3) * det;
  o[7] = (PD_M(2, 0) * s5 - PD_M(2, 2) * s2 + PD_M(2, 3) * s1) * det;
  o[11] = (-PD_M(2, 0) * s4 + PD_M(2, 1) * s2 - PD_M(2, 3) * s0) * det;
  o[15] = (PD_M(2, 0) * s3 - PD_M(2, 1) * s1 + PD_M(2, 2) * s0) * det;
#undef PD_M
  return (simd_float4x4){ { { o[0], o[1], o[2], o[3] }, { o[4], o[5], o[6], o[7] }, { o[8], o[9], o[10], o[11] },
                            { o[12], o[13], o[14], o[15] } } };
}

// quaternions (x, y, z imaginary, w real)
PD_SIMD simd_quatf simd_quaternion(float x, float y, float z, float w) { return (simd_quatf){ { x, y, z, w } }; }
PD_SIMD simd_quatf simd_quaternion(simd_float4 v) { return (simd_quatf){ v }; }
PD_SIMD simd_quatf simd_quaternion(float angle, simd_float3 axis) {
  simd_float3 n = simd_normalize(axis) * sinf(angle / 2);

  return (simd_quatf){ { n.x, n.y, n.z, cosf(angle / 2) } };
}
PD_SIMD simd_quatf simd_mul(simd_quatf p, simd_quatf q) {
  simd_float4 a = p.vector, b = q.vector;

  return (simd_quatf){ { a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y, a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
                         a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w, a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z } };
}
PD_SIMD simd_quatf simd_normalize(simd_quatf q) { return (simd_quatf){ simd_normalize(q.vector) }; }
PD_SIMD simd_float3x3 simd_matrix3x3(simd_quatf q) {
  float x = q.vector.x, y = q.vector.y, z = q.vector.z, w = q.vector.w;

  return (simd_float3x3){ { { 1 - 2 * (y * y + z * z), 2 * (x * y + z * w), 2 * (x * z - y * w) },
                            { 2 * (x * y - z * w), 1 - 2 * (x * x + z * z), 2 * (y * z + x * w) },
                            { 2 * (x * z + y * w), 2 * (y * z - x * w), 1 - 2 * (x * x + y * y) } } };
}
PD_SIMD simd_float4x4 simd_matrix4x4(simd_quatf q) {
  simd_float3x3 r = simd_matrix3x3(q);

  return (simd_float4x4){ { simd_make_float4(r.columns[0], 0), simd_make_float4(r.columns[1], 0),
                            simd_make_float4(r.columns[2], 0), { 0, 0, 0, 1 } } };
}

// lane tests on comparison results (true lanes are -1): the sign bit of every or any lane
PD_SIMD bool simd_all(simd_int2 v) { return (v.x & v.y) < 0; }
PD_SIMD bool simd_all(simd_int3 v) { return (v.x & v.y & v.z) < 0; }
PD_SIMD bool simd_all(simd_int4 v) { return (v.x & v.y & v.z & v.w) < 0; }
PD_SIMD bool simd_any(simd_int2 v) { return (v.x | v.y) < 0; }
PD_SIMD bool simd_any(simd_int3 v) { return (v.x | v.y | v.z) < 0; }
PD_SIMD bool simd_any(simd_int4 v) { return (v.x | v.y | v.z | v.w) < 0; }

#ifdef __cplusplus
// the simd:: names Objective-C++ samples use
namespace simd {
template <typename V> static inline bool all(V v) { return ::simd_all(v); }
template <typename V> static inline bool any(V v) { return ::simd_any(v); }
template <typename V> static inline auto dot(V a, V b) { return ::simd_dot(a, b); }
template <typename V> static inline V normalize(V v) { return ::simd_normalize(v); }
template <typename V> static inline auto length(V v) { return ::simd_length(v); }
static inline simd_float3 cross(simd_float3 a, simd_float3 b) { return ::simd_cross(a, b); }
}
#endif

// the older vector_ and matrix_ spellings
#define vector_dot simd_dot
#define vector_cross simd_cross
#define vector_length simd_length
#define vector_length_squared simd_length_squared
#define vector_distance simd_distance
#define vector_normalize simd_normalize
#define vector_mix simd_mix
#define vector_min simd_min
#define vector_max simd_max
#define vector_clamp simd_clamp
#define vector_abs simd_abs
#define matrix_multiply simd_mul
#define matrix_transpose simd_transpose
#define matrix_invert simd_inverse
#define matrix_determinant simd_determinant
#define matrix_from_columns simd_matrix
#define matrix_from_rows simd_matrix_from_rows
#define matrix_from_diagonal simd_diagonal_matrix
#define matrix_add simd_add

#endif
