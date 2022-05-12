%{/* Bison Grammar Parser                             -*- C -*-

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

#include <config.h>
#include "system.h"

#include "complain.h"
#include "conflicts.h"
#include "files.h"
#include "getargs.h"
#include "gram.h"
#include "muscle_tab.h"
#include "quotearg.h"
#include "reader.h"
#include "symlist.h"
#include "strverscmp.h"

#define YYLLOC_DEFAULT(Current, Rhs, N)  (Current) = lloc_default (Rhs, N)
static YYLTYPE lloc_default (YYLTYPE const *, int);

#define YY_LOCATION_PRINT(File, Loc) \
	  location_print (File, Loc)

static void version_check (location const *loc, char const *version);

/* Request detailed syntax error messages, and pass them to GRAM_ERROR.
   FIXME: depends on the undocumented availability of YYLLOC.  */
#undef  yyerror
#define yyerror(Msg) \
	gram_error (&yylloc, Msg)
static void gram_error (location const *, char const *);

static void add_param (char const *, char *, location);

static symbol_class current_class = unknown_sym;
static uniqstr current_type = 0;
static symbol *current_lhs;
static location current_lhs_location;
static int current_prec = 0;

#ifdef UINT_FAST8_MAX
# define YYTYPE_UINT8 uint_fast8_t
#endif
#ifdef INT_FAST8_MAX
# define YYTYPE_INT8 int_fast8_t
#endif
#ifdef UINT_FAST16_MAX
# define YYTYPE_UINT16 uint_fast16_t
#endif
#ifdef INT_FAST16_MAX
# define YYTYPE_INT16 int_fast16_t
#endif
%}

%debug
%verbose
%defines
%locations
%pure-parser
%error-verbose
%defines
%name-prefix="gram_"

%initial-action
{
  /* Bison's grammar can initial empty locations, hence a default
     location is needed. */
  @$.start.file   = @$.end.file   = current_file;
  @$.start.line   = @$.end.line   = 1;
  @$.start.column = @$.end.column = 0;
}

/* Only NUMBERS have a value.  */
%union
{
  symbol *symbol;
  symbol_list *list;
  int integer;
  char *chars;
  assoc assoc;
  uniqstr uniqstr;
};

/* Define the tokens together with their human representation.  */
%token GRAM_EOF 0 "end of file"
%token STRING     "string"
%token INT        "integer"

%token PERCENT_TOKEN       "%token"
%token PERCENT_NTERM       "%nterm"

%token PERCENT_TYPE        "%type"
%token PERCENT_DESTRUCTOR  "%destructor {...}"
%token PERCENT_PRINTER     "%printer {...}"

%token PERCENT_UNION       "%union {...}"

%token PERCENT_LEFT        "%left"
%token PERCENT_RIGHT       "%right"
%token PERCENT_NONASSOC    "%nonassoc"

%token PERCENT_PREC          "%prec"
%token PERCENT_DPREC         "%dprec"
%token PERCENT_MERGE         "%merge"


/*----------------------.
| Global Declarations.  |
`----------------------*/

%token
  PERCENT_DEBUG           "%debug"
  PERCENT_DEFAULT_PREC    "%default-prec"
  PERCENT_DEFINE          "%define"
  PERCENT_DEFINES         "%defines"
  PERCENT_ERROR_VERBOSE   "%error-verbose"
  PERCENT_EXPECT          "%expect"
  PERCENT_EXPECT_RR	  "%expect-rr"
  PERCENT_FILE_PREFIX     "%file-prefix"
  PERCENT_GLR_PARSER      "%glr-parser"
  PERCENT_INITIAL_ACTION  "%initial-action {...}"
  PERCENT_LEX_PARAM       "%lex-param {...}"
  PERCENT_LOCATIONS       "%locations"
  PERCENT_NAME_PREFIX     "%name-prefix"
  PERCENT_NO_DEFAULT_PREC "%no-default-prec"
  PERCENT_NO_LINES        "%no-lines"
  PERCENT_NONDETERMINISTIC_PARSER
			  "%nondeterministic-parser"
  PERCENT_OUTPUT          "%output"
  PERCENT_PARSE_PARAM     "%parse-param {...}"
  PERCENT_PURE_PARSER     "%pure-parser"
  PERCENT_REQUIRE	  "%require"
  PERCENT_SKELETON        "%skeleton"
  PERCENT_START           "%start"
  PERCENT_TOKEN_TABLE     "%token-table"
  PERCENT_VERBOSE         "%verbose"
  PERCENT_YACC            "%yacc"
