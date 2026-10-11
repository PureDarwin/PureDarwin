// the spinning cube's transform, shared by pdvkspin and pdvkdisplay

#ifndef CUBE_MATH_H
#define CUBE_MATH_H

#include <math.h>
#include <string.h>

// column-major 4x4: out = a * b
static inline void
mul(float *out, const float *a, const float *b)
{
	float t[16];

	for (int c = 0; c < 4; c++) {
		for (int r = 0; r < 4; r++) {
			t[c * 4 + r] = a[0 * 4 + r] * b[c * 4 + 0] + a[1 * 4 + r] * b[c * 4 + 1] +
			    a[2 * 4 + r] * b[c * 4 + 2] + a[3 * 4 + r] * b[c * 4 + 3];
		}
	}
	memcpy(out, t, sizeof(t));
}

// vulkan clip space: y flipped so +y is up on screen, depth 0..1, camera 4 units back on +z
static inline void
cube_mvp(float *m, float t, float aspect)
{
	float f = 1.0f / tanf(0.5f * 0.8f), zn = 0.1f, zf = 100.0f;
	float a = t * 1.1f, b = t * 0.7f;
	float proj[16] = { f / aspect, 0, 0, 0, 0, -f, 0, 0, 0, 0, zf / (zn - zf), -1, 0, 0, zn * zf / (zn - zf), 0 };
	float view[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, -4.5f, 1 };
	float ry[16] = { cosf(a), 0, -sinf(a), 0, 0, 1, 0, 0, sinf(a), 0, cosf(a), 0, 0, 0, 0, 1 };
	float rx[16] = { 1, 0, 0, 0, 0, cosf(b), sinf(b), 0, 0, -sinf(b), cosf(b), 0, 0, 0, 0, 1 };

	mul(m, ry, rx);
	mul(m, view, m);
	mul(m, proj, m);
}

#endif
