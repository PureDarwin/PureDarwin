/* VCG description handler for Bison.

   Copyright (C) 2001, 2002, 2003, 2004, 2005, 2006 Free Software
   Foundation, Inc.

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

#include "vcg.h"
#include "vcg_defaults.h"

/* Return an unambiguous printable representated, for NAME, suitable
   for C strings.  Use slot 2 since the user may use slots 0 and 1.
   */

static char const *
quote (char const *name)
{
  return quotearg_n_style (2, c_quoting_style, name);
}


/* Initialize a graph with the default values. */
void
new_graph (graph *g)
{
  g->title = G_TITLE;
  g->label = G_LABEL;

  g->infos[0] = G_INFOS1;
  g->infos[1] = G_INFOS2;
  g->infos[2] = G_INFOS3;

  g->color = G_COLOR;
  g->textcolor = G_TEXTCOLOR;
  g->bordercolor = G_BORDERCOLOR;

  g->width = G_WIDTH;
  g->height = G_HEIGHT;
  g->borderwidth = G_BORDERWIDTH;
  g->x = G_X;
  g->y = G_Y;
  g->folding = G_FOLDING;
  g->shrink = G_SHRINK;
  g->stretch = G_STRETCH;

  g->textmode = G_TEXTMODE;
  g->shape = G_SHAPE;

  g->vertical_order = G_VERTICAL_ORDER;
  g->horizontal_order = G_HORIZONTAL_ORDER;

  g->xmax = G_XMAX; /* Not output. */
  g->ymax = G_YMAX; /* Not output. */

  g->xbase = G_XBASE;
  g->ybase = G_YBASE;

  g->xspace = G_XSPACE;
  g->yspace = G_YSPACE;
  g->xlspace = G_XLSPACE; /* Not output. */

  g->xraster = G_XRASTER;
  g->yraster = G_YRASTER;
  g->xlraster = G_XLRASTER;

  g->hidden = G_HIDDEN; /* No default value. */

  g->classname = G_CLASSNAME; /* No class name association. */

  g->layout_downfactor = G_LAYOUT_DOWNFACTOR;
  g->layout_upfactor = G_LAYOUT_UPFACTOR;
  g->layout_nearfactor = G_LAYOUT_NEARFACTOR;
  g->layout_splinefactor = G_LAYOUT_SPLINEFACTOR;

  g->late_edge_labels = G_LATE_EDGE_LABELS;
  g->display_edge_labels = G_DISPLAY_EDGE_LABELS;
  g->dirty_edge_labels = G_DIRTY_EDGE_LABELS;
  g->finetuning = G_FINETUNING;
  g->ignore_singles = G_IGNORE_SINGLES;
  g->priority_phase = G_PRIORITY_PHASE;
  g->manhattan_edges = G_MANHATTAN_EDGES;
  g->smanhattan_edges = G_SMANHATTAN_EDGES;
  g->near_edges = G_NEAR_EDGES;

  g->orientation = G_ORIENTATION;
  g->node_alignment = G_NODE_ALIGNMENT;
  g->port_sharing = G_PORT_SHARING;
  g->arrow_mode = G_ARROW_MODE;
  g->treefactor = G_TREEFACTOR;
  g->spreadlevel = G_SPREADLEVEL;
  g->crossing_weight = G_CROSSING_WEIGHT;
  g->crossing_phase2 = G_CROSSING_PHASE2;
  g->crossing_optimization = G_CROSSING_OPTIMIZATION;
  g->view = G_VIEW;

  g->edges = G_EDGES;
  g->nodes = G_NODES;
  g->splines = G_SPLINES;

  g->bmax = G_BMAX;
  g->cmin = G_CMIN;
  g->cmax = G_CMAX;
  g->pmin = G_PMIN;
  g->pmax = G_PMAX;
  g->rmin = G_RMIN;
  g->rmax = G_RMAX;
  g->smax = G_SMAX;

  g->node_list = G_NODE_LIST;
  g->edge_list = G_EDGE_LIST;

  new_edge (&g->edge);
  new_node (&g->node);
}

