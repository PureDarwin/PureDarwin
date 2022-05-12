/* Lists of symbols for Bison

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

#include "complain.h"
#include "symlist.h"


/*--------------------------------------.
| Create a list containing SYM at LOC.  |
`--------------------------------------*/

symbol_list *
symbol_list_new (symbol *sym, location loc)
{
  symbol_list *res = xmalloc (sizeof *res);

  res->sym = sym;
  res->location = loc;

  res->midrule = NULL;

  res->action = NULL;
  res->used = false;

  res->ruleprec = NULL;
  res->dprec = 0;
  res->merger = 0;

  res->next = NULL;

  return res;
}


/*------------------.
| Print this list.  |
`------------------*/

void
symbol_list_print (const symbol_list *l, FILE *f)
{
  for (/* Nothing. */; l && l->sym; l = l->next)
    {
      symbol_print (l->sym, f);
      fprintf (stderr, l->used ? " used" : " unused");
      if (l && l->sym)
	fprintf (f, ", ");
    }
}


/*---------------------------------.
| Prepend SYM at LOC to the LIST.  |
`---------------------------------*/

symbol_list *
symbol_list_prepend (symbol_list *list, symbol *sym, location loc)
{
  symbol_list *res = symbol_list_new (sym, loc);
  res->next = list;
  return res;
}


/*-------------------------------------------------.
| Free the LIST, but not the symbols it contains.  |
`-------------------------------------------------*/

void
symbol_list_free (symbol_list *list)
{
  LIST_FREE (symbol_list, list);
}


/*--------------------.
| Return its length.  |
`--------------------*/

unsigned int
symbol_list_length (const symbol_list *l)
{
  int res = 0;
  for (/* Nothing. */; l; l = l->next)
    ++res;
  return res;
}


/*--------------------------------.
| Get symbol N in symbol list L.  |
`--------------------------------*/

symbol_list *
symbol_list_n_get (symbol_list *l, int n)
{
  int i;

  if (n < 0)
    return NULL;

  for (i = 0; i < n; ++i)
    {
      l = l->next;
      if (l == NULL || l->sym == NULL)
	return NULL;
    }

  return l;
}


/*--------------------------------------------------------------.
| Get the data type (alternative in the union) of the value for |
| symbol N in symbol list L.                                    |
`--------------------------------------------------------------*/

uniqstr
symbol_list_n_type_name_get (symbol_list *l, location loc, int n)
{
  l = symbol_list_n_get (l, n);
  if (!l)
    {
      complain_at (loc, _("invalid $ value: $%d"), n);
      return NULL;
    }
  return l->sym->type_name;
}


/*----------------------------------------.
| The symbol N in symbol list L is USED.  |
`----------------------------------------*/

void
symbol_list_n_used_set (symbol_list *l, int n, bool used)
{
  l = symbol_list_n_get (l, n);
  if (l)
    l->used = used;
}
