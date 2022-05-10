//
//  depinfo.c
//  stuff
//
//  Created by Michael Trent on 9/9/19.
//

#include "stuff/depinfo.h"

#include "stuff/errors.h"

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

struct dependency {
  uint8_t opcode;
  char* string;
};

struct depinfo
{
  int ndep;
  struct dependency* deps;
};

static char* dep_opcode_name(uint8_t);

struct depinfo* depinfo_alloc(void)
{
  return calloc(1, sizeof(struct depinfo));
}

void depinfo_free(struct depinfo* depinfo)
{
  if (depinfo)
  {
    for (int i = 0; i < depinfo->ndep; ++i)
    {
      free(depinfo->deps[i].string);
    }
    free(depinfo->deps);
    free(depinfo);
  }
}

void depinfo_add(struct depinfo* depinfo, uint8_t opcode, const char* string)
{
  assert(depinfo && "missing depinfo");
  
  depinfo->deps = reallocf(depinfo->deps,
                           sizeof(struct dependency) * (depinfo->ndep + 1));
  if (!depinfo->deps)
    fatal("out of memory");
  
  int i = depinfo->ndep++;
  depinfo->deps[i].opcode = opcode;
  depinfo->deps[i].string = strdup(string ? string : "");
}

int depinfo_count(struct depinfo* depinfo)
{
  assert(depinfo && "missing depinfo");
  return depinfo->ndep;
}

int depinfo_get(struct depinfo* depinfo, int i, uint8_t* opcode,
                const char** string)
{
  assert(depinfo && "missing depinfo");
  if (i < depinfo->ndep)
  {
    *opcode = depinfo->deps[i].opcode;
    *string = depinfo->deps[i].string;
    return 0;
  }
  return 1;
}

static int depcmp(const void* voida, const void* voidb)
{
  const struct dependency* a = (const struct dependency*)voida;
  const struct dependency* b = (const struct dependency*)voidb;
  if (a->opcode < b->opcode)
    return -1;
  if (a->opcode > b->opcode)
    return 1;
  return strcmp(a->string, b->string);
}

void depinfo_sort(struct depinfo* depinfo)
{
  assert(depinfo && "missing depinfo");
  qsort(depinfo->deps, depinfo->ndep, sizeof(struct dependency), depcmp);
}

struct depinfo* depinfo_read(const char* path, uint32_t flags)
{
  if (!path)
    return NULL;

  int fd = open(path, O_RDONLY);
  if (-1 == fd)
  {
    system_error("can't open %s", path);
    return NULL;
  }
  
  struct stat sb;
  if (-1 == fstat(fd, &sb))
  {
    system_error("can't open %s", path);
    close(fd);
    return NULL;
  }
  
  size_t size = (size_t)sb.st_size;
  unsigned char* buf = calloc(1, size + 1);
  
  ssize_t readed = read(fd, buf, size);
  if (readed != size)
  {
    if (-1 == readed)
      system_error("can't read from %s", path);
    else
      error("can't write %zu bytes from %s, read %zd bytes",
            size, path, readed);
    close(fd);
    return NULL;
  }
  close(fd);

  struct depinfo* depinfo = NULL;
  if (!(flags & DI_READ_NORETVAL))
  {
    depinfo = depinfo_alloc();
    if (!depinfo)
      return NULL;
  }
  
  // code  name           value
  // 0x11  INPUT_MISSING  /blah/blah/blah
  if (flags & DI_READ_LOG)
    printf("%4s  %-13s  %s\n", "code", "name", "value");

  int i = 0;
  while (i < size)
  {
    uint8_t opcode = buf[i++];

    char* string = NULL;
    if (i < size)
    {
      string = (char*)&buf[i];
      i += strlen(string) + 1;
    }
    
    if (!(flags & DI_READ_NORETVAL))
      depinfo_add(depinfo, opcode, string);
    
    if (flags & DI_READ_LOG)
    {
      const char* name = dep_opcode_name(opcode);
      printf("0x%02x  %-13s  %s", opcode, name, string);
      if (i > size)
        printf("(value extends beyond file)");
      printf("\n");
    }
  }
  
  free(buf);
  
  return depinfo;
}

int depinfo_write(struct depinfo* depinfo, const char* path)
{
  assert(depinfo && "missing depinfo");

  char* temp = NULL;
  asprintf(&temp, "%s.XXXXXX", path);
  
  int fd = mkstemp(temp);
  if (-1 == fd)
  {
    system_error("can't create %s", temp);
    return -1;
  }
  
  for (int i = 0; i < depinfo->ndep; ++i)
  {
    void* buf = &depinfo->deps[i].opcode;
    size_t size = sizeof(depinfo->deps[i].opcode);
    ssize_t wrote = write(fd, buf, size);
    if (wrote != size)
    {
      if (-1 == wrote)
        system_error("can't write to %s", temp);
      else
        error("can't write %zu bytes to %s, wrote %zd bytes",
              size, temp, wrote);
      close(fd);
      unlink(temp);
      return -1;
    }
    
    buf = depinfo->deps[i].string;
    size = strlen(depinfo->deps[i].string) + 1;
    wrote = write(fd, buf, size);
    if (wrote != size)
    {
      if (-1 == wrote)
        system_error("can't write to %s", temp);
      else
        error("can't write %zu bytes to %s, wrote %zd bytes",
              size, temp, wrote);
      close(fd);
      unlink(temp);
      return -1;
    }
  }
  
  if (-1 == fchmod(fd, 0644)) {
    system_error("can't chmod %s to 0644", temp);
    unlink(temp);
    return -1;
  }
  
  if (-1 == close(fd))
  {
    system_error("can't close %s", temp);
    unlink(temp);
    return -1;
  }
    
  if (-1 == rename(temp, path))
  {
    system_error("can't move %s to %s", temp, path);
    unlink(temp);
    return -1;
  }
  
  unlink(temp);

  return 0;
}

static char* dep_opcode_name(uint8_t opcode)
{
  switch (opcode) {
    case DEPINFO_TOOL:
      return "TOOL";
    case DEPINFO_OUTPUT:
      return "OUTPUT";
    case DEPINFO_INPUT_FOUND:
      return "INPUT_FOUND";
    case DEPINFO_INPUT_MISSING:
      return "INPUT_MISSING";
  }
  return "<UNKNOWN>";
}
