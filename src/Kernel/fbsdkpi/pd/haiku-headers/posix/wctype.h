/*
 * Copyright 2005-2012 Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _WCTYPE_H_
#define _WCTYPE_H_


#include <locale_t.h>
#include <_wint_t.h>

typedef int wctrans_t;

#ifdef __cplusplus
extern "C" {
#endif

extern int		iswalnum(wint_t wc);
extern int		iswalpha(wint_t wc);
extern int		iswcntrl(wint_t wc);
extern int		iswctype(wint_t wc, wctype_t desc);
extern int		iswdigit(wint_t wc);
extern int		iswgraph(wint_t wc);
extern int		iswlower(wint_t wc);
extern int		iswprint(wint_t wc);
extern int		iswpunct(wint_t wc);
extern int		iswspace(wint_t wc);
extern int		iswupper(wint_t wc);
extern int		iswxdigit(wint_t wc);

extern int		iswblank(wint_t wc);

extern wint_t	towctrans(wint_t wc, wctrans_t transition);
extern wint_t	towlower(wint_t wc);
extern wint_t	towupper(wint_t wc);

extern wctrans_t wctrans(const char *charClass);
extern wctype_t	wctype(const char *property);


extern int		iswalnum_l(wint_t wc, locale_t locale);
extern int		iswalpha_l(wint_t wc, locale_t locale);
extern int		iswcntrl_l(wint_t wc, locale_t locale);
extern int		iswctype_l(wint_t wc, wctype_t desc, locale_t locale);
extern int		iswdigit_l(wint_t wc, locale_t locale);
extern int		iswgraph_l(wint_t wc, locale_t locale);
extern int		iswlower_l(wint_t wc, locale_t locale);
extern int		iswprint_l(wint_t wc, locale_t locale);
extern int		iswpunct_l(wint_t wc, locale_t locale);
extern int		iswspace_l(wint_t wc, locale_t locale);
extern int		iswupper_l(wint_t wc, locale_t locale);
extern int		iswxdigit_l(wint_t wc, locale_t locale);

extern int		iswblank_l(wint_t wc, locale_t locale);

extern wint_t	towctrans_l(wint_t wc, wctrans_t transition, locale_t locale);
extern wint_t	towlower_l(wint_t wc, locale_t locale);
extern wint_t	towupper_l(wint_t wc, locale_t locale);

extern wctrans_t wctrans_l(const char *charClass, locale_t locale);
extern wctype_t	wctype_l(const char *property, locale_t locale);

#ifdef __cplusplus
}
#endif

#endif	/* _WCTYPE_H_ */
