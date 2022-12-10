/* Declaration for va_list error-reporting function
   Copyright (C) 2006 Free Software Foundation, Inc.

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

#ifndef _VERROR_H
#define _VERROR_H 1

#include "error.h"
#include <stdarg.h>

#ifdef	__cplusplus
extern "C" {
#endif

/* Print a message with `vfprintf (stderr, FORMAT, ARGS)';
   if ERRNUM is nonzero, follow it with ": " and strerror (ERRNUM).
   If STATUS is nonzero, terminate the program with `exit (STATUS)'.
   Use the globals error_print_progname and error_message_count similarly
   to error().  */

extern void verror (int __status, int __errnum, const char *__format,
                    va_list __args)
     __attribute__ ((__format__ (__printf__, 3, 0)));

/* Print a message with `vfprintf (stderr, FORMAT, ARGS)';
   if ERRNUM is nonzero, follow it with ": " and strerror (ERRNUM).
   If STATUS is nonzero, terminate the program with `exit (STATUS)'.
   If FNAME is not NULL, prepend the message with `FNAME:LINENO:'.
   Use the globals error_print_progname, error_message_count, and
   error_one_per_line similarly to error_at_line().  */

extern void verror_at_line (int __status, int __errnum, const char *__fname,
                            unsigned int __lineno, const char *__format,
                            va_list __args)
     __attribute__ ((__format__ (__printf__, 5, 0)));

#ifdef	__cplusplus
}
#endif

#endif /* verror.h */