/* Initialize a node with the default values. */
void
new_node (node *n)
{
  n->title = N_TITLE;
  n->label = N_LABEL;

  n->locx = N_LOCX; /* Default unspcified. */
  n->locy = N_LOCY; /* Default unspcified. */

  n->vertical_order = N_VERTICAL_ORDER;	/* Default unspcified. */
  n->horizontal_order = N_HORIZONTAL_ORDER;	/* Default unspcified. */

  n->width = N_WIDTH; /* We assume that we can't define it now. */
  n->height = N_HEIGHT; /* Also. */

  n->shrink = N_SHRINK;
  n->stretch = N_STRETCH;

  n->folding = N_FOLDING; /* No explicit default value. */

  n->shape = N_SHAPE;
  n->textmode = N_TEXTMODE;
  n->borderwidth = N_BORDERWIDTH;

  n->color = N_COLOR;
  n->textcolor = N_TEXTCOLOR;
  n->bordercolor = N_BORDERCOLOR;

  n->infos[0] = N_INFOS1;
  n->infos[1] = N_INFOS2;
  n->infos[2] = N_INFOS3;

  n->next = N_NEXT;
}

/* Initialize an edge with the default values. */
void
new_edge (edge *e)
{
  e->type = E_EDGE_TYPE;

  e->sourcename = E_SOURCENAME;
  e->targetname = E_TARGETNAME;
  e->label = E_LABEL;

  e->linestyle = E_LINESTYLE;
  e->thickness = E_THICKNESS;

  e->class = E_CLASS;

  e->color = E_COLOR;
  e->textcolor = E_TEXTCOLOR;
  e->arrowcolor = E_ARROWCOLOR;
  e->backarrowcolor = E_BACKARROWCOLOR;

  e->arrowsize = E_ARROWSIZE;
  e->backarrowsize = E_BACKARROWSIZE;
  e->arrowstyle = E_ARROWSTYLE;

  e->backarrowstyle = E_BACKARROWSTYLE;

  e->priority = E_PRIORITY;

  e->anchor = E_ANCHOR;

  e->horizontal_order = E_HORIZONTAL_ORDER;

  e->next = E_NEXT;
}

/*----------------------------------------------.
| Get functions.	                        |
| Return string corresponding to an enum value. |
`----------------------------------------------*/

static const char *
get_color_str (enum color color)
{
  switch (color)
    {
    default:		abort ();
    case white:		return "white";
    case blue:		return "blue";
    case red:		return "red";
    case green:		return "green";
    case yellow:	return "yellow";
    case magenta:	return "magenta";
    case cyan:		return "cyan";
    case darkgrey:	return "darkgrey";
    case darkblue:	return "darkblue";
    case darkred:	return "darkred";
    case darkgreen:	return "darkgreen";
    case darkyellow:	return "darkyellow";
    case darkmagenta:	return "darkmagenta";
    case darkcyan:	return "darkcyan";
    case gold:		return "gold";
    case lightgrey:	return "lightgrey";
    case lightblue:	return "lightblue";
    case lightred:	return "lightred";
    case lightgreen:	return "lightgreen";
    case lightyellow:	return "lightyellow";
    case lightmagenta:	return "lightmagenta";
    case lightcyan:	return "lightcyan";
    case lilac:		return "lilac";
    case turquoise:	return "turquoise";
    case aquamarine:	return "aquamarine";
    case khaki:		return "khaki";
    case purple:	return "purple";
    case yellowgreen:	return "yellowgreen";
    case pink:		return "pink";
    case orange:	return "orange";
    case orchid:	return "orchid";
    case black:		return "black";
    }
}

static const char *
get_textmode_str (enum textmode textmode)
{
  switch (textmode)
    {
    default:		abort ();
    case centered:	return "center";
    case left_justify:	return "left_justify";
    case right_justify:	return "right_justify";
    }
}

static const char *
get_shape_str (enum shape shape)
{
  switch (shape)
    {
    default:		abort ();
    case box:		return "box";
    case rhomb:		return "rhomb";
    case ellipse:	return "ellipse";
    case triangle:	return "triangle";
    }
}

