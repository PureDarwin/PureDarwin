/* Bison Grammar Scanner                             -*- C -*-

   Copyright (C) 2002, 2003, 2004, 2005, 2006 Free Software Foundation, Inc.

   This file is part of Bison, the GNU Compiler Compiler.

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

%option debug nodefault nounput noyywrap never-interactive
%option prefix="gram_" outfile="lex.yy.c"

%{
/* Work around a bug in flex 2.5.31.  See Debian bug 333231
   <http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=333231>.  */
#undef gram_wrap
#define gram_wrap() 1

#include "system.h"

#include <mbswidth.h>
#include <quote.h>

#include "complain.h"
#include "files.h"
#include "getargs.h"
#include "gram.h"
#include "quotearg.h"
#include "reader.h"
#include "uniqstr.h"

#define YY_USER_INIT					\
  do							\
    {							\
      scanner_cursor.file = current_file;		\
      scanner_cursor.line = 1;				\
      scanner_cursor.column = 1;			\
      code_start = scanner_cursor;			\
    }							\
  while (0)

/* Pacify "gcc -Wmissing-prototypes" when flex 2.5.31 is used.  */
int gram_get_lineno (void);
FILE *gram_get_in (void);
FILE *gram_get_out (void);
int gram_get_leng (void);
char *gram_get_text (void);
void gram_set_lineno (int);
void gram_set_in (FILE *);
void gram_set_out (FILE *);
int gram_get_debug (void);
void gram_set_debug (int);
int gram_lex_destroy (void);

/* Location of scanner cursor.  */
boundary scanner_cursor;

static void adjust_location (location *, char const *, size_t);
#define YY_USER_ACTION  adjust_location (loc, yytext, yyleng);

static size_t no_cr_read (FILE *, char *, size_t);
#define YY_INPUT(buf, result, size) ((result) = no_cr_read (yyin, buf, size))


/* OBSTACK_FOR_STRING -- Used to store all the characters that we need to
   keep (to construct ID, STRINGS etc.).  Use the following macros to
   use it.

   Use STRING_GROW to append what has just been matched, and
   STRING_FINISH to end the string (it puts the ending 0).
   STRING_FINISH also stores this string in LAST_STRING, which can be
   used, and which is used by STRING_FREE to free the last string.  */

static struct obstack obstack_for_string;

/* A string representing the most recently saved token.  */
char *last_string;

/* The location of the most recently saved token, if it was a
   BRACED_CODE token; otherwise, this has an unspecified value.  */
location last_braced_code_loc;

#define STRING_GROW   \
  obstack_grow (&obstack_for_string, yytext, yyleng)

#define STRING_FINISH					\
  do {							\
    obstack_1grow (&obstack_for_string, '\0');		\
    last_string = obstack_finish (&obstack_for_string);	\
  } while (0)

#define STRING_FREE \
  obstack_free (&obstack_for_string, last_string)

void
scanner_last_string_free (void)
{
  STRING_FREE;
}

/* Within well-formed rules, RULE_LENGTH is the number of values in
   the current rule so far, which says where to find `$0' with respect
   to the top of the stack.  It is not the same as the rule->length in
   the case of mid rule actions.

   Outside of well-formed rules, RULE_LENGTH has an undefined value.  */
static int rule_length;

static void rule_length_overflow (location) __attribute__ ((__noreturn__));

/* Increment the rule length by one, checking for overflow.  */
static inline void
increment_rule_length (location loc)
{
  rule_length++;

  /* Don't allow rule_length == INT_MAX, since that might cause
     confusion with strtol if INT_MAX == LONG_MAX.  */
  if (rule_length == INT_MAX)
    rule_length_overflow (loc);
}

static void handle_dollar (int token_type, char *cp, location loc);
static void handle_at (int token_type, char *cp, location loc);
static void handle_syncline (char *, location);
static unsigned long int scan_integer (char const *p, int base, location loc);
static int convert_ucn_to_byte (char const *hex_text);
static void unexpected_eof (boundary, char const *);
static void unexpected_newline (boundary, char const *);

%}
%x SC_COMMENT SC_LINE_COMMENT SC_YACC_COMMENT
%x SC_STRING SC_CHARACTER
%x SC_AFTER_IDENTIFIER
%x SC_ESCAPED_STRING SC_ESCAPED_CHARACTER
%x SC_PRE_CODE SC_BRACED_CODE SC_PROLOGUE SC_EPILOGUE

letter	  [.abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_]
id	  {letter}({letter}|[0-9])*
directive %{letter}({letter}|[0-9]|-)*
int	  [0-9]+

