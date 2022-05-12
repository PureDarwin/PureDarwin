/* GNU m4 -- A simple macro processor

   Copyright (C) 1989, 1990, 1991, 1992, 1993, 1994, 2000, 2004, 2006
   Free Software Foundation, Inc.

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

/* Code for all builtin macros, initialisation of symbol table, and
   expansion of user defined macros.  */

#include "m4.h"

extern FILE *popen ();

#include "regex.h"

#if HAVE_SYS_WAIT_H
# include <sys/wait.h>
#endif

#define ARG(i)	(argc > (i) ? TOKEN_DATA_TEXT (argv[i]) : "")

/* Initialisation of builtin and predefined macros.  The table
   "builtin_tab" is both used for initialisation, and by the "builtin"
   builtin.  */

#define DECLARE(name) \
  static void name (struct obstack *, int, token_data **)

DECLARE (m4___file__);
DECLARE (m4___line__);
DECLARE (m4___program__);
DECLARE (m4_builtin);
DECLARE (m4_changecom);
DECLARE (m4_changequote);
#ifdef ENABLE_CHANGEWORD
DECLARE (m4_changeword);
#endif
DECLARE (m4_debugmode);
DECLARE (m4_debugfile);
DECLARE (m4_decr);
DECLARE (m4_define);
DECLARE (m4_defn);
DECLARE (m4_divert);
DECLARE (m4_divnum);
DECLARE (m4_dnl);
DECLARE (m4_dumpdef);
DECLARE (m4_errprint);
DECLARE (m4_esyscmd);
DECLARE (m4_eval);
DECLARE (m4_format);
DECLARE (m4_ifdef);
DECLARE (m4_ifelse);
DECLARE (m4_include);
DECLARE (m4_incr);
DECLARE (m4_index);
DECLARE (m4_indir);
DECLARE (m4_len);
DECLARE (m4_m4exit);
DECLARE (m4_m4wrap);
DECLARE (m4_maketemp);
DECLARE (m4_patsubst);
DECLARE (m4_popdef);
DECLARE (m4_pushdef);
DECLARE (m4_regexp);
DECLARE (m4_shift);
DECLARE (m4_sinclude);
DECLARE (m4_substr);
DECLARE (m4_syscmd);
DECLARE (m4_sysval);
DECLARE (m4_traceoff);
DECLARE (m4_traceon);
DECLARE (m4_translit);
DECLARE (m4_undefine);
DECLARE (m4_undivert);

#undef DECLARE

static builtin
builtin_tab[] =
{

  /* name		GNUext	macros	blind	function */

  { "__file__",		TRUE,	FALSE,	FALSE,	m4___file__ },
  { "__line__",		TRUE,	FALSE,	FALSE,	m4___line__ },
  { "__program__",	TRUE,	FALSE,	FALSE,	m4___program__ },
  { "builtin",		TRUE,	FALSE,	TRUE,	m4_builtin },
  { "changecom",	FALSE,	FALSE,	FALSE,	m4_changecom },
  { "changequote",	FALSE,	FALSE,	FALSE,	m4_changequote },
#ifdef ENABLE_CHANGEWORD
  { "changeword",	TRUE,	FALSE,	TRUE,	m4_changeword },
#endif
  { "debugmode",	TRUE,	FALSE,	FALSE,	m4_debugmode },
  { "debugfile",	TRUE,	FALSE,	FALSE,	m4_debugfile },
  { "decr",		FALSE,	FALSE,	TRUE,	m4_decr },
  { "define",		FALSE,	TRUE,	TRUE,	m4_define },
  { "defn",		FALSE,	FALSE,	TRUE,	m4_defn },
  { "divert",		FALSE,	FALSE,	FALSE,	m4_divert },
  { "divnum",		FALSE,	FALSE,	FALSE,	m4_divnum },
  { "dnl",		FALSE,	FALSE,	FALSE,	m4_dnl },
  { "dumpdef",		FALSE,	FALSE,	FALSE,	m4_dumpdef },
  { "errprint",		FALSE,	FALSE,	TRUE,	m4_errprint },
  { "esyscmd",		TRUE,	FALSE,	TRUE,	m4_esyscmd },
  { "eval",		FALSE,	FALSE,	TRUE,	m4_eval },
  { "format",		TRUE,	FALSE,	TRUE,	m4_format },
  { "ifdef",		FALSE,	FALSE,	TRUE,	m4_ifdef },
  { "ifelse",		FALSE,	FALSE,	TRUE,	m4_ifelse },
  { "include",		FALSE,	FALSE,	TRUE,	m4_include },
  { "incr",		FALSE,	FALSE,	TRUE,	m4_incr },
  { "index",		FALSE,	FALSE,	TRUE,	m4_index },
  { "indir",		TRUE,	FALSE,	TRUE,	m4_indir },
  { "len",		FALSE,	FALSE,	TRUE,	m4_len },
  { "m4exit",		FALSE,	FALSE,	FALSE,	m4_m4exit },
  { "m4wrap",		FALSE,	FALSE,	TRUE,	m4_m4wrap },
  { "maketemp",		FALSE,	FALSE,	TRUE,	m4_maketemp },
  { "patsubst",		TRUE,	FALSE,	TRUE,	m4_patsubst },
  { "popdef",		FALSE,	FALSE,	TRUE,	m4_popdef },
  { "pushdef",		FALSE,	TRUE,	TRUE,	m4_pushdef },
  { "regexp",		TRUE,	FALSE,	TRUE,	m4_regexp },
  { "shift",		FALSE,	FALSE,	TRUE,	m4_shift },
  { "sinclude",		FALSE,	FALSE,	TRUE,	m4_sinclude },
  { "substr",		FALSE,	FALSE,	TRUE,	m4_substr },
  { "syscmd",		FALSE,	FALSE,	TRUE,	m4_syscmd },
  { "sysval",		FALSE,	FALSE,	FALSE,	m4_sysval },
  { "traceoff",		FALSE,	FALSE,	FALSE,	m4_traceoff },
  { "traceon",		FALSE,	FALSE,	FALSE,	m4_traceon },
  { "translit",		FALSE,	FALSE,	TRUE,	m4_translit },
  { "undefine",		FALSE,	FALSE,	TRUE,	m4_undefine },
  { "undivert",		FALSE,	FALSE,	FALSE,	m4_undivert },

  { 0,			FALSE,	FALSE,	FALSE,	0 },

  /* placeholder is intentionally stuck after the table end delimiter,
     so that we can easily find it, while not treating it as a real
     builtin.  */
  { "placeholder",	TRUE,	FALSE,	FALSE,	m4_placeholder },
};

static predefined const
predefined_tab[] =
{
#if UNIX
  { "unix",	"__unix__",	"" },
#elif W32_NATIVE
  { "windows",	"__windows__",	"" },
#elif OS2
  { "os2",	"__os2__",	"" },
#else
# warning Platform macro not provided
#endif
  { NULL,	"__gnu__",	"" },

  { NULL,	NULL,		NULL },
};

/*----------------------------------------.
| Find the builtin, which lives on ADDR.  |
`----------------------------------------*/

const builtin *
find_builtin_by_addr (builtin_func *func)
{
  const builtin *bp;

  for (bp = &builtin_tab[0]; bp->name != NULL; bp++)
    if (bp->func == func)
      return bp;
  if (func == m4_placeholder)
    return bp + 1;
  return NULL;
}

/*----------------------------------------------------------.
| Find the builtin, which has NAME.  On failure, return the |
| placeholder builtin.                                      |
`----------------------------------------------------------*/

