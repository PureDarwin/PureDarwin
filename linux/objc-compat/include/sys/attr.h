// sys/attr.h for Argon on Linux: getattrlist has no counterpart, callers take their fallback
#pragma once
#include <errno.h>
#include <stdint.h>
#define ATTR_BIT_MAP_COUNT 5
#define ATTR_VOL_INFO 0x80000000
#define ATTR_VOL_NAME 0x00002000
typedef uint32_t attrgroup_t;
struct attrlist { uint16_t bitmapcount; uint16_t reserved; attrgroup_t commonattr, volattr, dirattr, fileattr, forkattr; };
typedef struct attrreference { int32_t attr_dataoffset; uint32_t attr_length; } attrreference_t;
static inline int getattrlist(const char *path, void *list, void *buf, size_t size, unsigned long options)
{
    (void)path; (void)list; (void)buf; (void)size; (void)options;
    errno = ENOTSUP;
    return -1;
}
