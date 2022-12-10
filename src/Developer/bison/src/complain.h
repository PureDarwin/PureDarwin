/* Declaration for error-reporting function for Bison.
   Copyright (C) 2000, 2001, 2002 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 2, or (at your option) any
   later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,
   USA.  */

#ifndef COMPLAIN_H_
# define COMPLAIN_H_ 1

# include "location.h"

# ifdef	__cplusplus
extern "C" {
# endif

/* Informative messages, but we proceed.  */

void warn (char const *format, ...)
  __attribute__ ((__format__ (__printf__, 1, 2)));

void warn_at (location loc, char const *format, ...)
  __attribute__ ((__format__ (__printf__, 2, 3)));

/* Something bad happened, but let's continue and die later.  */

void complain (char const *format, ...)
  __attribute__ ((__format__ (__printf__, 1, 2)));

void complain_at (location loc, char const *format, ...)
  __attribute__ ((__format__ (__printf__, 2, 3)));

/* Something bad happened, and let's die now.  */

void fatal (char const *format, ...)
  __attribute__ ((__noreturn__, __format__ (__printf__, 1, 2)));

void fatal_at (location loc, char const *format, ...)
  __attribute__ ((__noreturn__, __format__ (__printf__, 2, 3)));

/* This variable is set each time `warn' is called.  */
extern bool warning_issued;

/* This variable is set each time `complain' is called.  */
extern bool complaint_issued;

# ifdef	__cplusplus
}
# endif

#endif /* !COMPLAIN_H_ */
