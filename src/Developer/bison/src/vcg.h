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

#ifndef VCG_H_
# define VCG_H_

/* VCG color map. The 32 prime predefined colors. */
enum color
{
  white		= 0,
  blue,
  red,
  green		= 3,
  yellow,
  magenta,
  cyan		= 6,
  darkgrey,
  darkblue,
  darkred	= 9,
  darkgreen,
  darkyellow,
  darkmagenta	= 12,
  darkcyan,
  gold,
  lightgrey	= 15,
  lightblue,
  lightred,
  lightgreen	= 18,
  lightyellow,
  lightmagenta,
  lightcyan	= 21,
  lilac,
  turquoise,
  aquamarine	= 24,
  khaki,
  purple,
  yellowgreen	= 27,
  pink,
  orange,
  orchid,
  black		= 31
};

/* VCG textmode. Specify the adjustement of the text within the border of a summary node. */
enum textmode
{
  centered,
  left_justify,
  right_justify
};

/* VCG shapes. Used for nodes shapes. */
enum shape
{
  box,
  rhomb,
  ellipse,
  triangle
};

/* Structure for colorentries.  */
struct colorentry
{
  int color_index;
  int red_cp;
  int green_cp;
  int blue_cp;
  struct colorentry *next;
};

/* Structure to construct lists of classnames. */
struct classname
{
  int no; /* Class number */
  const char *name; /* Name associated to the class no. */
  struct classname *next; /* next name class association. */
};

/* Structure is in infoname.  */
struct infoname
{
  int integer;
  char const *chars;
  struct infoname *next;
};

/* VCG decision yes/no. */
enum decision
{
  yes,
  no
};

/* VCG graph orientation. */
enum orientation
{
  top_to_bottom,
  bottom_to_top,
  left_to_right,
  right_to_left
};

/* VCG alignment for node alignement. */
enum alignment
{
  center,
  top,
  bottom
};

/* VCG arrow mode. */
enum arrow_mode
{
  fixed,
  free_a
};

/* VCG crossing weight type. */
enum crossing_type
{
  bary,
  median,
  barymedian,
  medianbary
};

/* VCG views. */
enum view
{
  normal_view,
  cfish,
  pfish,
  fcfish,
  fpfish
};

/*------------------------------------------------------.
| Node attributs list. structure that describes a node. |
`------------------------------------------------------*/

struct node
{
  /* Title the unique string identifying the node. This attribute is
     mandatory. */
  const char *title;

  /* Label the text displayed inside the node. If no label is specified
     then the title of the node will be used. Note that this text may
     contain control characters like NEWLINE that influences the size of
     the node. */
  const char *label;

  /* loc is the location as x, y position relatively to the system of
     coordinates of the graph. Locations are specified in the form
     loc: - x: xpos y: ypos "". The locations of nodes are only valid,
     if the whole graph is fully specified with locations and no part is
     folded. The layout algorithm of the tool calculates appropriate x, y
     positions, if at least one node that must be drawn (i.e., is not
     hidden by folding or edge classes) does not have fixed specified
     locations.
     Default is none. */
  int locx;
  int locy;

  /* vertical order is the level position (rank) of the node. We can also
     specify level: int. Level specifications are only valid, if the
     layout is calculated, i.e. if at least one node does not have a
     fixed location specification. The layout algorithm partitioned all
     nodes into levels 0...maxlevel. Nodes at the level 0 are on the
     upper corner. The algorithm is able to calculate appropriate levels
     for the nodes automatically, if no fixed levels are given.
     Specifications of levels are additional constraints, that may be
     ignored, if they are in conflict with near edge specifications.
     Default values are unspecified. */
  int vertical_order;

  /* horizontal order is the horizontal position of the node within a
     level. The nodes which are specified with horizontal positions are
     ordered according to these positions within the levels. The nodes
     which do not have this attribute are inserted into this ordering by
     the crossing reduction mechanism. Note that connected components are
     handled separately, thus it is not possible to intermix such
     components by specifying a horizontal order. If the algorithm for
     downward laid out trees is used, the horizontal order influences
     only the order of the child nodes at a node, but not the order of
     the whole level.
     Default is unspecified. */
  int horizontal_order;

