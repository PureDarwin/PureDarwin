/* Parse command line arguments for Bison.

   Copyright (C) 1984, 1986, 1989, 1992, 2000, 2001, 2002, 2003, 2004,
   2005, 2006 Free Software Foundation, Inc.

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

#include <config.h>
#include "system.h"

#include <argmatch.h>
#include <error.h>

/* Hack to get <getopt.h> to declare getopt with a prototype.  */
#if lint && ! defined __GNU_LIBRARY__
# define __GNU_LIBRARY__
# define HACK_FOR___GNU_LIBRARY___PROTOTYPE 1
#endif

#include <getopt.h>

#ifdef HACK_FOR___GNU_LIBRARY___PROTOTYPE
# undef __GNU_LIBRARY__
# undef HACK_FOR___GNU_LIBRARY___PROTOTYPE
#endif

#include "complain.h"
#include "files.h"
#include "getargs.h"
#include "uniqstr.h"

bool debug_flag;
bool defines_flag;
bool graph_flag;
bool locations_flag;
bool no_lines_flag;
bool no_parser_flag;
bool token_table_flag;
bool yacc_flag;	/* for -y */

bool error_verbose = false;

bool nondeterministic_parser = false;
bool glr_parser = false;
bool pure_parser = false;

int report_flag = report_none;
int trace_flag = trace_none;

const char *skeleton = NULL;
const char *include = NULL;

extern char *program_name;


/*---------------------.
| --trace's handling.  |
`---------------------*/

static const char * const trace_args[] =
{
  /* In a series of synonyms, present the most meaningful first, so
     that argmatch_valid be more readable.  */
  "none       - no report",
  "scan       - grammar scanner traces",
  "parse      - grammar parser traces",
  "automaton  - contruction of the automaton",
  "bitsets    - use of bitsets",
  "grammar    - reading, reducing of the grammar",
  "resource   - memory consumption (where available)",
  "sets       - grammar sets: firsts, nullable etc.",
  "tools      - m4 invocation",
  "m4         - m4 traces",
  "skeleton   - skeleton postprocessing",
  "time       - time consumption",
  "all        - all of the above",
  0
};

static const int trace_types[] =
{
  trace_none,
  trace_scan,
  trace_parse,
  trace_automaton,
  trace_bitsets,
  trace_grammar,
  trace_resource,
  trace_sets,
  trace_tools,
  trace_m4,
  trace_skeleton,
  trace_time,
  trace_all
};

ARGMATCH_VERIFY (trace_args, trace_types);

static void
trace_argmatch (char *args)
{
  if (args)
    {
      args = strtok (args, ",");
      do
	{
	  int trace = XARGMATCH ("--trace", args,
				 trace_args, trace_types);
	  if (trace == trace_none)
	    trace_flag = trace_none;
	  else
	    trace_flag |= trace;
	}
      while ((args = strtok (NULL, ",")));
    }
  else
    trace_flag = trace_all;
}


/*----------------------.
| --report's handling.  |
`----------------------*/

static const char * const report_args[] =
{
  /* In a series of synonyms, present the most meaningful first, so
     that argmatch_valid be more readable.  */
  "none",
  "state", "states",
  "itemset", "itemsets",
  "look-ahead", "lookahead", "lookaheads",
  "solved",
  "all",
  0
};

static const int report_types[] =
{
  report_none,
  report_states, report_states,
  report_states | report_itemsets, report_states | report_itemsets,
  report_states | report_look_ahead_tokens,
  report_states | report_look_ahead_tokens,
  report_states | report_look_ahead_tokens,
  report_states | report_solved_conflicts,
  report_all
};

ARGMATCH_VERIFY (report_args, report_types);

static void
report_argmatch (char *args)
{
  args = strtok (args, ",");
  do
    {
      int report = XARGMATCH ("--report", args,
			      report_args, report_types);
      if (report == report_none)
	report_flag = report_none;
      else
	report_flag |= report;
    }
  while ((args = strtok (NULL, ",")));
}


