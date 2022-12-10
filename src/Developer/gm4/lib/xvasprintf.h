/* vasprintf and asprintf with out-of-memory checking.
   Copyright (C) 2002-2004, 2006 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation,
   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  */

#ifndef _XVASPRINTF_H
#define _XVASPRINTF_H

/* Get va_list.  */
#include <stdarg.h>

#ifndef __attribute__
/* This feature is available in gcc versions 2.5 and later.  */
# if __GNUC__ < 2 || (__GNUC__ == 2 && __GNUC_MINOR__ < 5) || __STRICT_ANSI__
#  define __attribute__(Spec) /* empty */
# endif
/* The __-protected variants of `format' and `printf' attributes
   are accepted by gcc versions 2.6.4 (effectively 2.7) and later.  */
# if __GNUC__ < 2 || (__GNUC__ == 2 && __GNUC_MINOR__ < 7)
#  define __format__ format
#  define __printf__ printf
# endif
#endif

#ifdef	__cplusplus
extern "C" {
#endif

/* Write formatted output to a string dynamically allocated with malloc(),
   and return it.  Upon [ENOMEM] memory allocation error, call xalloc_die.
   On some other error
     - [EOVERFLOW] resulting string length is > INT_MAX,
     - [EINVAL] invalid format string,
     - [EILSEQ] error during conversion between wide and multibyte characters,
   return NULL.  */
extern char *xasprintf (const char *format, ...)
       __attribute__ ((__format__ (__printf__, 1, 2)));
extern char *xvasprintf (const char *format, va_list args)
       __attribute__ ((__format__ (__printf__, 1, 0)));

#ifdef	__cplusplus
}
#endif

#endif /* _XVASPRINTF_H */