const builtin *
find_builtin_by_name (const char *name)
{
  const builtin *bp;

  for (bp = &builtin_tab[0]; bp->name != NULL; bp++)
    if (strcmp (bp->name, name) == 0)
      return bp;
  return bp + 1;
}

/*-------------------------------------------------------------------------.
| Install a builtin macro with name NAME, bound to the C function given in |
| BP.  MODE is SYMBOL_INSERT or SYMBOL_PUSHDEF.  TRACED defines whether	   |
| NAME is to be traced.							   |
`-------------------------------------------------------------------------*/

void
define_builtin (const char *name, const builtin *bp, symbol_lookup mode)
{
  symbol *sym;

  sym = lookup_symbol (name, mode);
  SYMBOL_TYPE (sym) = TOKEN_FUNC;
  SYMBOL_MACRO_ARGS (sym) = bp->groks_macro_args;
  SYMBOL_BLIND_NO_ARGS (sym) = bp->blind_if_no_args;
  SYMBOL_FUNC (sym) = bp->func;
}

/*-------------------------------------------------------------------------.
| Define a predefined or user-defined macro, with name NAME, and expansion |
| TEXT.  MODE destinguishes between the "define" and the "pushdef" case.   |
| It is also used from main ().						   |
`-------------------------------------------------------------------------*/

void
define_user_macro (const char *name, const char *text, symbol_lookup mode)
{
  symbol *s;

  s = lookup_symbol (name, mode);
  if (SYMBOL_TYPE (s) == TOKEN_TEXT)
    free (SYMBOL_TEXT (s));

  SYMBOL_TYPE (s) = TOKEN_TEXT;
  SYMBOL_TEXT (s) = xstrdup (text);
}

/*-----------------------------------------------.
| Initialise all builtin and predefined macros.	 |
`-----------------------------------------------*/

void
builtin_init (void)
{
  const builtin *bp;
  const predefined *pp;
  char *string;

  for (bp = &builtin_tab[0]; bp->name != NULL; bp++)
    if (!no_gnu_extensions || !bp->gnu_extension)
      {
	if (prefix_all_builtins)
	  {
	    string = (char *) xmalloc (strlen (bp->name) + 4);
	    strcpy (string, "m4_");
	    strcat (string, bp->name);
	    define_builtin (string, bp, SYMBOL_INSERT);
	    free (string);
	  }
	else
	  define_builtin (bp->name, bp, SYMBOL_INSERT);
      }

  for (pp = &predefined_tab[0]; pp->func != NULL; pp++)
    if (no_gnu_extensions)
      {
	if (pp->unix_name != NULL)
	  define_user_macro (pp->unix_name, pp->func, SYMBOL_INSERT);
      }
    else
      {
	if (pp->gnu_name != NULL)
	  define_user_macro (pp->gnu_name, pp->func, SYMBOL_INSERT);
      }
}

/*------------------------------------------------------------------------.
| Give friendly warnings if a builtin macro is passed an inappropriate	  |
| number of arguments.  NAME is macro name for messages, ARGC is actual	  |
| number of arguments, MIN is the minimum number of acceptable arguments, |
| negative if not applicable, MAX is the maximum number, negative if not  |
| applicable.								  |
`------------------------------------------------------------------------*/

static boolean
bad_argc (token_data *name, int argc, int min, int max)
{
  boolean isbad = FALSE;

  if (min > 0 && argc < min)
    {
      if (!suppress_warnings)
	M4ERROR ((warning_status, 0,
		  "Warning: too few arguments to builtin `%s'",
		  TOKEN_DATA_TEXT (name)));
      isbad = TRUE;
    }
  else if (max > 0 && argc > max && !suppress_warnings)
    M4ERROR ((warning_status, 0,
	      "Warning: excess arguments to builtin `%s' ignored",
	      TOKEN_DATA_TEXT (name)));

  return isbad;
}

/*--------------------------------------------------------------------------.
| The function numeric_arg () converts ARG to an int pointed to by VALUEP.  |
| If the conversion fails, print error message for macro MACRO.  Return	    |
| TRUE iff conversion succeeds.						    |
`--------------------------------------------------------------------------*/

static boolean
numeric_arg (token_data *macro, const char *arg, int *valuep)
{
  char *endp;

  if (*arg == '\0')
    {
      *valuep = 0;
      M4ERROR ((warning_status, 0,
		"empty string treated as 0 in builtin `%s'",
		TOKEN_DATA_TEXT (macro)));
    }
  else
    {
      errno = 0;
      *valuep = strtol (arg, &endp, 10);
      if (*endp != '\0')
	{
	  M4ERROR ((warning_status, 0,
		    "non-numeric argument to builtin `%s'",
		    TOKEN_DATA_TEXT (macro)));
	  return FALSE;
	}
      if (isspace (to_uchar (*arg)))
	M4ERROR ((warning_status, 0,
		  "leading whitespace ignored in builtin `%s'",
		  TOKEN_DATA_TEXT (macro)));
      else if (errno == ERANGE)
	M4ERROR ((warning_status, 0,
		  "numeric overflow detected in builtin `%s'",
		  TOKEN_DATA_TEXT (macro)));
    }
  return TRUE;
}

/*------------------------------------------------------------------------.
| The function ntoa () converts VALUE to a signed ascii representation in |
| radix RADIX.								  |
`------------------------------------------------------------------------*/

/* Digits for number to ascii conversions.  */
static char const digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";

static const char *
ntoa (register eval_t value, int radix)
{
  boolean negative;
  unsigned_eval_t uvalue;
  static char str[256];
  register char *s = &str[sizeof str];

  *--s = '\0';

  if (value < 0)
    {
      negative = TRUE;
      uvalue = (unsigned_eval_t) -value;
    }
  else
    {
      negative = FALSE;
      uvalue = (unsigned_eval_t) value;
    }

  do
    {
      *--s = digits[uvalue % radix];
      uvalue /= radix;
    }
  while (uvalue > 0);

  if (negative)
    *--s = '-';
  return s;
}

/*----------------------------------------------------------------------.
| Format an int VAL, and stuff it into an obstack OBS.  Used for macros |
| expanding to numbers.						        |
`----------------------------------------------------------------------*/

static void
shipout_int (struct obstack *obs, int val)
{
  const char *s;

  s = ntoa ((eval_t) val, 10);
  obstack_grow (obs, s, strlen (s));
}

/*----------------------------------------------------------------------.
| Print ARGC arguments from the table ARGV to obstack OBS, separated by |
| SEP, and quoted by the current quotes, if QUOTED is TRUE.	        |
`----------------------------------------------------------------------*/

static void
dump_args (struct obstack *obs, int argc, token_data **argv,
	   const char *sep, boolean quoted)
{
  int i;
  size_t len = strlen (sep);

  for (i = 1; i < argc; i++)
    {
      if (i > 1)
	obstack_grow (obs, sep, len);
      if (quoted)
	obstack_grow (obs, lquote.string, lquote.length);
      obstack_grow (obs, TOKEN_DATA_TEXT (argv[i]),
		    strlen (TOKEN_DATA_TEXT (argv[i])));
      if (quoted)
	obstack_grow (obs, rquote.string, rquote.length);
    }
}

/* The rest of this file is code for builtins and expansion of user
   defined macros.  All the functions for builtins have a prototype as:

	void m4_MACRONAME (struct obstack *obs, int argc, char *argv[]);

   The function are expected to leave their expansion on the obstack OBS,
   as an unfinished object.  ARGV is a table of ARGC pointers to the
   individual arguments to the macro.  Please note that in general
   argv[argc] != NULL.  */

