// MPSKernels.metal - the compute kernels behind PureDarwin's MetalPerformanceShaders
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#include <metal_stdlib>
using namespace metal;

struct MPSBlurParams {
    int radius;
    int horizontal;
    // 0 reads zeros past the edge, 1 repeats the edge texel
    int edgeMode;
    int2 offset;
};

// one pass of a separable blur, the weights for -radius ... radius in order
kernel void mps_separable_blur(texture2d<float, access::read> source [[texture(0)]],
                               texture2d<float, access::write> destination [[texture(1)]],
                               constant float *weights [[buffer(0)]],
                               constant MPSBlurParams &params [[buffer(1)]],
                               uint2 gid [[thread_position_in_grid]])
{
    if (gid.x >= destination.get_width() || gid.y >= destination.get_height())
        return;

    int2 size = int2(source.get_width(), source.get_height());
    int2 centre = int2(gid) + params.offset;
    int2 step = params.horizontal ? int2(1, 0) : int2(0, 1);
    float4 sum = float4(0.0);

    for (int i = -params.radius; i <= params.radius; i++) {
        int2 p = centre + step * i;
        bool inside = p.x >= 0 && p.y >= 0 && p.x < size.x && p.y < size.y;
        if (!inside && params.edgeMode == 0)
            continue;
        p = clamp(p, int2(0), size - 1);
        sum += source.read(uint2(p)) * weights[i + params.radius];
    }
    destination.write(sum, gid);
}
