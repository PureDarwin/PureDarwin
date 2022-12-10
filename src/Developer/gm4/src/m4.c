/* GNU m4 -- A simple macro processor

   Copyright (C) 1989, 1990, 1991, 1992, 1993, 1994, 2004, 2005, 2006 Free
   Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301  USA
*/

#include "m4.h"

#include <getopt.h>
#include <signal.h>

static void usage (int);

/* Operate interactively (-e).  */
static int interactive = 0;

/* Enable sync output for /lib/cpp (-s).  */
int sync_output = 0;

/* Debug (-d[flags]).  */
int debug_level = 0;

/* Hash table size (should be a prime) (-Hsize).  */
size_t hash_table_size = HASHMAX;

/* Disable GNU extensions (-G).  */
int no_gnu_extensions = 0;

/* Prefix all builtin functions by `m4_'.  */
int prefix_all_builtins = 0;

/* Max length of arguments in trace output (-lsize).  */
int max_debug_argument_length = 0;

/* Suppress warnings about missing arguments.  */
int suppress_warnings = 0;

/* If not zero, then value of exit status for warning diagnostics.  */
int warning_status = 0;

/* Artificial limit for expansion_level in macro.c.  */
int nesting_limit = 1024;

#ifdef ENABLE_CHANGEWORD
/* User provided regexp for describing m4 words.  */
const char *user_word_regexp = "";
#endif

/* Name of frozen file to digest after initialization.  */
const char *frozen_file_to_read = NULL;

/* Name of frozen file to produce near completion.  */
const char *frozen_file_to_write = NULL;

/* The name this program was run with. */
const char *program_name;

/* If non-zero, display usage information and exit.  */
static int show_help = 0;

/* If non-zero, print the version on standard output and exit.  */
static int show_version = 0;

struct macro_definition
{
  struct macro_definition *next;
  int code;			/* D, U or t */
  const char *macro;
};
typedef struct macro_definition macro_definition;

/* Error handling functions.  */

/*-----------------------.
| Wrapper around error.  |
`-----------------------*/

void
m4_error (int status, int errnum, const char *format, ...)
{
  va_list args;
  va_start (args, format);
  verror_at_line (status, errnum, current_line ? current_file : NULL,
                  current_line, format, args);
}

/*-------------------------------.
| Wrapper around error_at_line.  |
`-------------------------------*/

void
m4_error_at_line (int status, int errnum, const char *file, int line,
                  const char *format, ...)
{
  va_list args;
  va_start (args, format);
  verror_at_line (status, errnum, line ? file : NULL, line, format, args);
}

#ifdef USE_STACKOVF

/*---------------------------------------.
| Tell user stack overflowed and abort.	 |
`---------------------------------------*/

static void
stackovf_handler (void)
{
  M4ERROR ((EXIT_FAILURE, 0,
	    "ERROR: stack overflow.  (Infinite define recursion?)"));
}

#endif /* USE_STACKOV */


/*---------------------------------------------.
| Print a usage message and exit with STATUS.  |
`---------------------------------------------*/

static void
usage (int status)
{
  if (status != EXIT_SUCCESS)
    fprintf (stderr, "Try `%s --help' for more information.\n", program_name);
  else
    {
      printf ("Usage: %s [OPTION]... [FILE]...\n", program_name);
      fputs ("\
Process macros in FILEs.  If no FILE or if FILE is `-', standard input\n\
is read.\n\
", stdout);
      fputs ("\
\n\
Mandatory or optional arguments to long options are mandatory or optional\n\
for short options too.\n\
\n\
Operation modes:\n\
      --help                   display this help and exit\n\
      --version                output version information and exit\n\
  -E, --fatal-warnings         stop execution after first warning\n\
  -e, --interactive            unbuffer output, ignore interrupts\n\
  -P, --prefix-builtins        force a `m4_' prefix to all builtins\n\
  -Q, --quiet, --silent        suppress some warnings for builtins\n\
", stdout);
#ifdef ENABLE_CHANGEWORD
      fputs ("\
  -W, --word-regexp=REGEXP     use REGEXP for macro name syntax\n\
", stdout);
#endif
      fputs ("\
\n\
Preprocessor features:\n\
  -D, --define=NAME[=VALUE]    define NAME has having VALUE, or empty\n\
  -I, --include=DIRECTORY      append DIRECTORY to include path\n\
  -s, --synclines              generate `#line NUM \"FILE\"' lines\n\
  -U, --undefine=NAME          undefine NAME\n\
", stdout);
      fputs ("\
\n\
Limits control:\n\
  -G, --traditional            suppress all GNU extensions\n\
  -H, --hashsize=PRIME         set symbol lookup hash table size [509]\n\
  -L, --nesting-limit=NUMBER   change artificial nesting limit [1024]\n\
", stdout);
      fputs ("\
\n\
Frozen state files:\n\
  -F, --freeze-state=FILE      produce a frozen state on FILE at end\n\
  -R, --reload-state=FILE      reload a frozen state from FILE at start\n\
", stdout);
      fputs ("\
\n\
Debugging:\n\
  -d, --debug[=FLAGS]          set debug level (no FLAGS implies `aeq')\n\
  -l, --arglength=NUM          restrict macro tracing size\n\
  -o, --error-output=FILE      redirect debug and trace output\n\
  -t, --trace=NAME             trace NAME when it will be defined\n\
", stdout);
      fputs ("\
\n\
FLAGS is any of:\n\
  a   show actual arguments\n\
  c   show before collect, after collect and after call\n\
  e   show expansion\n\
  f   say current input file name\n\
  i   show changes in input files\n\
  l   say current input line number\n\
  p   show results of path searches\n\
  q   quote values as necessary, with a or e flag\n\
  t   trace for all macro calls, not only traceon'ed\n\
  x   add a unique macro call id, useful with c flag\n\
  V   shorthand for all of the above flags\n\
", stdout);
      fputs ("\
\n\
If defined, the environment variable `M4PATH' is a colon-separated list\n\
of directories included after any specified by `-I'.\n\
", stdout);
      fputs ("\
\n\
Exit status is 0 for success, 1 for failure, 63 for frozen file version\n\
mismatch, or whatever value was passed to the m4exit macro.\n\
", stdout);
      printf ("\nReport bugs to <%s>.\n", PACKAGE_BUGREPORT);
    }

  if (close_stream (stdout) != 0)
    M4ERROR ((EXIT_FAILURE, errno, "write error"));
  exit (status);
}

