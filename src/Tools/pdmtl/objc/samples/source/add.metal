#include <metal_stdlib>
using namespace metal;

// out[i] = a[i] * SCALE + b[i], SCALE comes from the compile options' preprocessor macros (2 when absent)
#ifndef SCALE
#define SCALE 2
#endif

kernel void scale_add(device const float *a [[buffer(0)]], device const float *b [[buffer(1)]],
                      device float *out [[buffer(2)]], uint i [[thread_position_in_grid]]) {
  out[i] = a[i] * SCALE + b[i];
}
