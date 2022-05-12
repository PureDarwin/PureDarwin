/* Parse command line arguments for bison.
   Copyright (C) 1984, 1986, 1989, 1992, 2000, 2001, 2002, 2003, 2004, 2005, 2006
   Free Software Foundation, Inc.

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

#ifndef GETARGS_H_
# define GETARGS_H_

/* flags set by % directives */

/* for -S */
extern char const *skeleton;

/* for -I */
extern char const *include;

extern bool debug_flag;			/* for -t */
extern bool defines_flag;		/* for -d */
extern bool graph_flag;			/* for -g */
extern bool locations_flag;
extern bool no_lines_flag;		/* for -l */
extern bool no_parser_flag;		/* for -n */
extern bool token_table_flag;		/* for -k */
extern bool yacc_flag;			/* for -y */

extern bool error_verbose;


/* GLR_PARSER is true if the input file says to use the GLR
   (Generalized LR) parser, and to output some additional information
   used by the GLR algorithm.  */

extern bool glr_parser;

/* PURE_PARSER is true if should generate a parser that is all pure
   and reentrant.  */

extern bool pure_parser;

/* NONDETERMINISTIC_PARSER is true iff conflicts are accepted.  This
   is used by the GLR parser, and might be used in BackTracking
   parsers too.  */

extern bool nondeterministic_parser;

/* --trace.  */
enum trace
  {
    trace_none      = 0,
    trace_scan      = 1 << 0,
    trace_parse     = 1 << 1,
    trace_resource  = 1 << 2,
    trace_sets      = 1 << 3,
    trace_bitsets   = 1 << 4,
    trace_tools     = 1 << 5,
    trace_automaton = 1 << 6,
    trace_grammar   = 1 << 7,
    trace_time      = 1 << 8,
    trace_skeleton  = 1 << 9,
    trace_m4        = 1 << 10,
    trace_all       = ~0
  };
extern int trace_flag;

/* --report.  */
enum report
  {
    report_none             = 0,
    report_states           = 1 << 0,
    report_itemsets         = 1 << 1,
    report_look_ahead_tokens= 1 << 2,
    report_solved_conflicts = 1 << 3,
    report_all              = ~0
  };
extern int report_flag;

void getargs (int argc, char *argv[]);

#endif /* !GETARGS_H_ */