  /* width, height is the width and height of a node including the border.
     If no value (in pixels) is given then width and height are
     calculated from the size of the label.
     Default are width and height of the label. */
  int width;
  int height;

  /* shrink, stretch gives the shrinking and stretching factor of the
     node. The values of the attributes width, height, borderwidth and
     the size of the label text is scaled by ((stretch=shrink) \Lambda
     100) percent. Note that the actual scale value is determined by the
     scale value of a node relatively to a scale value of the graph,
     i.e. if (stretch,shrink) = (2,1) for the graph and (stretch,shrink)
     = (2,1) for the node of the graph, then the node is scaled by the
     factor 4 compared to the normal size. The scale value can also be
     specified by scaling: float.
     Default are 1,1. */
  int shrink;
  int stretch;

  /* folding specifies the default folding of the nodes. The folding k
     (with k ? 0) means that the graph part that is reachable via edges
     of a class less or equal to k is folded and displayed as one node.
     There are commands to unfold such summary nodes, see section 5. If
     no folding is specified for a node, then the node may be folded if
     it is in the region of another node that starts the folding. If
     folding 0 is specified, then the node is never folded. In this case
     the folding stops at the predecessors of this node, if it is
     reachable from another folding node. The summary node inherits some
     attributes from the original node which starts the folding (all
     color attributes, textmode and label, but not the location). A
     folded region may contain folded regions with smaller folding class
     values (nested foldings). If there is more than one node that start
     the folding of the same region (this implies that the folding class
     values are equal) then the attributes are inherited by one of these
     nodes nondeterministically. If foldnode attributes are specified,
     then the summary node attributes are inherited from these attributes.
     Default is none. */
  int folding;

  /* shape specifies the visual appearance of a node: box, rhomb, ellipse,
     and triangle. The drawing of ellipses is much slower than the drawing
     of the other shapes.
     Default is box. */
  enum shape shape;

  /* textmode specifies the adjustment of the text within the border of a
     node. The possibilities are center, left.justify and right.justify.
     Default is center. */
  enum textmode textmode;

  /* borderwidth specifies the thickness of the node's border in pixels.
     color is the background color of the node. If none is given, the
     node is white. For the possibilities, see the attribute color for
     graphs.
     Default is 2. */
  int borderwidth;

  /* node color.
     Default is white or transparent, */
  enum color color;

  /* textcolor is the color for the label text. bordercolor is the color
     of the border. Default color is the textcolor. info1, info2, info3
     combines additional text labels with a node or a folded graph. info1,
     Default is black. */
  enum color textcolor;

  /* info2, info3 can be selected from the menu. The corresponding text
     labels can be shown by mouse clicks on nodes.
     Default are null strings. */
  const char *infos[3];

  /* Node border color.
     Default is textcolor. */
  enum color bordercolor;

  /* Next node node... */
  struct node *next;
};

/* typedef alias. */
typedef struct node node;

/*-------------------------------------------------------.
| Edge attributs list. Structure that describes an edge. |
`-------------------------------------------------------*/

/* VCG Edge type. */
enum edge_type
{
  normal_edge,
  back_edge,
  near_edge,
  bent_near_edge
};

/* Structs enum definitions for edges. */
enum linestyle
{
  continuous,
  dashed,
  dotted,
  invisible
};

enum arrowstyle
{
  solid,
  line,
  none
};

/* The struct edge itself. */
struct edge
{

  /* Edge type.
     Default is normal edge. */
  enum edge_type type;

  /* Sourcename is the title of the source node of the edge.
     Default: none. */
  const char *sourcename; /* Mandatory. */

  /* Targetname is the title of the target node of the edge.
     Default: none. */
  const char *targetname; /* Mandatory. */

