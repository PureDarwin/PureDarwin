#include <asmjit/a64.h>

#include <stdio.h>

using namespace asmjit;

typedef int (*AddFn)(int, int);
typedef int (*ConstFn)(void);
typedef int (*SumFn)(int);

static int g_failures = 0;

static void
report(const char *what, bool ok, const char *detail)
{
	printf("  %-22s %s%s%s\n", what, ok ? "PASS" : "FAIL",
	    detail && detail[0] ? " - " : "", detail ? detail : "");
	if (!ok) {
		g_failures++;
	}
}

/* int add(int a, int b) { return a + b; } */
static void
test_add(JitRuntime &rt)
{
	CodeHolder code;
	Error err = code.init(rt.environment(), rt.cpu_features());

	if (err != Error::kOk) {
		report("add(a,b)", false, DebugUtils::error_as_string(err));
		return;
	}

	a64::Assembler a(&code);
	a.add(a64::w0, a64::w0, a64::w1);
	a.ret(a64::x30);

	AddFn fn = nullptr;
	err = rt.add(&fn, &code);
	if (err != Error::kOk) {
		report("add(a,b)", false, DebugUtils::error_as_string(err));
		return;
	}

	int r = fn(3, 4);
	bool ok = (r == 7) && (fn(-10, 4) == -6);
	char buf[64];
	snprintf(buf, sizeof(buf), "add(3,4)=%d", r);
	report("add(a,b)", ok, buf);
	rt.release(fn);
}

/* int c(void) { return 12345; } - exercises a materialised constant */
static void
test_const(JitRuntime &rt)
{
	CodeHolder code;
	Error err = code.init(rt.environment(), rt.cpu_features());

	if (err != Error::kOk) {
		report("return 12345", false, DebugUtils::error_as_string(err));
		return;
	}

	a64::Assembler a(&code);
	a.mov(a64::w0, 12345);
	a.ret(a64::x30);

	ConstFn fn = nullptr;
	err = rt.add(&fn, &code);
	if (err != Error::kOk) {
		report("return 12345", false, DebugUtils::error_as_string(err));
		return;
	}

	int r = fn();
	char buf[64];
	snprintf(buf, sizeof(buf), "got %d", r);
	report("return 12345", r == 12345, buf);
	rt.release(fn);
}

/*
 * int sum(int n) { int s = 0; while (n > 0) { s += n; n--; } return s; }
 *
 * A loop means labels, a backward branch and a real relocation pass - the
 * parts of a JIT that a bad icache story tends to break, because the branch
 * target is written after the branch itself.
 */
static void
test_loop(JitRuntime &rt)
{
	CodeHolder code;
	Error err = code.init(rt.environment(), rt.cpu_features());

	if (err != Error::kOk) {
		report("sum(1..n) loop", false, DebugUtils::error_as_string(err));
		return;
	}

	a64::Assembler a(&code);
	Label loop = a.new_label();
	Label done = a.new_label();

	a.mov(a64::w1, 0);              /* s = 0 */
	a.bind(loop);
	a.cmp(a64::w0, 0);
	a.b_le(done);
	a.add(a64::w1, a64::w1, a64::w0); /* s += n */
	a.sub(a64::w0, a64::w0, 1);       /* n--    */
	a.b(loop);
	a.bind(done);
	a.mov(a64::w0, a64::w1);
	a.ret(a64::x30);

	SumFn fn = nullptr;
	err = rt.add(&fn, &code);
	if (err != Error::kOk) {
		report("sum(1..n) loop", false, DebugUtils::error_as_string(err));
		return;
	}

	int r = fn(100);
	bool ok = (r == 5050) && (fn(0) == 0) && (fn(1) == 1);
	char buf[64];
	snprintf(buf, sizeof(buf), "sum(1..100)=%d", r);
	report("sum(1..n) loop", ok, buf);
	rt.release(fn);
}

/* Many functions live at once, then all called - the allocator's normal life. */
#define NFUNCS 200
static void
test_many(JitRuntime &rt)
{
	ConstFn fns[NFUNCS];
	int made = 0;
	bool ok = true;

	for (int i = 0; i < NFUNCS; i++) {
		CodeHolder code;

		if (code.init(rt.environment(), rt.cpu_features()) != Error::kOk) {
			break;
		}
		a64::Assembler a(&code);
		a.mov(a64::w0, i);
		a.ret(a64::x30);

		fns[i] = nullptr;
		if (rt.add(&fns[i], &code) != Error::kOk) {
			break;
		}
		made++;
	}

	if (made != NFUNCS) {
		char buf[64];
		snprintf(buf, sizeof(buf), "only %d/%d allocated", made, NFUNCS);
		report("200 live functions", false, buf);
		for (int i = 0; i < made; i++) {
			rt.release(fns[i]);
		}
		return;
	}

	for (int i = 0; i < NFUNCS; i++) {
		if (fns[i]() != i) {
			ok = false;
			break;
		}
	}
	for (int i = 0; i < NFUNCS; i++) {
		rt.release(fns[i]);
	}
	report("200 live functions", ok, ok ? "all returned their index" : "wrong value");
}

int
main(void)
{
	printf("=== PureDarwin asmjit test ===\n");
	printf("asmjit %u.%u.%u\n",
	    (ASMJIT_LIBRARY_VERSION >> 16) & 0xff,
	    (ASMJIT_LIBRARY_VERSION >> 8) & 0xff,
	    ASMJIT_LIBRARY_VERSION & 0xff);

	JitRuntime rt;

	printf("target arch = %u, JIT allocator ready\n",
	    (unsigned)rt.environment().arch());
	fflush(stdout);

	test_add(rt);
	test_const(rt);
	test_loop(rt);
	test_many(rt);
	fflush(stdout);

	printf("=== asmjit: %s ===\n",
	    g_failures == 0 ? "WORKS" : "FAILED");
	return g_failures == 0 ? 0 : 1;
}