/* The first section are macros for definining, undefining, examining,
   changing, ... other macros.  */

/*-------------------------------------------------------------------------.
| The function define_macro is common for the builtins "define",	   |
| "undefine", "pushdef" and "popdef".  ARGC and ARGV is as for the caller, |
| and MODE argument determines how the macro name is entered into the	   |
| symbol table.								   |
`-------------------------------------------------------------------------*/

static void
define_macro (int argc, token_data **argv, symbol_lookup mode)
{
  const builtin *bp;

  if (bad_argc (argv[0], argc, 2, 3))
    return;

  if (TOKEN_DATA_TYPE (argv[1]) != TOKEN_TEXT)
    return;

  if (argc == 2)
    {
      define_user_macro (ARG (1), "", mode);
      return;
    }

  switch (TOKEN_DATA_TYPE (argv[2]))
    {
    case TOKEN_TEXT:
      define_user_macro (ARG (1), ARG (2), mode);
      break;

    case TOKEN_FUNC:
      bp = find_builtin_by_addr (TOKEN_DATA_FUNC (argv[2]));
      if (bp == NULL)
	return;
      else
	define_builtin (ARG (1), bp, mode);
      break;

    default:
      M4ERROR ((warning_status, 0,
		"INTERNAL ERROR: bad token data type in define_macro ()"));
      abort ();
    }
}

static void
m4_define (struct obstack *obs, int argc, token_data **argv)
{
  define_macro (argc, argv, SYMBOL_INSERT);
}

static void
m4_undefine (struct obstack *obs, int argc, token_data **argv)
{
  int i;
  if (bad_argc (argv[0], argc, 2, -1))
    return;
  for (i = 1; i < argc; i++)
    lookup_symbol (ARG (i), SYMBOL_DELETE);
}

static void
m4_pushdef (struct obstack *obs, int argc, token_data **argv)
{
  define_macro (argc, argv,  SYMBOL_PUSHDEF);
}

static void
m4_popdef (struct obstack *obs, int argc, token_data **argv)
{
  int i;
  if (bad_argc (argv[0], argc, 2, -1))
    return;
  for (i = 1; i < argc; i++)
    lookup_symbol (ARG (i), SYMBOL_POPDEF);
}

/*---------------------.
| Conditionals of m4.  |
`---------------------*/

static void
m4_ifdef (struct obstack *obs, int argc, token_data **argv)
{
  symbol *s;
  const char *result;

  if (bad_argc (argv[0], argc, 3, 4))
    return;
  s = lookup_symbol (ARG (1), SYMBOL_LOOKUP);

  if (s != NULL && SYMBOL_TYPE (s) != TOKEN_VOID)
    result = ARG (2);
  else if (argc >= 4)
    result = ARG (3);
  else
    result = NULL;

  if (result != NULL)
    obstack_grow (obs, result, strlen (result));
}

static void
m4_ifelse (struct obstack *obs, int argc, token_data **argv)
{
  const char *result;
  token_data *argv0;

  if (argc == 2)
    return;

  if (bad_argc (argv[0], argc, 4, -1))
    return;
  else
    /* Diagnose excess arguments if 5, 8, 11, etc., actual arguments.  */
    bad_argc (argv[0], (argc + 2) % 3, -1, 1);

  argv0 = argv[0];
  argv++;
  argc--;

  result = NULL;
  while (result == NULL)

    if (strcmp (ARG (0), ARG (1)) == 0)
      result = ARG (2);

    else
      switch (argc)
	{
	case 3:
	  return;

	case 4:
	case 5:
	  result = ARG (3);
	  break;

	default:
	  argc -= 3;
	  argv += 3;
	}

  obstack_grow (obs, result, strlen (result));
}

/*---------------------------------------------------------------------.
| The function dump_symbol () is for use by "dumpdef".  It builds up a |
| table of all defined, un-shadowed, symbols.			       |
`---------------------------------------------------------------------*/

/* The structure dump_symbol_data is used to pass the information needed
   from call to call to dump_symbol.  */

struct dump_symbol_data
{
  struct obstack *obs;		/* obstack for table */
  symbol **base;		/* base of table */
  int size;			/* size of table */
};

static void
dump_symbol (symbol *sym, struct dump_symbol_data *data)
{
  if (!SYMBOL_SHADOWED (sym) && SYMBOL_TYPE (sym) != TOKEN_VOID)
    {
      obstack_blank (data->obs, sizeof (symbol *));
      data->base = (symbol **) obstack_base (data->obs);
      data->base[data->size++] = sym;
    }
}

/*------------------------------------------------------------------------.
| qsort comparison routine, for sorting the table made in m4_dumpdef ().  |
`------------------------------------------------------------------------*/

static int
dumpdef_cmp (const void *s1, const void *s2)
{
  return strcmp (SYMBOL_NAME (* (symbol *const *) s1),
		 SYMBOL_NAME (* (symbol *const *) s2));
}

/*-------------------------------------------------------------------------.
| Implementation of "dumpdef" itself.  It builds up a table of pointers to |
| symbols, sorts it and prints the sorted table.			   |
`-------------------------------------------------------------------------*/

static void
m4_dumpdef (struct obstack *obs, int argc, token_data **argv)
{
  symbol *s;
  int i;
  struct dump_symbol_data data;
  const builtin *bp;

  data.obs = obs;
  data.base = (symbol **) obstack_base (obs);
  data.size = 0;

  if (argc == 1)
    {
      hack_all_symbols (dump_symbol, (char *) &data);
    }
  else
    {
      for (i = 1; i < argc; i++)
	{
	  s = lookup_symbol (TOKEN_DATA_TEXT (argv[i]), SYMBOL_LOOKUP);
	  if (s != NULL && SYMBOL_TYPE (s) != TOKEN_VOID)
	    dump_symbol (s, &data);
	  else
	    M4ERROR ((warning_status, 0,
		      "undefined macro `%s'", TOKEN_DATA_TEXT (argv[i])));
	}
    }

  /* Make table of symbols invisible to expand_macro ().  */

  (void) obstack_finish (obs);

  qsort ((char *) data.base, data.size, sizeof (symbol *), dumpdef_cmp);

  for (; data.size > 0; --data.size, data.base++)
    {
      DEBUG_PRINT1 ("%s:\t", SYMBOL_NAME (data.base[0]));

      switch (SYMBOL_TYPE (data.base[0]))
	{
	case TOKEN_TEXT:
	  if (debug_level & DEBUG_TRACE_QUOTE)
	    DEBUG_PRINT3 ("%s%s%s\n",
			  lquote.string, SYMBOL_TEXT (data.base[0]), rquote.string);
	  else
	    DEBUG_PRINT1 ("%s\n", SYMBOL_TEXT (data.base[0]));
	  break;

	case TOKEN_FUNC:
	  bp = find_builtin_by_addr (SYMBOL_FUNC (data.base[0]));
	  if (bp == NULL)
	    {
	      M4ERROR ((warning_status, 0, "\
INTERNAL ERROR: builtin not found in builtin table"));
	      abort ();
	    }
	  DEBUG_PRINT1 ("<%s>\n", bp->name);
	  break;

	default:
	  M4ERROR ((warning_status, 0,
		    "INTERNAL ERROR: bad token data type in m4_dumpdef ()"));
	  abort ();
	  break;
	}
    }
}