  /* Label specifies the label of the edge. It is drawn if
     display.edge.labels is set to yes.
     Default: no label. */
  const char *label;

  /* Linestyle specifies the style the edge is drawn. Possibilities are:
     ffl continuous a solid line is drawn ( -- ) ffl dashed the edge
     consists of single dashes ( - - - ) ffl dotted the edge is made of
     single dots ( \Delta  \Delta  \Delta  ) ffl invisible the edge is not
     drawn. The attributes of its shape (color, thickness) are ignored.
     To draw a dashed or dotted line needs more time than solid lines.
     Default is continuous. */
  enum linestyle linestyle;

  /* Thickness is the thickness of an edge.
     Default is 2. */
  int thickness;

  /* Class specifies the folding class of the edge. Nodes reachable by
     edges of a class less or equal to a constant k specify folding
     regions of k. See the node attribute folding and the folding commands.
     Default is 1. */
  int class;

  /* color is the color of the edge.
     Default is black. */
  enum color color;

  /* textcolor is the color of the label of the edge. arrowcolor,
     backarrowcolor is the color of the arrow head and of the backarrow
     head. priority The positions of the nodes are mainly determined by
     the incoming and outgoing edges. One can think of rubberbands instead
     of edges that pull a node into its position. The priority of an edges
     corresponds to the strength of the rubberband.
     Default is color. */
  enum color textcolor;

  /* Arrow color.
     Default is color. */
  enum color arrowcolor;

  /* BackArrow color.
     Default is color. */
  enum color backarrowcolor;

  /* arrowsize, backarrowsize The arrow head is a right-angled, isosceles
     triangle and the cathetuses have length arrowsize.
     Default is 10. */
  int arrowsize;

  /* Backarrow size
     Default is 0. */
  int backarrowsize;

  /* arrowstyle, backarrowstyle Each edge has two arrow heads: the one
     appears at the target node (the normal arrow head), the other appears
     at the source node (the backarrow head). Normal edges only have the
     normal solid arrow head, while the backarrow head is not drawn, i.e.
     it is none. Arrowstyle is the style of the normal arrow head, and
     backarrowstyle is the style of the backarrow head. Styles are none,
     i.e. no arrow head, solid, and line.
     Default is solid. */
  enum arrowstyle arrowstyle;

  /* Default is none. */
  enum arrowstyle backarrowstyle;

  /* Default is 1. */
  int priority;

  /* Anchor. An anchor point describes the vertical position in a node
     where an edge goes out. This is useful, if node labels are several
     lines long, and outgoing edges are related to label lines. (E.g.,
     this allows a nice visualization of structs containing pointers as
     fields.).
     Default is none. */
  int anchor;

  /* Horizontal order is the horizontal position the edge. This is of
     interest only if the edge crosses several levels because it specifies
     the point where the edge crosses the level. within a level. The nodes
     which are specified with horizontal positions are ordered according
     to these positions within a level. The horizontal position of a long
     edge that crosses the level specifies between which two node of that
     level the edge has to be drawn. Other edges which do not have this
     attribute are inserted into this ordering by the crossing reduction
     mechanism. Note that connected components are handled separately,
     thus it is not possible to intermix such components by specifying a
     horizontal order.
     Default is unspcified. */
  int horizontal_order;

  /*
  ** Next edge node...
  */
  struct edge *next;

};

/*
** typedef alias.
*/
typedef struct edge edge;

/*--------------------------------------------------------.
| Graph attributs list. Structure that describes a graph. |
`--------------------------------------------------------*/

struct graph
{
  /* Graph title or name.
     Title specifies the name (a string) associated with the graph. The
     default name of a subgraph is the name of the outer graph, and the
     name of the outmost graph is the name of the specification input
     file. The name of a graph is used to identify this graph, e.g., if
     we want to express that an edge points to a subgraph. Such edges
     point to the root of the graph, i.e. the first node of the graph or
     the root of the first subgraph in the graph, if the subgraph is
     visualized explicitly.
     By default, it's the name of the vcg graph file description. */
  const char *title;

