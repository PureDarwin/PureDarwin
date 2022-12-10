/* Input parser for Bison

   Copyright (C) 1984, 1986, 1989, 1992, 1998, 2000, 2001, 2002, 2003,
   2005, 2006 Free Software Foundation, Inc.

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

#include "complain.h"
#include "conflicts.h"
#include "files.h"
#include "getargs.h"
#include "gram.h"
#include "muscle_tab.h"
#include "reader.h"
#include "symlist.h"
#include "symtab.h"

static void check_and_convert_grammar (void);

static symbol_list *grammar = NULL;
static bool start_flag = false;
merger_list *merge_functions;

/* Was %union seen?  */
bool typed = false;

/* Should rules have a default precedence?  */
bool default_prec = true;

/*-----------------------.
| Set the start symbol.  |
`-----------------------*/

void
grammar_start_symbol_set (symbol *sym, location loc)
{
  if (start_flag)
    complain_at (loc, _("multiple %s declarations"), "%start");
  else
    {
      start_flag = true;
      startsymbol = sym;
      startsymbol_location = loc;
    }
}


/*----------------------------------------------------------------.
| There are two prologues: one before %union, one after.  Augment |
| the current one.                                                |
`----------------------------------------------------------------*/

void
prologue_augment (const char *prologue, location loc)
{
  struct obstack *oout =
    !typed ? &pre_prologue_obstack : &post_prologue_obstack;

  obstack_fgrow1 (oout, "]b4_syncline(%d, [[", loc.start.line);
  MUSCLE_OBSTACK_SGROW (oout,
			quotearg_style (c_quoting_style, loc.start.file));
  obstack_sgrow (oout, "]])[\n");
  obstack_sgrow (oout, prologue);
}



/*-------------------------------------------------------------------.
| Return the merger index for a merging function named NAME, whose   |
| arguments have type TYPE.  Records the function, if new, in        |
| MERGER_LIST.							     |
`-------------------------------------------------------------------*/

static int
get_merge_function (uniqstr name, uniqstr type, location loc)
{
  merger_list *syms;
  merger_list head;
  int n;

  if (! glr_parser)
    return 0;

  if (type == NULL)
    type = uniqstr_new ("");

  head.next = merge_functions;
  for (syms = &head, n = 1; syms->next; syms = syms->next, n += 1)
    if (UNIQSTR_EQ (name, syms->next->name))
      break;
  if (syms->next == NULL)
    {
      syms->next = xmalloc (sizeof syms->next[0]);
      syms->next->name = uniqstr_new (name);
      syms->next->type = uniqstr_new (type);
      syms->next->next = NULL;
      merge_functions = head.next;
    }
  else if (!UNIQSTR_EQ (type, syms->next->type))
    warn_at (loc, _("result type clash on merge function %s: <%s> != <%s>"),
	     name, type, syms->next->type);
  return n;
}

/*--------------------------------------.
| Free all merge-function definitions.	|
`--------------------------------------*/

void
free_merger_functions (void)
{
  merger_list *L0 = merge_functions;
  while (L0)
    {
      merger_list *L1 = L0->next;
      free (L0);
      L0 = L1;
    }
}


/*-------------------------------------------------------------------.
| Parse the input grammar into a one symbol_list structure.  Each    |
| rule is represented by a sequence of symbols: the left hand side   |
| followed by the contents of the right hand side, followed by a     |
| null pointer instead of a symbol to terminate the rule.  The next  |
| symbol is the lhs of the following rule.                           |
|                                                                    |
| All actions are copied out, labelled by the rule number they apply |
| to.                                                                |
`-------------------------------------------------------------------*/

/* The (currently) last symbol of GRAMMAR. */
static symbol_list *grammar_end = NULL;

/* Append SYM to the grammar.  */
static void
grammar_symbol_append (symbol *sym, location loc)
{
  symbol_list *p = symbol_list_new (sym, loc);

  if (grammar_end)
    grammar_end->next = p;
  else
    grammar = p;

  grammar_end = p;

  /* A null SYM stands for an end of rule; it is not an actual
     part of it.  */
  if (sym)
    ++nritems;
}

