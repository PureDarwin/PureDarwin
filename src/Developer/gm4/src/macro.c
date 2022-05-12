/* GNU m4 -- A simple macro processor

   Copyright (C) 1989, 1990, 1991, 1992, 1993, 1994, 2006 Free Software
   Foundation, Inc.

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

/* This file contains the functions, that performs the basic argument
   parsing and macro expansion.  */

#include "m4.h"

static void expand_macro (symbol *);
static void expand_token (struct obstack *, token_type, token_data *);

/* Current recursion level in expand_macro ().  */
int expansion_level = 0;

/* The number of the current call of expand_macro ().  */
static int macro_call_id = 0;

/*----------------------------------------------------------------------.
| This function read all input, and expands each token, one at a time.  |
`----------------------------------------------------------------------*/

void
expand_input (void)
{
  token_type t;
  token_data td;

  while ((t = next_token (&td)) != TOKEN_EOF)
    expand_token ((struct obstack *) NULL, t, &td);
}


/*------------------------------------------------------------------------.
| Expand one token, according to its type.  Potential macro names	  |
| (TOKEN_WORD) are looked up in the symbol table, to see if they have a	  |
| macro definition.  If they have, they are expanded as macros, otherwise |
| the text are just copied to the output.				  |
`------------------------------------------------------------------------*/

static void
expand_token (struct obstack *obs, token_type t, token_data *td)
{
  symbol *sym;

  switch (t)
    {				/* TOKSW */
    case TOKEN_EOF:
    case TOKEN_MACDEF:
      break;

    case TOKEN_OPEN:
    case TOKEN_COMMA:
    case TOKEN_CLOSE:
    case TOKEN_SIMPLE:
    case TOKEN_STRING:
      shipout_text (obs, TOKEN_DATA_TEXT (td), strlen (TOKEN_DATA_TEXT (td)));
      break;

    case TOKEN_WORD:
      sym = lookup_symbol (TOKEN_DATA_TEXT (td), SYMBOL_LOOKUP);
      if (sym == NULL || SYMBOL_TYPE (sym) == TOKEN_VOID
	  || (SYMBOL_TYPE (sym) == TOKEN_FUNC
	      && SYMBOL_BLIND_NO_ARGS (sym)
	      && peek_token () != TOKEN_OPEN))
	{
#ifdef ENABLE_CHANGEWORD
	  shipout_text (obs, TOKEN_DATA_ORIG_TEXT (td),
			strlen (TOKEN_DATA_ORIG_TEXT (td)));
#else
	  shipout_text (obs, TOKEN_DATA_TEXT (td),
			strlen (TOKEN_DATA_TEXT (td)));
#endif
	}
      else
	expand_macro (sym);
      break;

    default:
      M4ERROR ((warning_status, 0,
		"INTERNAL ERROR: bad token type in expand_token ()"));
      abort ();
    }
}


/*-------------------------------------------------------------------------.
| This function parses one argument to a macro call.  It expects the first |
| left parenthesis, or the separating comma to have been read by the	   |
| caller.  It skips leading whitespace, and reads and expands tokens,	   |
| until it finds a comma or an right parenthesis at the same level of	   |
| parentheses.  It returns a flag indicating whether the argument read are |
| the last for the active macro call.  The argument are build on the	   |
| obstack OBS, indirectly through expand_token ().			   |
`-------------------------------------------------------------------------*/

static boolean
expand_argument (struct obstack *obs, token_data *argp)
{
  token_type t;
  token_data td;
  char *text;
  int paren_level;
  const char *file = current_file;
  int line = current_line;

  TOKEN_DATA_TYPE (argp) = TOKEN_VOID;

  /* Skip leading white space.  */
  do
    {
      t = next_token (&td);
    }
  while (t == TOKEN_SIMPLE && isspace (to_uchar (*TOKEN_DATA_TEXT (&td))));

  paren_level = 0;

  while (1)
    {

      switch (t)
	{			/* TOKSW */
	case TOKEN_COMMA:
	case TOKEN_CLOSE:
	  if (paren_level == 0)
	    {
	      /* The argument MUST be finished, whether we want it or not.  */
	      obstack_1grow (obs, '\0');
	      text = obstack_finish (obs);

	      if (TOKEN_DATA_TYPE (argp) == TOKEN_VOID)
		{
		  TOKEN_DATA_TYPE (argp) = TOKEN_TEXT;
		  TOKEN_DATA_TEXT (argp) = text;
		}
	      return (boolean) (t == TOKEN_COMMA);
	    }
	  /* fallthru */
	case TOKEN_OPEN:
	case TOKEN_SIMPLE:
	  text = TOKEN_DATA_TEXT (&td);

	  if (*text == '(')
	    paren_level++;
	  else if (*text == ')')
	    paren_level--;
	  expand_token (obs, t, &td);
	  break;

	case TOKEN_EOF:
	  /* current_file changed to "" if we see TOKEN_EOF, use the
	     previous value we stored earlier.  */
	  M4ERROR_AT_LINE ((EXIT_FAILURE, 0, file, line,
			    "ERROR: end of file in argument list"));
	  break;

	case TOKEN_WORD:
	case TOKEN_STRING:
	  expand_token (obs, t, &td);
	  break;

	case TOKEN_MACDEF:
	  if (obstack_object_size (obs) == 0)
	    {
	      TOKEN_DATA_TYPE (argp) = TOKEN_FUNC;
	      TOKEN_DATA_FUNC (argp) = TOKEN_DATA_FUNC (&td);
	    }
	  break;

	default:
	  M4ERROR ((warning_status, 0,
		    "INTERNAL ERROR: bad token type in expand_argument ()"));
	  abort ();
	}

      t = next_token (&td);
    }
}