  /* Graph label.
     Label the text displayed inside the node, when the graph is folded
     to a node. If no label is specified then the title of the graph will
     be used. Note that this text may contain control characters like
     NEWLINE that influences the size of the node.
     By default, it takes the title value */
  const char *label;

  /* Any informations.
     Info1, info2, info3 combines additional text labels with a node or a
     folded graph. info1, info2, info3 can be selected from the menu
     interactively. The corresponding text labels can be shown by mouse
     clicks on nodes.
     Default values are empty strings (here NULL pointers) */
  const char *infos[3];

  /* Background color and summary node colors
     Color specifies the background color for the outermost graph, or the
     color of the summary node for subgraphs. Colors are given in the enum
     declared above. If more than these default colors are needed, a
     color map with maximal 256 entries can be used. The first 32 entries
     correspond to the colors just listed. A color of the color map can
     selected by the color map index, an integer, for instance red has
     index 2, green has index 3, etc.
     Default is white for background and white or transparent for summary
     nodes. */
  enum color color;

  /* Textcolor.
     need explanations ???
     default is black for summary nodes. */
  enum color textcolor;

  /* Bordercolor is the color of the summary node's border. Default color
     is the textcolor. width, height are width and height of the
     displayed part of the window of the outermost graph in pixels, or
     width and height of the summary node of inner subgraphs.
     Default is the default of the textcolor. */
  enum color bordercolor;

  /* Width, height are width and height of the displayed part of the
     window of the outermost graph in pixels, or width and height of the
     summary node of inner subgraphs.
     Default value is 100. */
  int width;
  int height;

  /* Specify the thickness if summary node's border in pixels.
     default value is 2. */
  int borderwidth;

  /* x, y are the x-position and y-position of the graph's window in
     pixels, relatively to the root screen, if it is the outermost graph.
     The origin of the window is upper, left hand. For inner subgraphs,
     it is the position of the folded summary node. The position can also
     be specified in the form loc: fx:int y:intg.
     The default value is 0. */
  int x;
  int y;

  /* folding of a subgraph is 1, if the subgraph is fused, and 0, if the
     subgraph is visualized explicitly. There are commands to unfold such
     summary nodes.
     Default value is 0 */
  int folding;

  /* Shrink, stretch gives the shrinking and stretching factor for the
     graph's representation (default is 1, 1). ((stretch=shrink) \Lambda
     100) is the scaling of the graph in percentage, e.g.,
     (stretch,shrink) = (1,1) or (2,2) or (3,3) : : : is normal size,
     (stretch,shrink) = (1,2) is half size, (stretch,shrink) = (2,1) is
     double size. For subgraphs, it is also the scaling factor of the
     summary node. The scaling factor can also be specified by scaling:
     float (here, scaling 1.0 means normal size). */
  int shrink;
  int stretch;

  /* textmode specifies the adjustment of the text within the border of a
     summary node. The possibilities are center, left.justify and
     right.justify.
     Default value is center.*/
  enum textmode textmode;

  /* Shape can be specified for subgraphs only. It is the shape of the
     subgraph summary node that appears if the subgraph is folded: box,
     rhomb, ellipse, and triangle. vertical order is the level position
     (rank) of the summary node of an inner subgraph, if this subgraph is
     folded. We can also specify level: int. The level is only
     recognized, if an automatical layout is calculated. horizontal order
     is the horizontal position of the summary node within a level. The
     nodes which are specified with horizontal positions are ordered
     according to these positions within the levels. The nodes which do
     not have this attribute are inserted into this ordering by the
     crossing reduction mechanism. Note that connected
     components are handled separately, thus it is not possible to
     intermix such components by specifying a horizontal order. If the
     algorithm for downward laid out trees is used, the horizontal order
     influences only the order of the child nodes at a node, but not the
     order of the whole level.
     Default is box, other: rhomb, ellipse, triangle. */
  enum shape shape;

