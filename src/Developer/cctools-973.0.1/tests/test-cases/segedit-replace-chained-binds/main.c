#include <mach-o/getsect.h>
#include <mach-o/ldsyms.h>
#include <stdio.h>

#ifdef __LP64__
#define MACH_HEADER mach_header_64
#else
#define MACH_HEADER mach_header
#endif

int gGlobal = 0;

int main(void)
{
  const struct MACH_HEADER* mhp = &_mh_execute_header;
  unsigned long size;
  const unsigned char* buf = getsectiondata(mhp, "Memento", "Mori", &size);
  if (buf) {
    printf("%s", buf);
  }
  return buf ? gGlobal : 1;
}