/* POSIX says that a tag must be both an id and a C union member, but
   historically almost any character is allowed in a tag.  We disallow
   NUL and newline, as this simplifies our implementation.  */
tag	 [^\0\n>]+

/* Zero or more instances of backslash-newline.  Following GCC, allow
   white space between the backslash and the newline.  */
splice	 (\\[ \f\t\v]*\n)*

%%
%{
  /* Nesting level of the current code in braces.  */
  int braces_level IF_LINT (= 0);

  /* Parent context state, when applicable.  */
  int context_state IF_LINT (= 0);

  /* Token type to return, when applicable.  */
  int token_type IF_LINT (= 0);

  /* Location of most recent identifier, when applicable.  */
  location id_loc IF_LINT (= empty_location);

  /* Where containing code started, when applicable.  Its initial
     value is relevant only when yylex is invoked in the SC_EPILOGUE
     start condition.  */
  boundary code_start = scanner_cursor;

  /* Where containing comment or string or character literal started,
     when applicable.  */
  boundary token_start IF_LINT (= scanner_cursor);
%}


  /*-----------------------.
  | Scanning white space.  |
  `-----------------------*/

<INITIAL,SC_AFTER_IDENTIFIER,SC_PRE_CODE>
{
  /* Comments and white space.  */
  ","	       warn_at (*loc, _("stray `,' treated as white space"));
  [ \f\n\t\v]  |
  "//".*       ;
  "/*" {
    token_start = loc->start;
    context_state = YY_START;
    BEGIN SC_YACC_COMMENT;
  }

  /* #line directives are not documented, and may be withdrawn or
     modified in future versions of Bison.  */
  ^"#line "{int}" \"".*"\"\n" {
    handle_syncline (yytext + sizeof "#line " - 1, *loc);
  }
}


  /*----------------------------.
  | Scanning Bison directives.  |
  `----------------------------*/
<INITIAL>
{
  "%binary"               return PERCENT_NONASSOC;
  "%debug"                return PERCENT_DEBUG;
  "%default"[-_]"prec"    return PERCENT_DEFAULT_PREC;
  "%define"               return PERCENT_DEFINE;
  "%defines"              return PERCENT_DEFINES;
  "%destructor"		  token_type = PERCENT_DESTRUCTOR; BEGIN SC_PRE_CODE;
  "%dprec"		  return PERCENT_DPREC;
  "%error"[-_]"verbose"   return PERCENT_ERROR_VERBOSE;
  "%expect"               return PERCENT_EXPECT;
  "%expect"[-_]"rr"	  return PERCENT_EXPECT_RR;
  "%file-prefix"          return PERCENT_FILE_PREFIX;
  "%fixed"[-_]"output"[-_]"files"   return PERCENT_YACC;
  "%initial-action"       token_type = PERCENT_INITIAL_ACTION; BEGIN SC_PRE_CODE;
  "%glr-parser"           return PERCENT_GLR_PARSER;
  "%left"                 return PERCENT_LEFT;
  "%lex-param"		  token_type = PERCENT_LEX_PARAM; BEGIN SC_PRE_CODE;
  "%locations"            return PERCENT_LOCATIONS;
  "%merge"		  return PERCENT_MERGE;
  "%name"[-_]"prefix"     return PERCENT_NAME_PREFIX;
  "%no"[-_]"default"[-_]"prec"	return PERCENT_NO_DEFAULT_PREC;
  "%no"[-_]"lines"        return PERCENT_NO_LINES;
  "%nonassoc"             return PERCENT_NONASSOC;
  "%nondeterministic-parser"   return PERCENT_NONDETERMINISTIC_PARSER;
  "%nterm"                return PERCENT_NTERM;
  "%output"               return PERCENT_OUTPUT;
  "%parse-param"	  token_type = PERCENT_PARSE_PARAM; BEGIN SC_PRE_CODE;
  "%prec"                 rule_length--; return PERCENT_PREC;
  "%printer"              token_type = PERCENT_PRINTER; BEGIN SC_PRE_CODE;
  "%pure"[-_]"parser"     return PERCENT_PURE_PARSER;
  "%require"              return PERCENT_REQUIRE;
  "%right"                return PERCENT_RIGHT;
  "%skeleton"             return PERCENT_SKELETON;
  "%start"                return PERCENT_START;
  "%term"                 return PERCENT_TOKEN;
  "%token"                return PERCENT_TOKEN;
  "%token"[-_]"table"     return PERCENT_TOKEN_TABLE;
  "%type"                 return PERCENT_TYPE;
  "%union"		  token_type = PERCENT_UNION; BEGIN SC_PRE_CODE;
  "%verbose"              return PERCENT_VERBOSE;
  "%yacc"                 return PERCENT_YACC;

  {directive} {
    complain_at (*loc, _("invalid directive: %s"), quote (yytext));
  }

  "="                     return EQUAL;
  "|"                     rule_length = 0; return PIPE;
  ";"                     return SEMICOLON;

  {id} {
    val->symbol = symbol_get (yytext, *loc);
    id_loc = *loc;
    increment_rule_length (*loc);
    BEGIN SC_AFTER_IDENTIFIER;
  }

  {int} {
    val->integer = scan_integer (yytext, 10, *loc);
    return INT;
  }
  0[xX][0-9abcdefABCDEF]+ {
    val->integer = scan_integer (yytext, 16, *loc);
    return INT;
  }

  /* Characters.  We don't check there is only one.  */
  "'"	      STRING_GROW; token_start = loc->start; BEGIN SC_ESCAPED_CHARACTER;

  /* Strings. */
  "\""	      token_start = loc->start; BEGIN SC_ESCAPED_STRING;

  /* Prologue. */
  "%{"        code_start = loc->start; BEGIN SC_PROLOGUE;

  /* Code in between braces.  */
  "{" {
    if (current_rule && current_rule->action)
      grammar_midrule_action ();
    STRING_GROW;
    token_type = BRACED_CODE;
    braces_level = 0;
    code_start = loc->start;
    BEGIN SC_BRACED_CODE;
  }

  /* A type. */
  "<"{tag}">" {
    obstack_grow (&obstack_for_string, yytext + 1, yyleng - 2);
    STRING_FINISH;
    val->uniqstr = uniqstr_new (last_string);
    STRING_FREE;
    return TYPE;
  }

  "%%" {
    static int percent_percent_count;
    if (++percent_percent_count == 2)
      BEGIN SC_EPILOGUE;
    return PERCENT_PERCENT;
  }

  . {
    complain_at (*loc, _("invalid character: %s"), quote (yytext));
  }

  <<EOF>> {
    loc->start = loc->end = scanner_cursor;
    yyterminate ();
  }
}


  /*-----------------------------------------------------------------.
  | Scanning after an identifier, checking whether a colon is next.  |
  `-----------------------------------------------------------------*/

