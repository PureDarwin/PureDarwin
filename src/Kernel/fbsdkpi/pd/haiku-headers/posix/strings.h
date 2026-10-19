/*
 * Copyright 2014 Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _STRINGS_H_
#define _STRINGS_H_


#include <locale_t.h>
#include <sys/types.h>


#ifdef __cplusplus
extern "C" {
#endif

extern int		ffs(int i);
extern int		ffsl(long i);
extern int		ffsll(long long i);

extern int		strcasecmp(const char *string1, const char *string2);
extern int		strncasecmp(const char *string1, const char *string2,
	size_t length);

extern int		strcasecmp_l(const char *string1, const char *string2, locale_t locale);
extern int		strncasecmp_l(const char *string1, const char *string2,
	size_t length, locale_t locale);

/* legacy compatibility -- might be removed one day */
#define bcmp(a, b, length) memcmp((a), (b), (length))
#define bcopy(source, dest, length) memmove((dest), (source), (length))
#define bzero(buffer, length) memset((buffer), 0, (length))

extern char 		*index(const char *s, int c);
extern char 		*rindex(char const *s, int c);

#ifdef __cplusplus
}
#endif

#endif	/* _STRINGS_H_ */