/*---------------------------------------------------------------------.
| The builtin "builtin" allows calls to builtin macros, even if their  |
| definition has been overridden or shadowed.  It is thus possible to  |
| redefine builtins, and still access their original definition.  This |
| macro is not available in compatibility mode.			       |
`---------------------------------------------------------------------*/

static void
m4_builtin (struct obstack *obs, int argc, token_data **argv)
{
  const builtin *bp;
  const char *name = ARG (1);

  if (bad_argc (argv[0], argc, 2, -1))
    return;

  bp = find_builtin_by_name (name);
  if (bp->func == m4_placeholder)
    M4ERROR ((warning_status, 0,
	      "undefined builtin `%s'", name));
  else
    (*bp->func) (obs, argc - 1, argv + 1);
}

/*------------------------------------------------------------------------.
| The builtin "indir" allows indirect calls to macros, even if their name |
| is not a proper macro name.  It is thus possible to define macros with  |
| ill-formed names for internal use in larger macro packages.  This macro |
| is not available in compatibility mode.				  |
`------------------------------------------------------------------------*/

static void
m4_indir (struct obstack *obs, int argc, token_data **argv)
{
  symbol *s;
  const char *name = ARG (1);

  if (bad_argc (argv[0], argc, 2, -1))
    return;

  s = lookup_symbol (name, SYMBOL_LOOKUP);
  if (s == NULL || SYMBOL_TYPE (s) == TOKEN_VOID)
    M4ERROR ((warning_status, 0,
	      "undefined macro `%s'", name));
  else
    call_macro (s, argc - 1, argv + 1, obs);
}

/*-------------------------------------------------------------------------.
| The macro "defn" returns the quoted definition of the macro named by the |
| first argument.  If the macro is builtin, it will push a special	   |
| macro-definition token on ht input stack.				   |
`-------------------------------------------------------------------------*/

static void
m4_defn (struct obstack *obs, int argc, token_data **argv)
{
  symbol *s;
  builtin_func *b;

  if (bad_argc (argv[0], argc, 2, 2))
    return;

  s = lookup_symbol (ARG (1), SYMBOL_LOOKUP);
  if (s == NULL)
    return;

  switch (SYMBOL_TYPE (s))
    {
    case TOKEN_TEXT:
      obstack_grow (obs, lquote.string, lquote.length);
      obstack_grow (obs, SYMBOL_TEXT (s), strlen (SYMBOL_TEXT (s)));
      obstack_grow (obs, rquote.string, rquote.length);
      break;

    case TOKEN_FUNC:
      b = SYMBOL_FUNC (s);
      if (b == m4_placeholder)
	M4ERROR ((warning_status, 0, "\
builtin `%s' requested by frozen file is not supported", ARG (1)));
      else
	push_macro (b);
      break;

    case TOKEN_VOID:
      break;

    default:
      M4ERROR ((warning_status, 0,
		"INTERNAL ERROR: bad symbol type in m4_defn ()"));
      abort ();
    }
}

/*------------------------------------------------------------------------.
| This section contains macros to handle the builtins "syscmd", "esyscmd" |
| and "sysval".  "esyscmd" is GNU specific.				  |
`------------------------------------------------------------------------*/

/* Helper macros for readability.  */
#if UNIX || defined WEXITSTATUS
# define M4SYSVAL_EXITBITS(status)                       \
   (WIFEXITED (status) ? WEXITSTATUS (status) : 0)
# define M4SYSVAL_TERMSIGBITS(status)                    \
   (WIFSIGNALED (status) ? WTERMSIG (status) << 8 : 0)

#else /* ! UNIX && ! defined WEXITSTATUS */
/* Platforms such as mingw do not support the notion of reporting
   which signal terminated a process.  Furthermore if WEXITSTATUS was
   not provided, then the exit value is in the low eight bits.  */
# define M4SYSVAL_EXITBITS(status) status
# define M4SYSVAL_TERMSIGBITS(status) 0
#endif /* ! UNIX && ! defined WEXITSTATUS */

/* Fallback definitions if <stdlib.h> or <sys/wait.h> are inadequate.  */
#ifndef WEXITSTATUS
# define WEXITSTATUS(status) (((status) >> 8) & 0xff)
#endif
#ifndef WTERMSIG
# define WTERMSIG(status) ((status) & 0x7f)
#endif
#ifndef WIFSIGNALED
# define WIFSIGNALED(status) (WTERMSIG (status) != 0)
#endif
#ifndef WIFEXITED
# define WIFEXITED(status) (WTERMSIG (status) == 0)
#endif

/* Exit code from last "syscmd" command.  */
static int sysval;

static void
m4_syscmd (struct obstack *obs, int argc, token_data **argv)
{
  if (bad_argc (argv[0], argc, 2, 2))
    {
      /* The empty command is successful.  */
      sysval = 0;
      return;
    }

  debug_flush_files ();
  sysval = system (ARG (1));
#if FUNC_SYSTEM_BROKEN
  /* OS/2 has a buggy system() that returns exit status in the lowest eight
     bits, although pclose() and WEXITSTATUS are defined to return exit
     status in the next eight bits.  This approach can't detect signals, but
     at least syscmd(`ls') still works when stdout is a terminal.  An
     alternate approach is popen/insert_file/pclose, but that makes stdout
     a pipe, which can change how some child processes behave.  */
  if (sysval != -1)
    sysval <<= 8;
#endif /* FUNC_SYSTEM_BROKEN */
}

static void
m4_esyscmd (struct obstack *obs, int argc, token_data **argv)
{
  FILE *pin;
  int ch;

  if (bad_argc (argv[0], argc, 2, 2))
    {
      /* The empty command is successful.  */
      sysval = 0;
      return;
    }

  debug_flush_files ();
  errno = 0;
  pin = popen (ARG (1), "r");
  if (pin == NULL)
    {
      M4ERROR ((warning_status, errno,
		"cannot open pipe to command `%s'", ARG (1)));
      sysval = -1;
    }
  else
    {
      while ((ch = getc (pin)) != EOF)
	obstack_1grow (obs, (char) ch);
      sysval = pclose (pin);
    }
}

static void
m4_sysval (struct obstack *obs, int argc, token_data **argv)
{
  shipout_int (obs, (sysval == -1 ? 127
		     : (M4SYSVAL_EXITBITS (sysval)
			| M4SYSVAL_TERMSIGBITS (sysval))));
}

/*-------------------------------------------------------------------------.
| This section contains the top level code for the "eval" builtin.  The	   |
| actual work is done in the function evaluate (), which lives in eval.c.  |
`-------------------------------------------------------------------------*/

static void
m4_eval (struct obstack *obs, int argc, token_data **argv)
{
  eval_t value = 0;
  int radix = 10;
  int min = 1;
  const char *s;

  if (bad_argc (argv[0], argc, 2, 4))
    return;

  if (*ARG (2) && !numeric_arg (argv[0], ARG (2), &radix))
    return;

  if (radix < 1 || radix > (int) strlen (digits))
    {
      M4ERROR ((warning_status, 0,
		"radix in builtin `%s' out of range (radix = %d)",
		ARG (0), radix));
      return;
    }

  if (argc >= 4 && !numeric_arg (argv[0], ARG (3), &min))
    return;
  if (min < 0)
    {
      M4ERROR ((warning_status, 0,
		"negative width to builtin `%s'", ARG (0)));
      return;
    }

  if (!*ARG (1))
    M4ERROR ((warning_status, 0,
	      "empty string treated as 0 in builtin `%s'", ARG (0)));
  else if (evaluate (ARG (1), &value))
    return;

  if (radix == 1)
    {
      if (value < 0)
	{
	  obstack_1grow (obs, '-');
	  value = -value;
	}
      /* This assumes 2's-complement for correctly handling INT_MIN.  */
      while (min-- - value > 0)
	obstack_1grow (obs, '0');
      while (value-- != 0)
	obstack_1grow (obs, '1');
      obstack_1grow (obs, '\0');
      return;
    }

  s = ntoa (value, radix);

  if (*s == '-')
    {
      obstack_1grow (obs, '-');
      s++;
    }
  for (min -= strlen (s); --min >= 0;)
    obstack_1grow (obs, '0');

  obstack_grow (obs, s, strlen (s));
}

