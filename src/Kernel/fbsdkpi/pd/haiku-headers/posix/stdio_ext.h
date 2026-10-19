/*
 * Copyright 2008-2025, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _STDIO_EXT_H_
#define _STDIO_EXT_H_


#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FSETLOCKING_QUERY		0
#define FSETLOCKING_INTERNAL	1
#define FSETLOCKING_BYCALLER	2

extern void		_flushlbf(void);
extern int		__fsetlocking(FILE* stream, int type);
extern void		__fseterr(FILE* stream);

extern int		__freading(FILE* stream);
extern int		__fwriting(FILE* stream);
extern int		__freadable(FILE* stream);
extern int		__fwritable(FILE* stream);
extern size_t	__freadahead(FILE* stream);
extern const char* __freadptr(FILE* stream, size_t* size);
extern void		__freadptrinc(FILE* stream, size_t inc);
extern int		__flbf(FILE* stream);
extern size_t	__fbufsize(FILE* stream);
extern size_t	__fpending(FILE* stream);
extern void		__fpurge(FILE* stream);

#ifdef __cplusplus
}
#endif

#endif /* _STDIO_EXT_H_ */