<SC_AFTER_IDENTIFIER>
{
  ":" {
    rule_length = 0;
    *loc = id_loc;
    BEGIN INITIAL;
    return ID_COLON;
  }
  . {
    scanner_cursor.column -= mbsnwidth (yytext, yyleng, 0);
    yyless (0);
    *loc = id_loc;
    BEGIN INITIAL;
    return ID;
  }
  <<EOF>> {
    *loc = id_loc;
    BEGIN INITIAL;
    return ID;
  }
}


  /*---------------------------------------------------------------.
  | Scanning a Yacc comment.  The initial `/ *' is already eaten.  |
  `---------------------------------------------------------------*/

<SC_YACC_COMMENT>
{
  "*/"     BEGIN context_state;
  .|\n	   ;
  <<EOF>>  unexpected_eof (token_start, "*/"); BEGIN context_state;
}


  /*------------------------------------------------------------.
  | Scanning a C comment.  The initial `/ *' is already eaten.  |
  `------------------------------------------------------------*/

<SC_COMMENT>
{
  "*"{splice}"/"  STRING_GROW; BEGIN context_state;
  <<EOF>>	  unexpected_eof (token_start, "*/"); BEGIN context_state;
}


  /*--------------------------------------------------------------.
  | Scanning a line comment.  The initial `//' is already eaten.  |
  `--------------------------------------------------------------*/

<SC_LINE_COMMENT>
{
  "\n"		 STRING_GROW; BEGIN context_state;
  {splice}	 STRING_GROW;
  <<EOF>>	 BEGIN context_state;
}


  /*------------------------------------------------.
  | Scanning a Bison string, including its escapes. |
  | The initial quote is already eaten.             |
  `------------------------------------------------*/

<SC_ESCAPED_STRING>
{
  "\"" {
    STRING_FINISH;
    loc->start = token_start;
    val->chars = last_string;
    increment_rule_length (*loc);
    BEGIN INITIAL;
    return STRING;
  }
  \n		unexpected_newline (token_start, "\"");	BEGIN INITIAL;
  <<EOF>>	unexpected_eof (token_start, "\"");	BEGIN INITIAL;
}

  /*----------------------------------------------------------.
  | Scanning a Bison character literal, decoding its escapes. |
  | The initial quote is already eaten.			      |
  `----------------------------------------------------------*/

