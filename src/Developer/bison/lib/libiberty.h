/* Fake libiberty.h for Bison.
   Copyright (C) 2002, 2003, 2004 Free Software Foundation, Inc.

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


/* Bison depends on libiberty's implementation of bitsets, which
   requires a `libiberty.h' file.  This file provides the minimum
   services.  */

#ifndef BISON_LIBIBERTY_H_
# define BISON_LIBIBERTY_H_ 1

# ifndef __attribute__
#  if __GNUC__ < 2 || (__GNUC__ == 2 && __GNUC_MINOR__ < 8) || __STRICT_ANSI__
#   define __attribute__(x)
#  endif
# endif

# define ATTRIBUTE_UNUSED __attribute__ ((__unused__))

# include "xalloc.h"

#endif /* ! BISON_LIBIBERTY_H_ */
