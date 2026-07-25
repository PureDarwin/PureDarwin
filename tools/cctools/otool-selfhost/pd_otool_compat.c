#include <sys/stat.h>

extern int pd_fstat_inode64(int fd, struct stat *sb) __asm("_fstat$INODE64");

int
fstat(int fd, struct stat *sb)
{
    return pd_fstat_inode64(fd, sb);
}
