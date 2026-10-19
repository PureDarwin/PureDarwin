/*
 * Copyright 2008-2012 Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _WCHAR_H
#define _WCHAR_H


#include <limits.h>
#include <locale_t.h>
#include <stddef.h>
#include <stdio.h>
#include <time.h>

#include <_mbstate_t.h>
#include <_wint_t.h>


#ifndef WCHAR_MIN
#define WCHAR_MIN	0x00000000UL
#define WCHAR_MAX	0x7FFFFFFFUL
#endif

/*
 * Haiku is always using UTF32 in wchars, other encodings can be handled
 * by converting to/from wchar by means of mbrtowc() or wcrtomb().
 */


#ifdef __cplusplus
extern "C" {
#endif

extern wint_t	btowc(int);

extern wint_t	fgetwc(FILE *stream);
extern wchar_t	*fgetws(wchar_t *dest, int destLength, FILE *stream);
extern wint_t	fputwc(wchar_t wc, FILE *stream);
extern int		fputws(const wchar_t *wcs, FILE *stream);
extern int		fwide(FILE *stream, int mode);
extern int		fwprintf(FILE *stream, const wchar_t *format, ...);
extern int		fwscanf(FILE *stream, const wchar_t *format, ...);
extern wint_t	getwc(FILE *stream);
extern wint_t	getwchar(void);

extern size_t 	mbrlen(const char *s, size_t n, mbstate_t *mbState);
extern size_t 	mbrtowc(wchar_t *dest, const char *src, size_t srcLength,
					mbstate_t *mbState);
extern int		mbsinit(const mbstate_t *mbState);
extern size_t	mbsnrtowcs(wchar_t *dest, const char **src, size_t srcLength,
					size_t destLength, mbstate_t *mbState);
extern size_t	mbsrtowcs(wchar_t *dest, const char **src, size_t destLength,
					mbstate_t *mbState);

extern wint_t	putwc(wchar_t wc, FILE *stream);
extern wint_t	putwchar(wchar_t wc);

extern int		swprintf(wchar_t *dest, size_t destLength,
					const wchar_t *format, ...);
extern int		swscanf(const wchar_t *wcs, const wchar_t *format, ...);

extern wint_t	ungetwc(wint_t wc, FILE *stream);

extern int		vfwprintf(FILE *stream, const wchar_t *format, va_list arg);
extern int		vfwscanf(FILE *stream, const wchar_t *format, va_list arg);
extern int		vswprintf(wchar_t *dest, size_t destLength,
					const wchar_t *format, va_list arg);
extern int		vswscanf(const wchar_t *wcs, const wchar_t *format,
					va_list arg);
extern int		vwprintf(const wchar_t *format, va_list arg);
extern int		vwscanf(const wchar_t *format, va_list arg);

extern wchar_t	*wcpcpy(wchar_t *dest, const wchar_t *src);
extern wchar_t	*wcpncpy(wchar_t *dest, const wchar_t *src, size_t srcLength);
extern size_t   wcrtomb(char *dest, wchar_t wc, mbstate_t *mbState);
extern int		wcscasecmp(const wchar_t *wcs1, const wchar_t *wcs2);
extern int		wcscasecmp_l(const wchar_t *wcs1, const wchar_t *wcs2, locale_t locale);
extern wchar_t	*wcscat(wchar_t *dest, const wchar_t *src);
extern wchar_t	*wcschr(const wchar_t *wcs, wchar_t wc);
#ifdef _DEFAULT_SOURCE
extern wchar_t	*wcschrnul(const wchar_t *wcs, wchar_t wc);
#endif
extern int      wcscmp(const wchar_t *wcs1, const wchar_t *wcs2);
extern int      wcscoll(const wchar_t *wcs1, const wchar_t *wcs2);
extern int		wcscoll_l(const wchar_t *wcs1, const wchar_t *wcs2, locale_t locale);
extern wchar_t	*wcscpy(wchar_t *dest, const wchar_t *src);
extern size_t	wcscspn(const wchar_t *wcs, const wchar_t *reject);
extern wchar_t	*wcsdup(const wchar_t *wcs);
extern size_t	wcsftime(wchar_t *dest, size_t destLength,
					const wchar_t *format, const struct tm *timeptr);
extern size_t	wcslcat(wchar_t *dest, const wchar_t *src, size_t maxLength);
extern size_t	wcslcpy(wchar_t *dest, const wchar_t *src, size_t maxLength);
extern size_t	wcslen(const wchar_t *wcs);
extern int		wcsncasecmp(const wchar_t *wcs1, const wchar_t *wcs2,
					size_t maxLength);
extern int		wcsncasecmp_l(const wchar_t *wcs1, const wchar_t *wcs2,
					size_t maxLength, locale_t locale);
extern wchar_t 	*wcsncat(wchar_t *dest, const wchar_t *src, size_t srcLength);
extern int		wcsncmp(const wchar_t *wcs1, const wchar_t *wcs2,
					size_t length);
extern wchar_t	*wcsncpy(wchar_t *dest, const wchar_t *src, size_t srcLength);
extern size_t	wcsnlen(const wchar_t *wcs, size_t maxLength);
extern size_t   wcsnrtombs(char *dest, const wchar_t **src, size_t srcLength,
					size_t destLength, mbstate_t *mbState);
extern wchar_t	*wcspbrk(const wchar_t *wcs, const wchar_t *accept);
extern wchar_t	*wcsrchr(const wchar_t *wcs, wchar_t wc);
extern size_t   wcsrtombs(char *dest, const wchar_t **src, size_t destLength,
					mbstate_t *mbState);
extern size_t	wcsspn(const wchar_t *wcs, const wchar_t *accept);
extern wchar_t	*wcsstr(const wchar_t *haystack, const wchar_t *needle);
extern double	wcstod(const wchar_t *wcs, wchar_t **endPtr);
extern float	wcstof(const wchar_t *wcs, wchar_t **endPtr);
extern wchar_t	*wcstok(wchar_t *wcs1, const wchar_t *wcs2, wchar_t **ptr);
extern long		wcstol(const wchar_t *wcs, wchar_t **endPtr, int base);
extern long double	wcstold(const wchar_t *wcs, wchar_t **endPtr);
extern long long	wcstoll(const wchar_t *wcs, wchar_t **endPtr, int base);
extern unsigned long	wcstoul(const wchar_t *wcs, wchar_t **endPtr,
							int base);
extern unsigned long long	wcstoull(const wchar_t *wcs, wchar_t **endPtr,
								int base);
extern wchar_t	*wcswcs(const wchar_t *haystack, const wchar_t *needle);
extern int		wcswidth(const wchar_t *wcs, size_t length);
extern size_t	wcsxfrm(wchar_t *dest, const wchar_t *src, size_t destLength);
extern size_t	wcsxfrm_l(wchar_t *dest, const wchar_t *src, size_t destLength,
					locale_t locale);
extern int		wctob(wint_t wc);
extern int		wcwidth(wchar_t wc);
extern wchar_t	*wmemchr(const wchar_t *wcs, wchar_t wc, size_t n);
extern int		wmemcmp(const wchar_t *wcs1, const wchar_t *wcs2, size_t n);
extern wchar_t	*wmemcpy(wchar_t *dest, const wchar_t *src, size_t n);
extern wchar_t	*wmemmove(wchar_t *dest, const wchar_t *src, size_t n);
#ifdef _DEFAULT_SOURCE
extern wchar_t	*wmempcpy(wchar_t *dest, const wchar_t *src, size_t n);
#endif
extern wchar_t	*wmemset(wchar_t *dest, wchar_t wc, size_t n);

extern int		wprintf(const wchar_t *format, ...);
extern int		wscanf(const wchar_t *format, ...);

#ifdef __cplusplus
}
#endif

#endif /* _WCHAR_H */
