// the auipc result lives in a callee-saved copy across calls, its %pcrel_lo users read the copy
extern void ext(int);
static volatile int g_loop;
int copied(int n)
{
	int sum = 0;
	for (int i = 0; i < n; i++) {
		g_loop = i;
		ext(i);
		__asm__ volatile(".fill 1100, 4, 0x00000013");
		sum += g_loop;
		ext(sum);
	}
	return sum + g_loop;
}