;

%token TYPE            "type"
%token EQUAL           "="
%token SEMICOLON       ";"
%token PIPE            "|"
%token ID              "identifier"
%token ID_COLON        "identifier:"
%token PERCENT_PERCENT "%%"
%token PROLOGUE        "%{...%}"
%token EPILOGUE        "epilogue"
%token BRACED_CODE     "{...}"


%type <chars> STRING string_content
	      "%destructor {...}"
	      "%initial-action {...}"
	      "%lex-param {...}"
	      "%parse-param {...}"
	      "%printer {...}"
	      "%union {...}"
	      PROLOGUE EPILOGUE
%printer { fprintf (stderr, "\"%s\"", $$); }
	      STRING string_content
%printer { fprintf (stderr, "{\n%s\n}", $$); }
	      "%destructor {...}"
	      "%initial-action {...}"
	      "%lex-param {...}"
	      "%parse-param {...}"
	      "%printer {...}"
	      "%union {...}"
	      PROLOGUE EPILOGUE
%type <uniqstr> TYPE
%printer { fprintf (stderr, "<%s>", $$); } TYPE
%type <integer> INT
%printer { fprintf (stderr, "%d", $$); } INT
%type <symbol> ID symbol string_as_id
%printer { fprintf (stderr, "%s", $$->tag); } ID symbol string_as_id
%type <symbol> ID_COLON
%printer { fprintf (stderr, "%s:", $$->tag); } ID_COLON
%type <assoc> precedence_declarator
%type <list>  symbols.1
%%

input:
  declarations "%%" grammar epilogue.opt
;


	/*------------------------------------.
	| Declarations: before the first %%.  |
	`------------------------------------*/

declarations:
  /* Nothing */
| declarations declaration
;

declaration:
  grammar_declaration
| PROLOGUE                                 { prologue_augment ($1, @1); }
| "%debug"                                 { debug_flag = true; }
| "%define" string_content
    {
      static char one[] = "1";
      muscle_insert ($2, one);
    }
| "%define" string_content string_content  { muscle_insert ($2, $3); }
| "%defines"                               { defines_flag = true; }
| "%error-verbose"                         { error_verbose = true; }
| "%expect" INT                            { expected_sr_conflicts = $2; }
| "%expect-rr" INT			   { expected_rr_conflicts = $2; }
| "%file-prefix" "=" string_content        { spec_file_prefix = $3; }
| "%glr-parser"
    {
      nondeterministic_parser = true;
      glr_parser = true;
    }
| "%initial-action {...}"
    {
      muscle_code_grow ("initial_action", $1, @1);
    }
| "%lex-param {...}"			   { add_param ("lex_param", $1, @1); }
| "%locations"                             { locations_flag = true; }
| "%name-prefix" "=" string_content        { spec_name_prefix = $3; }
| "%no-lines"                              { no_lines_flag = true; }
| "%nondeterministic-parser"		   { nondeterministic_parser = true; }
| "%output" "=" string_content             { spec_outfile = $3; }
| "%parse-param {...}"			   { add_param ("parse_param", $1, @1); }
| "%pure-parser"                           { pure_parser = true; }
| "%require" string_content                { version_check (&@2, $2); }
| "%skeleton" string_content               { skeleton = $2; }
| "%token-table"                           { token_table_flag = true; }
| "%verbose"                               { report_flag = report_states; }
| "%yacc"                                  { yacc_flag = true; }
| /*FIXME: Err?  What is this horror doing here? */ ";"
;

grammar_declaration:
  precedence_declaration
| symbol_declaration
| "%start" symbol
    {
      grammar_start_symbol_set ($2, @2);
    }
| "%union {...}"
    {
      char const *body = $1;

      if (typed)
	{
	  /* Concatenate the union bodies, turning the first one's
	     trailing '}' into '\n', and omitting the second one's '{'.  */
	  char *code = muscle_find ("stype");
	  code[strlen (code) - 1] = '\n';
	  body++;
	}

      typed = true;
      muscle_code_grow ("stype", body, @1);
    }
