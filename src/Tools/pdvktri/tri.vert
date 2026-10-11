#version 450
layout(location = 0) out vec3 c;
vec2 p[3] = vec2[](vec2(0.0, -0.8), vec2(0.8, 0.8), vec2(-0.8, 0.8));
vec3 k[3] = vec3[](vec3(1, 0, 0), vec3(0, 1, 0), vec3(0, 0, 1));
void main() { gl_Position = vec4(p[gl_VertexIndex], 0.0, 1.0); c = k[gl_VertexIndex]; }
