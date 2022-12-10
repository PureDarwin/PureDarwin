/* Compute look-ahead criteria for Bison.

   Copyright (C) 1984, 1986, 1989, 2000, 2001, 2002, 2003, 2004, 2005,
   2006 Free Software Foundation, Inc.

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


/* Compute how to make the finite state machine deterministic; find
   which rules need look-ahead in each state, and which look-ahead
   tokens they accept.  */

#include <config.h>
#include "system.h"

#include <bitset.h>
#include <bitsetv.h>
#include <quotearg.h>

#include "LR0.h"
#include "complain.h"
#include "derives.h"
#include "getargs.h"
#include "gram.h"
#include "lalr.h"
#include "nullable.h"
#include "reader.h"
#include "relation.h"
#include "symtab.h"

goto_number *goto_map;
static goto_number ngotos;
state_number *from_state;
state_number *to_state;

/* Linked list of goto numbers.  */
typedef struct goto_list
{
  struct goto_list *next;
  goto_number value;
} goto_list;


/* LA is a LR by NTOKENS matrix of bits.  LA[l, i] is 1 if the rule
   LArule[l] is applicable in the appropriate state when the next
   token is symbol i.  If LA[l, i] and LA[l, j] are both 1 for i != j,
   it is a conflict.  */

static bitsetv LA = NULL;
size_t nLA;


/* And for the famous F variable, which name is so descriptive that a
   comment is hardly needed.  <grin>.  */
static bitsetv F = NULL;

static goto_number **includes;
static goto_list **lookback;




static void
set_goto_map (void)
{
  state_number s;
  goto_number *temp_map;

  goto_map = xcalloc (nvars + 1, sizeof *goto_map);
  temp_map = xnmalloc (nvars + 1, sizeof *temp_map);

  ngotos = 0;
  for (s = 0; s < nstates; ++s)
    {
      transitions *sp = states[s]->transitions;
      int i;
      for (i = sp->num - 1; i >= 0 && TRANSITION_IS_GOTO (sp, i); --i)
	{
	  ngotos++;

	  /* Abort if (ngotos + 1) would overflow.  */
	  assert (ngotos != GOTO_NUMBER_MAXIMUM);

	  goto_map[TRANSITION_SYMBOL (sp, i) - ntokens]++;
	}
    }

  {
    goto_number k = 0;
    int i;
    for (i = ntokens; i < nsyms; i++)
      {
	temp_map[i - ntokens] = k;
	k += goto_map[i - ntokens];
      }

    for (i = ntokens; i < nsyms; i++)
      goto_map[i - ntokens] = temp_map[i - ntokens];

    goto_map[nsyms - ntokens] = ngotos;
    temp_map[nsyms - ntokens] = ngotos;
  }

  from_state = xcalloc (ngotos, sizeof *from_state);
  to_state = xcalloc (ngotos, sizeof *to_state);

  for (s = 0; s < nstates; ++s)
    {
      transitions *sp = states[s]->transitions;
      int i;
      for (i = sp->num - 1; i >= 0 && TRANSITION_IS_GOTO (sp, i); --i)
	{
	  goto_number k = temp_map[TRANSITION_SYMBOL (sp, i) - ntokens]++;
	  from_state[k] = s;
	  to_state[k] = sp->states[i]->number;
	}
    }

  free (temp_map);
}



/*----------------------------------------------------------.
| Map a state/symbol pair into its numeric representation.  |
`----------------------------------------------------------*/

static goto_number
map_goto (state_number s0, symbol_number sym)
{
  goto_number high;
  goto_number low;
  goto_number middle;
  state_number s;

  low = goto_map[sym - ntokens];
  high = goto_map[sym - ntokens + 1] - 1;

  for (;;)
    {
      assert (low <= high);
      middle = (low + high) / 2;
      s = from_state[middle];
      if (s == s0)
	return middle;
      else if (s < s0)
	low = middle + 1;
      else
	high = middle - 1;
    }
}