<SC_ESCAPED_CHARACTER>
{
  "'" {
    unsigned char last_string_1;
    STRING_GROW;
    STRING_FINISH;
    loc->start = token_start;
    val->symbol = symbol_get (quotearg_style (escape_quoting_style,
					      last_string),
			      *loc);
    symbol_class_set (val->symbol, token_sym, *loc, false);
    last_string_1 = last_string[1];
    symbol_user_token_number_set (val->symbol, last_string_1, *loc);
    STRING_FREE;
    increment_rule_length (*loc);
    BEGIN INITIAL;
    return ID;
  }
  \n		unexpected_newline (token_start, "'");	BEGIN INITIAL;
  <<EOF>>	unexpected_eof (token_start, "'");	BEGIN INITIAL;
}

<SC_ESCAPED_CHARACTER,SC_ESCAPED_STRING>
{
  \0	    complain_at (*loc, _("invalid null character"));
}


  /*----------------------------.
  | Decode escaped characters.  |
  `----------------------------*/

<SC_ESCAPED_STRING,SC_ESCAPED_CHARACTER>
{
  \\[0-7]{1,3} {
    unsigned long int c = strtoul (yytext + 1, NULL, 8);
    if (UCHAR_MAX < c)
      complain_at (*loc, _("invalid escape sequence: %s"), quote (yytext));
    else if (! c)
      complain_at (*loc, _("invalid null character: %s"), quote (yytext));
    else
      obstack_1grow (&obstack_for_string, c);
  }

  \\x[0-9abcdefABCDEF]+ {
    verify (UCHAR_MAX < ULONG_MAX);
    unsigned long int c = strtoul (yytext + 2, NULL, 16);
    if (UCHAR_MAX < c)
      complain_at (*loc, _("invalid escape sequence: %s"), quote (yytext));
    else if (! c)
      complain_at (*loc, _("invalid null character: %s"), quote (yytext));
    else
      obstack_1grow (&obstack_for_string, c);
  }

  \\a	obstack_1grow (&obstack_for_string, '\a');
  \\b	obstack_1grow (&obstack_for_string, '\b');
  \\f	obstack_1grow (&obstack_for_string, '\f');
  \\n	obstack_1grow (&obstack_for_string, '\n');
  \\r	obstack_1grow (&obstack_for_string, '\r');
  \\t	obstack_1grow (&obstack_for_string, '\t');
  \\v	obstack_1grow (&obstack_for_string, '\v');

  /* \\[\"\'?\\] would be shorter, but it confuses xgettext.  */
  \\("\""|"'"|"?"|"\\")  obstack_1grow (&obstack_for_string, yytext[1]);

  \\(u|U[0-9abcdefABCDEF]{4})[0-9abcdefABCDEF]{4} {
    int c = convert_ucn_to_byte (yytext);
    if (c < 0)
      complain_at (*loc, _("invalid escape sequence: %s"), quote (yytext));
    else if (! c)
      complain_at (*loc, _("invalid null character: %s"), quote (yytext));
    else
      obstack_1grow (&obstack_for_string, c);
  }
  \\(.|\n)	{
    complain_at (*loc, _("unrecognized escape sequence: %s"), quote (yytext));
    STRING_GROW;
  }
}

  /*--------------------------------------------.
  | Scanning user-code characters and strings.  |
  `--------------------------------------------*/

<SC_CHARACTER,SC_STRING>
{
  {splice}|\\{splice}[^\n$@\[\]]	STRING_GROW;
}

<SC_CHARACTER>
{
  "'"		STRING_GROW; BEGIN context_state;
  \n		unexpected_newline (token_start, "'"); BEGIN context_state;
  <<EOF>>	unexpected_eof (token_start, "'"); BEGIN context_state;
}

<SC_STRING>
{
  "\""		STRING_GROW; BEGIN context_state;
  \n		unexpected_newline (token_start, "\""); BEGIN context_state;
  <<EOF>>	unexpected_eof (token_start, "\""); BEGIN context_state;
}


  /*---------------------------------------------------.
  | Strings, comments etc. can be found in user code.  |
  `---------------------------------------------------*/

<SC_BRACED_CODE,SC_PROLOGUE,SC_EPILOGUE>
{
  "'" {
    STRING_GROW;
    context_state = YY_START;
    token_start = loc->start;
    BEGIN SC_CHARACTER;
  }
  "\"" {
    STRING_GROW;
    context_state = YY_START;
    token_start = loc->start;
    BEGIN SC_STRING;
  }
  "/"{splice}"*" {
    STRING_GROW;
    context_state = YY_START;
    token_start = loc->start;
    BEGIN SC_COMMENT;
  }
  "/"{splice}"/" {
    STRING_GROW;
    context_state = YY_START;
    BEGIN SC_LINE_COMMENT;
  }
}


  /*---------------------------------------------------------------.
  | Scanning after %union etc., possibly followed by white space.  |
  | For %union only, allow arbitrary C code to appear before the   |
  | following brace, as an extension to POSIX.			   |
  `---------------------------------------------------------------*/