/* The rule currently being defined, and the previous rule.
   CURRENT_RULE points to the first LHS of the current rule, while
   PREVIOUS_RULE_END points to the *end* of the previous rule (NULL).  */
symbol_list *current_rule = NULL;
static symbol_list *previous_rule_end = NULL;


/*----------------------------------------------.
| Create a new rule for LHS in to the GRAMMAR.  |
`----------------------------------------------*/

void
grammar_current_rule_begin (symbol *lhs, location loc)
{
  if (!start_flag)
    {
      startsymbol = lhs;
      startsymbol_location = loc;
      start_flag = true;
    }

  /* Start a new rule and record its lhs.  */
  ++nrules;
  previous_rule_end = grammar_end;
  grammar_symbol_append (lhs, loc);
  current_rule = grammar_end;

  /* Mark the rule's lhs as a nonterminal if not already so.  */
  if (lhs->class == unknown_sym)
    {
      lhs->class = nterm_sym;
      lhs->number = nvars;
      ++nvars;
    }
  else if (lhs->class == token_sym)
    complain_at (loc, _("rule given for %s, which is a token"), lhs->tag);
}


/*----------------------------------------------------------------------.
| A symbol should be used if it has a destructor, or if it is a         |
| mid-rule symbol (i.e., the generated LHS replacing a mid-rule         |
| action) that was assigned to, as in "exp: { $$ = 1; } { $$ = $1; }".  |
`----------------------------------------------------------------------*/

static bool
symbol_should_be_used (symbol_list const *s)
{
  return (s->sym->destructor
	  || (s->midrule && s->midrule->used));
}

/*----------------------------------------------------------------.
| Check that the rule R is properly defined.  For instance, there |
| should be no type clash on the default action.                  |
`----------------------------------------------------------------*/

static void
grammar_rule_check (const symbol_list *r)
{
  /* Type check.

     If there is an action, then there is nothing we can do: the user
     is allowed to shoot herself in the foot.

     Don't worry about the default action if $$ is untyped, since $$'s
     value can't be used.  */
  if (!r->action && r->sym->type_name)
    {
      symbol *first_rhs = r->next->sym;
      /* If $$ is being set in default way, report if any type mismatch.  */
      if (first_rhs)
	{
	  char const *lhs_type = r->sym->type_name;
	  const char *rhs_type =
	    first_rhs->type_name ? first_rhs->type_name : "";
	  if (!UNIQSTR_EQ (lhs_type, rhs_type))
	    warn_at (r->location,
		     _("type clash on default action: <%s> != <%s>"),
		     lhs_type, rhs_type);
	}
      /* Warn if there is no default for $$ but we need one.  */
      else
	warn_at (r->location,
		 _("empty rule for typed nonterminal, and no action"));
    }

  /* Check that symbol values that should be used are in fact used.  */
  {
    symbol_list const *l = r;
    int n = 0;
    for (; l && l->sym; l = l->next, ++n)
      if (! (l->used
	     || !symbol_should_be_used (l)
	     /* The default action, $$ = $1, `uses' both.  */
	     || (!r->action && (n == 0 || n == 1))))
	{
	  if (n)
	    warn_at (r->location, _("unused value: $%d"), n);
	  else
	    warn_at (r->location, _("unset value: $$"));
	}
  }
}


/*-------------------------------------.
| End the currently being grown rule.  |
`-------------------------------------*/

void
grammar_current_rule_end (location loc)
{
  /* Put an empty link in the list to mark the end of this rule  */
  grammar_symbol_append (NULL, grammar_end->location);
  current_rule->location = loc;
  grammar_rule_check (current_rule);
}


/*-------------------------------------------------------------------.
| The previous action turns out the be a mid-rule action.  Attach it |
| to the current rule, i.e., create a dummy symbol, attach it this   |
| mid-rule action, and append this dummy nonterminal to the current  |
| rule.                                                              |
`-------------------------------------------------------------------*/