/*--------------------------------------.
| Decode options and launch execution.  |
`--------------------------------------*/

static const struct option long_options[] =
{
  {"arglength", required_argument, NULL, 'l'},
  {"debug", optional_argument, NULL, 'd'},
  {"diversions", required_argument, NULL, 'N'},
  {"error-output", required_argument, NULL, 'o'},
  {"fatal-warnings", no_argument, NULL, 'E'},
  {"freeze-state", required_argument, NULL, 'F'},
  {"hashsize", required_argument, NULL, 'H'},
  {"include", required_argument, NULL, 'I'},
  {"interactive", no_argument, NULL, 'e'},
  {"nesting-limit", required_argument, NULL, 'L'},
  {"prefix-builtins", no_argument, NULL, 'P'},
  {"quiet", no_argument, NULL, 'Q'},
  {"reload-state", required_argument, NULL, 'R'},
  {"silent", no_argument, NULL, 'Q'},
  {"synclines", no_argument, NULL, 's'},
  {"traditional", no_argument, NULL, 'G'},
  {"word-regexp", required_argument, NULL, 'W'},

  {"help", no_argument, &show_help, 1},
  {"version", no_argument, &show_version, 1},

  /* These are somewhat troublesome.  */
  { "define", required_argument, NULL, 'D' },
  { "undefine", required_argument, NULL, 'U' },
  { "trace", required_argument, NULL, 't' },

  { 0, 0, 0, 0 },
};

/* Global catchall for any errors that should affect final error status, but
   where we try to continue execution in the meantime.  */
int retcode;

#ifdef ENABLE_CHANGEWORD
#define OPTSTRING "B:D:EF:GH:I:L:N:PQR:S:T:U:W:d::el:o:st:"
#else
#define OPTSTRING "B:D:EF:GH:I:L:N:PQR:S:T:U:d::el:o:st:"
#endif