<SC_PRE_CODE>
{
  . {
    bool valid = yytext[0] == '{' || token_type == PERCENT_UNION;
    scanner_cursor.column -= mbsnwidth (yytext, yyleng, 0);
    yyless (0);

    if (valid)
      {
	braces_level = -1;
	code_start = loc->start;
	BEGIN SC_BRACED_CODE;
      }
    else
      {
	complain_at (*loc, _("missing `{' in %s"),
		     token_name (token_type));
	obstack_sgrow (&obstack_for_string, "{}");
	STRING_FINISH;
	val->chars = last_string;
	BEGIN INITIAL;
	return token_type;
      }
  }

  <<EOF>>  unexpected_eof (scanner_cursor, "{}"); BEGIN INITIAL;
}


  /*---------------------------------------------------------------.
  | Scanning some code in braces (%union and actions). The initial |
  | "{" is already eaten.                                          |
  `---------------------------------------------------------------*/

<SC_BRACED_CODE>
{
  "{"|"<"{splice}"%"  STRING_GROW; braces_level++;
  "%"{splice}">"      STRING_GROW; braces_level--;
  "}" {
    bool outer_brace = --braces_level < 0;

    /* As an undocumented Bison extension, append `;' before the last
       brace in braced code, so that the user code can omit trailing
       `;'.  But do not append `;' if emulating Yacc, since Yacc does
       not append one.

       FIXME: Bison should warn if a semicolon seems to be necessary
       here, and should omit the semicolon if it seems unnecessary
       (e.g., after ';', '{', or '}', each followed by comments or
       white space).  Such a warning shouldn't depend on --yacc; it
       should depend on a new --pedantic option, which would cause
       Bison to warn if it detects an extension to POSIX.  --pedantic
       should also diagnose other Bison extensions like %yacc.
       Perhaps there should also be a GCC-style --pedantic-errors
       option, so that such warnings are diagnosed as errors.  */
    if (outer_brace && token_type == BRACED_CODE && ! yacc_flag)
      obstack_1grow (&obstack_for_string, ';');

    obstack_1grow (&obstack_for_string, '}');

    if (outer_brace)
      {
	STRING_FINISH;
	loc->start = code_start;
	val->chars = last_string;
	increment_rule_length (*loc);
	last_braced_code_loc = *loc;
	BEGIN INITIAL;
	return token_type;
      }
  }

  /* Tokenize `<<%' correctly (as `<<' `%') rather than incorrrectly
     (as `<' `<%').  */
  "<"{splice}"<"  STRING_GROW;

  "$"("<"{tag}">")?(-?[0-9]+|"$")  handle_dollar (token_type, yytext, *loc);
  "@"(-?[0-9]+|"$")		   handle_at (token_type, yytext, *loc);

  "$"  {
    warn_at (*loc, _("stray `$'"));
    obstack_sgrow (&obstack_for_string, "$][");
  }
  "@"  {
    warn_at (*loc, _("stray `@'"));
    obstack_sgrow (&obstack_for_string, "@@");
  }

  <<EOF>>  unexpected_eof (code_start, "}"); BEGIN INITIAL;
}


  /*--------------------------------------------------------------.
  | Scanning some prologue: from "%{" (already scanned) to "%}".  |
  `--------------------------------------------------------------*/

<SC_PROLOGUE>
{
  "%}" {
    STRING_FINISH;
    loc->start = code_start;
    val->chars = last_string;
    BEGIN INITIAL;
    return PROLOGUE;
  }

  <<EOF>>  unexpected_eof (code_start, "%}"); BEGIN INITIAL;
}


  /*---------------------------------------------------------------.
  | Scanning the epilogue (everything after the second "%%", which |
  | has already been eaten).                                       |
  `---------------------------------------------------------------*/

<SC_EPILOGUE>
{
  <<EOF>> {
    STRING_FINISH;
    loc->start = code_start;
    val->chars = last_string;
    BEGIN INITIAL;
    return EPILOGUE;
  }
}


  /*-----------------------------------------.
  | Escape M4 quoting characters in C code.  |
  `-----------------------------------------*/

