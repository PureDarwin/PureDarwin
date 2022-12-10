/* String copying.
   Copyright (C) 1995, 2001, 2003 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  */

#ifndef _STPCPY_H
#define _STPCPY_H

#if HAVE_STPCPY

/* Get stpcpy() declaration.  */
#include <string.h>

#else

#ifdef __cplusplus
extern "C" {
#endif

/* Copy SRC to DST, returning the address of the terminating '\0' in DST.  */
extern char *stpcpy (char *dst, const char *src);

#ifdef __cplusplus
}
#endif

#endif

#endif /* _STPCPY_H */