static const char *
get_decision_str (enum decision decision)
{
  switch (decision)
    {
    default:	abort ();
    case no:	return "no";
    case yes:	return "yes";
    }
}

static const char *
get_orientation_str (enum orientation orientation)
{
  switch (orientation)
    {
    default:		abort ();
    case top_to_bottom:	return "top_to_bottom";
    case bottom_to_top: return "bottom_to_top";
    case left_to_right: return "left_to_right";
    case right_to_left: return "right_to_left";
    }
}

static const char *
get_node_alignment_str (enum alignment alignment)
{
  switch (alignment)
    {
    default:		abort ();
    case center:	return "center";
    case top:		return "top";
    case bottom:	return "bottom";
    }
}

static const char *
get_arrow_mode_str (enum arrow_mode arrow_mode)
{
  switch (arrow_mode)
    {
    default:		abort ();
    case fixed:		return "fixed";
    case free_a:	return "free";
    }
}

static const char *
get_crossing_type_str (enum crossing_type crossing_type)
{
  switch (crossing_type)
    {
    default:		abort ();
    case bary:		return "bary";
    case median:	return "median";
    case barymedian:	return "barymedian";
    case medianbary:	return "medianbary";
    }
}

static const char *
get_view_str (enum view view)
{
  /* There is no way with vcg 1.30 to specify a normal view explicitly,
     so it is an error here if view == normal_view.  */
  switch (view)
    {
    default:		abort ();
    case cfish:		return "cfish";
    case pfish:		return "pfish";
    case fcfish:	return "fcfish";
    case fpfish:	return "fpfish";
    }
}

static const char *
get_linestyle_str (enum linestyle linestyle)
{
  switch (linestyle)
    {
    default:		abort ();
    case continuous:	return "continuous";
    case dashed:	return "dashed";
    case dotted:	return "dotted";
    case invisible:	return "invisible";
    }
}

static const char *
get_arrowstyle_str (enum arrowstyle arrowstyle)
{
  switch (arrowstyle)
    {
    default:	abort ();
    case solid:	return "solid";
    case line:	return "line";
    case none:	return "none";
    }
}

/*------------------------------.
| Add functions.	        |
| Edge and nodes into a graph.  |
`------------------------------*/

void
add_node (graph *g, node *n)
{
  n->next = g->node_list;
  g->node_list = n;
}

void
add_edge (graph *g, edge *e)
{
  e->next = g->edge_list;
  g->edge_list = e;
}

void
add_classname (graph *g, int val, const char *name)
{
  struct classname *classname = xmalloc (sizeof *classname);
  classname->no = val;
  classname->name = name;
  classname->next = g->classname;
  g->classname = classname;
}

void
add_infoname (graph *g, int integer, const char *str)
{
  struct infoname *infoname = xmalloc (sizeof *infoname);
  infoname->integer = integer;
  infoname->chars = str;
  infoname->next = g->infoname;
  g->infoname = infoname;
}

/* Build a colorentry struct and add it to the list.  */
void
add_colorentry (graph *g, int color_idx, int red_cp,
		int green_cp, int blue_cp)
{
  struct colorentry *ce = xmalloc (sizeof *ce);
  ce->color_index = color_idx;
  ce->red_cp = red_cp;
  ce->green_cp = green_cp;
  ce->blue_cp = blue_cp;
  ce->next = g->colorentry;
  g->colorentry = ce;
}

/*-------------------------------------.
| Open and close functions (formatted) |
`-------------------------------------*/

void
open_edge (edge *e, FILE *fout)
{
  switch (e->type)
    {
    case normal_edge:
      fputs ("\tedge: {\n", fout);
      break;
    case back_edge:
      fputs ("\tbackedge: {\n", fout);
      break;
    case near_edge:
      fputs ("\tnearedge: {\n", fout);
      break;
    case bent_near_edge:
      fputs ("\tbentnearedge: {\n", fout);
      break;
    default:
      fputs ("\tedge: {\n", fout);
    }
}

void
close_edge (FILE *fout)
{
  fputs ("\t}\n", fout);
}

void
open_node (FILE *fout)
{
  fputs ("\tnode: {\n", fout);
}