static void
initialize_F (void)
{
  goto_number **reads = xnmalloc (ngotos, sizeof *reads);
  goto_number *edge = xnmalloc (ngotos + 1, sizeof *edge);
  goto_number nedges = 0;

  goto_number i;

  F = bitsetv_create (ngotos, ntokens, BITSET_FIXED);

  for (i = 0; i < ngotos; i++)
    {
      state_number stateno = to_state[i];
      transitions *sp = states[stateno]->transitions;

      int j;
      FOR_EACH_SHIFT (sp, j)
	bitset_set (F[i], TRANSITION_SYMBOL (sp, j));

      for (; j < sp->num; j++)
	{
	  symbol_number sym = TRANSITION_SYMBOL (sp, j);
	  if (nullable[sym - ntokens])
	    edge[nedges++] = map_goto (stateno, sym);
	}

      if (nedges == 0)
	reads[i] = NULL;
      else
	{
	  reads[i] = xnmalloc (nedges + 1, sizeof reads[i][0]);
	  memcpy (reads[i], edge, nedges * sizeof edge[0]);
	  reads[i][nedges] = END_NODE;
	  nedges = 0;
	}
    }

  relation_digraph (reads, ngotos, &F);

  for (i = 0; i < ngotos; i++)
    free (reads[i]);

  free (reads);
  free (edge);
}


static void
add_lookback_edge (state *s, rule *r, goto_number gotono)
{
  int ri = state_reduction_find (s, r);
  goto_list *sp = xmalloc (sizeof *sp);
  sp->next = lookback[(s->reductions->look_ahead_tokens - LA) + ri];
  sp->value = gotono;
  lookback[(s->reductions->look_ahead_tokens - LA) + ri] = sp;
}



static void
build_relations (void)
{
  goto_number *edge = xnmalloc (ngotos + 1, sizeof *edge);
  state_number *states1 = xnmalloc (ritem_longest_rhs () + 1, sizeof *states1);
  goto_number i;

  includes = xnmalloc (ngotos, sizeof *includes);

  for (i = 0; i < ngotos; i++)
    {
      int nedges = 0;
      symbol_number symbol1 = states[to_state[i]]->accessing_symbol;
      rule **rulep;

      for (rulep = derives[symbol1 - ntokens]; *rulep; rulep++)
	{
	  bool done;
	  int length = 1;
	  item_number const *rp;
	  state *s = states[from_state[i]];
	  states1[0] = s->number;

	  for (rp = (*rulep)->rhs; ! item_number_is_rule_number (*rp); rp++)
	    {
	      s = transitions_to (s->transitions,
				  item_number_as_symbol_number (*rp));
	      states1[length++] = s->number;
	    }

	  if (!s->consistent)
	    add_lookback_edge (s, *rulep, i);

	  length--;
	  done = false;
	  while (!done)
	    {
	      done = true;
	      /* Each rhs ends in an item number, and there is a
		 sentinel before the first rhs, so it is safe to
		 decrement RP here.  */
	      rp--;
	      if (ISVAR (*rp))
		{
		  /* Downcasting from item_number to symbol_number.  */
		  edge[nedges++] = map_goto (states1[--length],
					     item_number_as_symbol_number (*rp));
		  if (nullable[*rp - ntokens])
		    done = false;
		}
	    }
	}

      if (nedges == 0)
	includes[i] = NULL;
      else
	{
	  int j;
	  includes[i] = xnmalloc (nedges + 1, sizeof includes[i][0]);
	  for (j = 0; j < nedges; j++)
	    includes[i][j] = edge[j];
	  includes[i][nedges] = END_NODE;
	}
    }

  free (edge);
  free (states1);

  relation_transpose (&includes, ngotos);
}



static void
compute_FOLLOWS (void)
{
  goto_number i;

  relation_digraph (includes, ngotos, &F);

  for (i = 0; i < ngotos; i++)
    free (includes[i]);

  free (includes);
}