<SC_COMMENT,SC_LINE_COMMENT,SC_STRING,SC_CHARACTER,SC_BRACED_CODE,SC_PROLOGUE,SC_EPILOGUE>
{
  \$	obstack_sgrow (&obstack_for_string, "$][");
  \@	obstack_sgrow (&obstack_for_string, "@@");
  \[	obstack_sgrow (&obstack_for_string, "@{");
  \]	obstack_sgrow (&obstack_for_string, "@}");
}


  /*-----------------------------------------------------.
  | By default, grow the string obstack with the input.  |
  `-----------------------------------------------------*/

<SC_COMMENT,SC_LINE_COMMENT,SC_BRACED_CODE,SC_PROLOGUE,SC_EPILOGUE,SC_STRING,SC_CHARACTER,SC_ESCAPED_STRING,SC_ESCAPED_CHARACTER>.	|
<SC_COMMENT,SC_LINE_COMMENT,SC_BRACED_CODE,SC_PROLOGUE,SC_EPILOGUE>\n	STRING_GROW;

%%

/* Keeps track of the maximum number of semantic values to the left of
   a handle (those referenced by $0, $-1, etc.) are required by the
   semantic actions of this grammar. */
int max_left_semantic_context = 0;

/* If BUF is null, add BUFSIZE (which in this case must be less than
   INT_MAX) to COLUMN; otherwise, add mbsnwidth (BUF, BUFSIZE, 0) to
   COLUMN.  If an overflow occurs, or might occur but is undetectable,
   return INT_MAX.  Assume COLUMN is nonnegative.  */

static inline int
add_column_width (int column, char const *buf, size_t bufsize)
{
  size_t width;
  unsigned int remaining_columns = INT_MAX - column;

  if (buf)
    {
      if (INT_MAX / 2 <= bufsize)
	return INT_MAX;
      width = mbsnwidth (buf, bufsize, 0);
    }
  else
    width = bufsize;

  return width <= remaining_columns ? column + width : INT_MAX;
}

/* Set *LOC and adjust scanner cursor to account for token TOKEN of
   size SIZE.  */

static void
adjust_location (location *loc, char const *token, size_t size)
{
  int line = scanner_cursor.line;
  int column = scanner_cursor.column;
  char const *p0 = token;
  char const *p = token;
  char const *lim = token + size;

  loc->start = scanner_cursor;

  for (p = token; p < lim; p++)
    switch (*p)
      {
      case '\n':
	line += line < INT_MAX;
	column = 1;
	p0 = p + 1;
	break;

      case '\t':
	column = add_column_width (column, p0, p - p0);
	column = add_column_width (column, NULL, 8 - ((column - 1) & 7));
	p0 = p + 1;
	break;

      default:
	break;
      }

  scanner_cursor.line = line;
  scanner_cursor.column = column = add_column_width (column, p0, p - p0);

  loc->end = scanner_cursor;

  if (line == INT_MAX && loc->start.line != INT_MAX)
    warn_at (*loc, _("line number overflow"));
  if (column == INT_MAX && loc->start.column != INT_MAX)
    warn_at (*loc, _("column number overflow"));
}


/* Read bytes from FP into buffer BUF of size SIZE.  Return the
   number of bytes read.  Remove '\r' from input, treating \r\n
   and isolated \r as \n.  */

static size_t
no_cr_read (FILE *fp, char *buf, size_t size)
{
  size_t bytes_read = fread (buf, 1, size, fp);
  if (bytes_read)
    {
      char *w = memchr (buf, '\r', bytes_read);
      if (w)
	{
	  char const *r = ++w;
	  char const *lim = buf + bytes_read;

	  for (;;)
	    {
	      /* Found an '\r'.  Treat it like '\n', but ignore any
		 '\n' that immediately follows.  */
	      w[-1] = '\n';
	      if (r == lim)
		{
		  int ch = getc (fp);
		  if (ch != '\n' && ungetc (ch, fp) != ch)
		    break;
		}
	      else if (*r == '\n')
		r++;

	      /* Copy until the next '\r'.  */
	      do
		{
		  if (r == lim)
		    return w - buf;
		}
	      while ((*w++ = *r++) != '\r');
	    }

	  return w - buf;
	}
    }

  return bytes_read;
}


/*------------------------------------------------------------------.
| TEXT is pointing to a wannabee semantic value (i.e., a `$').      |
|                                                                   |
| Possible inputs: $[<TYPENAME>]($|integer)                         |
|                                                                   |
| Output to OBSTACK_FOR_STRING a reference to this semantic value.  |
`------------------------------------------------------------------*/

