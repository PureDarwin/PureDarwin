#ifndef __APPLE__

#ifndef STRLCPY_H
#define STRLCPY_H

#ifdef __cplusplus
extern "C" {
#endif
#include <stddef.h>

size_t strlcpy(char *dst, const char *src, size_t siz);

#ifdef __cplusplus
}
#endif

#endif

#endif /* ! __APPLE__ */