static void
compute_look_ahead_tokens (void)
{
  size_t i;
  goto_list *sp;

  for (i = 0; i < nLA; i++)
    for (sp = lookback[i]; sp; sp = sp->next)
      bitset_or (LA[i], LA[i], F[sp->value]);

  /* Free LOOKBACK. */
  for (i = 0; i < nLA; i++)
    LIST_FREE (goto_list, lookback[i]);

  free (lookback);
  bitsetv_free (F);
}


/*-----------------------------------------------------.
| Count the number of look-ahead tokens required for S |
| (N_LOOK_AHEAD_TOKENS member).                        |
`-----------------------------------------------------*/

static int
state_look_ahead_tokens_count (state *s)
{
  int k;
  int n_look_ahead_tokens = 0;
  reductions *rp = s->reductions;
  transitions *sp = s->transitions;

  /* We need a look-ahead either to distinguish different
     reductions (i.e., there are two or more), or to distinguish a
     reduction from a shift.  Otherwise, it is straightforward,
     and the state is `consistent'.  */
  if (rp->num > 1
      || (rp->num == 1 && sp->num &&
	  !TRANSITION_IS_DISABLED (sp, 0) && TRANSITION_IS_SHIFT (sp, 0)))
    n_look_ahead_tokens += rp->num;
  else
    s->consistent = 1;

  for (k = 0; k < sp->num; k++)
    if (!TRANSITION_IS_DISABLED (sp, k) && TRANSITION_IS_ERROR (sp, k))
      {
	s->consistent = 0;
	break;
      }

  return n_look_ahead_tokens;
}


/*-----------------------------------------------------.
| Compute LA, NLA, and the look_ahead_tokens members.  |
`-----------------------------------------------------*/

static void
initialize_LA (void)
{
  state_number i;
  bitsetv pLA;

  /* Compute the total number of reductions requiring a look-ahead.  */
  nLA = 0;
  for (i = 0; i < nstates; i++)
    nLA += state_look_ahead_tokens_count (states[i]);
  /* Avoid having to special case 0.  */
  if (!nLA)
    nLA = 1;

  pLA = LA = bitsetv_create (nLA, ntokens, BITSET_FIXED);
  lookback = xcalloc (nLA, sizeof *lookback);

  /* Initialize the members LOOK_AHEAD_TOKENS for each state whose reductions
     require look-ahead tokens.  */
  for (i = 0; i < nstates; i++)
    {
      int count = state_look_ahead_tokens_count (states[i]);
      if (count)
	{
	  states[i]->reductions->look_ahead_tokens = pLA;
	  pLA += count;
	}
    }
}


/*----------------------------------------------.
| Output the look-ahead tokens for each state.  |
`----------------------------------------------*/

static void
look_ahead_tokens_print (FILE *out)
{
  state_number i;
  int j, k;
  fprintf (out, "Look-ahead tokens: BEGIN\n");
  for (i = 0; i < nstates; ++i)
    {
      reductions *reds = states[i]->reductions;
      bitset_iterator iter;
      int n_look_ahead_tokens = 0;

      if (reds->look_ahead_tokens)
	for (k = 0; k < reds->num; ++k)
	  if (reds->look_ahead_tokens[k])
	    ++n_look_ahead_tokens;

      fprintf (out, "State %d: %d look-ahead tokens\n",
	       i, n_look_ahead_tokens);

      if (reds->look_ahead_tokens)
	for (j = 0; j < reds->num; ++j)
	  BITSET_FOR_EACH (iter, reds->look_ahead_tokens[j], k, 0)
	  {
	    fprintf (out, "   on %d (%s) -> rule %d\n",
		     k, symbols[k]->tag,
		     reds->rules[j]->number);
	  };
    }
  fprintf (out, "Look-ahead tokens: END\n");
}

void
lalr (void)
{
  initialize_LA ();
  set_goto_map ();
  initialize_F ();
  build_relations ();
  compute_FOLLOWS ();
  compute_look_ahead_tokens ();

  if (trace_flag & trace_sets)
    look_ahead_tokens_print (stderr);
}


void
lalr_free (void)
{
  state_number s;
  for (s = 0; s < nstates; ++s)
    states[s]->reductions->look_ahead_tokens = NULL;
  bitsetv_free (LA);
}
