// every kind of %pcrel_lo user, near its auipc and more than 2KB away from it
#define FILL __asm__ volatile(".fill 1100, 4, 0x00000013")
#define R10(x) x x x x x x x x x x
#define R100(x) R10(R10(x))
#define R700(x) R100(x) R100(x) R100(x) R100(x) R100(x) R100(x) R100(x)

volatile int g_int;
volatile double g_dbl = 1.5;
// its own section keeps it out of the merged globals, so each store names it through %pcrel_lo
__attribute__((section("__DATA,__other"))) volatile int g_other;
extern volatile int g_ext;

int near_load(void) { int a = g_int; return a + g_int; }
int far_load(void) { int a = g_int; FILL; return a + g_int; }
void near_store(int v) { g_int = v; g_int = v + 1; }
void far_store(int v) { g_int = v; FILL; g_int = v + 1; }
double far_fp(void) { double a = g_dbl; FILL; return a + g_dbl; }
int far_ext(void) { int a = g_ext; FILL; return a + g_ext; }
// no inline asm, 700 stores through one auipc run past 2KB on their own
int far_code(void) { R700(g_other = 3;) return g_other; }
