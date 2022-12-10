//
//  test_util.c
//  libstuff_test
//
//  Created by Michael Trent on 5/31/19.
//

#include "test_util.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int test_write_tmp_data(const void* data, size_t size, char** name_p)
{
  if (!data || !name_p) {
    errno = EINVAL;
    return -1;
  }

  char* name = strdup("/tmp/libstuff_test.XXXXXX");
  int fd = mkstemp(name);
  if (-1 == fd) {
    fprintf(stderr, "error: cannot make temporary file: %s\n",
            strerror(errno));
    free(name);
    return -1;
  }

  const unsigned char* uchars = (const unsigned char*)data;
  while (size) {
    const size_t limit = 0x7FFFFFFF;
    size_t towrite = size < limit ? size : limit;
    ssize_t wrote = write(fd, uchars, towrite);
    if (-1 == wrote) {
      fprintf(stderr, "error: cannot write to file %s: %s\n", name,
              strerror(errno));
      free(name);
      return -1;
    }
    else if (0 == wrote) {
      break;
    }
    else {
      size -= wrote;
      uchars += wrote;
    }
  }

  *name_p = name;
  return 0;
}
