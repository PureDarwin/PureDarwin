/* C11 <uchar.h> over Darwin's wide-character functions.
 *
 * Darwin never shipped this header, but it does not need a new implementation:
 * wchar_t here is a 32-bit type holding UCS-4, so char32_t conversions are
 * exactly mbrtowc/wcrtomb. Only the 16-bit variants would need real work, and
 * nothing in the tree uses them - they are deliberately left out rather than
 * given a wrong implementation.
 */
#ifndef PD_UCHAR_H
#define PD_UCHAR_H

#include <stddef.h>
#include <stdint.h>
#include <wchar.h>

/* In C++ these are builtin types; in C they are typedefs this header owns. */
#if !defined(__cplusplus)
#if !defined(__CHAR16_TYPE__)
typedef uint_least16_t char16_t;
#else
typedef __CHAR16_TYPE__ char16_t;
#endif
#if !defined(__CHAR32_TYPE__)
typedef uint_least32_t char32_t;
#else
typedef __CHAR32_TYPE__ char32_t;
#endif
#endif /* !__cplusplus */

static inline size_t
mbrtoc32(char32_t *pc32, const char *s, size_t n, mbstate_t *ps)
{
    static mbstate_t internal;
    wchar_t wc = 0;
    size_t rc;

    if (ps == NULL)
        ps = &internal;

    rc = mbrtowc(&wc, s, n, ps);
    if (pc32 != NULL && rc != (size_t)-1 && rc != (size_t)-2)
        *pc32 = (char32_t)wc;

    return rc;
}

static inline size_t
c32rtomb(char *s, char32_t c32, mbstate_t *ps)
{
    static mbstate_t internal;

    if (ps == NULL)
        ps = &internal;

    return wcrtomb(s, (wchar_t)c32, ps);
}

#endif /* PD_UCHAR_H */
