//
//  main.c
//  depinfo
//
//  Created by Michael Trent on 9/10/19.
//

#include "stuff/depinfo.h"
#include "stuff/errors.h"

#include <stdio.h>
#include <stdlib.h>

char* progname;

void usage(const char * __restrict format, ...);

int main(int argc, const char * argv[])
{
  progname = (char*)*argv++;
  argc--;
  
  if (argc == 0)
      usage(NULL);
  
  int showPaths = argc > 1;
  while (argc > 0)
  {
    if (showPaths)
      printf("%s:\n", *argv);
    depinfo_read(*argv, DI_READ_LOG | DI_READ_NORETVAL);
    if (errors)
      return 1;
    argv++;
    argc--;
  }
  return 0;
}

void usage(const char * __restrict format, ...)
{
  const char* basename = strrchr(progname, '/');
  if (basename)
    basename++;
  else
    basename = progname;
  
  va_list args;
  va_start(args, format);
  
  if (format) {
    fprintf(stderr, "error: ");
    vfprintf(stderr, format, args);;
    fprintf(stderr, "\n");
  }
  
  va_end(args);

  fprintf(stderr, "usage: %s <file> ...\n", basename);

  exit(EXIT_FAILURE);
}