  /* Vertical order is the level position (rank) of the summary node of an
     inner subgraph, if this subgraph is folded. We can also specify
     level: int. The level is only recognized, if an automatical layout is
     calculated.  */
  int vertical_order;

  /* Horizontal order is the horizontal position of the summary node within
     a level. The nodes which are specified with horizontal positions are
     ordered according to these positions within the levels. The nodes which
     do not have this attribute are inserted into this ordering by the
     crossing reduction mechanism. Note that connected components are
     handled separately, thus it is not possible to intermix such components
     by specifying a horizontal order. If the algorithm for downward laid
     out trees is used, the horizontal order influences only the order of
     the child nodes at a node, but not the order of the whole level.  */
  int horizontal_order;

  /* xmax, ymax specify the maximal size of the virtual window that is
     used to display the graph. This is usually larger than the displayed
     part, thus the width and height of the displayed part cannot be
     greater than xmax and ymax. Only those parts of the graph are drawn
     that are inside the virtual window. The virtual window can be moved
     over the potential infinite system of coordinates by special
     positioning commands.
     Defaults are 90 and 90. */
  int xmax;
  int ymax;

  /* xy-base: specify the upper left corner coordinates of the graph
     relatively to the root window.
     Defaults are 5, 5. */
  int xbase;
  int ybase;

  /* xspace, yspace the minimum horizontal and vertical distance between
     nodes. xlspace is the horizontal distance between lines at the
     points where they cross the levels. (At these points, dummy nodes
     are used. In fact, this is the horizontal distance between dummy
     nodes.) It is recommended to set xlspace to a larger value, if
     splines are used to draw edges, to prevent sharp bendings.
     Default are 20 and 70. */
  int xspace;
  int yspace;

  /* The horizontal space between lines at the point where they cross
     the levels.
     defaults value is 1/2 xspace (polygone) and 4/5 xspace (splines)*/
  int xlspace;

  /* xraster, yraster specifies the raster distance for the position of
     the nodes. The center of a node is aligned to this raster. xlraster
     is the horizontal raster for the positions of the line control
     points (the dummy nodes). It should be a divisor of xraster.
     defaults are 1,1. */
  int xraster;
  int yraster;

  /* xlraster is the horizontal raster for the positions of the line
     control points (the dummy nodes). It should be a divisor of xraster.
     defaults is 1. */
  int xlraster;

  /* hidden specifies the classes of edges that are hidden.
     Edges that are within such a class are not laid out nor drawn.
     Nodes that are only reachable (forward or backward) by edges of an
     hidden class are not drawn. However, nodes that are not reachable
     at all are drawn. (But see attribute ignore.singles.) Specification
     of classes of hidden edges allows to hide parts of a graph, e.g.,
     annotations of a syntax tree. This attribute is only allowed at the
     outermost level. More than one settings are possible to specify
     exactly the set of classes that are hidden. Note the important
     difference between hiding of edges and the edge line style invisible.
     Hidden edges are not existent in the layout. Edges with line style
     invisible are existent in the layout; they need space and may
     produce crossings and influence the layout, but you cannot see
     them.
     No default value. */
  int hidden;

  /* Classname allows to introduce names for the edge classes. The names
     are used in the menus. infoname allows to introduce names for the
     additional text labels. The names are used in the menus.
     defaults are 1,2,3...
     By default, no class names. */
  struct classname *classname;

  /* Infoname allows to introduce names for the additional text labels.
     The names are used in the menus.
     Infoname is given by an integer and a string.
     The default value is NULL.  */
  struct infoname *infoname;

  /* Colorentry allows to fill the color map. A color is a triplet of integer
     values for the red/green/blue-part. Each integer is between 0 (off) and
     255 (on), e.g., 0 0 0 is black and 255 255 255 is white. For instance
     colorentry 75 : 70 130 180 sets the map entry 75 to steel blue. This
     color can be used by specifying just the number 75.
     Default id NULL.  */
  struct colorentry *colorentry;