/*-------------------------------------------.
| Display the help message and exit STATUS.  |
`-------------------------------------------*/

static void usage (int) ATTRIBUTE_NORETURN;

static void
usage (int status)
{
  if (status != 0)
    fprintf (stderr, _("Try `%s --help' for more information.\n"),
	     program_name);
  else
    {
      /* Some efforts were made to ease the translators' task, please
	 continue.  */
      fputs (_("\
GNU bison generates parsers for LALR(1) grammars.\n"), stdout);
      putc ('\n', stdout);

      fprintf (stdout, _("\
Usage: %s [OPTION]... FILE\n"), program_name);
      putc ('\n', stdout);

      fputs (_("\
If a long option shows an argument as mandatory, then it is mandatory\n\
for the equivalent short option also.  Similarly for optional arguments.\n"),
	     stdout);
      putc ('\n', stdout);

      fputs (_("\
Operation modes:\n\
  -h, --help                 display this help and exit\n\
  -V, --version              output version information and exit\n\
      --print-localedir      output directory containing locale-dependent data\n\
  -y, --yacc                 emulate POSIX yacc\n"), stdout);
      putc ('\n', stdout);

      fputs (_("\
Parser:\n\
  -S, --skeleton=FILE        specify the skeleton to use\n\
  -t, --debug                instrument the parser for debugging\n\
      --locations            enable locations computation\n\
  -p, --name-prefix=PREFIX   prepend PREFIX to the external symbols\n\
  -l, --no-lines             don't generate `#line' directives\n\
  -n, --no-parser            generate the tables only\n\
  -k, --token-table          include a table of token names\n\
"), stdout);
      putc ('\n', stdout);

      fputs (_("\
Output:\n\
  -d, --defines              also produce a header file\n\
  -r, --report=THINGS        also produce details on the automaton\n\
  -v, --verbose              same as `--report=state'\n\
  -b, --file-prefix=PREFIX   specify a PREFIX for output files\n\
  -o, --output=FILE          leave output to FILE\n\
  -g, --graph                also produce a VCG description of the automaton\n\
"), stdout);
      putc ('\n', stdout);

      fputs (_("\
THINGS is a list of comma separated words that can include:\n\
  `state'        describe the states\n\
  `itemset'      complete the core item sets with their closure\n\
  `look-ahead'   explicitly associate look-ahead tokens to items\n\
  `solved'       describe shift/reduce conflicts solving\n\
  `all'          include all the above information\n\
  `none'         disable the report\n\
"), stdout);
      putc ('\n', stdout);

      fputs (_("\
Report bugs to <bug-bison@gnu.org>.\n"), stdout);
    }

  exit (status);
}


/*------------------------------.
| Display the version message.  |
`------------------------------*/

static void
version (void)
{
  /* Some efforts were made to ease the translators' task, please
     continue.  */
  printf (_("bison (GNU Bison) %s"), VERSION);
  putc ('\n', stdout);
  fputs (_("Written by Robert Corbett and Richard Stallman.\n"), stdout);
  putc ('\n', stdout);

  fprintf (stdout,
	   _("Copyright (C) %d Free Software Foundation, Inc.\n"), 2006);

  fputs (_("\
This is free software; see the source for copying conditions.  There is NO\n\
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n\
"),
	 stdout);
}


/*----------------------.
| Process the options.  |
`----------------------*/

/* Shorts options.  */
static char const short_options[] = "yvegdhr:ltknVo:b:p:S:T::";

/* Values for long options that do not have single-letter equivalents.  */
enum
{
  LOCATIONS_OPTION = CHAR_MAX + 1,
  PRINT_LOCALEDIR_OPTION
};

static struct option const long_options[] =
{
  /* Operation modes. */
  { "help",            no_argument,	0,   'h' },
  { "version",         no_argument,	0,   'V' },
  { "print-localedir", no_argument,	0,   PRINT_LOCALEDIR_OPTION },

