#include <string.h>

int
bcmp(const void *s1, const void *s2, size_t n)
{
    return memcmp(s1, s2, n);
}

int
flock(int fd, int operation)
{
    (void)fd;
    (void)operation;
    return 0;
}
