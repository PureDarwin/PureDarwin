/* Definitions for symtab.c and callers, part of Bison.

   Copyright (C) 1984, 1989, 1992, 2000, 2001, 2002, 2004, 2005, 2006
   Free Software Foundation, Inc.

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

#ifndef SYMTAB_H_
# define SYMTAB_H_

# include "assoc.h"
# include "location.h"
# include "uniqstr.h"

/*----------.
| Symbols.  |
`----------*/

/* Symbol classes.  */
typedef enum
{
  unknown_sym,
  token_sym,		/* terminal symbol */
  nterm_sym		/* non-terminal */
} symbol_class;


/* Internal token numbers. */
typedef int symbol_number;
#define SYMBOL_NUMBER_MAXIMUM INT_MAX


typedef struct symbol symbol;

/* When extending this structure, be sure to complete
   symbol_check_alias_consistency.  */
struct symbol
{
  /* The key, name of the symbol.  */
  uniqstr tag;
  /* The location of its first occurrence.  */
  location location;

  /* Its %type and associated printer and destructor.  */
  uniqstr type_name;
  location type_location;

  /* Does not own the memory. */
  const char *destructor;
  location destructor_location;

  /* Does not own the memory. */
  const char *printer;
  location printer_location;

  symbol_number number;
  location prec_location;
  int prec;
  assoc assoc;
  int user_token_number;

  /* Points to the other in the identifier-symbol pair for an alias.
     Special value USER_NUMBER_ALIAS in the identifier half of the
     identifier-symbol pair for an alias.  */
  symbol *alias;
  symbol_class class;
  bool declared;
};

/* Undefined user number.  */
#define USER_NUMBER_UNDEFINED -1

/* `symbol->user_token_number == USER_NUMBER_ALIAS' means this symbol
   *has* (not is) a string literal alias.  For instance, `%token foo
   "foo"' has `"foo"' numbered regularly, and `foo' numbered as
   USER_NUMBER_ALIAS.  */
#define USER_NUMBER_ALIAS -9991

/* Undefined internal token number.  */
#define NUMBER_UNDEFINED (-1)

/* Print a symbol (for debugging). */
void symbol_print (symbol *s, FILE *f);

/* Fetch (or create) the symbol associated to KEY.  */
symbol *symbol_get (const char *key, location loc);

/* Generate a dummy nonterminal, whose name cannot conflict with the
   user's names.  */
symbol *dummy_symbol_get (location loc);

/* Declare the new symbol SYM.  Make it an alias of SYMVAL.  */
void symbol_make_alias (symbol *sym, symbol *symval, location loc);

/* Set the TYPE_NAME associated with SYM.  Do nothing if passed 0 as
   TYPE_NAME.  */
void symbol_type_set (symbol *sym, uniqstr type_name, location loc);

/* Set the DESTRUCTOR associated with SYM.  */
void symbol_destructor_set (symbol *sym, const char *destructor, location loc);

/* Set the PRINTER associated with SYM.  */
void symbol_printer_set (symbol *sym, const char *printer, location loc);

/* Set the PRECEDENCE associated with SYM.  Ensure that SYMBOL is a
   terminal.  Do nothing if invoked with UNDEF_ASSOC as ASSOC.  */
void symbol_precedence_set (symbol *sym, int prec, assoc a, location loc);

/* Set the CLASS associated with SYM.  */
void symbol_class_set (symbol *sym, symbol_class class, location loc,
		       bool declaring);

/* Set the USER_TOKEN_NUMBER associated with SYM.  */
void symbol_user_token_number_set (symbol *sym, int user_number, location loc);


/* Distinguished symbols.  AXIOM is the real start symbol, that used
   by the automaton.  STARTSYMBOL is the one specified by the user.
   */
extern symbol *errtoken;
extern symbol *undeftoken;
extern symbol *endtoken;
extern symbol *accept;
extern symbol *startsymbol;
extern location startsymbol_location;


/*---------------.
| Symbol table.  |
`---------------*/


/* Create the symbol table.  */
void symbols_new (void);

/* Free all the memory allocated for symbols.  */
void symbols_free (void);

/* Check that all the symbols are defined.  Report any undefined
   symbols and consider them nonterminals.  */
void symbols_check_defined (void);

/* Perform various sanity checks, assign symbol numbers, and set up
   TOKEN_TRANSLATIONS.  */
void symbols_pack (void);

#endif /* !SYMTAB_H_ */
