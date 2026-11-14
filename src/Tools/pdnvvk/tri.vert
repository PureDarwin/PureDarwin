#version 450
// a triangle from the vertex index, one colour per corner
layout(location = 0) out vec3 color;

void main()
{
	vec2 pos[3] = vec2[](vec2(0.0, -0.7), vec2(0.7, 0.6), vec2(-0.7, 0.6));
	vec3 col[3] = vec3[](vec3(1.0, 0.2, 0.2), vec3(0.2, 1.0, 0.2), vec3(0.2, 0.4, 1.0));

	gl_Position = vec4(pos[gl_VertexIndex], 0.0, 1.0);
	color = col[gl_VertexIndex];
}
