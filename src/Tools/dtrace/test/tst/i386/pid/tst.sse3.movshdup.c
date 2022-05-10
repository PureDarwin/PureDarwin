/*
 * ASSERTION : To test that SSE3 instructions can be instrumented by the pid
 * provider.
 *
 * SECTION: pid provider, x86_64
 *
 */

float g_vr[4];
struct myvect {
	union {
		float v[3];
		struct {
			float x,y,z; 
		};
	};
};

float multiply (struct myvect* v) {
	__asm__ volatile("movshdup (%0), %%xmm0" : : "r"(v) : "xmm0");

	return g_vr[0];
}

int main(void) {
	struct myvect v1, v2;
	float t = multiply(&v2);
	while(1) {

	}
}