| "%destructor {...}" symbols.1
    {
      symbol_list *list;
      for (list = $2; list; list = list->next)
	symbol_destructor_set (list->sym, $1, @1);
      symbol_list_free ($2);
    }
| "%printer {...}" symbols.1
    {
      symbol_list *list;
      for (list = $2; list; list = list->next)
	symbol_printer_set (list->sym, $1, @1);
      symbol_list_free ($2);
    }
| "%default-prec"
    {
      default_prec = true;
    }
| "%no-default-prec"
    {
      default_prec = false;
    }
;

symbol_declaration:
  "%nterm" { current_class = nterm_sym; } symbol_defs.1
    {
      current_class = unknown_sym;
      current_type = NULL;
    }
| "%token" { current_class = token_sym; } symbol_defs.1
    {
      current_class = unknown_sym;
      current_type = NULL;
    }
| "%type" TYPE symbols.1
    {
      symbol_list *list;
      for (list = $3; list; list = list->next)
	symbol_type_set (list->sym, $2, @2);
      symbol_list_free ($3);
    }
;

precedence_declaration:
  precedence_declarator type.opt symbols.1
    {
      symbol_list *list;
      ++current_prec;
      for (list = $3; list; list = list->next)
	{
	  symbol_type_set (list->sym, current_type, @2);
	  symbol_precedence_set (list->sym, current_prec, $1, @1);
	}
      symbol_list_free ($3);
      current_type = NULL;
    }
;

precedence_declarator:
  "%left"     { $$ = left_assoc; }
| "%right"    { $$ = right_assoc; }
| "%nonassoc" { $$ = non_assoc; }
;

type.opt:
  /* Nothing. */ { current_type = NULL; }
| TYPE           { current_type = $1; }
;

/* One or more nonterminals to be %typed. */

symbols.1:
  symbol            { $$ = symbol_list_new ($1, @1); }
| symbols.1 symbol  { $$ = symbol_list_prepend ($1, $2, @2); }
;

/* One token definition.  */
symbol_def:
  TYPE
     {
       current_type = $1;
     }
| ID
     {
       symbol_class_set ($1, current_class, @1, true);
       symbol_type_set ($1, current_type, @1);
     }
| ID INT
    {
      symbol_class_set ($1, current_class, @1, true);
      symbol_type_set ($1, current_type, @1);
      symbol_user_token_number_set ($1, $2, @2);
    }
| ID string_as_id
    {
      symbol_class_set ($1, current_class, @1, true);
      symbol_type_set ($1, current_type, @1);
      symbol_make_alias ($1, $2, @$);
    }
| ID INT string_as_id
    {
      symbol_class_set ($1, current_class, @1, true);
      symbol_type_set ($1, current_type, @1);
      symbol_user_token_number_set ($1, $2, @2);
      symbol_make_alias ($1, $3, @$);
    }
;

/* One or more symbol definitions. */
symbol_defs.1:
  symbol_def
| symbol_defs.1 symbol_def
;


	/*------------------------------------------.
	| The grammar section: between the two %%.  |
	`------------------------------------------*/

grammar:
  rules_or_grammar_declaration
| grammar rules_or_grammar_declaration
;

/* As a Bison extension, one can use the grammar declarations in the
   body of the grammar.  */
rules_or_grammar_declaration:
  rules
| grammar_declaration ";"
| error ";"
    {
      yyerrok;
    }
;

rules:
  ID_COLON { current_lhs = $1; current_lhs_location = @1; } rhses.1
;

rhses.1:
  rhs                { grammar_current_rule_end (@1); }
| rhses.1 "|" rhs    { grammar_current_rule_end (@3); }
| rhses.1 ";"
;

rhs:
  /* Nothing.  */
    { grammar_current_rule_begin (current_lhs, current_lhs_location); }
| rhs symbol
    { grammar_current_rule_symbol_append ($2, @2); }
| rhs action
| rhs "%prec" symbol
    { grammar_current_rule_prec_set ($3, @3); }
| rhs "%dprec" INT
    { grammar_current_rule_dprec_set ($3, @3); }
