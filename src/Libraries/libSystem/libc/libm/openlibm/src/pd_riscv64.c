#include <math.h>

// riscv64 darwin long double is double, the ld80 tree that would provide this is not built
long double
nanl(const char *tagp)
{
	return nan(tagp);
}
