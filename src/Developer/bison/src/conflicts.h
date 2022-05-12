/* Find and resolve or report look-ahead conflicts for bison,
   Copyright (C) 2000, 2001, 2002, 2004 Free Software Foundation, Inc.

   This file is part of Bison, the GNU Compiler Compiler.

   Bison is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   Bison is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Bison; see the file COPYING.  If not, write to the Free
   Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.  */

#ifndef CONFLICTS_H_
# define CONFLICTS_H_
# include "state.h"

void conflicts_solve (void);
void conflicts_print (void);
int conflicts_total_count (void);
void conflicts_output (FILE *out);
void conflicts_free (void);

/* Were there conflicts? */
extern int expected_sr_conflicts;
extern int expected_rr_conflicts;
#endif /* !CONFLICTS_H_ */
