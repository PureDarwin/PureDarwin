// runs every near and far user and checks the values they read and wrote
int near_load(void), far_load(void), far_ext(void), far_code(void);
void near_store(int), far_store(int);
double far_fp(void);
extern volatile int g_int, g_ext, g_other;
void puts_sbi(const char *);
static void num(unsigned long v) { char b[24]; int i = 0; do { b[i++] = '0' + v % 10; v /= 10; } while (v); b[i] = 0; char o[24]; for (int j = 0; j < i; j++) o[j] = b[i - 1 - j]; o[i] = 0; puts_sbi(o); }
void kmain(void)
{
	// fp registers stay off until sstatus.FS says otherwise
	__asm__ volatile("li t0, 0x2000\n csrs sstatus, t0" ::: "t0");
	int bad = 0, checks = 0;
	g_int = 20; checks++; if (near_load() != 40) bad++;
	g_int = 21; checks++; if (far_load() != 42) bad++;
	near_store(7); checks++; if (g_int != 8) bad++;
	far_store(9); checks++; if (g_int != 10) bad++;
	checks++; if (far_fp() != 3.0) bad++;
	checks++; if (far_ext() != 10) bad++;
	g_other = 0; checks++; if (far_code() != 3 || g_other != 3) bad++;
	puts_sbi("pcrel checks "); num(checks); puts_sbi(" mismatches "); num(bad); puts_sbi("\n");
	register long a0 __asm__("a0") = 0, a1 __asm__("a1") = 0;
	register long a6 __asm__("a6") = 0, a7 __asm__("a7") = 0x53525354;
	__asm__ volatile("ecall" : "+r"(a0), "+r"(a1) : "r"(a6), "r"(a7) : "memory");
	for (;;) __asm__ volatile("wfi");
}
