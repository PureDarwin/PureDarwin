/* Prepare the LALR and GLR parser tables.
   Copyright (C) 2002, 2004 Free Software Foundation, Inc.

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

#ifndef TABLES_H_
# define TABLES_H_

# include "state.h"

/* The parser tables consist of these tables.

   YYTRANSLATE = vector mapping yylex's token numbers into bison's
   token numbers.

   YYTNAME = vector of string-names indexed by bison token number.

   YYTOKNUM = vector of yylex token numbers corresponding to entries
   in YYTNAME.

   YYRLINE = vector of line-numbers of all rules.  For yydebug
   printouts.

   YYRHS = vector of items of all rules.  This is exactly what RITEMS
   contains.  For yydebug and for semantic parser.

   YYPRHS[R] = index in YYRHS of first item for rule R.

   YYR1[R] = symbol number of symbol that rule R derives.

   YYR2[R] = number of symbols composing right hand side of rule R.

   YYSTOS[S] = the symbol number of the symbol that leads to state S.

   YYDEFACT[S] = default rule to reduce with in state s, when YYTABLE
   doesn't specify something else to do.  Zero means the default is an
   error.

   YYDEFGOTO[I] = default state to go to after a reduction of a rule
   that generates variable NTOKENS + I, except when YYTABLE specifies
   something else to do.

   YYPACT[S] = index in YYTABLE of the portion describing state S.
   The look-ahead token's type is used to index that portion to find
   out what to do.

   If the value in YYTABLE is positive, we shift the token and go to
   that state.

   If the value is negative, it is minus a rule number to reduce by.

   If the value is zero, the default action from YYDEFACT[S] is used.

   YYPGOTO[I] = the index in YYTABLE of the portion describing what to
   do after reducing a rule that derives variable I + NTOKENS.  This
   portion is indexed by the parser state number, S, as of before the
   text for this nonterminal was read.  The value from YYTABLE is the
   state to go to if the corresponding value in YYCHECK is S.

   YYTABLE = a vector filled with portions for different uses, found
   via YYPACT and YYPGOTO.

   YYCHECK = a vector indexed in parallel with YYTABLE.  It indicates,
   in a roundabout way, the bounds of the portion you are trying to
   examine.

   Suppose that the portion of YYTABLE starts at index P and the index
   to be examined within the portion is I.  Then if YYCHECK[P+I] != I,
   I is outside the bounds of what is actually allocated, and the
   default (from YYDEFACT or YYDEFGOTO) should be used.  Otherwise,
   YYTABLE[P+I] should be used.

   YYFINAL = the state number of the termination state.

   YYLAST ( = high) the number of the last element of YYTABLE, i.e.,
   sizeof (YYTABLE) - 1.  */

extern int nvectors;

typedef int base_number;
extern base_number *base;
/* A distinguished value of BASE, negative infinite.  During the
   computation equals to BASE_MINIMUM, later mapped to BASE_NINF to
   keep parser tables small.  */
extern base_number base_ninf;

extern unsigned int *conflict_table;
extern unsigned int *conflict_list;
extern int conflict_list_cnt;

extern base_number *table;
extern base_number *check;
/* The value used in TABLE to denote explicit syntax errors
   (%nonassoc), a negative infinite.  */
extern base_number table_ninf;

extern state_number *yydefgoto;
extern rule_number *yydefact;
extern int high;

void tables_generate (void);
void tables_free (void);

#endif /* !TABLES_H_ */