static void
m4_incr (struct obstack *obs, int argc, token_data **argv)
{
  int value;

  if (bad_argc (argv[0], argc, 2, 2))
    return;

  if (!numeric_arg (argv[0], ARG (1), &value))
    return;

  shipout_int (obs, value + 1);
}

static void
m4_decr (struct obstack *obs, int argc, token_data **argv)
{
  int value;

  if (bad_argc (argv[0], argc, 2, 2))
    return;

  if (!numeric_arg (argv[0], ARG (1), &value))
    return;

  shipout_int (obs, value - 1);
}

/* This section contains the macros "divert", "undivert" and "divnum" for
   handling diversion.  The utility functions used lives in output.c.  */

/*-----------------------------------------------------------------------.
| Divert further output to the diversion given by ARGV[1].  Out of range |
| means discard further output.						 |
`-----------------------------------------------------------------------*/

static void
m4_divert (struct obstack *obs, int argc, token_data **argv)
{
  int i = 0;

  if (bad_argc (argv[0], argc, 1, 2))
    return;

  if (argc >= 2 && !numeric_arg (argv[0], ARG (1), &i))
    return;

  make_diversion (i);
}

/*-----------------------------------------------------.
| Expand to the current diversion number, -1 if none.  |
`-----------------------------------------------------*/

static void
m4_divnum (struct obstack *obs, int argc, token_data **argv)
{
  if (bad_argc (argv[0], argc, 1, 1))
    return;
  shipout_int (obs, current_diversion);
}

/*-----------------------------------------------------------------------.
| Bring back the diversion given by the argument list.  If none is	 |
| specified, bring back all diversions.  GNU specific is the option of	 |
| undiverting named files, by passing a non-numeric argument to undivert |
| ().									 |
`-----------------------------------------------------------------------*/

static void
m4_undivert (struct obstack *obs, int argc, token_data **argv)
{
  int i, file;
  FILE *fp;
  char *endp;

  if (argc == 1)
    undivert_all ();
  else
    for (i = 1; i < argc; i++)
      {
	file = strtol (ARG (i), &endp, 10);
	if (*endp == '\0' && !isspace (to_uchar (*ARG (i))))
	  insert_diversion (file);
	else if (no_gnu_extensions)
	  M4ERROR ((warning_status, 0,
		    "non-numeric argument to builtin `%s'", ARG (0)));
	else
	  {
	    fp = path_search (ARG (i), NULL);
	    if (fp != NULL)
	      {
		insert_file (fp);
		fclose (fp);
	      }
	    else
	      M4ERROR ((warning_status, errno,
			"cannot undivert `%s'", ARG (i)));
	  }
      }
}

/* This section contains various macros, which does not fall into any
   specific group.  These are "dnl", "shift", "changequote", "changecom"
   and "changeword".  */

/*------------------------------------------------------------------------.
| Delete all subsequent whitespace from input.  The function skip_line () |
| lives in input.c.							  |
`------------------------------------------------------------------------*/

static void
m4_dnl (struct obstack *obs, int argc, token_data **argv)
{
  if (bad_argc (argv[0], argc, 1, 1))
    return;

  skip_line ();
}

/*-------------------------------------------------------------------------.
| Shift all argument one to the left, discarding the first argument.  Each |
| output argument is quoted with the current quotes.			   |
`-------------------------------------------------------------------------*/

static void
m4_shift (struct obstack *obs, int argc, token_data **argv)
{
  if (bad_argc (argv[0], argc, 2, -1))
    return;
  dump_args (obs, argc - 1, argv + 1, ",", TRUE);
}

/*--------------------------------------------------------------------------.
| Change the current quotes.  The function set_quotes () lives in input.c.  |
`--------------------------------------------------------------------------*/

static void
m4_changequote (struct obstack *obs, int argc, token_data **argv)
{
  if (bad_argc (argv[0], argc, 1, 3))
    return;

  set_quotes ((argc >= 2) ? TOKEN_DATA_TEXT (argv[1]) : NULL,
	     (argc >= 3) ? TOKEN_DATA_TEXT (argv[2]) : NULL);
}

/*--------------------------------------------------------------------.
| Change the current comment delimiters.  The function set_comment () |
| lives in input.c.						      |
`--------------------------------------------------------------------*/

static void
m4_changecom (struct obstack *obs, int argc, token_data **argv)
{
  if (bad_argc (argv[0], argc, 1, 3))
    return;

  if (argc == 1)
    set_comment ("", "");	/* disable comments */
  else
    set_comment (TOKEN_DATA_TEXT (argv[1]),
		(argc >= 3) ? TOKEN_DATA_TEXT (argv[2]) : NULL);
}

#ifdef ENABLE_CHANGEWORD

/*-----------------------------------------------------------------------.
| Change the regular expression used for breaking the input into words.	 |
| The function set_word_regexp () lives in input.c.			 |
`-----------------------------------------------------------------------*/

static void
m4_changeword (struct obstack *obs, int argc, token_data **argv)
{
  if (bad_argc (argv[0], argc, 2, 2))
    return;

  set_word_regexp (TOKEN_DATA_TEXT (argv[1]));
}

#endif /* ENABLE_CHANGEWORD */

/* This section contains macros for inclusion of other files -- "include"
   and "sinclude".  This differs from bringing back diversions, in that
   the input is scanned before being copied to the output.  */

/*-------------------------------------------------------------------------.
| Generic include function.  Include the file given by the first argument, |
| if it exists.  Complain about inaccesible files iff SILENT is FALSE.	   |
`-------------------------------------------------------------------------*/

static void
include (int argc, token_data **argv, boolean silent)
{
  FILE *fp;
  const char *name;

  if (bad_argc (argv[0], argc, 2, 2))
    return;

  fp = path_search (ARG (1), &name);
  if (fp == NULL)
    {
      if (!silent)
	M4ERROR ((warning_status, errno,
		  "cannot open `%s'", ARG (1)));
      return;
    }

  push_file (fp, name);
  free ((char *) name);
}

/*------------------------------------------------.
| Include a file, complaining in case of errors.  |
`------------------------------------------------*/

static void
m4_include (struct obstack *obs, int argc, token_data **argv)
{
  include (argc, argv, FALSE);
}

/*----------------------------------.
| Include a file, ignoring errors.  |
`----------------------------------*/

static void
m4_sinclude (struct obstack *obs, int argc, token_data **argv)
{
  include (argc, argv, TRUE);
}

/* More miscellaneous builtins -- "maketemp", "errprint", "__file__",
   "__line__", and "__program__".  The last three are GNU specific.  */

/*------------------------------------------------------------------.
| Use the first argument as at template for a temporary file name.  |
`------------------------------------------------------------------*/

