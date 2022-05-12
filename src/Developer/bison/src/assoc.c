/* Associativity information.
   Copyright (C) 2002, 2005, 2006 Free Software Foundation, Inc.

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

#include "assoc.h"


const char *
assoc_to_string (assoc a)
{
  switch (a)
    {
    default:
      abort ();

    case undef_assoc:
      return "undefined associativity";

    case right_assoc:
      return "%right";

    case left_assoc:
      return "%left";

    case non_assoc:
      return "%nonassoc";
    }
}
