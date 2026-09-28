// bare-metal riscv64 mach-o smoke test: prints over the sbi console, then waits forever
static long sbi_call(long ext, long fid, long a0)
{
	register long x10 asm("a0") = a0;
	register long x16 asm("a6") = fid;
	register long x17 asm("a7") = ext;
	asm volatile("ecall" : "+r"(x10) : "r"(x16), "r"(x17) : "memory");
	return x10;
}

// legacy console putchar, every sbi implementation still has it
static void putc_sbi(char c)
{
	sbi_call(0x01, 0, c);
}

static const char banner[] = "hello from a riscv64 mach-o\n";
int counter;

void puts_sbi(const char *s)
{
	while (*s)
		putc_sbi(*s++);
}

void kmain(void)
{
	counter++;
	puts_sbi(banner);
	for (;;)
		asm volatile("wfi");
}
