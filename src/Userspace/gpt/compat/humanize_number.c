/*	$NetBSD: humanize_number.c,v 1.19 2024/01/20 14:52:47 christos Exp $	*/

/*
 * Copyright (c) 1997, 1998, 1999, 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center, by Luke Mewburn and by Tomas Svensson.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: humanize_number.c,v 1.19 2024/01/20 14:52:47 christos Exp $");
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>

int
humanize_number(char *buf, size_t len, int64_t bytes,
    const char *suffix, int scale, int flags)
{
	const char *prefixes, *sep;
	int	b, i, r, s1, s2, sign;
	int64_t	divisor, max, post = 1;
	size_t	baselen;
	int	maxscale;

	_DIAGASSERT(buf != NULL);
	_DIAGASSERT(scale >= 0);

	if (suffix == NULL)
		suffix = "";

	if (flags & HN_DIVISOR_1000) {
		/* SI for decimal multiplies */
		divisor = 1000;
		if (flags & HN_B)
			prefixes = "B\0k\0M\0G\0T\0P\0E";
		else
			prefixes = "\0\0k\0M\0G\0T\0P\0E";
	} else {
		/*
		 * binary multiplies
		 * XXX IEC 60027-2 recommends Ki, Mi, Gi...
		 */
		divisor = 1024;
		if (flags & HN_B)
			prefixes = "B\0K\0M\0G\0T\0P\0E";
		else
			prefixes = "\0\0K\0M\0G\0T\0P\0E";
	}

#define	SCALE2PREFIX(scale)	(&prefixes[(scale) << 1])
	maxscale = 6;

	if (scale < 0 || (scale > maxscale &&
	    (scale & (HN_AUTOSCALE | HN_GETSCALE)) == 0))
		return (-1);

	if (buf == NULL)
		return (-1);

	if (len > 0)
		buf[0] = '\0';

	if (bytes < 0) {
		sign = -1;
		baselen = 3;		/* sign, digit, prefix */
		if (-bytes < INT64_MAX / 100)
			bytes *= -100;
		else {
			bytes = -bytes;
			post = 100;
			baselen += 2;
		}
	} else {
		sign = 1;
		baselen = 2;		/* digit, prefix */
		if (bytes < INT64_MAX / 100)
			bytes *= 100;
		else {
			post = 100;
			baselen += 2;
		}
	}
	if (flags & HN_NOSPACE)
		sep = "";
	else {
		sep = " ";
		baselen++;
	}
	baselen += strlen(suffix);

	/* Check if enough room for `x y' + suffix + `\0' */
	if (len < baselen + 1)
		return (-1);

	if (scale & (HN_AUTOSCALE | HN_GETSCALE)) {
		/*
		 * 19 is number of digits in biggest possible int64_t
		 * If we don't do this, the calc of max just below can
		 * overflow, leading to absurd results.   If the buffer
		 * is big enough for the number, simply use it, no scaling.
		 */
		if (len - baselen > 19)
			i = 0;
		else {
			size_t j;
			/* See if there are additional columns to be used. */
			for (max = 100, j = len - baselen; j-- > 0;)
				max *= 10;

			/*
			 * Divide the number until it fits the avail buffer.
			 * If there will be an overflow by the rounding below,
			 * (the "-50") divide once more.
			 */
			for (i = 0; bytes >= max - 50 && i < maxscale; i++)
				bytes /= divisor;
		}
		if (scale & HN_GETSCALE) {
			_DIAGASSERT(__type_fit(int, i));
			return i;
		}
	} else {
		/* XXX
		 * we already know scale <= maxscale, so
		 * i < scale ==> i < maxscale
		 */
		for (i = 0; i < scale && i < maxscale; i++)
			bytes /= divisor;
	}

	if (i == 0) {
		/*
		 * Cannot go the bytes *= post route, as
		 * that can cause overflow of bytes
		 *
		 * but if we already scaled up, undo that.
		 */
		if (post == 1)
			bytes /= 100;

		r = snprintf(buf, len, "%" PRId64 "%s%s%s",
		    sign * bytes, sep, SCALE2PREFIX(0), suffix);
	} else {
		/*
		 * Here this is safe, as if i > 0, we have already
		 * divided bytes by at least 1000, post <= 100, so ...
		 */
		bytes *= post;

		/* If a value <= 9.9 after rounding and ... */
		if (bytes < 995 && i > 0 && flags & HN_DECIMAL) {
			/* baselen + \0 + .N */
			if (len < baselen + 1 + 1 +
			    strlen(localeconv()->decimal_point))
				return (-1);
			b = ((int)bytes + 5) / 10;
			s1 = b / 10;
			s2 = b % 10;
			r = snprintf(buf, len, "%d%s%d%s%s%s",
			    sign * s1, localeconv()->decimal_point, s2,
			    sep, SCALE2PREFIX(i), suffix);
		} else
			r = snprintf(buf, len, "%" PRId64 "%s%s%s",
			    sign * ((bytes + 50) / 100),
			    sep, SCALE2PREFIX(i), suffix);
	}

	if ((size_t)r > len)
		r = -1;

	return (r);
}