void
grammar_midrule_action (void)
{
  /* Since the action was written out with this rule's number, we must
     give the new rule this number by inserting the new rule before
     it.  */

  /* Make a DUMMY nonterminal, whose location is that of the midrule
     action.  Create the MIDRULE.  */
  location dummy_location = current_rule->action_location;
  symbol *dummy = dummy_symbol_get (dummy_location);
  symbol_list *midrule = symbol_list_new (dummy, dummy_location);

  /* Make a new rule, whose body is empty, before the current one, so
     that the action just read can belong to it.  */
  ++nrules;
  ++nritems;
  /* Attach its location and actions to that of the DUMMY.  */
  midrule->location = dummy_location;
  midrule->action = current_rule->action;
  midrule->action_location = dummy_location;
  current_rule->action = NULL;
  /* If $$ was used in the action, the LHS of the enclosing rule was
     incorrectly flagged as used.  */
  midrule->used = current_rule->used;
  current_rule->used = false;

  if (previous_rule_end)
    previous_rule_end->next = midrule;
  else
    grammar = midrule;

  /* End the dummy's rule.  */
  midrule->next = symbol_list_new (NULL, dummy_location);
  grammar_rule_check (midrule);
  midrule->next->next = current_rule;

  previous_rule_end = midrule->next;

  /* Insert the dummy nonterminal replacing the midrule action into
     the current rule.  Bind it to its dedicated rule.  */
  grammar_current_rule_symbol_append (dummy, dummy_location);
  grammar_end->midrule = midrule;
}

/* Set the precedence symbol of the current rule to PRECSYM. */

void
grammar_current_rule_prec_set (symbol *precsym, location loc)
{
  if (current_rule->ruleprec)
    complain_at (loc, _("only one %s allowed per rule"), "%prec");
  current_rule->ruleprec = precsym;
}

/* Attach dynamic precedence DPREC to the current rule. */

void
grammar_current_rule_dprec_set (int dprec, location loc)
{
  if (! glr_parser)
    warn_at (loc, _("%s affects only GLR parsers"), "%dprec");
  if (dprec <= 0)
    complain_at (loc, _("%s must be followed by positive number"), "%dprec");
  else if (current_rule->dprec != 0)
    complain_at (loc, _("only one %s allowed per rule"), "%dprec");
  current_rule->dprec = dprec;
}

/* Attach a merge function NAME with argument type TYPE to current
   rule. */

void
grammar_current_rule_merge_set (uniqstr name, location loc)
{
  if (! glr_parser)
    warn_at (loc, _("%s affects only GLR parsers"), "%merge");
  if (current_rule->merger != 0)
    complain_at (loc, _("only one %s allowed per rule"), "%merge");
  current_rule->merger =
    get_merge_function (name, current_rule->sym->type_name, loc);
}

/* Attach SYM to the current rule.  If needed, move the previous
   action as a mid-rule action.  */

void
grammar_current_rule_symbol_append (symbol *sym, location loc)
{
  if (current_rule->action)
    grammar_midrule_action ();
  grammar_symbol_append (sym, loc);
}

/* Attach an ACTION to the current rule.  */

void
grammar_current_rule_action_append (const char *action, location loc)
{
  /* There's no need to invoke grammar_midrule_action here, since the
     scanner already did it if necessary.  */
  current_rule->action = action;
  current_rule->action_location = loc;
}


/*---------------------------------------------------------------.
| Convert the rules into the representation using RRHS, RLHS and |
| RITEM.                                                         |
`---------------------------------------------------------------*/