| rhs "%merge" TYPE
    { grammar_current_rule_merge_set ($3, @3); }
;

symbol:
  ID              { $$ = $1; }
| string_as_id    { $$ = $1; }
;

/* Handle the semantics of an action specially, with a mid-rule
   action, so that grammar_current_rule_action_append is invoked
   immediately after the braced code is read by the scanner.

   This implementation relies on the LALR(1) parsing algorithm.
   If grammar_current_rule_action_append were executed in a normal
   action for this rule, then when the input grammar contains two
   successive actions, the scanner would have to read both actions
   before reducing this rule.  That wouldn't work, since the scanner
   relies on all preceding input actions being processed by
   grammar_current_rule_action_append before it scans the next
   action.  */
action:
    { grammar_current_rule_action_append (last_string, last_braced_code_loc); }
  BRACED_CODE
;

/* A string used as an ID: quote it.  */
string_as_id:
  STRING
    {
      $$ = symbol_get (quotearg_style (c_quoting_style, $1), @1);
      symbol_class_set ($$, token_sym, @1, false);
    }
;

/* A string used for its contents.  Don't quote it.  */
string_content:
  STRING
    { $$ = $1; }
;


epilogue.opt:
  /* Nothing.  */
| "%%" EPILOGUE
    {
      muscle_code_grow ("epilogue", $2, @2);
      scanner_last_string_free ();
    }
;

%%


/* Return the location of the left-hand side of a rule whose
   right-hand side is RHS[1] ... RHS[N].  Ignore empty nonterminals in
   the right-hand side, and return an empty location equal to the end
   boundary of RHS[0] if the right-hand side is empty.  */

static YYLTYPE
lloc_default (YYLTYPE const *rhs, int n)
{
  int i;
  YYLTYPE loc;

  /* SGI MIPSpro 7.4.1m miscompiles "loc.start = loc.end = rhs[n].end;".
     The bug is fixed in 7.4.2m, but play it safe for now.  */
  loc.start = rhs[n].end;
  loc.end = rhs[n].end;

  /* Ignore empty nonterminals the start of the the right-hand side.
     Do not bother to ignore them at the end of the right-hand side,
     since empty nonterminals have the same end as their predecessors.  */
  for (i = 1; i <= n; i++)
    if (! equal_boundaries (rhs[i].start, rhs[i].end))
      {
	loc.start = rhs[i].start;
	break;
      }

  return loc;
}


/* Add a lex-param or a parse-param (depending on TYPE) with
   declaration DECL and location LOC.  */

static void
add_param (char const *type, char *decl, location loc)
{
  static char const alphanum[26 + 26 + 1 + 10] =
    "abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "_"
    "0123456789";
  char const *name_start = NULL;
  char *p;

  /* Stop on last actual character.  */
  for (p = decl; p[1]; p++)
    if ((p == decl
	 || ! memchr (alphanum, p[-1], sizeof alphanum))
	&& memchr (alphanum, p[0], sizeof alphanum - 10))
      name_start = p;

  /* Strip the surrounding '{' and '}', and any blanks just inside
     the braces.  */
  while (*--p == ' ' || *p == '\t')
    continue;
  p[1] = '\0';
  while (*++decl == ' ' || *decl == '\t')
    continue;

  if (! name_start)
    complain_at (loc, _("missing identifier in parameter declaration"));
  else
    {
      char *name;
      size_t name_len;

      for (name_len = 1;
	   memchr (alphanum, name_start[name_len], sizeof alphanum);
	   name_len++)
	continue;

      name = xmalloc (name_len + 1);
      memcpy (name, name_start, name_len);
      name[name_len] = '\0';
      muscle_pair_list_grow (type, decl, name);
      free (name);
    }

  scanner_last_string_free ();
}

static void
version_check (location const *loc, char const *version)
{
  if (strverscmp (version, PACKAGE_VERSION) > 0)
    {
      complain_at (*loc, "require bison %s, but have %s",
		   version, PACKAGE_VERSION);
      exit (63);
    }
}

static void
gram_error (location const *loc, char const *msg)
{
  complain_at (*loc, "%s", msg);
}

char const *
token_name (int type)
{
  return yytname[YYTRANSLATE (type)];
}