  /* Layout downfactor, layout upfactor, layout nearfactor The layout
     algorithm partitions the set of edges into edges pointing upward,
     edges pointing downward, and edges pointing sidewards. The last type
     of edges is also called near edges. If the layout.downfactor is
     large compared to the layout.upfactor and the layout.nearfactor,
     then the positions of the nodes is mainly determined by the edges
     pointing downwards. If the layout.upfactor is large compared to the
     layout.downfactor and the layout.nearfactor, then the positions of
     the nodes is mainly determined by the edges pointing upwards. If the
     layout.nearfactor is large, then the positions of the nodes is
     mainly determined by the edges pointing sidewards. These attributes
     have no effect, if the method for downward laid out trees is used.
     Default is normal. */
  int layout_downfactor;
  int layout_upfactor;
  int layout_nearfactor;
  /* Layout splinefactor determines the bending at splines. The factor
     100 indicates a very sharp bending, a factor 1 indicates a very flat
     bending. Useful values are 30 : : : 80. */
  int layout_splinefactor;

  /* Late edge labels yes means that the graph is first partitioned and
     then, labels are introduced. The default algorithm first creates
     labels and then partitions the graph, which yield a more compact
     layout, but may have more crossings.
     Default is no. */
  enum decision late_edge_labels;

  /* Display edge labels yes means display labels and no means don't
     display edge labels.
     Default vaule is no. */
  enum decision display_edge_labels;

  /* Dirty edge labels yes enforces a fast layout of edge labels, which
     may very ugly because several labels may be drawn at the same place.
     Dirty edge labels cannot be used if splines are used.
     Default is no.
  */
  enum decision dirty_edge_labels;

  /* Finetuning no switches the fine tuning phase of the graph layout
     algorithm off, while it is on as default. The fine tuning phase
     tries to give all edges the same length.
     Default is yes. */
  enum decision finetuning;

  /* Ignore singles yes hides all nodes which would appear single and
     unconnected from the remaining graph. Such nodes have no edge at all
     and are sometimes very ugly. Default is to show all nodes.
     Default is no. */
  enum decision ignore_singles;

  /* priority phase yes replaces the normal pendulum method by a
     specialized method: It forces straight long edges with 90 degree,
     just as the straight phase. In fact, the straight phase is a fine
     tune phase of the priority method. This phase is also recommended,
     if an orthogonal layout is selected (see manhattan.edges).
     Default is no. */
  enum decision priority_phase;

  /* manhattan edges yes switches the orthogonal layout on. Orthogonal
     layout (or manhattan layout) means that all edges consist of line
     segments with gradient 0 or 90 degree. Vertical edge segments might
     by shared by several edges, while horizontal edge segments are never
     shared. This results in very aesthetical layouts just for flowcharts.
     If the orthogonal layout is used, then the priority phase and
     straight phase should be used. Thus, these both phases are switched
     on, too, unless priority layout and straight line tuning are
     switched off explicitly.
     Default is no. */
  enum decision manhattan_edges;

  /* Smanhattan edges yes switches a specialized orthogonal layout on:
     Here, all horizontal edge segments between two levels share the same
     horizontal line, i.e. not only vertical edge segments are shared,
     but horizontal edge segments are shared by several edges, too. This
     looks nice for trees but might be too confusing in general, because
     the location of an edge might be ambiguously.
     Default is no. */
  enum decision smanhattan_edges;

  /* Near edges no suppresses near edges and bent near edges in the
     graph layout.
     Default is yes. */
  enum decision near_edges;

  /* Orientation specifies the orientation of the graph: top.to.bottom,
     bottom.to.top, left.to.right or right.to.left. Note: the normal
     orientation is top.to.bottom. All explanations here are given
     relatively to the normal orientation, i.e., e.g., if the orientation
     is left to right, the attribute xlspace is not the horizontal but
     the vertical distance between lines, etc.
     Default is to_to_bottom. */
  enum orientation orientation;

