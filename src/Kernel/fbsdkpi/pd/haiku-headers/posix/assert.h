/*
 * Copyright 2004-2015 Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */

/* Include guards are only on part of the file, as assert.h is required
   to support being included multiple times.

   E.g. the following is required to be valid:

   #undef NDEBUG
   #include <assert.h>

   assert(0); // this assertion will be triggered

   #define NDEBUG
   #include <assert.h>

   assert(0); // this assertion will not be triggered
*/

#undef assert

#ifndef NDEBUG
	/* defining NDEBUG disables assert() functionality */

#ifndef _ASSERT_H_
#define _ASSERT_H_

#ifdef __cplusplus
extern "C" {
#endif

extern void __assert_fail(const char *assertion, const char *file,
				unsigned int line, const char *function)
	__attribute__ ((noreturn));

extern void __assert_perror_fail(int error, const char *file,
				unsigned int line, const char *function)
	__attribute__ ((noreturn));

#ifdef __cplusplus
}
#endif

#endif /* !_ASSERT_H_ */

#define assert(assertion) \
	((assertion) ? (void)0 : __assert_fail(#assertion, __FILE__, __LINE__, __PRETTY_FUNCTION__))

#else	/* NDEBUG */
#	define assert(condition) ((void)0)
#endif

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L && __STDC_VERSION__ <= 201710L \
    && !defined(static_assert) && !defined(__cplusplus)
#	define static_assert _Static_assert
#endif
