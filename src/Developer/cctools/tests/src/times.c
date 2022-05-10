#include <stdio.h>
#include <sys/stat.h>
#include <time.h>

int main(int argc, const char* argv[])
{
  for(int i = 1; i < argc; ++i) {
    const char* path = argv[i];
    struct stat sb;

    printf("%s\n", path);
    if (0 == stat(path, &sb)) {
      printf("  atime: %ld\t%ld\n", sb.st_atimespec.tv_sec,
                                    sb.st_atimespec.tv_nsec);
      printf("  mtime: %ld\t%ld\n", sb.st_mtimespec.tv_sec,
                                    sb.st_mtimespec.tv_nsec);
      printf("  ctime: %ld\t%ld\n", sb.st_ctimespec.tv_sec,
                                    sb.st_ctimespec.tv_nsec);
      printf("  btime: %ld\t%ld\n", sb.st_birthtimespec.tv_sec,
                                    sb.st_birthtimespec.tv_nsec);
    }
  }
}