  /* Node alignment specified the vertical alignment of nodes at the
     horizontal reference line of the levels. If top is specified, the
     tops of all nodes of a level have the same y-coordinate; on bottom,
     the bottoms have the same y-coordinate, on center the nodes are
     centered at the levels.
     Default is center. */
  enum alignment node_alignment;

  /* Port sharing no suppresses the sharing of ports of edges at the
     nodes. Normally, if multiple edges are adjacent to the same node,
     and the arrow head of all these edges has the same visual appearance
     (color, size, etc.), then these edges may share a port at a node,
     i.e. only one arrow head is draw, and all edges are incoming into
     this arrow head. This allows to have many edges adjacent to one node
     without getting confused by too many arrow heads. If no port sharing
     is used, each edge has its own port, i.e. its own place where it is
     adjacent to the node.
     Default is yes. */
  enum decision port_sharing;

  /* Arrow mode fixed (default) should be used, if port sharing is used,
     because then, only a fixed set of rotations for the arrow heads are
     used. If the arrow mode is free, then each arrow head is rotated
     individually to each edge. But this can yield to a black spot, where
     nothing is recognizable, if port sharing is used, since all these
     qdifferently rotated arrow heads are drawn at the same place. If the
     arrow mode is fixed, then the arrow head is rotated only in steps of
     45 degree, and only one arrow head occurs at each port.
     Default is fixed. */
  enum arrow_mode arrow_mode;

  /* Treefactor The algorithm tree for downward laid out trees tries to
     produce a medium dense, balanced tree-like layout. If the tree
     factor is greater than 0.5, the tree edges are spread, i.e. they
     get a larger gradient. This may improve the readability of the tree.
     Note: it is not obvious whether spreading results in a more dense or
     wide layout. For a tree, there is a tree factor such that the whole
     tree is minimal wide.
     Default is 0.5. */
  float treefactor;

  /* Spreadlevel This parameter only influences the algorithm tree, too.
     For large, balanced trees, spreading of the uppermost nodes would
     enlarge the width of the tree too much, such that the tree does not
     fit anymore in a window. Thus, the spreadlevel specifies the minimal
     level (rank) where nodes are spread. Nodes of levels upper than
     spreadlevel are not spread.
     Default is 1. */
  int spreadlevel;

  /* Crossing weight specifies the weight that is used for the crossing
     reduction: bary (default), median, barymedian or medianbary. We
     cannot give a general recommendation, which is the best method. For
     graphs with very large average degree of edges (number of incoming
     and outgoing edges at a node), the weight bary is the fastest
     method. With the weights barymedian and medianbary, equal weights of
     different nodes are not very probable, thus the crossing reduction
     phase 2 might be very fast.
     Default is bary. */
  enum crossing_type crossing_weight;

  /* Crossing phase2 is the most time consuming phase of the crossing
     reduction. In this phase, the nodes that happen to have equal
     crossing weights are permuted. By specifying no, this phase is
     suppressed.
     Default is yes. */
  enum decision crossing_phase2;

  /* Crossing optimization is a postprocessing phase after the normal
     crossing reduction: we try to optimize locally, by exchanging pairs
     of nodes to reduce the crossings. Although this phase is not very
     time consuming, it can be suppressed by specifying no.
     Default is yes. */
  enum decision crossing_optimization;

