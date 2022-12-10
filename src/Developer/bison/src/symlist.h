/* Lists of symbols for Bison

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

#ifndef SYMLIST_H_
# define SYMLIST_H_

# include "location.h"
# include "symtab.h"

/* A list of symbols, used during the parsing to store the rules.  */
typedef struct symbol_list
{
  /* The symbol.  */
  symbol *sym;
  location location;

  /* If this symbol is the generated lhs for a mid-rule, a pointer to
     that mid-rule.  */
  struct symbol_list *midrule;

  /* The action is attached to the LHS of a rule. */
  const char *action;
  location action_location;

  /* Whether this symbol's value is used in the current action. */
  bool used;

  /* Precedence/associativity.  */
  symbol *ruleprec;
  int dprec;
  int merger;

  /* The list.  */
  struct symbol_list *next;
} symbol_list;


/* Create a list containing SYM at LOC.  */
symbol_list *symbol_list_new (symbol *sym, location loc);

/* Print it.  */
void symbol_list_print (const symbol_list *l, FILE *f);

/* Prepend SYM at LOC to the LIST.  */
symbol_list *symbol_list_prepend (symbol_list *l,
				  symbol *sym,
				  location loc);

/* Free the LIST, but not the symbols it contains.  */
void symbol_list_free (symbol_list *l);

/* Return its length. */
unsigned int symbol_list_length (const symbol_list *l);

/* Get symbol N in symbol list L.  */
symbol_list *symbol_list_n_get (symbol_list *l, int n);

/* Get the data type (alternative in the union) of the value for
   symbol N in rule RULE.  */
uniqstr symbol_list_n_type_name_get (symbol_list *l, location loc, int n);

/* The symbol N in symbol list L is USED.  */
void symbol_list_n_used_set (symbol_list *l, int n, bool used);

#endif /* !SYMLIST_H_ */
