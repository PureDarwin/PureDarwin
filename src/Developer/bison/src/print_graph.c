/* Output a VCG description on generated parser, for Bison,

   Copyright (C) 2001, 2002, 2003, 2004, 2005 Free Software Foundation, Inc.

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

#include "LR0.h"
#include "closure.h"
#include "complain.h"
#include "conflicts.h"
#include "files.h"
#include "getargs.h"
#include "gram.h"
#include "lalr.h"
#include "print_graph.h"
#include "reader.h"
#include "state.h"
#include "symtab.h"
#include "vcg.h"

static graph static_graph;
static FILE *fgraph = NULL;


/*----------------------------.
| Construct the node labels.  |
`----------------------------*/

static void
print_core (struct obstack *oout, state *s)
{
  size_t i;
  item_number *sitems = s->items;
  size_t snritems = s->nitems;

  /* Output all the items of a state, not only its kernel.  */
  if (report_flag & report_itemsets)
    {
      closure (sitems, snritems);
      sitems = itemset;
      snritems = nritemset;
    }

  obstack_fgrow1 (oout, "state %2d\n", s->number);
  for (i = 0; i < snritems; i++)
    {
      item_number *sp;
      item_number *sp1;
      rule_number r;

      sp1 = sp = ritem + sitems[i];

      while (*sp >= 0)
	sp++;

      r = item_number_as_rule_number (*sp);

      if (i)
	obstack_1grow (oout, '\n');
      obstack_fgrow1 (oout, " %s -> ",
		      rules[r].lhs->tag);

      for (sp = rules[r].rhs; sp < sp1; sp++)
	obstack_fgrow1 (oout, "%s ", symbols[*sp]->tag);

      obstack_1grow (oout, '.');

      for (/* Nothing */; *sp >= 0; ++sp)
	obstack_fgrow1 (oout, " %s", symbols[*sp]->tag);

      /* Experimental feature: display the look-ahead tokens. */
      if (report_flag & report_look_ahead_tokens)
	{
	  /* Find the reduction we are handling.  */
	  reductions *reds = s->reductions;
	  int redno = state_reduction_find (s, &rules[r]);

	  /* Print them if there are.  */
	  if (reds->look_ahead_tokens && redno != -1)
	    {
	      bitset_iterator biter;
	      int k;
	      char const *sep = "";
	      obstack_sgrow (oout, "[");
	      BITSET_FOR_EACH (biter, reds->look_ahead_tokens[redno], k, 0)
		{
		  obstack_fgrow2 (oout, "%s%s", sep, symbols[k]->tag);
		  sep = ", ";
		}
	      obstack_sgrow (oout, "]");
	    }
	}
    }
}


/*---------------------------------------------------------------.
| Output in graph_obstack edges specifications in incidence with |
| current node.                                                  |
`---------------------------------------------------------------*/

static void
print_actions (state *s, const char *node_name)
{
  int i;

  transitions *trans = s->transitions;
  reductions *reds = s->reductions;

  static char buff[10];
  edge e;

  if (!trans->num && !reds)
    return;

  for (i = 0; i < trans->num; i++)
    if (!TRANSITION_IS_DISABLED (trans, i))
      {
	state *s1 = trans->states[i];
	symbol_number sym = s1->accessing_symbol;

	new_edge (&e);

	if (s->number > s1->number)
	  e.type = back_edge;
	open_edge (&e, fgraph);
	/* The edge source is the current node.  */
	e.sourcename = node_name;
	sprintf (buff, "%d", s1->number);
	e.targetname = buff;
	/* Shifts are blue, gotos are green, and error is red. */
	if (TRANSITION_IS_ERROR (trans, i))
	  e.color = red;
	else
	  e.color = TRANSITION_IS_SHIFT (trans, i) ? blue : green;
	e.label = symbols[sym]->tag;
	output_edge (&e, fgraph);
	close_edge (fgraph);
      }
}


/*-------------------------------------------------------------.
| Output in FGRAPH the current node specifications and exiting |
| edges.                                                       |
`-------------------------------------------------------------*/

static void
print_state (state *s)
{
  static char name[10];
  struct obstack node_obstack;
  node n;

  /* The labels of the nodes are their the items.  */
  obstack_init (&node_obstack);
  new_node (&n);
  sprintf (name, "%d", s->number);
  n.title = name;
  print_core (&node_obstack, s);
  obstack_1grow (&node_obstack, '\0');
  n.label = obstack_finish (&node_obstack);

  open_node (fgraph);
  output_node (&n, fgraph);
  close_node (fgraph);

  /* Output the edges.  */
  print_actions (s, name);

  obstack_free (&node_obstack, 0);
}


void
print_graph (void)
{
  state_number i;

  /* Output file.  */
  fgraph = xfopen (spec_graph_file, "w");

  new_graph (&static_graph);

  static_graph.display_edge_labels = yes;

  static_graph.port_sharing = no;
  static_graph.finetuning = yes;
  static_graph.priority_phase = yes;
  static_graph.splines = yes;

  static_graph.crossing_weight = median;

  /* Output graph options. */
  open_graph (fgraph);
  output_graph (&static_graph, fgraph);

  /* Output nodes and edges. */
  new_closure (nritems);
  for (i = 0; i < nstates; i++)
    print_state (states[i]);
  free_closure ();

  /* Close graph. */
  close_graph (&static_graph, fgraph);
  xfclose (fgraph);
}
