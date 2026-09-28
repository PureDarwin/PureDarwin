// every ctz copy against a bit loop, over each bit position and a spread of values
int ctz64_a(unsigned long long), ctz32_a(unsigned), ctz64_b(unsigned long long), ctz32_b(unsigned);
int ctz64_c(unsigned long long), ctz32_c(unsigned);
void puts_sbi(const char *);
static void putc1(char c) { register long a0 asm("a0") = c; register long a7 asm("a7") = 1; asm volatile("ecall" : "+r"(a0) : "r"(a7) : "memory"); }
static int ref(unsigned long long x) { int n = 0; while (!(x & 1)) { x >>= 1; n++; } return n; }
static void num(unsigned long v) { char b[24]; int i = 0; do { b[i++] = '0' + v % 10; v /= 10; } while (v); while (i) putc1(b[--i]); }
void kmain(void)
{
	int (*f64[])(unsigned long long) = { ctz64_a, ctz64_b, ctz64_c };
	int (*f32[])(unsigned) = { ctz32_a, ctz32_b, ctz32_c };
	unsigned long long s = 0x9e3779b97f4a7c15ULL;
	unsigned long checks = 0, bad = 0;
	for (int round = 0; round < 2000; round++) {
		s ^= s << 13; s ^= s >> 7; s ^= s << 17;
		for (int bit = 0; bit < 64; bit++) {
			unsigned long long x = (s | 1ULL << 63) << bit;
			if (!x) continue;
			for (int k = 0; k < 3; k++) {
				checks++;
				if (f64[k](x) != ref(x)) bad++;
				if ((unsigned)x) { checks++; if (f32[k]((unsigned)x) != ref((unsigned)x)) bad++; }
			}
		}
	}
	puts_sbi("ctz checks "); num(checks); puts_sbi(" mismatches "); num(bad); puts_sbi("\n");
	// sbi system reset, so qemu exits once the line is out
	register long a0 asm("a0") = 0, a1 asm("a1") = 0;
	register long a6 asm("a6") = 0, a7 asm("a7") = 0x53525354;
	asm volatile("ecall" : "+r"(a0), "+r"(a1) : "r"(a6), "r"(a7) : "memory");
	for (;;) asm volatile("wfi");
}
