/* <stdlib.h> plus reallocarray.
 *
 * reallocarray is OpenBSD's overflow-checked realloc, picked up by glibc and
 * FreeBSD and used widely in ports. The 11.3 SDK used for cross-compiling
 * predates Apple's own, so the declaration is missing.
 *
 * This deliberately does NOT go into libSystem: exporting the symbol there
 * makes every port's link-test for reallocarray succeed while the SDK header
 * still fails to declare it, which breaks cairo, harfbuzz and the X libraries
 * at compile time. Keeping it header-local means only the ports that opt into
 * this include directory are affected.
 */
#ifndef PD_STDLIB_COMPAT_H
#define PD_STDLIB_COMPAT_H

/* foot builds with -Werror and clang warns on #include_next as an extension,
 * so silence it locally rather than loosening the port's warning flags. */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-include-next"
#include_next <stdlib.h>
#pragma clang diagnostic pop

#include <errno.h>
#include <stdint.h>

/* Square root of SIZE_MAX: below it on both sides, the product cannot
 * overflow, so the division is only reached when one side is large. */
#define PD_MUL_NO_OVERFLOW ((size_t)1 << (sizeof(size_t) * 4))

static inline void *
reallocarray(void *optr, size_t nmemb, size_t size)
{
    if ((nmemb >= PD_MUL_NO_OVERFLOW || size >= PD_MUL_NO_OVERFLOW) &&
        nmemb > 0 && SIZE_MAX / nmemb < size) {
        errno = ENOMEM;
        return NULL;
    }
    return realloc(optr, size * nmemb);
}

#endif /* PD_STDLIB_COMPAT_H */
