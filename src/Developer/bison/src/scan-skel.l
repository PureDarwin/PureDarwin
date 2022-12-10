/* Scan Bison Skeletons.                                       -*- C -*-

   Copyright (C) 2001, 2002, 2003, 2004, 2005, 2006 Free Software
   Foundation, Inc.

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

%option nodefault noyywrap nounput never-interactive debug
%option prefix="skel_" outfile="lex.yy.c"

%{
/* Work around a bug in flex 2.5.31.  See Debian bug 333231
   <http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=333231>.  */
#undef skel_wrap
#define skel_wrap() 1

#include "system.h"

#include <error.h>
#include <quotearg.h>

#include "complain.h"
#include "getargs.h"
#include "files.h"
#include "scan-skel.h"

/* Pacify "gcc -Wmissing-prototypes" when flex 2.5.31 is used.  */
int skel_lex (void);
int skel_get_lineno (void);
FILE *skel_get_in (void);
FILE *skel_get_out (void);
int skel_get_leng (void);
char *skel_get_text (void);
void skel_set_lineno (int);
void skel_set_in (FILE *);
void skel_set_out (FILE *);
int skel_get_debug (void);
void skel_set_debug (int);
int skel_lex_destroy (void);

#define QPUTS(String) \
   fputs (quotearg_style (c_quoting_style, String), yyout)

#define BASE_QPUTS(File) \
   QPUTS (base_name (File))

%}
%%

%{
  int lineno IF_LINT (= 0);
  char *outname = NULL;
%}

"@output ".*\n {
  char const *file_name = yytext + sizeof "@output " - 1;
  yytext[yyleng - 1] = '\0';

  /* Decode special file names.  They include the directory part,
     contrary to their "free" occurrences, used for issuing #includes,
     which must not include the directory part.  */

  if (*file_name == '@')
    {
      if (strcmp (file_name, "@output_header_name@") == 0)
	file_name = spec_defines_file;
      else if (strcmp (file_name, "@output_parser_name@") == 0)
	file_name = parser_file_name;
      else
	fatal ("invalid token in skeleton: %s", yytext);
    }
  if (outname)
    {
      free (outname);
      xfclose (yyout);
    }
  outname = xstrdup (file_name);
  yyout = xfopen (outname, "w");
  lineno = 1;
}

"@@"	   fputc ('@', yyout);
"@{"	   fputc ('[', yyout);
"@}"	   fputc (']', yyout);

"@oline@"  fprintf (yyout, "%d", lineno + 1);
"@ofile@"  QPUTS (outname);
"@dir_prefix@" QPUTS (dir_prefix);
"@output_parser_name@"	BASE_QPUTS (parser_file_name);
"@output_header_name@"	BASE_QPUTS (spec_defines_file);

  /* This pattern must not match more than the previous @ patterns. */
@[^{}@\n]* fatal ("invalid @ in skeleton: %s", yytext);
\n	   lineno++; ECHO;
[^@\n]+	   ECHO;

<<EOF>> {
  if (outname)
    {
      free (outname);
      xfclose (yyout);
    }
  return EOF;
}
%%

/*------------------------.
| Scan a Bison skeleton.  |
`------------------------*/

void
scan_skel (FILE *in)
{
  skel_in = in;
  skel__flex_debug = trace_flag & trace_skeleton;
  skel_lex ();
  /* Reclaim Flex's buffers.  */
  yy_delete_buffer (YY_CURRENT_BUFFER);
}
