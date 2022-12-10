/* Provide a complete sys/stat header file.
   Copyright (C) 2006 Free Software Foundation, Inc.
   Written by Eric Blake.

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

#ifndef _gl_SYS_STAT_H
#define _gl_SYS_STAT_H

/* This file is supposed to be used on platforms where <sys/stat.h> is
   incomplete.  It is intended to provide definitions and prototypes
   needed by an application.  Start with what the system provides.  */
#include @ABSOLUTE_SYS_STAT_H@

/* mingw does not support symlinks, therefore it does not have lstat.  But
   without links, stat does just fine.  */
#if ! HAVE_LSTAT
# define lstat stat
#endif

/* mingw's _mkdir() function has 1 argument, but we pass 2 arguments.
   Additionally, it declares _mkdir (and depending on compile flags, an
   alias mkdir), only in the nonstandard io.h.  */
#if ! HAVE_DECL_MKDIR && HAVE_IO_H
# include <io.h>

static inline int
rpl_mkdir (char const *name, mode_t mode)
{
  return _mkdir (name);
}

# define mkdir rpl_mkdir
#endif

#endif /* _gl_SYS_STAT_H */
