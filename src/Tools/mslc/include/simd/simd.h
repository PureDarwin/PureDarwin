// simd/simd.h for mslc: the vector types under their simd names
#ifndef PD_SIMD_H
#define PD_SIMD_H

#include <metal_stdlib>

#define PD_SIMD(T)                                                                                                  \
  typedef T simd_##T##2 __attribute__((ext_vector_type(2)));                                                        \
  typedef T simd_##T##3 __attribute__((ext_vector_type(3)));                                                        \
  typedef T simd_##T##4 __attribute__((ext_vector_type(4)));                                                        \
  typedef T vector_##T##2 __attribute__((ext_vector_type(2)));                                                      \
  typedef T vector_##T##3 __attribute__((ext_vector_type(3)));                                                      \
  typedef T vector_##T##4 __attribute__((ext_vector_type(4)));

PD_SIMD(float)
PD_SIMD(half)
PD_SIMD(int)
PD_SIMD(uint)

// their constructors T(a, b, ...) as metal_stdlib makes floatN(...)
#define simd_float2(...) ::metal::__pd_make<::simd_float2>(__VA_ARGS__)
#define vector_float2(...) ::metal::__pd_make<::vector_float2>(__VA_ARGS__)
#define simd_float3(...) ::metal::__pd_make<::simd_float3>(__VA_ARGS__)
#define vector_float3(...) ::metal::__pd_make<::vector_float3>(__VA_ARGS__)
#define simd_float4(...) ::metal::__pd_make<::simd_float4>(__VA_ARGS__)
#define vector_float4(...) ::metal::__pd_make<::vector_float4>(__VA_ARGS__)
#define simd_half2(...) ::metal::__pd_make<::simd_half2>(__VA_ARGS__)
#define vector_half2(...) ::metal::__pd_make<::vector_half2>(__VA_ARGS__)
#define simd_half3(...) ::metal::__pd_make<::simd_half3>(__VA_ARGS__)
#define vector_half3(...) ::metal::__pd_make<::vector_half3>(__VA_ARGS__)
#define simd_half4(...) ::metal::__pd_make<::simd_half4>(__VA_ARGS__)
#define vector_half4(...) ::metal::__pd_make<::vector_half4>(__VA_ARGS__)
#define simd_int2(...) ::metal::__pd_make<::simd_int2>(__VA_ARGS__)
#define vector_int2(...) ::metal::__pd_make<::vector_int2>(__VA_ARGS__)
#define simd_int3(...) ::metal::__pd_make<::simd_int3>(__VA_ARGS__)
#define vector_int3(...) ::metal::__pd_make<::vector_int3>(__VA_ARGS__)
#define simd_int4(...) ::metal::__pd_make<::simd_int4>(__VA_ARGS__)
#define vector_int4(...) ::metal::__pd_make<::vector_int4>(__VA_ARGS__)
#define simd_uint2(...) ::metal::__pd_make<::simd_uint2>(__VA_ARGS__)
#define vector_uint2(...) ::metal::__pd_make<::vector_uint2>(__VA_ARGS__)
#define simd_uint3(...) ::metal::__pd_make<::simd_uint3>(__VA_ARGS__)
#define vector_uint3(...) ::metal::__pd_make<::vector_uint3>(__VA_ARGS__)
#define simd_uint4(...) ::metal::__pd_make<::simd_uint4>(__VA_ARGS__)
#define vector_uint4(...) ::metal::__pd_make<::vector_uint4>(__VA_ARGS__)

// the matrices under their simd names, columns by rows as in metal_stdlib
#define PD_SIMD_MATRIX(T, C, R)                                                                                     \
  typedef metal::T##C##x##R simd_##T##C##x##R;                                                                     \
  typedef metal::T##C##x##R matrix_##T##C##x##R;
#define PD_SIMD_MATRICES(T)                                                                                         \
  PD_SIMD_MATRIX(T, 2, 2) PD_SIMD_MATRIX(T, 2, 3) PD_SIMD_MATRIX(T, 2, 4)                                           \
  PD_SIMD_MATRIX(T, 3, 2) PD_SIMD_MATRIX(T, 3, 3) PD_SIMD_MATRIX(T, 3, 4)                                           \
  PD_SIMD_MATRIX(T, 4, 2) PD_SIMD_MATRIX(T, 4, 3) PD_SIMD_MATRIX(T, 4, 4)

PD_SIMD_MATRICES(float)
PD_SIMD_MATRICES(half)

#endif