static void
m4_maketemp (struct obstack *obs, int argc, token_data **argv)
{
  int fd;
  if (bad_argc (argv[0], argc, 2, 2))
    return;
  errno = 0;
  if ((fd = mkstemp (ARG (1))) < 0)
    {
      M4ERROR ((warning_status, errno, "cannot create tempfile `%s'",
		ARG (1)));
      return;
    }
  close(fd);
  obstack_grow (obs, ARG (1), strlen (ARG (1)));
}

/*----------------------------------------.
| Print all arguments on standard error.  |
`----------------------------------------*/

static void
m4_errprint (struct obstack *obs, int argc, token_data **argv)
{
  if (bad_argc (argv[0], argc, 2, -1))
    return;
  dump_args (obs, argc, argv, " ", FALSE);
  obstack_1grow (obs, '\0');
  debug_flush_files ();
  fprintf (stderr, "%s", (char *) obstack_finish (obs));
  fflush (stderr);
}

static void
m4___file__ (struct obstack *obs, int argc, token_data **argv)
{
  if (bad_argc (argv[0], argc, 1, 1))
    return;
  obstack_grow (obs, lquote.string, lquote.length);
  obstack_grow (obs, current_file, strlen (current_file));
  obstack_grow (obs, rquote.string, rquote.length);
}

static void
m4___line__ (struct obstack *obs, int argc, token_data **argv)
{
  if (bad_argc (argv[0], argc, 1, 1))
    return;
  shipout_int (obs, current_line);
}

static void
m4___program__ (struct obstack *obs, int argc, token_data **argv)
{
  if (bad_argc (argv[0], argc, 1, 1))
    return;
  obstack_grow (obs, lquote.string, lquote.length);
  obstack_grow (obs, program_name, strlen (program_name));
  obstack_grow (obs, rquote.string, rquote.length);
}

/* This section contains various macros for exiting, saving input until
   EOF is seen, and tracing macro calls.  That is: "m4exit", "m4wrap",
   "traceon" and "traceoff".  */

/*-------------------------------------------------------------------------.
| Exit immediately, with exitcode specified by the first argument, 0 if no |
| arguments are present.						   |
`-------------------------------------------------------------------------*/

static void
m4_m4exit (struct obstack *obs, int argc, token_data **argv)
{
  int exit_code = EXIT_SUCCESS;

  /* Warn on bad arguments, but still exit.  */
  bad_argc (argv[0], argc, 1, 2);
  if (argc >= 2 && !numeric_arg (argv[0], ARG (1), &exit_code))
    exit_code = EXIT_FAILURE;
  if (exit_code < 0 || exit_code > 255)
    {
      M4ERROR ((warning_status, 0,
		"exit status out of range: `%d'", exit_code));
      exit_code = EXIT_FAILURE;
    }
  if (close_stream (stdout) != 0)
    {
      M4ERROR ((warning_status, errno, "write error"));
      if (exit_code == 0)
	exit_code = EXIT_FAILURE;
    }
  /* Change debug stream back to stderr, to force flushing debug stream and
     detect any errors it might have encountered.  */
  debug_set_output (NULL);
  if (exit_code == 0 && retcode != 0)
    exit_code = retcode;
  exit (exit_code);
}

/*-------------------------------------------------------------------------.
| Save the argument text until EOF has been seen, allowing for user	   |
| specified cleanup action.  GNU version saves all arguments, the standard |
| version only the first.						   |
`-------------------------------------------------------------------------*/

static void
m4_m4wrap (struct obstack *obs, int argc, token_data **argv)
{
  if (bad_argc (argv[0], argc, 2, -1))
    return;
  if (no_gnu_extensions)
    obstack_grow (obs, ARG (1), strlen (ARG (1)));
  else
    dump_args (obs, argc, argv, " ", FALSE);
  obstack_1grow (obs, '\0');
  push_wrapup (obstack_finish (obs));
}

/* Enable tracing of all specified macros, or all, if none is specified.
   Tracing is disabled by default, when a macro is defined.  This can be
   overridden by the "t" debug flag.  */

/*-----------------------------------------------------------------------.
| Set_trace () is used by "traceon" and "traceoff" to enable and disable |
| tracing of a macro.  It disables tracing if DATA is NULL, otherwise it |
| enable tracing.							 |
`-----------------------------------------------------------------------*/

static void
set_trace (symbol *sym, const char *data)
{
  SYMBOL_TRACED (sym) = (boolean) (data != NULL);
  /* Remove placeholder from table if macro is undefined and untraced.  */
  if (SYMBOL_TYPE (sym) == TOKEN_VOID && data == NULL)
    lookup_symbol (SYMBOL_NAME (sym), SYMBOL_POPDEF);
}

static void
m4_traceon (struct obstack *obs, int argc, token_data **argv)
{
  symbol *s;
  int i;

  if (argc == 1)
    hack_all_symbols (set_trace, (char *) obs);
  else
    for (i = 1; i < argc; i++)
      {
	s = lookup_symbol (TOKEN_DATA_TEXT (argv[i]), SYMBOL_INSERT);
	set_trace (s, (char *) obs);
      }
}

/*------------------------------------------------------------------------.
| Disable tracing of all specified macros, or all, if none is specified.  |
`------------------------------------------------------------------------*/

static void
m4_traceoff (struct obstack *obs, int argc, token_data **argv)
{
  symbol *s;
  int i;

  if (argc == 1)
    hack_all_symbols (set_trace, NULL);
  else
    for (i = 1; i < argc; i++)
      {
	s = lookup_symbol (TOKEN_DATA_TEXT (argv[i]), SYMBOL_LOOKUP);
	if (s != NULL)
	  set_trace (s, NULL);
      }
}

/*----------------------------------------------------------------------.
| On-the-fly control of the format of the tracing output.  It takes one |
| argument, which is a character string like given to the -d option, or |
| none in which case the debug_level is zeroed.			        |
`----------------------------------------------------------------------*/

static void
m4_debugmode (struct obstack *obs, int argc, token_data **argv)
{
  int new_debug_level;
  int change_flag;

  if (bad_argc (argv[0], argc, 1, 2))
    return;

  if (argc == 1)
    debug_level = 0;
  else
    {
      if (ARG (1)[0] == '+' || ARG (1)[0] == '-')
	{
	  change_flag = ARG (1)[0];
	  new_debug_level = debug_decode (ARG (1) + 1);
	}
      else
	{
	  change_flag = 0;
	  new_debug_level = debug_decode (ARG (1));
	}

      if (new_debug_level < 0)
	M4ERROR ((warning_status, 0,
		  "Debugmode: bad debug flags: `%s'", ARG (1)));
      else
	{
	  switch (change_flag)
	    {
	    case 0:
	      debug_level = new_debug_level;
	      break;

	    case '+':
	      debug_level |= new_debug_level;
	      break;

	    case '-':
	      debug_level &= ~new_debug_level;
	      break;
	    }
	}
    }
}

/*-------------------------------------------------------------------------.
| Specify the destination of the debugging output.  With one argument, the |
| argument is taken as a file name, with no arguments, revert to stderr.   |
`-------------------------------------------------------------------------*/

static void
m4_debugfile (struct obstack *obs, int argc, token_data **argv)
{
  if (bad_argc (argv[0], argc, 1, 2))
    return;

  if (argc == 1)
    debug_set_output (NULL);
  else if (!debug_set_output (ARG (1)))
    M4ERROR ((warning_status, errno,
	      "cannot set error file: `%s'", ARG (1)));
}

/* This section contains text processing macros: "len", "index",
   "substr", "translit", "format", "regexp" and "patsubst".  The last
   three are GNU specific.  */