void
close_node (FILE *fout)
{
  fputs ("\t}\n", fout);
}

void
open_graph (FILE *fout)
{
  fputs ("graph: {\n", fout);
}

void
close_graph (graph *g, FILE *fout)
{
  fputc ('\n', fout);

  /* FIXME: Unallocate nodes and edges if required.  */
  {
    node *n;

    for (n = g->node_list; n; n = n->next)
      {
	open_node (fout);
	output_node (n, fout);
	close_node (fout);
      }
  }

  fputc ('\n', fout);

  {
    edge *e;

    for (e = g->edge_list; e; e = e->next)
      {
	open_edge (e, fout);
	output_edge (e, fout);
	close_edge (fout);
      }
  }

  fputs ("}\n", fout);
}

/*-------------------------------------------.
| Output functions (formatted) in file FOUT  |
`-------------------------------------------*/

void
output_node (node *n, FILE *fout)
{
  if (n->title != N_TITLE)
    fprintf (fout, "\t\ttitle:\t%s\n", quote (n->title));
  if (n->label != N_LABEL)
    fprintf (fout, "\t\tlabel:\t%s\n", quote (n->label));

  if ((n->locx != N_LOCX) && (n->locy != N_LOCY))
    fprintf (fout, "\t\tloc { x: %d  y: %d }\t\n", n->locx, n->locy);

  if (n->vertical_order != N_VERTICAL_ORDER)
    fprintf (fout, "\t\tvertical_order:\t%d\n", n->vertical_order);
  if (n->horizontal_order != N_HORIZONTAL_ORDER)
    fprintf (fout, "\t\thorizontal_order:\t%d\n", n->horizontal_order);

  if (n->width != N_WIDTH)
    fprintf (fout, "\t\twidth:\t%d\n", n->width);
  if (n->height != N_HEIGHT)
    fprintf (fout, "\t\theight:\t%d\n", n->height);

  if (n->shrink != N_SHRINK)
    fprintf (fout, "\t\tshrink:\t%d\n", n->shrink);
  if (n->stretch != N_STRETCH)
    fprintf (fout, "\t\tstretch:\t%d\n", n->stretch);

  if (n->folding != N_FOLDING)
    fprintf (fout, "\t\tfolding:\t%d\n", n->folding);

  if (n->textmode != N_TEXTMODE)
    fprintf (fout, "\t\ttextmode:\t%s\n",
	     get_textmode_str (n->textmode));

  if (n->shape != N_SHAPE)
    fprintf (fout, "\t\tshape:\t%s\n", get_shape_str (n->shape));

  if (n->borderwidth != N_BORDERWIDTH)
    fprintf (fout, "\t\tborderwidth:\t%d\n", n->borderwidth);

  if (n->color != N_COLOR)
    fprintf (fout, "\t\tcolor:\t%s\n", get_color_str (n->color));
  if (n->textcolor != N_TEXTCOLOR)
    fprintf (fout, "\t\ttextcolor:\t%s\n",
	     get_color_str (n->textcolor));
  if (n->bordercolor != N_BORDERCOLOR)
    fprintf (fout, "\t\tbordercolor:\t%s\n",
	     get_color_str (n->bordercolor));

  {
    int i;
    for (i = 0; i < 3; ++i)
      if (n->infos[i])
	fprintf (fout, "\t\tinfo%d:\t%s\n",
		 i, quote (n->infos[i]));
  }
}

