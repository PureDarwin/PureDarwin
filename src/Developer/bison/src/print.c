/* Print information on generated parser, for bison,

   Copyright (C) 1984, 1986, 1989, 2000, 2001, 2002, 2003, 2004, 2005
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

#include <config.h>
#include "system.h"

#include <bitset.h>
#include <quotearg.h>

#include "LR0.h"
#include "closure.h"
#include "conflicts.h"
#include "files.h"
#include "getargs.h"
#include "gram.h"
#include "lalr.h"
#include "print.h"
#include "reader.h"
#include "reduce.h"
#include "state.h"
#include "symtab.h"

static bitset shift_set;
static bitset look_ahead_set;

#if 0
static void
print_token (int extnum, int token)
{
  fprintf (out, _(" type %d is %s\n"), extnum, tags[token]);
}
#endif



/*---------------------------------------.
| *WIDTH := max (*WIDTH, strlen (STR)).  |
`---------------------------------------*/

static void
max_length (size_t *width, const char *str)
{
  size_t len = strlen (str);
  if (len > *width)
    *width = len;
}

/*--------------------------------.
| Report information on a state.  |
`--------------------------------*/

static void
print_core (FILE *out, state *s)
{
  size_t i;
  item_number *sitems = s->items;
  size_t snritems = s->nitems;
  symbol *previous_lhs = NULL;

  /* Output all the items of a state, not only its kernel.  */
  if (report_flag & report_itemsets)
    {
      closure (sitems, snritems);
      sitems = itemset;
      snritems = nritemset;
    }

  if (!snritems)
    return;

  fputc ('\n', out);

  for (i = 0; i < snritems; i++)
    {
      item_number *sp;
      item_number *sp1;
      rule_number r;

      sp1 = sp = ritem + sitems[i];

      while (*sp >= 0)
	sp++;

      r = item_number_as_rule_number (*sp);

      rule_lhs_print (&rules[r], previous_lhs, out);
      previous_lhs = rules[r].lhs;

      for (sp = rules[r].rhs; sp < sp1; sp++)
	fprintf (out, " %s", symbols[*sp]->tag);
      fputs (" .", out);
      for (/* Nothing */; *sp >= 0; ++sp)
	fprintf (out, " %s", symbols[*sp]->tag);

      /* Display the look-ahead tokens?  */
      if (report_flag & report_look_ahead_tokens)
	state_rule_look_ahead_tokens_print (s, &rules[r], out);

      fputc ('\n', out);
    }
}


/*------------------------------------------------------------.
| Report the shifts iff DISPLAY_SHIFTS_P or the gotos of S on |
| OUT.                                                        |
`------------------------------------------------------------*/

static void
print_transitions (state *s, FILE *out, bool display_transitions_p)
{
  transitions *trans = s->transitions;
  size_t width = 0;
  int i;

  /* Compute the width of the look-ahead token column.  */
  for (i = 0; i < trans->num; i++)
    if (!TRANSITION_IS_DISABLED (trans, i)
	&& TRANSITION_IS_SHIFT (trans, i) == display_transitions_p)
      {
	symbol *sym = symbols[TRANSITION_SYMBOL (trans, i)];
	max_length (&width, sym->tag);
      }

  /* Nothing to report. */
  if (!width)
    return;

  fputc ('\n', out);
  width += 2;

  /* Report look-ahead tokens and shifts.  */
  for (i = 0; i < trans->num; i++)
    if (!TRANSITION_IS_DISABLED (trans, i)
	&& TRANSITION_IS_SHIFT (trans, i) == display_transitions_p)
      {
	symbol *sym = symbols[TRANSITION_SYMBOL (trans, i)];
	const char *tag = sym->tag;
	state *s1 = trans->states[i];
	int j;

	fprintf (out, "    %s", tag);
	for (j = width - strlen (tag); j > 0; --j)
	  fputc (' ', out);
	if (display_transitions_p)
	  fprintf (out, _("shift, and go to state %d\n"), s1->number);
	else
	  fprintf (out, _("go to state %d\n"), s1->number);
      }
}


/*--------------------------------------------------------.
| Report the explicit errors of S raised from %nonassoc.  |
`--------------------------------------------------------*/

static void
print_errs (FILE *out, state *s)
{
  errs *errp = s->errs;
  size_t width = 0;
  int i;

  /* Compute the width of the look-ahead token column.  */
  for (i = 0; i < errp->num; ++i)
    if (errp->symbols[i])
      max_length (&width, errp->symbols[i]->tag);

  /* Nothing to report. */
  if (!width)
    return;

  fputc ('\n', out);
  width += 2;

  /* Report look-ahead tokens and errors.  */
  for (i = 0; i < errp->num; ++i)
    if (errp->symbols[i])
      {
	const char *tag = errp->symbols[i]->tag;
	int j;
	fprintf (out, "    %s", tag);
	for (j = width - strlen (tag); j > 0; --j)
	  fputc (' ', out);
	fputs (_("error (nonassociative)\n"), out);
      }
}


