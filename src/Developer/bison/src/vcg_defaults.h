/* VCG description handler for Bison.

   Copyright (C) 2001, 2002, 2005 Free Software Foundation, Inc.

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

#ifndef VCG_DEFAULTS_H_
# define VCG_DEFAULTS_H_

/* Graph defaults. */
# define G_TITLE		NULL
# define G_LABEL		NULL
# define G_INFOS1		NULL
# define G_INFOS2		NULL
# define G_INFOS3		NULL

# define G_COLOR		white
# define G_TEXTCOLOR		black
# define G_BORDERCOLOR		G_TEXTCOLOR

# define G_WIDTH		100
# define G_HEIGHT		100
# define G_BORDERWIDTH		2

# define G_X			0
# define G_Y			0

# define G_FOLDING		0

# define G_SHRINK		1
# define G_STRETCH		1

# define G_TEXTMODE		centered
# define G_SHAPE		box

# define G_VERTICAL_ORDER	0	/* Unspecified for subgraphs.  */
# define G_HORIZONTAL_ORDER	0	/* Unspecified for subgraphs.  */

# define G_XMAX			90	/* Not output.  */
# define G_YMAX			90	/* Not output.  */

# define G_XBASE		5
# define G_YBASE		5

# define G_XSPACE		20
# define G_YSPACE		70
# define G_XLSPACE		(G_XSPACE / 2)	/* Not output */

# define G_XRASTER		1
# define G_YRASTER		1
# define G_XLRASTER		1

# define G_HIDDEN		(-1)	/* No default value.  */

# define G_CLASSNAME		NULL	/* No class name association.  */
# define G_INFONAME		NULL
# define G_COLORENTRY		NULL

# define G_LAYOUTALGORITHM	normal
# define G_LAYOUT_DOWNFACTOR	1
# define G_LAYOUT_UPFACTOR	1
# define G_LAYOUT_NEARFACTOR	1
# define G_LAYOUT_SPLINEFACTOR	70

# define G_LATE_EDGE_LABELS	no
# define G_DISPLAY_EDGE_LABELS	no
# define G_DIRTY_EDGE_LABELS	no
# define G_FINETUNING		yes
# define G_IGNORE_SINGLES	no
# define G_LONG_STRAIGHT_PHASE	no
# define G_PRIORITY_PHASE	no
# define G_MANHATTAN_EDGES	no
# define G_SMANHATTAN_EDGES	no
# define G_NEAR_EDGES		yes

# define G_ORIENTATION		top_to_bottom
# define G_NODE_ALIGNMENT	center
# define G_PORT_SHARING		yes
# define G_ARROW_MODE		fixed
# define G_TREEFACTOR		0.5
# define G_SPREADLEVEL		1
# define G_CROSSING_WEIGHT	bary
# define G_CROSSING_PHASE2	yes
# define G_CROSSING_OPTIMIZATION	yes
# define G_VIEW			normal_view

# define G_EDGES		yes
# define G_NODES		yes
# define G_SPLINES		no

# define G_BMAX			100
# define G_CMIN			0
# define G_CMAX			(-1)	/* Infinity */
# define G_PMIN			0
# define G_PMAX			100
# define G_RMIN			0
# define G_RMAX			100
# define G_SMAX			100

# define G_NODE_LIST		NULL
# define G_EDGE_LIST		NULL

/* Nodes defaults. */
# define N_TITLE		NULL
# define N_LABEL		NULL

# define N_LOCX			(-1)	/* Default unspcified */
# define N_LOCY			(-1)	/* Default unspcified */

# define N_VERTICAL_ORDER	(-1)	/* Default unspcified */
# define N_HORIZONTAL_ORDER	(-1)	/* Default unspcified */

# define N_WIDTH		(-1)	/* We assume that we can't define it now. */
# define N_HEIGHT		(-1)	/* also. */

# define N_SHRINK		1
# define N_STRETCH		1

# define N_FOLDING		(-1)	/* no explicit default value. */

# define N_SHAPE		box
# define N_TEXTMODE		centered
# define N_BORDERWIDTH		2

# define N_COLOR		white
# define N_TEXTCOLOR		black
# define N_BORDERCOLOR		N_TEXTCOLOR

# define N_INFOS1		NULL
# define N_INFOS2		NULL
# define N_INFOS3		NULL

# define N_NEXT			NULL

/* Edge defaults. */
# define E_EDGE_TYPE		normal_edge

# define E_SOURCENAME		NULL	/* Mandatory. */
# define E_TARGETNAME		NULL	/* Mandatory. */
# define E_LABEL		NULL

# define E_LINESTYLE		continuous
# define E_THICKNESS		2

# define E_CLASS		1

# define E_COLOR		black
# define E_TEXTCOLOR		E_COLOR
# define E_ARROWCOLOR		E_COLOR
# define E_BACKARROWCOLOR	E_COLOR

# define E_ARROWSIZE		10
# define E_BACKARROWSIZE	0

# define E_ARROWSTYLE		solid
# define E_BACKARROWSTYLE	none

# define E_PRIORITY		1

# define E_ANCHOR		(-1)

# define E_HORIZONTAL_ORDER	(-1)

# define E_NEXT			NULL

#endif /* not VCG_DEFAULTS_H_ */