void
output_edge (edge *e, FILE *fout)
{
  /* FIXME: SOURCENAME and TARGETNAME are mandatory
     so it has to be fatal not to give these informations.  */
  if (e->sourcename != E_SOURCENAME)
    fprintf (fout, "\t\tsourcename:\t%s\n", quote (e->sourcename));
  if (e->targetname != E_TARGETNAME)
    fprintf (fout, "\t\ttargetname:\t%s\n", quote (e->targetname));

  if (e->label != E_LABEL)
    fprintf (fout, "\t\tlabel:\t%s\n", quote (e->label));

  if (e->linestyle != E_LINESTYLE)
    fprintf (fout, "\t\tlinestyle:\t%s\n", get_linestyle_str (e->linestyle));

  if (e->thickness != E_THICKNESS)
    fprintf (fout, "\t\tthickness:\t%d\n", e->thickness);
  if (e->class != E_CLASS)
    fprintf (fout, "\t\tclass:\t%d\n", e->class);

  if (e->color != E_COLOR)
    fprintf (fout, "\t\tcolor:\t%s\n", get_color_str (e->color));
  if (e->color != E_TEXTCOLOR)
    fprintf (fout, "\t\ttextcolor:\t%s\n",
	     get_color_str (e->textcolor));
  if (e->arrowcolor != E_ARROWCOLOR)
    fprintf (fout, "\t\tarrowcolor:\t%s\n",
	     get_color_str (e->arrowcolor));
  if (e->backarrowcolor != E_BACKARROWCOLOR)
    fprintf (fout, "\t\tbackarrowcolor:\t%s\n",
	     get_color_str (e->backarrowcolor));

  if (e->arrowsize != E_ARROWSIZE)
    fprintf (fout, "\t\tarrowsize:\t%d\n", e->arrowsize);
  if (e->backarrowsize != E_BACKARROWSIZE)
    fprintf (fout, "\t\tbackarrowsize:\t%d\n", e->backarrowsize);

  if (e->arrowstyle != E_ARROWSTYLE)
    fprintf (fout, "\t\tarrowstyle:\t%s\n",
	     get_arrowstyle_str (e->arrowstyle));
  if (e->backarrowstyle != E_BACKARROWSTYLE)
    fprintf (fout, "\t\tbackarrowstyle:\t%s\n",
	     get_arrowstyle_str (e->backarrowstyle));

  if (e->priority != E_PRIORITY)
    fprintf (fout, "\t\tpriority:\t%d\n", e->priority);
  if (e->anchor != E_ANCHOR)
    fprintf (fout, "\t\tanchor:\t%d\n", e->anchor);
  if (e->horizontal_order != E_HORIZONTAL_ORDER)
    fprintf (fout, "\t\thorizontal_order:\t%d\n", e->horizontal_order);
}