/*-------------------------------------------------------------.
| Return the default rule of S if it has one, NULL otherwise.  |
`-------------------------------------------------------------*/

static rule *
state_default_rule (state *s)
{
  reductions *reds = s->reductions;
  rule *default_rule = NULL;
  int cmax = 0;
  int i;

  /* No need for a look-ahead.  */
  if (s->consistent)
    return reds->rules[0];

  /* 1. Each reduction is possibly masked by the look-ahead tokens on which
     we shift (S/R conflicts)...  */
  bitset_zero (shift_set);
  {
    transitions *trans = s->transitions;
    FOR_EACH_SHIFT (trans, i)
      {
	/* If this state has a shift for the error token, don't use a
	     default rule.  */
	if (TRANSITION_IS_ERROR (trans, i))
	  return NULL;
	bitset_set (shift_set, TRANSITION_SYMBOL (trans, i));
      }
  }

  /* 2. Each reduction is possibly masked by the look-ahead tokens on which
     we raise an error (due to %nonassoc).  */
  {
    errs *errp = s->errs;
    for (i = 0; i < errp->num; i++)
      if (errp->symbols[i])
	bitset_set (shift_set, errp->symbols[i]->number);
  }

  for (i = 0; i < reds->num; ++i)
    {
      int count = 0;

      /* How many non-masked look-ahead tokens are there for this
	 reduction?  */
      bitset_andn (look_ahead_set, reds->look_ahead_tokens[i], shift_set);
      count = bitset_count (look_ahead_set);

      if (count > cmax)
	{
	  cmax = count;
	  default_rule = reds->rules[i];
	}

      /* 3. And finally, each reduction is possibly masked by previous
	 reductions (in R/R conflicts, we keep the first reductions).
	 */
      bitset_or (shift_set, shift_set, reds->look_ahead_tokens[i]);
    }

  return default_rule;
}


/*--------------------------------------------------------------------------.
| Report a reduction of RULE on LOOK_AHEAD_TOKEN (which can be `default').  |
| If not ENABLED, the rule is masked by a shift or a reduce (S/R and        |
| R/R conflicts).                                                           |
`--------------------------------------------------------------------------*/

static void
print_reduction (FILE *out, size_t width,
		 const char *look_ahead_token,
		 rule *r, bool enabled)
{
  int j;
  fprintf (out, "    %s", look_ahead_token);
  for (j = width - strlen (look_ahead_token); j > 0; --j)
    fputc (' ', out);
  if (!enabled)
    fputc ('[', out);
  if (r->number)
    fprintf (out, _("reduce using rule %d (%s)"), r->number, r->lhs->tag);
  else
    fprintf (out, _("accept"));
  if (!enabled)
    fputc (']', out);
  fputc ('\n', out);
}


/*-------------------------------------------.
| Report on OUT the reduction actions of S.  |
`-------------------------------------------*/

static void
print_reductions (FILE *out, state *s)
{
  transitions *trans = s->transitions;
  reductions *reds = s->reductions;
  rule *default_rule = NULL;
  size_t width = 0;
  int i, j;

  if (reds->num == 0)
    return;

  default_rule = state_default_rule (s);

  bitset_zero (shift_set);
  FOR_EACH_SHIFT (trans, i)
    bitset_set (shift_set, TRANSITION_SYMBOL (trans, i));

  /* Compute the width of the look-ahead token column.  */
  if (default_rule)
    width = strlen (_("$default"));

  if (reds->look_ahead_tokens)
    for (i = 0; i < ntokens; i++)
      {
	bool count = bitset_test (shift_set, i);

	for (j = 0; j < reds->num; ++j)
	  if (bitset_test (reds->look_ahead_tokens[j], i))
	    {
	      if (! count)
		{
		  if (reds->rules[j] != default_rule)
		    max_length (&width, symbols[i]->tag);
		  count = true;
		}
	      else
		{
		  max_length (&width, symbols[i]->tag);
		}
	    }
      }

  /* Nothing to report. */
  if (!width)
    return;

  fputc ('\n', out);
  width += 2;

  /* Report look-ahead tokens (or $default) and reductions.  */
  if (reds->look_ahead_tokens)
    for (i = 0; i < ntokens; i++)
      {
	bool defaulted = false;
	bool count = bitset_test (shift_set, i);

	for (j = 0; j < reds->num; ++j)
	  if (bitset_test (reds->look_ahead_tokens[j], i))
	    {
	      if (! count)
		{
		  if (reds->rules[j] != default_rule)
		    print_reduction (out, width,
				     symbols[i]->tag,
				     reds->rules[j], true);
		  else
		    defaulted = true;
		  count = true;
		}
	      else
		{
		  if (defaulted)
		    print_reduction (out, width,
				     symbols[i]->tag,
				     default_rule, true);
		  defaulted = false;
		  print_reduction (out, width,
				   symbols[i]->tag,
				   reds->rules[j], false);
		}
	    }
      }

  if (default_rule)
    print_reduction (out, width,
		     _("$default"), default_rule, true);
}


/*--------------------------------------------------------------.
| Report on OUT all the actions (shifts, gotos, reductions, and |
| explicit erros from %nonassoc) of S.                          |
`--------------------------------------------------------------*/