static inline bool
handle_action_dollar (char *text, location loc)
{
  const char *type_name = NULL;
  char *cp = text + 1;

  if (! current_rule)
    return false;

  /* Get the type name if explicit. */
  if (*cp == '<')
    {
      type_name = ++cp;
      while (*cp != '>')
	++cp;
      *cp = '\0';
      ++cp;
    }

  if (*cp == '$')
    {
      if (!type_name)
	type_name = symbol_list_n_type_name_get (current_rule, loc, 0);
      if (!type_name && typed)
	complain_at (loc, _("$$ of `%s' has no declared type"),
		     current_rule->sym->tag);
      if (!type_name)
	type_name = "";
      obstack_fgrow1 (&obstack_for_string,
		      "]b4_lhs_value([%s])[", type_name);
      current_rule->used = true;
    }
  else
    {
      long int num = strtol (cp, NULL, 10);

      if (1 - INT_MAX + rule_length <= num && num <= rule_length)
	{
	  int n = num;
	  if (max_left_semantic_context < 1 - n)
	    max_left_semantic_context = 1 - n;
	  if (!type_name && 0 < n)
	    type_name = symbol_list_n_type_name_get (current_rule, loc, n);
	  if (!type_name && typed)
	    complain_at (loc, _("$%d of `%s' has no declared type"),
			 n, current_rule->sym->tag);
	  if (!type_name)
	    type_name = "";
	  obstack_fgrow3 (&obstack_for_string,
			  "]b4_rhs_value(%d, %d, [%s])[",
			  rule_length, n, type_name);
	  symbol_list_n_used_set (current_rule, n, true);
	}
      else
	complain_at (loc, _("integer out of range: %s"), quote (text));
    }

  return true;
}


/*----------------------------------------------------------------.
| Map `$?' onto the proper M4 symbol, depending on its TOKEN_TYPE |
| (are we in an action?).                                         |
`----------------------------------------------------------------*/

static void
handle_dollar (int token_type, char *text, location loc)
{
  switch (token_type)
    {
    case BRACED_CODE:
      if (handle_action_dollar (text, loc))
	return;
      break;

    case PERCENT_DESTRUCTOR:
    case PERCENT_INITIAL_ACTION:
    case PERCENT_PRINTER:
      if (text[1] == '$')
	{
	  obstack_sgrow (&obstack_for_string, "]b4_dollar_dollar[");
	  return;
	}
      break;

    default:
      break;
    }

  complain_at (loc, _("invalid value: %s"), quote (text));
}


/*------------------------------------------------------.
| TEXT is a location token (i.e., a `@...').  Output to |
| OBSTACK_FOR_STRING a reference to this location.      |
`------------------------------------------------------*/

static inline bool
handle_action_at (char *text, location loc)
{
  char *cp = text + 1;
  locations_flag = true;

  if (! current_rule)
    return false;

  if (*cp == '$')
    obstack_sgrow (&obstack_for_string, "]b4_lhs_location[");
  else
    {
      long int num = strtol (cp, NULL, 10);

      if (1 - INT_MAX + rule_length <= num && num <= rule_length)
	{
	  int n = num;
	  obstack_fgrow2 (&obstack_for_string, "]b4_rhs_location(%d, %d)[",
			  rule_length, n);
	}
      else
	complain_at (loc, _("integer out of range: %s"), quote (text));
    }

  return true;
}


/*----------------------------------------------------------------.
| Map `@?' onto the proper M4 symbol, depending on its TOKEN_TYPE |
| (are we in an action?).                                         |
`----------------------------------------------------------------*/

static void
handle_at (int token_type, char *text, location loc)
{
  switch (token_type)
    {
    case BRACED_CODE:
      handle_action_at (text, loc);
      return;

    case PERCENT_INITIAL_ACTION:
    case PERCENT_DESTRUCTOR:
    case PERCENT_PRINTER:
      if (text[1] == '$')
	{
	  obstack_sgrow (&obstack_for_string, "]b4_at_dollar[");
	  return;
	}
      break;

    default:
      break;
    }

  complain_at (loc, _("invalid value: %s"), quote (text));
}


/*------------------------------------------------------.
| Scan NUMBER for a base-BASE integer at location LOC.  |
`------------------------------------------------------*/

static unsigned long int
scan_integer (char const *number, int base, location loc)
{
  verify (INT_MAX < ULONG_MAX);
  unsigned long int num = strtoul (number, NULL, base);

  if (INT_MAX < num)
    {
      complain_at (loc, _("integer out of range: %s"), quote (number));
      num = INT_MAX;
    }

  return num;
}


/*------------------------------------------------------------------.
| Convert universal character name UCN to a single-byte character,  |
| and return that character.  Return -1 if UCN does not correspond  |
| to a single-byte character.					    |
`------------------------------------------------------------------*/