  /* Parser. */
  { "name-prefix",   required_argument,	  0,   'p' },
  { "include",       required_argument,   0,   'I' },

  /* Output. */
  { "file-prefix", required_argument,	0,   'b' },
  { "output",	   required_argument,	0,   'o' },
  { "output-file", required_argument,	0,   'o' },
  { "graph",	   optional_argument,	0,   'g' },
  { "report",	   required_argument,   0,   'r' },
  { "verbose",	   no_argument,	        0,   'v' },

  /* Hidden. */
  { "trace",         optional_argument,   0,     'T' },

  /* Output.  */
  { "defines",     optional_argument,   0,   'd' },

  /* Operation modes.  */
  { "fixed-output-files", no_argument,  0,   'y' },
  { "yacc",	          no_argument,  0,   'y' },

  /* Parser.  */
  { "debug",	      no_argument,               0,   't' },
  { "locations",      no_argument,		 0, LOCATIONS_OPTION },
  { "no-lines",       no_argument,               0,   'l' },
  { "no-parser",      no_argument,               0,   'n' },
  { "raw",            no_argument,               0,     0 },
  { "skeleton",       required_argument,         0,   'S' },
  { "token-table",    no_argument,               0,   'k' },

  {0, 0, 0, 0}
};

/* Under DOS, there is no difference on the case.  This can be
   troublesome when looking for `.tab' etc.  */
#ifdef MSDOS
# define AS_FILE_NAME(File) (strlwr (File), (File))
#else
# define AS_FILE_NAME(File) (File)
#endif

void
getargs (int argc, char *argv[])
{
  int c;

  while ((c = getopt_long (argc, argv, short_options, long_options, NULL))
	 != -1)
    switch (c)
      {
      case 0:
	/* Certain long options cause getopt_long to return 0.  */
	break;

      case 'y':
	yacc_flag = true;
	break;

      case 'h':
	usage (EXIT_SUCCESS);

      case 'V':
	version ();
	exit (EXIT_SUCCESS);

      case PRINT_LOCALEDIR_OPTION:
	printf ("%s\n", LOCALEDIR);
	exit (EXIT_SUCCESS);

      case 'g':
	/* Here, the -g and --graph=FILE options are differentiated.  */
	graph_flag = true;
	if (optarg)
	  spec_graph_file = AS_FILE_NAME (optarg);
	break;

      case 'v':
	report_flag |= report_states;
	break;

      case 'S':
	skeleton = AS_FILE_NAME (optarg);
	break;

      case 'I':
	include = AS_FILE_NAME (optarg);
	break;

      case 'd':
	/* Here, the -d and --defines options are differentiated.  */
	defines_flag = true;
	if (optarg)
	  spec_defines_file = AS_FILE_NAME (optarg);
	break;

      case 'l':
	no_lines_flag = true;
	break;

      case LOCATIONS_OPTION:
	locations_flag = true;
	break;

      case 'k':
	token_table_flag = true;
	break;

      case 'n':
	no_parser_flag = true;
	break;

      case 't':
	debug_flag = true;
	break;

      case 'o':
	spec_outfile = AS_FILE_NAME (optarg);
	break;

      case 'b':
	spec_file_prefix = AS_FILE_NAME (optarg);
	break;

      case 'p':
	spec_name_prefix = optarg;
	break;

      case 'r':
	report_argmatch (optarg);
	break;

      case 'T':
	trace_argmatch (optarg);
	break;

      default:
	usage (EXIT_FAILURE);
      }

  if (argc - optind != 1)
    {
      if (argc - optind < 1)
	error (0, 0, _("missing operand after `%s'"), argv[argc - 1]);
      else
	error (0, 0, _("extra operand `%s'"), argv[optind + 1]);
      usage (EXIT_FAILURE);
    }

  current_file = grammar_file = uniqstr_new (argv[optind]);
}