int
main (int argc, char *const *argv, char *const *envp)
{
  macro_definition *head;	/* head of deferred argument list */
  macro_definition *tail;
  macro_definition *new;
  int optchar;			/* option character */

  macro_definition *defines;
  FILE *fp;

  program_name = argv[0];
  retcode = EXIT_SUCCESS;

  include_init ();
  debug_init ();
#ifdef USE_STACKOVF
  setup_stackovf_trap (argv, envp, stackovf_handler);
#endif

  /* First, we decode the arguments, to size up tables and stuff.  */

  head = tail = NULL;

  while (optchar = getopt_long (argc, (char **) argv, OPTSTRING,
				long_options, NULL),
	 optchar != EOF)
    switch (optchar)
      {
      default:
	usage (EXIT_FAILURE);

      case 0:
	break;

      case 'B':			/* compatibility junk */
      case 'N':
      case 'S':
      case 'T':
	break;

      case 'D':
      case 'U':
      case 't':

	/* Arguments that cannot be handled until later are accumulated.  */

	new = (macro_definition *) xmalloc (sizeof (macro_definition));
	new->code = optchar;
	new->macro = optarg;
	new->next = NULL;

	if (head == NULL)
	  head = new;
	else
	  tail->next = new;
	tail = new;

	break;

      case 'E':
	warning_status = EXIT_FAILURE;
	break;

      case 'F':
	frozen_file_to_write = optarg;
	break;

      case 'G':
	no_gnu_extensions = 1;
	break;

      case 'H':
	hash_table_size = atol (optarg);
	if (hash_table_size == 0)
	  hash_table_size = HASHMAX;
	break;

      case 'I':
	add_include_directory (optarg);
	break;

      case 'L':
	nesting_limit = atoi (optarg);
	break;

      case 'P':
	prefix_all_builtins = 1;
	break;

      case 'Q':
	suppress_warnings = 1;
	break;

      case 'R':
	frozen_file_to_read = optarg;
	break;

#ifdef ENABLE_CHANGEWORD
      case 'W':
	user_word_regexp = optarg;
	break;
#endif

      case 'd':
	debug_level = debug_decode (optarg);
	if (debug_level < 0)
	  {
	    error (0, 0, "bad debug flags: `%s'", optarg);
	    debug_level = 0;
	  }
	break;

      case 'e':
	interactive = 1;
	break;

      case 'l':
	max_debug_argument_length = atoi (optarg);
	if (max_debug_argument_length <= 0)
	  max_debug_argument_length = 0;
	break;

      case 'o':
	if (!debug_set_output (optarg))
	  error (0, errno, "%s", optarg);
	break;

      case 's':
	sync_output = 1;
	break;
      }

  if (show_version)
    {
      printf ("%s\n", PACKAGE_STRING);
      fputs ("\
Copyright (C) 2006 Free Software Foundation, Inc.\n\
This is free software; see the source for copying conditions.  There is NO\n\
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n\
\n\
Written by Rene' Seindal.\n\
", stdout);

      if (close_stream (stdout) != 0)
	M4ERROR ((EXIT_FAILURE, errno, "write error"));
      exit (EXIT_SUCCESS);
    }

  if (show_help)
    usage (EXIT_SUCCESS);

  defines = head;

  /* Do the basic initialisations.  */

  input_init ();
  output_init ();
  symtab_init ();
  include_env_init ();

  if (frozen_file_to_read)
    reload_frozen_state (frozen_file_to_read);
  else
    builtin_init ();

  /* Handle deferred command line macro definitions.  Must come after
     initialisation of the symbol table.  */

  while (defines != NULL)
    {
      macro_definition *next;
      char *macro_value;
      symbol *sym;

      switch (defines->code)
	{
	case 'D':
	  macro_value = strchr (defines->macro, '=');
	  if (macro_value == NULL)
	    macro_value = "";
	  else
	    *macro_value++ = '\0';
	  define_user_macro (defines->macro, macro_value, SYMBOL_INSERT);
	  break;

	case 'U':
	  lookup_symbol (defines->macro, SYMBOL_DELETE);
	  break;

	case 't':
	  sym = lookup_symbol (defines->macro, SYMBOL_INSERT);
	  SYMBOL_TRACED (sym) = TRUE;
	  break;

	default:
	  M4ERROR ((warning_status, 0,
		    "INTERNAL ERROR: bad code in deferred arguments"));
	  abort ();
	}

      next = defines->next;
      free (defines);
      defines = next;
    }

  /* Interactive mode means unbuffered output, and interrupts ignored.  */

  if (interactive)
    {
      signal (SIGINT, SIG_IGN);
      setbuf (stdout, (char *) NULL);
    }

  /* Handle the various input files.  Each file is pushed on the input,
     and the input read.  Wrapup text is handled separately later.  */

  if (optind == argc)
    {
      push_file (stdin, "stdin");
      expand_input ();
    }
  else
    for (; optind < argc; optind++)
      {
	if (strcmp (argv[optind], "-") == 0)
	  push_file (stdin, "stdin");
	else
	  {
	    const char *name;
	    fp = path_search (argv[optind], &name);
	    if (fp == NULL)
	      {
		error (0, errno, "%s", argv[optind]);
		/* Set the status to EXIT_FAILURE, even though we
		   continue to process files after a missing file.  */
		retcode = EXIT_FAILURE;
		continue;
	      }
	    push_file (fp, name);
	    free ((char *) name);
	  }
	expand_input ();
      }
#undef NEXTARG

  /* Now handle wrapup text.  */

  while (pop_wrapup ())
    expand_input ();

  /* Change debug stream back to stderr, to force flushing debug stream and
     detect any errors it might have encountered.  */
  debug_set_output (NULL);

  if (frozen_file_to_write)
    produce_frozen_state (frozen_file_to_write);
  else
    {
      make_diversion (0);
      undivert_all ();
    }

  if (close_stream (stdout) != 0)
    M4ERROR ((EXIT_FAILURE, errno, "write error"));
  exit (retcode);
}
