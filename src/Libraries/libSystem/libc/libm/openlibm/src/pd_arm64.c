#include <stdint.h>

/* ARM64 has a 32-bit FPSR and uses IEEE double for C long double. */
const uint32_t __fe_dfl_env = 0;

int
fegetenv(uint32_t *envp)
{
    uint64_t fpsr;
    __asm__ volatile("mrs %0, fpsr" : "=r"(fpsr));
    *envp = (uint32_t)fpsr;
    return 0;
}

int
fesetenv(const uint32_t *envp)
{
    uint64_t fpsr = *envp;
    __asm__ volatile("msr fpsr, %0" : : "r"(fpsr));
    return 0;
}

double
nanl(const char *tagp)
{
    (void)tagp;
    return __builtin_nan("");
}