static void
print_actions (FILE *out, state *s)
{
  /* Print shifts.  */
  print_transitions (s, out, true);
  print_errs (out, s);
  print_reductions (out, s);
  /* Print gotos.  */
  print_transitions (s, out, false);
}


/*----------------------------------.
| Report all the data on S on OUT.  |
`----------------------------------*/

static void
print_state (FILE *out, state *s)
{
  fputs ("\n\n", out);
  fprintf (out, _("state %d"), s->number);
  fputc ('\n', out);
  print_core (out, s);
  print_actions (out, s);
  if ((report_flag & report_solved_conflicts) && s->solved_conflicts)
    {
      fputc ('\n', out);
      fputs (s->solved_conflicts, out);
    }
}

/*-----------------------------------------.
| Print information on the whole grammar.  |
`-----------------------------------------*/

#define END_TEST(End)				\
do {						\
  if (column + strlen(buffer) > (End))		\
    {						\
      fprintf (out, "%s\n   ", buffer);		\
      column = 3;				\
      buffer[0] = 0;				\
    }						\
} while (0)


static void
print_grammar (FILE *out)
{
  symbol_number i;
  char buffer[90];
  int column = 0;

  grammar_rules_print (out);

  /* TERMINAL (type #) : rule #s terminal is on RHS */
  fprintf (out, "%s\n\n", _("Terminals, with rules where they appear"));
  for (i = 0; i < max_user_token_number + 1; i++)
    if (token_translations[i] != undeftoken->number)
      {
	const char *tag = symbols[token_translations[i]]->tag;
	rule_number r;
	item_number *rhsp;

	buffer[0] = 0;
	column = strlen (tag);
	fputs (tag, out);
	END_TEST (50);
	sprintf (buffer, " (%d)", i);

	for (r = 0; r < nrules; r++)
	  for (rhsp = rules[r].rhs; *rhsp >= 0; rhsp++)
	    if (item_number_as_symbol_number (*rhsp) == token_translations[i])
	      {
		END_TEST (65);
		sprintf (buffer + strlen (buffer), " %d", r);
		break;
	      }
	fprintf (out, "%s\n", buffer);
      }
  fputs ("\n\n", out);


  fprintf (out, "%s\n\n", _("Nonterminals, with rules where they appear"));
  for (i = ntokens; i < nsyms; i++)
    {
      int left_count = 0, right_count = 0;
      rule_number r;
      const char *tag = symbols[i]->tag;

      for (r = 0; r < nrules; r++)
	{
	  item_number *rhsp;
	  if (rules[r].lhs->number == i)
	    left_count++;
	  for (rhsp = rules[r].rhs; *rhsp >= 0; rhsp++)
	    if (item_number_as_symbol_number (*rhsp) == i)
	      {
		right_count++;
		break;
	      }
	}

      buffer[0] = 0;
      fputs (tag, out);
      column = strlen (tag);
      sprintf (buffer, " (%d)", i);
      END_TEST (0);

      if (left_count > 0)
	{
	  END_TEST (50);
	  sprintf (buffer + strlen (buffer), _(" on left:"));

	  for (r = 0; r < nrules; r++)
	    {
	      END_TEST (65);
	      if (rules[r].lhs->number == i)
		sprintf (buffer + strlen (buffer), " %d", r);
	    }
	}

      if (right_count > 0)
	{
	  if (left_count > 0)
	    sprintf (buffer + strlen (buffer), ",");
	  END_TEST (50);
	  sprintf (buffer + strlen (buffer), _(" on right:"));
	  for (r = 0; r < nrules; r++)
	    {
	      item_number *rhsp;
	      for (rhsp = rules[r].rhs; *rhsp >= 0; rhsp++)
		if (item_number_as_symbol_number (*rhsp) == i)
		  {
		    END_TEST (65);
		    sprintf (buffer + strlen (buffer), " %d", r);
		    break;
		  }
	    }
	}
      fprintf (out, "%s\n", buffer);
    }
}

void
print_results (void)
{
  state_number i;

  /* We used to use just .out if SPEC_NAME_PREFIX (-p) was used, but
     that conflicts with Posix.  */
  FILE *out = xfopen (spec_verbose_file, "w");

  reduce_output (out);
  grammar_rules_partial_print (out,
			       _("Rules never reduced"), rule_never_reduced_p);
  conflicts_output (out);

  print_grammar (out);

  /* If the whole state item sets, not only the kernels, are wanted,
     `closure' will be run, which needs memory allocation/deallocation.   */
  if (report_flag & report_itemsets)
    new_closure (nritems);
  /* Storage for print_reductions.  */
  shift_set =  bitset_create (ntokens, BITSET_FIXED);
  look_ahead_set = bitset_create (ntokens, BITSET_FIXED);
  for (i = 0; i < nstates; i++)
    print_state (out, states[i]);
  bitset_free (shift_set);
  bitset_free (look_ahead_set);
  if (report_flag & report_itemsets)
    free_closure ();

  xfclose (out);
}