/*-------------------------------------------------------------------------.
| Collect all the arguments to a call of the macro SYM.  The arguments are |
| stored on the obstack ARGUMENTS and a table of pointers to the arguments |
| on the obstack ARGPTR.						   |
`-------------------------------------------------------------------------*/

static void
collect_arguments (symbol *sym, struct obstack *argptr,
		   struct obstack *arguments)
{
  token_data td;
  token_data *tdp;
  boolean more_args;
  boolean groks_macro_args = SYMBOL_MACRO_ARGS (sym);

  TOKEN_DATA_TYPE (&td) = TOKEN_TEXT;
  TOKEN_DATA_TEXT (&td) = SYMBOL_NAME (sym);
  tdp = (token_data *) obstack_copy (arguments, &td, sizeof (td));
  obstack_grow (argptr, &tdp, sizeof (tdp));

  if (peek_token () == TOKEN_OPEN)
    {
      next_token (&td);		/* gobble parenthesis */
      do
	{
	  more_args = expand_argument (arguments, &td);

	  if (!groks_macro_args && TOKEN_DATA_TYPE (&td) == TOKEN_FUNC)
	    {
	      TOKEN_DATA_TYPE (&td) = TOKEN_TEXT;
	      TOKEN_DATA_TEXT (&td) = "";
	    }
	  tdp = (token_data *)
	    obstack_copy (arguments, &td, sizeof (td));
	  obstack_grow (argptr, &tdp, sizeof (tdp));
	}
      while (more_args);
    }
}


/*------------------------------------------------------------------------.
| The actual call of a macro is handled by call_macro ().  call_macro ()  |
| is passed a symbol SYM, whose type is used to call either a builtin	  |
| function, or the user macro expansion function expand_user_macro ()	  |
| (lives in builtin.c).  There are ARGC arguments to the call, stored in  |
| the ARGV table.  The expansion is left on the obstack EXPANSION.  Macro |
| tracing is also handled here.						  |
`------------------------------------------------------------------------*/

void
call_macro (symbol *sym, int argc, token_data **argv,
		 struct obstack *expansion)
{
  switch (SYMBOL_TYPE (sym))
    {
    case TOKEN_FUNC:
      (*SYMBOL_FUNC (sym)) (expansion, argc, argv);
      break;

    case TOKEN_TEXT:
      expand_user_macro (expansion, sym, argc, argv);
      break;

    default:
      M4ERROR ((warning_status, 0,
		"INTERNAL ERROR: bad symbol type in call_macro ()"));
      abort ();
    }
}

/*-------------------------------------------------------------------------.
| The macro expansion is handled by expand_macro ().  It parses the	   |
| arguments, using collect_arguments (), and builds a table of pointers to |
| the arguments.  The arguments themselves are stored on a local obstack.  |
| Expand_macro () uses call_macro () to do the call of the macro.	   |
|									   |
| Expand_macro () is potentially recursive, since it calls expand_argument |
| (), which might call expand_token (), which might call expand_macro ().  |
`-------------------------------------------------------------------------*/

static void
expand_macro (symbol *sym)
{
  struct obstack arguments;
  struct obstack argptr;
  token_data **argv;
  int argc;
  struct obstack *expansion;
  const char *expanded;
  boolean traced;
  int my_call_id;

  SYMBOL_PENDING_EXPANSIONS (sym)++;
  expansion_level++;
  if (expansion_level > nesting_limit)
    M4ERROR ((EXIT_FAILURE, 0,
	      "ERROR: recursion limit of %d exceeded, use -L<N> to change it",
	      nesting_limit));

  macro_call_id++;
  my_call_id = macro_call_id;

  traced = (boolean) ((debug_level & DEBUG_TRACE_ALL) || SYMBOL_TRACED (sym));

  obstack_init (&argptr);
  obstack_init (&arguments);

  if (traced && (debug_level & DEBUG_TRACE_CALL))
    trace_prepre (SYMBOL_NAME (sym), my_call_id);

  collect_arguments (sym, &argptr, &arguments);

  argc = obstack_object_size (&argptr) / sizeof (token_data *);
  argv = (token_data **) obstack_finish (&argptr);

  if (traced)
    trace_pre (SYMBOL_NAME (sym), my_call_id, argc, argv);

  expansion = push_string_init ();
  call_macro (sym, argc, argv, expansion);
  expanded = push_string_finish ();

  if (traced)
    trace_post (SYMBOL_NAME (sym), my_call_id, argc, argv, expanded);

  --expansion_level;
  --SYMBOL_PENDING_EXPANSIONS (sym);

  if (SYMBOL_DELETED (sym))
    free_symbol (sym);

  obstack_free (&arguments, NULL);
  obstack_free (&argptr, NULL);
}
