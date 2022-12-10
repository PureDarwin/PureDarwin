/* Open and close files for Bison.

   Copyright (C) 1984, 1986, 1989, 1992, 2000, 2001, 2002, 2003, 2004, 2005
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

#include <config.h>
#include "system.h"

#include <error.h>
#include <get-errno.h>
#include <quote.h>
#include <xstrndup.h>

#include "complain.h"
#include "dirname.h"
#include "files.h"
#include "getargs.h"
#include "gram.h"
#include "stdio-safer.h"

struct obstack pre_prologue_obstack;
struct obstack post_prologue_obstack;

/* Initializing some values below (such SPEC_NAME_PREFIX to `yy') is
   tempting, but don't do that: for the time being our handling of the
   %directive vs --option leaves precedence to the options by deciding
   that if a %directive sets a variable which is really set (i.e., not
   NULL), then the %directive is ignored.  As a result, %name-prefix,
   for instance, will not be honored.  */

char const *spec_outfile = NULL;       /* for -o. */
char const *spec_file_prefix = NULL;   /* for -b. */
char const *spec_name_prefix = NULL;   /* for -p. */
char const *spec_verbose_file = NULL;  /* for --verbose. */
char const *spec_graph_file = NULL;    /* for -g. */
char const *spec_defines_file = NULL;  /* for --defines. */
char const *parser_file_name;

uniqstr grammar_file = NULL;
uniqstr current_file = NULL;

/* If --output=dir/foo.c was specified,
   DIR_PREFIX is `dir/' and ALL_BUT_EXT and ALL_BUT_TAB_EXT are `dir/foo'.

   If --output=dir/foo.tab.c was specified, DIR_PREFIX is `dir/',
   ALL_BUT_EXT is `dir/foo.tab', and ALL_BUT_TAB_EXT is `dir/foo'.

   If --output was not specified but --file-prefix=dir/foo was specified,
   ALL_BUT_EXT = `foo.tab' and ALL_BUT_TAB_EXT = `foo'.

   If neither --output nor --file was specified but the input grammar
   is name dir/foo.y, ALL_BUT_EXT and ALL_BUT_TAB_EXT are `foo'.

   If neither --output nor --file was specified, DIR_PREFIX is the
   empty string (meaning the current directory); otherwise it is
   `dir/'.  */

static char const *all_but_ext;
static char const *all_but_tab_ext;
char const *dir_prefix;

/* C source file extension (the parser source).  */
static char const *src_extension = NULL;
/* Header file extension (if option ``-d'' is specified).  */
static char const *header_extension = NULL;

/*-----------------------------------------------------------------.
| Return a newly allocated string composed of the concatenation of |
| STR1, and STR2.                                                  |
`-----------------------------------------------------------------*/

static char *
concat2 (char const *str1, char const *str2)
{
  size_t len = strlen (str1) + strlen (str2);
  char *res = xmalloc (len + 1);
  char *cp;
  cp = stpcpy (res, str1);
  cp = stpcpy (cp, str2);
  return res;
}

/*-----------------------------------------------------------------.
| Try to open file NAME with mode MODE, and print an error message |
| if fails.                                                        |
`-----------------------------------------------------------------*/

FILE *
xfopen (const char *name, const char *mode)
{
  FILE *ptr;

  ptr = fopen_safer (name, mode);
  if (!ptr)
    error (EXIT_FAILURE, get_errno (), _("cannot open file `%s'"), name);

  return ptr;
}

/*-------------------------------------------------------------.
| Try to close file PTR, and print an error message if fails.  |
`-------------------------------------------------------------*/

void
xfclose (FILE *ptr)
{
  if (ptr == NULL)
    return;

  if (ferror (ptr))
    error (EXIT_FAILURE, 0, _("I/O error"));

  if (fclose (ptr) != 0)
    error (EXIT_FAILURE, get_errno (), _("cannot close file"));
}


/*------------------------------------------------------------------.
| Compute ALL_BUT_EXT, ALL_BUT_TAB_EXT and output files extensions. |
`------------------------------------------------------------------*/

/* Replace all characters FROM by TO in the string IN.
   and returns a new allocated string.  */
static char *
tr (const char *in, char from, char to)
{
  char *temp;
  char *out = xmalloc (strlen (in) + 1);

  for (temp = out; *in; in++, out++)
    if (*in == from)
      *out = to;
    else
      *out = *in;
  *out = 0;
  return (temp);
}

/* Compute extensions from the grammar file extension.  */
static void
compute_exts_from_gf (const char *ext)
{
  src_extension = tr (ext, 'y', 'c');
  src_extension = tr (src_extension, 'Y', 'C');
  header_extension = tr (ext, 'y', 'h');
  header_extension = tr (header_extension, 'Y', 'H');
}

/* Compute extensions from the given c source file extension.  */
static void
compute_exts_from_src (const char *ext)
{
  /* We use this function when the user specifies `-o' or `--output',
     so the extenions must be computed unconditionally from the file name
     given by this option.  */
  src_extension = xstrdup (ext);
  header_extension = tr (ext, 'c', 'h');
  header_extension = tr (header_extension, 'C', 'H');
}