void
output_graph (graph *g, FILE *fout)
{
  if (g->title)
    fprintf (fout, "\ttitle:\t%s\n", quote (g->title));
  if (g->label)
    fprintf (fout, "\tlabel:\t%s\n", quote (g->label));

  {
    int i;
    for (i = 0; i < 3; ++i)
      if (g->infos[i])
	fprintf (fout, "\tinfo%d:\t%s\n", i, quote (g->infos[i]));
  }

  if (g->color != G_COLOR)
    fprintf (fout, "\tcolor:\t%s\n", get_color_str (g->color));
  if (g->textcolor != G_TEXTCOLOR)
    fprintf (fout, "\ttextcolor:\t%s\n", get_color_str (g->textcolor));
  if (g->bordercolor != G_BORDERCOLOR)
    fprintf (fout, "\tbordercolor:\t%s\n",
	     get_color_str (g->bordercolor));

  if (g->width != G_WIDTH)
    fprintf (fout, "\twidth:\t%d\n", g->width);
  if (g->height != G_HEIGHT)
    fprintf (fout, "\theight:\t%d\n", g->height);
  if (g->borderwidth != G_BORDERWIDTH)
    fprintf (fout, "\tborderwidth:\t%d\n", g->borderwidth);

  if (g->x != G_X)
    fprintf (fout, "\tx:\t%d\n", g->x);
  if (g->y != G_Y)
    fprintf (fout, "\ty:\t%d\n", g->y);

  if (g->folding != G_FOLDING)
    fprintf (fout, "\tfolding:\t%d\n", g->folding);

  if (g->shrink != G_SHRINK)
    fprintf (fout, "\tshrink:\t%d\n", g->shrink);
  if (g->stretch != G_STRETCH)
    fprintf (fout, "\tstretch:\t%d\n", g->stretch);

  if (g->textmode != G_TEXTMODE)
    fprintf (fout, "\ttextmode:\t%s\n",
	     get_textmode_str (g->textmode));

  if (g->shape != G_SHAPE)
    fprintf (fout, "\tshape:\t%s\n", get_shape_str (g->shape));

  if (g->vertical_order != G_VERTICAL_ORDER)
    fprintf (fout, "\tvertical_order:\t%d\n", g->vertical_order);
  if (g->horizontal_order != G_HORIZONTAL_ORDER)
    fprintf (fout, "\thorizontal_order:\t%d\n", g->horizontal_order);

  if (g->xmax != G_XMAX)
    fprintf (fout, "\txmax:\t%d\n", g->xmax);
  if (g->ymax != G_YMAX)
    fprintf (fout, "\tymax:\t%d\n", g->ymax);

  if (g->xbase != G_XBASE)
    fprintf (fout, "\txbase:\t%d\n", g->xbase);
  if (g->ybase != G_YBASE)
    fprintf (fout, "\tybase:\t%d\n", g->ybase);

  if (g->xspace != G_XSPACE)
    fprintf (fout, "\txspace:\t%d\n", g->xspace);
  if (g->yspace != G_YSPACE)
    fprintf (fout, "\tyspace:\t%d\n", g->yspace);
  if (g->xlspace != G_XLSPACE)
    fprintf (fout, "\txlspace:\t%d\n", g->xlspace);

  if (g->xraster != G_XRASTER)
    fprintf (fout, "\txraster:\t%d\n", g->xraster);
  if (g->yraster != G_YRASTER)
    fprintf (fout, "\tyraster:\t%d\n", g->yraster);
  if (g->xlraster != G_XLRASTER)
    fprintf (fout, "\txlraster:\t%d\n", g->xlraster);

  if (g->hidden != G_HIDDEN)
    fprintf (fout, "\thidden:\t%d\n", g->hidden);

  /* FIXME: Unallocate struct list if required.
     Maybe with a little function.  */
  if (g->classname != G_CLASSNAME)
    {
      struct classname *ite;

      for (ite = g->classname; ite; ite = ite->next)
	fprintf (fout, "\tclassname %d :\t%s\n", ite->no, ite->name);
    }

  if (g->infoname != G_INFONAME)
    {
      struct infoname *ite;

      for (ite = g->infoname; ite; ite = ite->next)
	fprintf (fout, "\tinfoname %d :\t%s\n", ite->integer, ite->chars);
    }

  if (g->colorentry != G_COLORENTRY)
    {
      struct colorentry *ite;

      for (ite = g->colorentry; ite; ite = ite->next)
	{
	  fprintf (fout, "\tcolorentry %d :\t%d %d %d\n",
		   ite->color_index,
		   ite->red_cp,
		   ite->green_cp,
		   ite->blue_cp);
	}
    }

  if (g->layout_downfactor != G_LAYOUT_DOWNFACTOR)
    fprintf (fout, "\tlayout_downfactor:\t%d\n", g->layout_downfactor);
  if (g->layout_upfactor != G_LAYOUT_UPFACTOR)
    fprintf (fout, "\tlayout_upfactor:\t%d\n", g->layout_upfactor);
  if (g->layout_nearfactor != G_LAYOUT_NEARFACTOR)
    fprintf (fout, "\tlayout_nearfactor:\t%d\n", g->layout_nearfactor);
  if (g->layout_splinefactor != G_LAYOUT_SPLINEFACTOR)
    fprintf (fout, "\tlayout_splinefactor:\t%d\n",
	     g->layout_splinefactor);

  if (g->late_edge_labels != G_LATE_EDGE_LABELS)
    fprintf (fout, "\tlate_edge_labels:\t%s\n",
	     get_decision_str (g->late_edge_labels));
  if (g->display_edge_labels != G_DISPLAY_EDGE_LABELS)
    fprintf (fout, "\tdisplay_edge_labels:\t%s\n",
	     get_decision_str (g->display_edge_labels));
  if (g->dirty_edge_labels != G_DIRTY_EDGE_LABELS)
    fprintf (fout, "\tdirty_edge_labels:\t%s\n",
	     get_decision_str (g->dirty_edge_labels));
  if (g->finetuning != G_FINETUNING)
    fprintf (fout, "\tfinetuning:\t%s\n",
	     get_decision_str (g->finetuning));
  if (g->ignore_singles != G_IGNORE_SINGLES)
    fprintf (fout, "\tignore_singles:\t%s\n",
	     get_decision_str (g->ignore_singles));
  if (g->priority_phase != G_PRIORITY_PHASE)
    fprintf (fout, "\tpriority_phase:\t%s\n",
	     get_decision_str (g->priority_phase));
  if (g->manhattan_edges != G_MANHATTAN_EDGES)
    fprintf (fout,
	     "\tmanhattan_edges:\t%s\n",
	     get_decision_str (g->manhattan_edges));
  if (g->smanhattan_edges != G_SMANHATTAN_EDGES)
    fprintf (fout,
	     "\tsmanhattan_edges:\t%s\n",
	     get_decision_str (g->smanhattan_edges));
  if (g->near_edges != G_NEAR_EDGES)
    fprintf (fout, "\tnear_edges:\t%s\n",
	     get_decision_str (g->near_edges));

  if (g->orientation != G_ORIENTATION)
    fprintf (fout, "\torientation:\t%s\n",
	     get_orientation_str (g->orientation));

  if (g->node_alignment != G_NODE_ALIGNMENT)
    fprintf (fout, "\tnode_alignment:\t%s\n",
	     get_node_alignment_str (g->node_alignment));

  if (g->port_sharing != G_PORT_SHARING)
    fprintf (fout, "\tport_sharing:\t%s\n",
	     get_decision_str (g->port_sharing));

  if (g->arrow_mode != G_ARROW_MODE)
    fprintf (fout, "\tarrow_mode:\t%s\n",
	     get_arrow_mode_str (g->arrow_mode));

  if (g->treefactor != G_TREEFACTOR)
    fprintf (fout, "\ttreefactor:\t%f\n", g->treefactor);
  if (g->spreadlevel != G_SPREADLEVEL)
    fprintf (fout, "\tspreadlevel:\t%d\n", g->spreadlevel);

  if (g->crossing_weight != G_CROSSING_WEIGHT)
    fprintf (fout, "\tcrossing_weight:\t%s\n",
	     get_crossing_type_str (g->crossing_weight));
  if (g->crossing_phase2 != G_CROSSING_PHASE2)
    fprintf (fout, "\tcrossing_phase2:\t%s\n",
	     get_decision_str (g->crossing_phase2));
  if (g->crossing_optimization != G_CROSSING_OPTIMIZATION)
    fprintf (fout, "\tcrossing_optimization:\t%s\n",
	     get_decision_str (g->crossing_optimization));

  if (g->view != normal_view)
    fprintf (fout, "\tview:\t%s\n", get_view_str (g->view));

  if (g->edges != G_EDGES)
    fprintf (fout, "\tedges:\t%s\n", get_decision_str (g->edges));

  if (g->nodes != G_NODES)
    fprintf (fout,"\tnodes:\t%s\n", get_decision_str (g->nodes));

  if (g->splines != G_SPLINES)
    fprintf (fout, "\tsplines:\t%s\n", get_decision_str (g->splines));

  if (g->bmax != G_BMAX)
    fprintf (fout, "\tbmax:\t%d\n", g->bmax);
  if (g->cmin != G_CMIN)
    fprintf (fout, "\tcmin:\t%d\n", g->cmin);
  if (g->cmax != G_CMAX)
    fprintf (fout, "\tcmax:\t%d\n", g->cmax);
  if (g->pmin != G_PMIN)
    fprintf (fout, "\tpmin:\t%d\n", g->pmin);
  if (g->pmax != G_PMAX)
    fprintf (fout, "\tpmax:\t%d\n", g->pmax);
  if (g->rmin != G_RMIN)
    fprintf (fout, "\trmin:\t%d\n", g->rmin);
  if (g->rmax != G_RMAX)
    fprintf (fout, "\trmax:\t%d\n", g->rmax);
  if (g->smax != G_SMAX)
    fprintf (fout, "\tsmax:\t%d\n", g->smax);
}
