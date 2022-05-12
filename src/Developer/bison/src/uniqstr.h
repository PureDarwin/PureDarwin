/* Keeping a unique copy of strings.

   Copyright (C) 2002, 2003 Free Software Foundation, Inc.

   This file is part of Bison, the GNU Compiler Compiler.

   Bison is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   Bison is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Bison; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.  */

#ifndef UNIQSTR_H_
# define UNIQSTR_H_

/*-----------------------------------------.
| Pointers to unique copies of C strings.  |
`-----------------------------------------*/

typedef char const *uniqstr;

/* Return the uniqstr for STR.  */
uniqstr uniqstr_new (char const *str);

/* Two uniqstr values have the same value iff they are the same.  */
#define UNIQSTR_EQ(USTR1, USTR2) ((USTR1) == (USTR2))

/*--------------------------------------.
| Initializing, destroying, debugging.  |
`--------------------------------------*/

/* Create the string table.  */
void uniqstrs_new (void);

/* Die if STR is not a uniqstr.  */
void uniqstr_assert (char const *str);

/* Free all the memory allocated for symbols.  */
void uniqstrs_free (void);

/* Report them all.  */
void uniqstrs_print (void);

#endif /* ! defined UNIQSTR_H_ */
