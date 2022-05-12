/* Locations for Bison

   Copyright (C) 2002, 2005 Free Software Foundation, Inc.

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

#include <config.h>
#include "system.h"

#include <quotearg.h>

#include "location.h"

location const empty_location;

/* Output to OUT the location LOC.
   Warning: it uses quotearg's slot 3.  */
void
location_print (FILE *out, location loc)
{
  fprintf (out, "%s:%d.%d",
	   quotearg_n_style (3, escape_quoting_style, loc.start.file),
	   loc.start.line, loc.start.column);

  if (loc.start.file != loc.end.file)
    fprintf (out, "-%s:%d.%d",
	     quotearg_n_style (3, escape_quoting_style, loc.end.file),
	     loc.end.line, loc.end.column - 1);
  else if (loc.start.line < loc.end.line)
    fprintf (out, "-%d.%d", loc.end.line, loc.end.column - 1);
  else if (loc.start.column < loc.end.column - 1)
    fprintf (out, "-%d", loc.end.column - 1);
}
