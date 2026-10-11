// pdmtl window demo: a plasma background and three spinning blended stars that follow the mouse.
// demo-xcode13.metallib: metal 31001.332 (Xcode 13), demo-xcode26.metallib: metal 32023.883 (Xcode 26.6)
#include <metal_stdlib>
using namespace metal;

struct U {
  float time, aspect;
  float2 mouse;
};

struct VO {
  float4 pos [[position]];
  float4 col;
  float2 uv;
};

vertex VO v_bg(uint vid [[vertex_id]]) {
  float2 p = float2((vid & 1) ? 1.0f : -1.0f, (vid & 2) ? 1.0f : -1.0f);
  VO o;

  o.pos = float4(p, 0, 1);
  o.uv = p;
  o.col = float4(1);
  return o;
}

fragment float4 f_plasma(VO in [[stage_in]], constant U &u [[buffer(0)]]) {
  float2 p = in.uv * float2(u.aspect, 1.0f) * 3.0f;
  float t = u.time;
  float v = sin(p.x + t) + sin(p.y * 1.3f - t * 0.7f) + sin(length(p) * 2.0f - t * 1.5f);
  float3 c = 0.5f + 0.5f * cos(v + float3(0.0f, 2.1f, 4.2f));
  return float4(c * 0.6f, 1.0f);
}

// a ten-point star as ten triangles around its centre, one instance per star
vertex VO v_star(uint vid [[vertex_id]], uint iid [[instance_id]], constant U &u [[buffer(0)]]) {
  uint tri = vid / 3, k = vid % 3;
  uint pt = tri + (k == 2 ? 1 : 0);
  float scale = 0.55f - 0.15f * float(iid);
  float spin = u.time * (0.8f + 0.6f * float(iid)) * (iid == 1 ? -1.0f : 1.0f);
  float a = float(pt % 10) * (2.0f * M_PI_F / 10.0f) + spin;
  float r = (k == 0) ? 0.0f : ((pt % 2) ? 0.4f : 1.0f) * scale;
  float2 p = float2(cos(a), sin(a)) * r + u.mouse * (0.3f + 0.35f * float(iid));
  VO o;

  o.pos = float4(p.x / u.aspect, p.y, 0, 1);
  float3 hue = 0.5f + 0.5f * cos(float(iid) * 2.1f + u.time + float3(0.0f, 2.1f, 4.2f));
  o.col = k == 0 ? float4(1, 1, 1, 0.9f) : float4(hue, 0.75f);
  o.uv = p;
  return o;
}

fragment float4 f_star(VO in [[stage_in]]) {
  return in.col;
}
