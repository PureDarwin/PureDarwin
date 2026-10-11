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

#endif
