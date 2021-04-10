/* ===---------- eprintf.c - Implements __eprintf --------------------------===
 *
 *                     The LLVM Compiler Infrastructure
 *
 * This file is dual licensed under the MIT and the University of Illinois Open
 * Source Licenses. See LICENSE.TXT for details.
 *
 * ===----------------------------------------------------------------------===
 */

#ifdef __APPLE__

#include <stdio.h>
#include <stdlib.h>

/*
 * __eprintf() was used in an old version of <assert.h>.
 * It can eventually go away, but it is needed when linking
 * .o files built with the old <assert.h>.
 *
 * It should never be exported from a dylib, so it is marked
 * visibility hidden.
 */
__attribute__((visibility("hidden")))
void __eprintf(const char* format, const char* assertion_expression,
                const char* line, const char* file)
{
    fprintf(stderr, format, assertion_expression, line, file);
    fflush(stderr);
    abort();
}

#endif /* __APPLE__ */
