#version 450
layout(push_constant) uniform pc { mat4 mvp; };
layout(location = 0) out vec3 c;
// faces wound counter-clockwise seen from outside; corner i is (bit0, bit1, bit2) -> -1/+1
const int idx[36] = int[](4,5,7, 4,7,6,  0,2,3, 0,3,1,  1,3,7, 1,7,5,
                          0,4,6, 0,6,2,  6,7,3, 6,3,2,  0,1,5, 0,5,4);
const float shade[6] = float[](1.0, 0.75, 0.9, 0.8, 0.95, 0.7);
void main()
{
	int i = idx[gl_VertexIndex];
	vec3 p = vec3((i & 1) != 0 ? 1.0 : -1.0, (i & 2) != 0 ? 1.0 : -1.0, (i & 4) != 0 ? 1.0 : -1.0);
	gl_Position = mvp * vec4(p, 1.0);
	c = (p * 0.5 + 0.5) * shade[gl_VertexIndex / 6];
}
