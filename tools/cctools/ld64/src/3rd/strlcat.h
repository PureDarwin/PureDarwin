#ifndef __APPLE__

#ifndef STRLCAT_H
#define STRLCAT_H

#ifdef __cplusplus
extern "C" {
#endif
#include <stddef.h>

size_t strlcat(char *dst, const char *src, size_t siz);

#ifdef __cplusplus
}
#endif

#endif

#endif /* ! __APPLE__ */