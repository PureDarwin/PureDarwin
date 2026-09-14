/*
 * Apple's __math_errhandling, behind math.h's math_errhandling macro.
 * openlibm reports errors through floating-point exceptions, not errno.
 */
#include <math.h>

int __math_errhandling(void)
{
    return MATH_ERREXCEPT;
}