/*---------------------------------------------.
| Expand to the length of the first argument.  |
`---------------------------------------------*/

static void
m4_len (struct obstack *obs, int argc, token_data **argv)
{
  if (bad_argc (argv[0], argc, 2, 2))
    return;
  shipout_int (obs, strlen (ARG (1)));
}

/*-------------------------------------------------------------------------.
| The macro expands to the first index of the second argument in the first |
| argument.								   |
`-------------------------------------------------------------------------*/

static void
m4_index (struct obstack *obs, int argc, token_data **argv)
{
  const char *cp, *last;
  int l1, l2, retval;

  if (bad_argc (argv[0], argc, 3, 3))
    {
      /* builtin(`index') is blank, but index(`abc') is 0.  */
      if (argc == 2)
	shipout_int (obs, 0);
      return;
    }

  l1 = strlen (ARG (1));
  l2 = strlen (ARG (2));

  last = ARG (1) + l1 - l2;

  for (cp = ARG (1); cp <= last; cp++)
    {
      if (strncmp (cp, ARG (2), l2) == 0)
	break;
    }
  retval = (cp <= last) ? cp - ARG (1) : -1;

  shipout_int (obs, retval);
}

/*-------------------------------------------------------------------------.
| The macro "substr" extracts substrings from the first argument, starting |
| from the index given by the second argument, extending for a length	   |
| given by the third argument.  If the third argument is missing, the	   |
| substring extends to the end of the first argument.			   |
`-------------------------------------------------------------------------*/

static void
m4_substr (struct obstack *obs, int argc, token_data **argv)
{
  int start = 0;
  int length, avail;

  if (bad_argc (argv[0], argc, 3, 4))
    {
      /* builtin(`substr') is blank, but substr(`abc') is abc.  */
      if (argc == 2)
	obstack_grow (obs, ARG (1), strlen (ARG (1)));
      return;
    }

  length = avail = strlen (ARG (1));
  if (!numeric_arg (argv[0], ARG (2), &start))
    return;

  if (argc >= 4 && !numeric_arg (argv[0], ARG (3), &length))
    return;

  if (start < 0 || length <= 0 || start >= avail)
    return;

  if (start + length > avail)
    length = avail - start;
  obstack_grow (obs, ARG (1) + start, length);
}

/*------------------------------------------------------------------------.
| For "translit", ranges are allowed in the second and third argument.	  |
| They are expanded in the following function, and the expanded strings,  |
| without any ranges left, are used to translate the characters of the	  |
| first argument.  A single - (dash) can be included in the strings by	  |
| being the first or the last character in the string.  If the first	  |
| character in a range is after the first in the character set, the range |
| is made backwards, thus 9-0 is the string 9876543210.			  |
`------------------------------------------------------------------------*/

static const char *
expand_ranges (const char *s, struct obstack *obs)
{
  char from;
  char to;

  for (from = '\0'; *s != '\0'; from = *s++)
    {
      if (*s == '-' && from != '\0')
	{
	  to = *++s;
	  if (to == '\0')
	    {
	      /* trailing dash */
	      obstack_1grow (obs, '-');
	      break;
	    }
	  else if (from <= to)
	    {
	      while (from++ < to)
		obstack_1grow (obs, from);
	    }
	  else
	    {
	      while (--from >= to)
		obstack_1grow (obs, from);
	    }
	}
      else
	obstack_1grow (obs, *s);
    }
  obstack_1grow (obs, '\0');
  return obstack_finish (obs);
}

/*----------------------------------------------------------------------.
| The macro "translit" translates all characters in the first argument, |
| which are present in the second argument, into the corresponding      |
| character from the third argument.  If the third argument is shorter  |
| than the second, the extra characters in the second argument, are     |
| deleted from the first (pueh).				        |
`----------------------------------------------------------------------*/

static void
m4_translit (struct obstack *obs, int argc, token_data **argv)
{
  register const char *data, *tmp;
  const char *from, *to;
  int tolen;

  if (bad_argc (argv[0], argc, 3, 4))
    {
      /* builtin(`translit') is blank, but translit(`abc') is abc.  */
      if (argc == 2)
	obstack_grow (obs, ARG (1), strlen (ARG (1)));
      return;
    }

  from = ARG (2);
  if (strchr (from, '-') != NULL)
    {
      from = expand_ranges (from, obs);
      if (from == NULL)
	return;
    }

  if (argc >= 4)
    {
      to = ARG (3);
      if (strchr (to, '-') != NULL)
	{
	  to = expand_ranges (to, obs);
	  if (to == NULL)
	    return;
	}
    }
  else
    to = "";

  tolen = strlen (to);

  for (data = ARG (1); *data; data++)
    {
      tmp = strchr (from, *data);
      if (tmp == NULL)
	{
	  obstack_1grow (obs, *data);
	}
      else
	{
	  if (tmp - from < tolen)
	    obstack_1grow (obs, *(to + (tmp - from)));
	}
    }
}

/*----------------------------------------------------------------------.
| Frontend for printf like formatting.  The function format () lives in |
| the file format.c.						        |
`----------------------------------------------------------------------*/

static void
m4_format (struct obstack *obs, int argc, token_data **argv)
{
  if (bad_argc (argv[0], argc, 2, -1))
    return;
  format (obs, argc - 1, argv + 1);
}

/*-------------------------------------------------------------------------.
| Function to perform substitution by regular expressions.  Used by the	   |
| builtins regexp and patsubst.  The changed text is placed on the	   |
| obstack.  The substitution is REPL, with \& substituted by this part of  |
| VICTIM matched by the last whole regular expression, taken from REGS[0], |
| and \N substituted by the text matched by the Nth parenthesized	   |
| sub-expression, taken from REGS[N].					   |
`-------------------------------------------------------------------------*/

static int substitute_warned = 0;

static void
substitute (struct obstack *obs, const char *victim, const char *repl,
	    struct re_registers *regs)
{
  register unsigned int ch;

  for (;;)
    {
      while ((ch = *repl++) != '\\')
	{
	  if (ch == '\0')
	    return;
	  obstack_1grow (obs, ch);
	}

      switch ((ch = *repl++))
	{
	case '0':
	  if (!substitute_warned)
	    {
	      M4ERROR ((warning_status, 0, "\
Warning: \\0 will disappear, use \\& instead in replacements"));
	      substitute_warned = 1;
	    }
	  /* Fall through.  */

	case '&':
	  obstack_grow (obs, victim + regs->start[0],
			regs->end[0] - regs->start[0]);
	  break;

	case '1': case '2': case '3': case '4': case '5': case '6':
	case '7': case '8': case '9':
	  ch -= '0';
	  if (regs->num_regs - 1 <= ch)
	    M4ERROR ((warning_status, 0, "\
Warning: sub-expression %d not present", ch));
	  else if (regs->end[ch] > 0)
	    obstack_grow (obs, victim + regs->start[ch],
			  regs->end[ch] - regs->start[ch]);
	  break;

	case '\0':
	  M4ERROR ((warning_status, 0, "\
Warning: trailing \\ ignored in replacement"));
	  return;

	default:
	  obstack_1grow (obs, ch);
	  break;
	}
    }
}

/*------------------------------------------.
| Initialize regular expression variables.  |
`------------------------------------------*/

static void
init_pattern_buffer (struct re_pattern_buffer *buf, struct re_registers *regs)
{
  buf->translate = NULL;
  buf->fastmap = NULL;
  buf->buffer = NULL;
  buf->allocated = 0;
  regs->start = NULL;
  regs->end = NULL;
}

