#version 450
layout(location = 0) in vec3 c;
layout(location = 0) out vec4 o;
void main() { o = vec4(c, 1.0); }