static void
packgram (void)
{
  unsigned int itemno = 0;
  rule_number ruleno = 0;
  symbol_list *p = grammar;

  ritem = xnmalloc (nritems + 1, sizeof *ritem);

  /* This sentinel is used by build_relations in gram.c.  */
  *ritem++ = 0;

  rules = xnmalloc (nrules, sizeof *rules);

  while (p)
    {
      symbol *ruleprec = p->ruleprec;
      rules[ruleno].user_number = ruleno;
      rules[ruleno].number = ruleno;
      rules[ruleno].lhs = p->sym;
      rules[ruleno].rhs = ritem + itemno;
      rules[ruleno].prec = NULL;
      rules[ruleno].dprec = p->dprec;
      rules[ruleno].merger = p->merger;
      rules[ruleno].precsym = NULL;
      rules[ruleno].location = p->location;
      rules[ruleno].useful = true;
      rules[ruleno].action = p->action;
      rules[ruleno].action_location = p->action_location;

      p = p->next;
      while (p && p->sym)
	{
	  /* item_number = symbol_number.
	     But the former needs to contain more: negative rule numbers. */
	  ritem[itemno++] = symbol_number_as_item_number (p->sym->number);
	  /* A rule gets by default the precedence and associativity
	     of the last token in it.  */
	  if (p->sym->class == token_sym && default_prec)
	    rules[ruleno].prec = p->sym;
	  if (p)
	    p = p->next;
	}

      /* If this rule has a %prec,
         the specified symbol's precedence replaces the default.  */
      if (ruleprec)
	{
	  rules[ruleno].precsym = ruleprec;
	  rules[ruleno].prec = ruleprec;
	}
      ritem[itemno++] = rule_number_as_item_number (ruleno);
      ++ruleno;

      if (p)
	p = p->next;
    }

  assert (itemno == nritems);

  if (trace_flag & trace_sets)
    ritem_print (stderr);
}

/*------------------------------------------------------------------.
| Read in the grammar specification and record it in the format     |
| described in gram.h.  All actions are copied into ACTION_OBSTACK, |
| in each case forming the body of a C function (YYACTION) which    |
| contains a switch statement to decide which action to execute.    |
`------------------------------------------------------------------*/

void
reader (void)
{
  /* Initialize the symbol table.  */
  symbols_new ();

  /* Construct the accept symbol. */
  accept = symbol_get ("$accept", empty_location);
  accept->class = nterm_sym;
  accept->number = nvars++;

  /* Construct the error token */
  errtoken = symbol_get ("error", empty_location);
  errtoken->class = token_sym;
  errtoken->number = ntokens++;

  /* Construct a token that represents all undefined literal tokens.
     It is always token number 2.  */
  undeftoken = symbol_get ("$undefined", empty_location);
  undeftoken->class = token_sym;
  undeftoken->number = ntokens++;

  /* Initialize the obstacks. */
  obstack_init (&pre_prologue_obstack);
  obstack_init (&post_prologue_obstack);

  gram_in = xfopen (grammar_file, "r");

  gram__flex_debug = trace_flag & trace_scan;
  gram_debug = trace_flag & trace_parse;
  scanner_initialize ();
  gram_parse ();

  if (! complaint_issued)
    check_and_convert_grammar ();

  xfclose (gram_in);
}


/*-------------------------------------------------------------.
| Check the grammar that has just been read, and convert it to |
| internal form.					       |
`-------------------------------------------------------------*/

static void
check_and_convert_grammar (void)
{
  /* Grammar has been read.  Do some checking.  */
  if (nrules == 0)
    fatal (_("no rules in the input grammar"));

  /* Report any undefined symbols and consider them nonterminals.  */
  symbols_check_defined ();

  /* If the user did not define her ENDTOKEN, do it now. */
  if (!endtoken)
    {
      endtoken = symbol_get ("$end", empty_location);
      endtoken->class = token_sym;
      endtoken->number = 0;
      /* Value specified by POSIX.  */
      endtoken->user_token_number = 0;
    }

  /* Insert the initial rule, whose line is that of the first rule
     (not that of the start symbol):

     accept: %start EOF.  */
  {
    symbol_list *p = symbol_list_new (accept, empty_location);
    p->location = grammar->location;
    p->next = symbol_list_new (startsymbol, empty_location);
    p->next->next = symbol_list_new (endtoken, empty_location);
    p->next->next->next = symbol_list_new (NULL, empty_location);
    p->next->next->next->next = grammar;
    nrules += 1;
    nritems += 3;
    grammar = p;
  }

  assert (nsyms <= SYMBOL_NUMBER_MAXIMUM && nsyms == ntokens + nvars);

  /* Assign the symbols their symbol numbers.  Write #defines for the
     token symbols into FDEFINES if requested.  */
  symbols_pack ();

  /* Convert the grammar into the format described in gram.h.  */
  packgram ();

  /* The grammar as a symbol_list is no longer needed. */
  LIST_FREE (symbol_list, grammar);
}
