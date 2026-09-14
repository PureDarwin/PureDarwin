/*
 * nexttowardl(3). openlibm aliases it to nextafterl with __strong_reference,
 * which emits nothing on Mach-O; for long double the two are identical.
 */
#include <math.h>

long double nexttowardl(long double x, long double y)
{
    return nextafterl(x, y);
}