/*----------------------------------------.
| Clean up regular expression variables.  |
`----------------------------------------*/

static void
free_pattern_buffer (struct re_pattern_buffer *buf, struct re_registers *regs)
{
  regfree (buf);
  free (regs->start);
  free (regs->end);
}

/*--------------------------------------------------------------------------.
| Regular expression version of index.  Given two arguments, expand to the  |
| index of the first match of the second argument (a regexp) in the first.  |
| Expand to -1 if here is no match.  Given a third argument, is changes	    |
| the expansion to this argument.					    |
`--------------------------------------------------------------------------*/

static void
m4_regexp (struct obstack *obs, int argc, token_data **argv)
{
  const char *victim;		/* first argument */
  const char *regexp;		/* regular expression */
  const char *repl;		/* replacement string */

  struct re_pattern_buffer buf;	/* compiled regular expression */
  struct re_registers regs;	/* for subexpression matches */
  const char *msg;		/* error message from re_compile_pattern */
  int startpos;			/* start position of match */
  int length;			/* length of first argument */

  if (bad_argc (argv[0], argc, 3, 4))
    {
      /* builtin(`regexp') is blank, but regexp(`abc') is 0.  */
      if (argc == 2)
	shipout_int (obs, 0);
      return;
    }

  victim = TOKEN_DATA_TEXT (argv[1]);
  regexp = TOKEN_DATA_TEXT (argv[2]);

  init_pattern_buffer (&buf, &regs);
  msg = re_compile_pattern (regexp, strlen (regexp), &buf);

  if (msg != NULL)
    {
      M4ERROR ((warning_status, 0,
		"bad regular expression: `%s': %s", regexp, msg));
      free_pattern_buffer (&buf, &regs);
      return;
    }

  length = strlen (victim);
  /* Avoid overhead of allocating regs if we won't use it.  */
  startpos = re_search (&buf, victim, length, 0, length,
			argc == 3 ? NULL : &regs);

  if (startpos == -2)
    M4ERROR ((warning_status, 0,
	       "error matching regular expression `%s'", regexp));
  else if (argc == 3)
    shipout_int (obs, startpos);
  else if (startpos >= 0)
    {
      repl = TOKEN_DATA_TEXT (argv[3]);
      substitute (obs, victim, repl, &regs);
    }

  free_pattern_buffer (&buf, &regs);
}

/*--------------------------------------------------------------------------.
| Substitute all matches of a regexp occuring in a string.  Each match of   |
| the second argument (a regexp) in the first argument is changed to the    |
| third argument, with \& substituted by the matched text, and \N	    |
| substituted by the text matched by the Nth parenthesized sub-expression.  |
`--------------------------------------------------------------------------*/

static void
m4_patsubst (struct obstack *obs, int argc, token_data **argv)
{
  const char *victim;		/* first argument */
  const char *regexp;		/* regular expression */

  struct re_pattern_buffer buf;	/* compiled regular expression */
  struct re_registers regs;	/* for subexpression matches */
  const char *msg;		/* error message from re_compile_pattern */
  int matchpos;			/* start position of match */
  int offset;			/* current match offset */
  int length;			/* length of first argument */

  if (bad_argc (argv[0], argc, 3, 4))
    {
      /* builtin(`patsubst') is blank, but patsubst(`abc') is abc.  */
      if (argc == 2)
	obstack_grow (obs, ARG (1), strlen (ARG (1)));
      return;
    }

  regexp = TOKEN_DATA_TEXT (argv[2]);

  init_pattern_buffer (&buf, &regs);
  msg = re_compile_pattern (regexp, strlen (regexp), &buf);

  if (msg != NULL)
    {
      M4ERROR ((warning_status, 0,
		"bad regular expression `%s': %s", regexp, msg));
      free (buf.buffer);
      return;
    }

  victim = TOKEN_DATA_TEXT (argv[1]);
  length = strlen (victim);

  offset = 0;
  matchpos = 0;
  while (offset <= length)
    {
      matchpos = re_search (&buf, victim, length,
			    offset, length - offset, &regs);
      if (matchpos < 0)
	{

	  /* Match failed -- either error or there is no match in the
	     rest of the string, in which case the rest of the string is
	     copied verbatim.  */

	  if (matchpos == -2)
	    M4ERROR ((warning_status, 0,
		      "error matching regular expression `%s'", regexp));
	  else if (offset < length)
	    obstack_grow (obs, victim + offset, length - offset);
	  break;
	}

      /* Copy the part of the string that was skipped by re_search ().  */

      if (matchpos > offset)
	obstack_grow (obs, victim + offset, matchpos - offset);

      /* Handle the part of the string that was covered by the match.  */

      substitute (obs, victim, ARG (3), &regs);

      /* Update the offset to the end of the match.  If the regexp
	 matched a null string, advance offset one more, to avoid
	 infinite loops.  */

      offset = regs.end[0];
      if (regs.start[0] == regs.end[0])
	obstack_1grow (obs, victim[offset++]);
    }
  obstack_1grow (obs, '\0');

  free_pattern_buffer (&buf, &regs);
}

/* Finally, a placeholder builtin.  This builtin is not installed by
   default, but when reading back frozen files, this is associated
   with any builtin we don't recognize (for example, if the frozen
   file was created with a changeword capable m4, but is then loaded
   by a different m4 that does not support changeword).  This way, we
   can keep 'm4 -R' quiet in the common case that the user did not
   know or care about the builtin when the frozen file was created,
   while still flagging it as a potential error if an attempt is made
   to actually use the builtin.  */

/*--------------------------------------------------------------------.
| Issue a warning that this macro is a placeholder for an unsupported |
| builtin that was requested while reloading a frozen file.           |
`--------------------------------------------------------------------*/

void
m4_placeholder (struct obstack *obs, int argc, token_data **argv)
{
  M4ERROR ((warning_status, 0, "\
builtin `%s' requested by frozen file is not supported", ARG (0)));
}

/*-------------------------------------------------------------------------.
| This function handles all expansion of user defined and predefined	   |
| macros.  It is called with an obstack OBS, where the macros expansion	   |
| will be placed, as an unfinished object.  SYM points to the macro	   |
| definition, giving the expansion text.  ARGC and ARGV are the arguments, |
| as usual.								   |
`-------------------------------------------------------------------------*/

void
expand_user_macro (struct obstack *obs, symbol *sym,
		   int argc, token_data **argv)
{
  register const char *text;
  int i;

  for (text = SYMBOL_TEXT (sym); *text != '\0';)
    {
      if (*text != '$')
	{
	  obstack_1grow (obs, *text);
	  text++;
	  continue;
	}
      text++;
      switch (*text)
	{
	case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
	  if (no_gnu_extensions)
	    {
	      i = *text++ - '0';
	    }
	  else
	    {
	      for (i = 0; isdigit (to_uchar (*text)); text++)
		i = i*10 + (*text - '0');
	    }
	  if (i < argc)
	    obstack_grow (obs, TOKEN_DATA_TEXT (argv[i]),
			  strlen (TOKEN_DATA_TEXT (argv[i])));
	  break;

	case '#':		/* number of arguments */
	  shipout_int (obs, argc - 1);
	  text++;
	  break;

	case '*':		/* all arguments */
	case '@':		/* ... same, but quoted */
	  dump_args (obs, argc, argv, ",", *text == '@');
	  text++;
	  break;

	default:
	  obstack_1grow (obs, '$');
	  break;
	}
    }
}