  /* View allows to select the fisheye views. Because
     of the fixed size of the window that shows the graph, we normally
     can only see a small amount of a large graph. If we shrink the graph
     such that it fits into the window, we cannot recognize any detail
     anymore. Fisheye views are coordinate transformations: the view onto
     the graph is distort, to overcome this usage deficiency. The polar
     fisheye is easy to explain: assume a projection of the plane that
     contains the graph picture onto a spheric ball. If we now look onto
     this ball in 3 D, we have a polar fisheye view. There is a focus
     point which is magnified such that we see all details. Parts of the
     plane that are far away from the focus point are demagnified very
     much. Cartesian fisheye have a similar effect; only the formula for
     the coordinate transformation is different. Selecting cfish means
     the cartesian fisheye is used which demagnifies such that the whole
     graph is visible (self adaptable cartesian fisheye). With fcfish,
     the cartesian fisheye shows the region of a fixed radius around the
     focus point (fixed radius cartesian fisheye). This region might be
     smaller than the whole graph, but the demagnification needed to show
     this region in the window is also not so large, thus more details
     are recognizable. With pfish the self adaptable polar fisheye is
     selected that shows the whole graph, and with fpfish the fixed
     radius polar fisheye is selected.
     Default is normal view.  */
  enum view view;

  /* Edges no suppresses the drawing of edges.
     Default is yes. */
  enum decision edges;

  /* Nodes no suppresses the drawing of nodes.
     Default is yes. */
  enum decision nodes;

  /* Splines specifies whether splines are used to draw edges (yes or no).
     As default, polygon segments are used to draw edges, because this is
     much faster. Note that the spline drawing routine is not fully
     validated, and is very slow. Its use is mainly to prepare high
     quality PostScript output for very small graphs.
     Default is no. */
  enum decision splines;

  /* Bmax set the maximal number of iterations that are done for the
     reduction of edge bendings.
   Default is 100. */
  int bmax;

  /* Cmin set the minimal number of iterations that are done for the
     crossing reduction with the crossing weights. The normal method
     stops if two consecutive checks does not reduce the number of
     crossings anymore. However, this increasing of the number of
     crossings might be locally, such that after some more iterations,
     the crossing number might decrease much more.
     Default is 0. */
  int cmin;

  /* Cmax set the maximal number of interactions for crossing reduction.
     This is helpful for speeding up the layout process.
     Default is -1, which represents infinity.  */
  int cmax;

  /* Pmin set the minimal number of iterations that is done with the
     pendulum method. Similar to the crossing reduction, this method
     stops if the `imbalancement weight' does not decreases anymore.
     However, the increasing of the imbalancement weight might be locally,
     such that after some more iterations, the imbalancement weight might
     decrease much more.
     Default is 0. */
  int pmin;

  /* Pmax set the maximal number of iterations of the pendulum method.
     This is helpful for speedup the layout process.
     Default is 100. */
  int pmax;

  /* Rmin set the minimal number of iterations that is done with the
     rubberband method. This is similar as for the pendulum method.
     Default is 0. */
  int rmin;

  /* Rmax set the maximal number of iterations of the rubberband method.
     This is helpful for speedup the layout process.
     Default is 100. */
  int rmax;

  /* Smax set the maximal number of iterations of the straight line
     recognition phase (useful only, if the straight line recognition
     phase is switched on, see attribute straight.phase).
     Default is 100. */
  int smax;

  /* Generic values.
   */
  node node;
  edge edge;

  /* List of nodes declared.
     Pointer. */
  node *node_list;

  /* List of edges declared.
     Pointer. */
  edge *edge_list;

};

/* Graph typedefs. */
typedef struct graph graph;

void new_graph (graph *g);
void new_node (node *n);
void new_edge (edge *e);

void add_node (graph *g, node *n);
void add_edge (graph *g, edge *e);

void add_colorentry (graph *g, int color_idx, int red_cp,
		     int green_cp, int blue_cp);
void add_classname (graph *g, int val, const char *name);
void add_infoname (graph *g, int val, const char *name);

void open_node (FILE *fout);
void output_node (node *n, FILE *fout);
void close_node (FILE *fout);

void open_edge (edge *e, FILE *fout);
void output_edge (edge *e, FILE *fout);
void close_edge (FILE *fout);

void open_graph (FILE *fout);
void output_graph (graph *g, FILE *fout);
void close_graph (graph *g, FILE *fout);

#endif /* VCG_H_ */