/* Decompose FILE_NAME in four parts: *BASE, *TAB, and *EXT, the fourth
   part, (the directory) is ranging from FILE_NAME to the char before
   *BASE, so we don't need an additional parameter.

   *EXT points to the last period in the basename, or NULL if none.

   If there is no *EXT, *TAB is NULL.  Otherwise, *TAB points to
   `.tab' or `_tab' if present right before *EXT, or is NULL. *TAB
   cannot be equal to *BASE.

   None are allocated, they are simply pointers to parts of FILE_NAME.
   Examples:

   '/tmp/foo.tab.c' -> *BASE = 'foo.tab.c', *TAB = '.tab.c', *EXT =
   '.c'

   'foo.c' -> *BASE = 'foo.c', *TAB = NULL, *EXT = '.c'

   'tab.c' -> *BASE = 'tab.c', *TAB = NULL, *EXT = '.c'

   '.tab.c' -> *BASE = '.tab.c', *TAB = NULL, *EXT = '.c'

   'foo.tab' -> *BASE = 'foo.tab', *TAB = NULL, *EXT = '.tab'

   'foo_tab' -> *BASE = 'foo_tab', *TAB = NULL, *EXT = NULL

   'foo' -> *BASE = 'foo', *TAB = NULL, *EXT = NULL.  */

static void
file_name_split (const char *file_name,
		 const char **base, const char **tab, const char **ext)
{
  *base = base_name (file_name);

  /* Look for the extension, i.e., look for the last dot. */
  *ext = strrchr (*base, '.');
  *tab = NULL;

  /* If there is an extension, check if there is a `.tab' part right
     before.  */
  if (*ext)
    {
      size_t baselen = *ext - *base;
      size_t dottablen = 4;
      if (dottablen < baselen
	  && (strncmp (*ext - dottablen, ".tab", dottablen) == 0
	      || strncmp (*ext - dottablen, "_tab", dottablen) == 0))
	*tab = *ext - dottablen;
    }
}


static void
compute_file_name_parts (void)
{
  const char *base, *tab, *ext;

  /* Compute ALL_BUT_EXT and ALL_BUT_TAB_EXT from SPEC_OUTFILE
     or GRAMMAR_FILE.

     The precise -o name will be used for FTABLE.  For other output
     files, remove the ".c" or ".tab.c" suffix.  */
  if (spec_outfile)
    {
      file_name_split (spec_outfile, &base, &tab, &ext);
      dir_prefix = xstrndup (spec_outfile, base - spec_outfile);

      /* ALL_BUT_EXT goes up the EXT, excluding it. */
      all_but_ext =
	xstrndup (spec_outfile,
		  (strlen (spec_outfile) - (ext ? strlen (ext) : 0)));

      /* ALL_BUT_TAB_EXT goes up to TAB, excluding it.  */
      all_but_tab_ext =
	xstrndup (spec_outfile,
		  (strlen (spec_outfile)
		   - (tab ? strlen (tab) : (ext ? strlen (ext) : 0))));

      if (ext)
	compute_exts_from_src (ext);
    }
  else
    {
      file_name_split (grammar_file, &base, &tab, &ext);

      if (spec_file_prefix)
	{
	  /* If --file-prefix=foo was specified, ALL_BUT_TAB_EXT = `foo'.  */
	  dir_prefix = xstrndup (grammar_file, base - grammar_file);
	  all_but_tab_ext = xstrdup (spec_file_prefix);
	}
      else if (yacc_flag)
	{
	  /* If --yacc, then the output is `y.tab.c'.  */
	  dir_prefix = "";
	  all_but_tab_ext = "y";
	}
      else
	{
	  /* Otherwise, ALL_BUT_TAB_EXT is computed from the input
	     grammar: `foo/bar.yy' => `bar'.  */
	  dir_prefix = "";
	  all_but_tab_ext =
	    xstrndup (base, (strlen (base) - (ext ? strlen (ext) : 0)));
	}

      all_but_ext = concat2 (all_but_tab_ext, TAB_EXT);

      /* Compute the extensions from the grammar file name.  */
      if (ext && !yacc_flag)
	compute_exts_from_gf (ext);
    }
}


/* Compute the output file names.  Warn if we detect conflicting
   outputs to the same file.  */

void
compute_output_file_names (void)
{
  char const *name[4];
  int i;
  int j;
  int names = 0;

  compute_file_name_parts ();

  /* If not yet done. */
  if (!src_extension)
    src_extension = ".c";
  if (!header_extension)
    header_extension = ".h";

  name[names++] = parser_file_name =
    spec_outfile ? spec_outfile : concat2 (all_but_ext, src_extension);

  if (defines_flag)
    {
      if (! spec_defines_file)
	spec_defines_file = concat2 (all_but_ext, header_extension);
      name[names++] = spec_defines_file;
    }

  if (graph_flag)
    {
      if (! spec_graph_file)
	spec_graph_file = concat2 (all_but_tab_ext, ".vcg");
      name[names++] = spec_graph_file;
    }

  if (report_flag)
    {
      spec_verbose_file = concat2 (all_but_tab_ext, OUTPUT_EXT);
      name[names++] = spec_verbose_file;
    }

  for (j = 0; j < names; j++)
    for (i = 0; i < j; i++)
      if (strcmp (name[i], name[j]) == 0)
	warn (_("conflicting outputs to file %s"), quote (name[i]));
}