static int
convert_ucn_to_byte (char const *ucn)
{
  verify (UCHAR_MAX <= INT_MAX);
  unsigned long int code = strtoul (ucn + 2, NULL, 16);

  /* FIXME: Currently we assume Unicode-compatible unibyte characters
     on ASCII hosts (i.e., Latin-1 on hosts with 8-bit bytes).  On
     non-ASCII hosts we support only the portable C character set.
     These limitations should be removed once we add support for
     multibyte characters.  */

  if (UCHAR_MAX < code)
    return -1;

#if ! ('$' == 0x24 && '@' == 0x40 && '`' == 0x60 && '~' == 0x7e)
  {
    /* A non-ASCII host.  Use CODE to index into a table of the C
       basic execution character set, which is guaranteed to exist on
       all Standard C platforms.  This table also includes '$', '@',
       and '`', which are not in the basic execution character set but
       which are unibyte characters on all the platforms that we know
       about.  */
    static signed char const table[] =
      {
	'\0',   -1,   -1,   -1,   -1,   -1,   -1, '\a',
	'\b', '\t', '\n', '\v', '\f', '\r',   -1,   -1,
	  -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
	  -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
	 ' ',  '!',  '"',  '#',  '$',  '%',  '&', '\'',
	 '(',  ')',  '*',  '+',  ',',  '-',  '.',  '/',
	 '0',  '1',  '2',  '3',  '4',  '5',  '6',  '7',
	 '8',  '9',  ':',  ';',  '<',  '=',  '>',  '?',
	 '@',  'A',  'B',  'C',  'D',  'E',  'F',  'G',
	 'H',  'I',  'J',  'K',  'L',  'M',  'N',  'O',
	 'P',  'Q',  'R',  'S',  'T',  'U',  'V',  'W',
	 'X',  'Y',  'Z',  '[', '\\',  ']',  '^',  '_',
	 '`',  'a',  'b',  'c',  'd',  'e',  'f',  'g',
	 'h',  'i',  'j',  'k',  'l',  'm',  'n',  'o',
	 'p',  'q',  'r',  's',  't',  'u',  'v',  'w',
	 'x',  'y',  'z',  '{',  '|',  '}',  '~'
      };

    code = code < sizeof table ? table[code] : -1;
  }
#endif

  return code;
}


/*----------------------------------------------------------------.
| Handle `#line INT "FILE"'.  ARGS has already skipped `#line '.  |
`----------------------------------------------------------------*/

static void
handle_syncline (char *args, location loc)
{
  char *after_num;
  unsigned long int lineno = strtoul (args, &after_num, 10);
  char *file = strchr (after_num, '"') + 1;
  *strchr (file, '"') = '\0';
  if (INT_MAX <= lineno)
    {
      warn_at (loc, _("line number overflow"));
      lineno = INT_MAX;
    }
  scanner_cursor.file = current_file = uniqstr_new (file);
  scanner_cursor.line = lineno;
  scanner_cursor.column = 1;
}


/*---------------------------------.
| Report a rule that is too long.  |
`---------------------------------*/

static void
rule_length_overflow (location loc)
{
  fatal_at (loc, _("rule is too long"));
}


/*----------------------------------------------------------------.
| For a token or comment starting at START, report message MSGID, |
| which should say that an end marker was found before		  |
| the expected TOKEN_END.					  |
`----------------------------------------------------------------*/

static void
unexpected_end (boundary start, char const *msgid, char const *token_end)
{
  location loc;
  loc.start = start;
  loc.end = scanner_cursor;
  complain_at (loc, _(msgid), token_end);
}


/*------------------------------------------------------------------------.
| Report an unexpected EOF in a token or comment starting at START.       |
| An end of file was encountered and the expected TOKEN_END was missing.  |
`------------------------------------------------------------------------*/

static void
unexpected_eof (boundary start, char const *token_end)
{
  unexpected_end (start, N_("missing `%s' at end of file"), token_end);
}


/*----------------------------------------.
| Likewise, but for unexpected newlines.  |
`----------------------------------------*/

static void
unexpected_newline (boundary start, char const *token_end)
{
  unexpected_end (start, N_("missing `%s' at end of line"), token_end);
}


/*-------------------------.
| Initialize the scanner.  |
`-------------------------*/

void
scanner_initialize (void)
{
  obstack_init (&obstack_for_string);
}


/*-----------------------------------------------.
| Free all the memory allocated to the scanner.  |
`-----------------------------------------------*/

void
scanner_free (void)
{
  obstack_free (&obstack_for_string, 0);
  /* Reclaim Flex's buffers.  */
  yy_delete_buffer (YY_CURRENT_BUFFER);
}
