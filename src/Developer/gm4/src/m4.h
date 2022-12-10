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

/* We use <config.h> instead of "config.h" so that a compilation
   using -I. -I$srcdir will use ./config.h rather than $srcdir/config.h
   (which it would do because it found this file in $srcdir).  */

#include <config.h>

/* Canonicalize UNIX recognition macros.  */
#if defined unix || defined __unix || defined __unix__ \
  || defined _POSIX_VERSION || defined _POSIX2_VERSION \
  || defined __NetBSD__ || defined __OpenBSD__ \
  || defined __APPLE__ || defined __APPLE_CC__
# define UNIX 1
#endif

/* Canonicalize Windows recognition macros.  */
#if (defined _WIN32 || defined __WIN32__) && !defined __CYGWIN__
# define W32_NATIVE 1
#endif

/* Canonicalize OS/2 recognition macro.  */
#ifdef __EMX__
# define OS2 1
#endif

#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>

#include "binary-io.h"
#include "cloexec.h"
#include "close-stream.h"
#include "error.h"
#include "exit.h"
#include "obstack.h"
#include "stdio--.h"
#include "stdlib--.h"
#include "unistd--.h"
#include "verror.h"
#include "xalloc.h"

/* If FALSE is defined, we presume TRUE is defined too.  In this case,
   merely typedef boolean as being int.  Or else, define these all.  */
#ifndef FALSE
/* Do not use `enum boolean': this tag is used in SVR4 <sys/types.h>.  */
typedef enum { FALSE = 0, TRUE = 1 } boolean;
#else
typedef int boolean;
#endif

#if ! HAVE_MKSTEMP
int mkstemp (char *);
#endif

/* Used for version mismatch, when -R detects a frozen file it can't parse.  */
#define EXIT_MISMATCH 63

/* Various declarations.  */

struct string
  {
    char *string;		/* characters of the string */
    size_t length;		/* length of the string */
  };
typedef struct string STRING;

/* Memory allocation.  */
#define obstack_chunk_alloc	xmalloc
#define obstack_chunk_free	free

/* Those must come first.  */
typedef struct token_data token_data;
typedef void builtin_func (struct obstack *, int, token_data **);

/* Take advantage of GNU C compiler source level optimization hints,
   using portable macros.  */
#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ > 6)
#  define M4_GNUC_ATTRIBUTE(args)	__attribute__(args)
#else
#  define M4_GNUC_ATTRIBUTE(args)
#endif  /* __GNUC__ */

#define M4_GNUC_UNUSED		M4_GNUC_ATTRIBUTE((__unused__))
#define M4_GNUC_PRINTF(fmt, arg)			\
  M4_GNUC_ATTRIBUTE((__format__ (__printf__, fmt, arg)))

/* File: m4.c  --- global definitions.  */

/* Option flags.  */
extern int sync_output;			/* -s */
extern int debug_level;			/* -d */
extern size_t hash_table_size;		/* -H */
extern int no_gnu_extensions;		/* -G */
extern int prefix_all_builtins;		/* -P */
extern int max_debug_argument_length;	/* -l */
extern int suppress_warnings;		/* -Q */
extern int warning_status;		/* -E */
extern int nesting_limit;		/* -L */
#ifdef ENABLE_CHANGEWORD
extern const char *user_word_regexp;	/* -W */
#endif

/* Error handling.  */
extern int retcode;
extern const char *program_name;

void m4_error (int, int, const char *, ...) M4_GNUC_PRINTF(3, 4);
void m4_error_at_line (int, int, const char *, int,
                       const char *, ...) M4_GNUC_PRINTF(5, 6);

#define M4ERROR(Arglist) (m4_error Arglist)
#define M4ERROR_AT_LINE(Arglist) (m4_error_at_line Arglist)

#ifdef USE_STACKOVF
void setup_stackovf_trap (char *const *, char *const *,
                          void (*handler) (void));
#endif

/* File: debug.c  --- debugging and tracing function.  */

extern FILE *debug;

/* The value of debug_level is a bitmask of the following.  */

/* a: show arglist in trace output */
#define DEBUG_TRACE_ARGS 1
/* e: show expansion in trace output */
#define DEBUG_TRACE_EXPANSION 2
/* q: quote args and expansion in trace output */
#define DEBUG_TRACE_QUOTE 4
/* t: trace all macros -- overrides trace{on,off} */
#define DEBUG_TRACE_ALL 8
/* l: add line numbers to trace output */
#define DEBUG_TRACE_LINE 16
/* f: add file name to trace output */
#define DEBUG_TRACE_FILE 32
/* p: trace path search of include files */
#define DEBUG_TRACE_PATH 64
/* c: show macro call before args collection */
#define DEBUG_TRACE_CALL 128
/* i: trace changes of input files */
#define DEBUG_TRACE_INPUT 256
/* x: add call id to trace output */
#define DEBUG_TRACE_CALLID 512

/* V: very verbose --  print everything */
#define DEBUG_TRACE_VERBOSE 1023
/* default flags -- equiv: aeq */
#define DEBUG_TRACE_DEFAULT 7

#define DEBUG_PRINT1(Fmt, Arg1) \
  do								\
    {								\
      if (debug != NULL)					\
	fprintf (debug, Fmt, Arg1);				\
    }								\
  while (0)

#define DEBUG_PRINT3(Fmt, Arg1, Arg2, Arg3) \
  do								\
    {								\
      if (debug != NULL)					\
	fprintf (debug, Fmt, Arg1, Arg2, Arg3);			\
    }								\
  while (0)

#define DEBUG_MESSAGE(Fmt) \
  do								\
    {								\
      if (debug != NULL)					\
	{							\
	  debug_message_prefix ();				\
	  fprintf (debug, Fmt);					\
	  putc ('\n', debug);					\
	}							\
    }								\
  while (0)

#define DEBUG_MESSAGE1(Fmt, Arg1) \
  do								\
    {								\
      if (debug != NULL)					\
	{							\
	  debug_message_prefix ();				\
	  fprintf (debug, Fmt, Arg1);				\
	  putc ('\n', debug);					\
	}							\
    }								\
  while (0)

#define DEBUG_MESSAGE2(Fmt, Arg1, Arg2) \
  do								\
    {								\
      if (debug != NULL)					\
	{							\
	  debug_message_prefix ();				\
	  fprintf (debug, Fmt, Arg1, Arg2);			\
	  putc ('\n', debug);					\
	}							\
    }								\
  while (0)

void debug_init (void);
int debug_decode (const char *);
void debug_flush_files (void);
boolean debug_set_output (const char *);
void debug_message_prefix (void);

void trace_prepre (const char *, int);
void trace_pre (const char *, int, int, token_data **);
void trace_post (const char *, int, int, token_data **, const char *);

/* File: input.c  --- lexical definitions.  */

/* Various different token types.  */
enum token_type
{
  TOKEN_EOF,			/* end of file */
  TOKEN_STRING,			/* a quoted string or comment */
  TOKEN_WORD,			/* an identifier */
  TOKEN_OPEN,			/* ( */
  TOKEN_COMMA,			/* , */
  TOKEN_CLOSE,			/* ) */
  TOKEN_SIMPLE,			/* any other single character */
  TOKEN_MACDEF			/* a macro's definition (see "defn") */
};

/* The data for a token, a macro argument, and a macro definition.  */
enum token_data_type
{
  TOKEN_VOID,
  TOKEN_TEXT,
  TOKEN_FUNC
};

struct token_data
{
  enum token_data_type type;
  union
    {
      struct
	{
	  char *text;
#ifdef ENABLE_CHANGEWORD
	  char *original_text;
#endif
	}
      u_t;
      builtin_func *func;
    }
  u;
};

#define TOKEN_DATA_TYPE(Td)		((Td)->type)
#define TOKEN_DATA_TEXT(Td)		((Td)->u.u_t.text)
#ifdef ENABLE_CHANGEWORD
# define TOKEN_DATA_ORIG_TEXT(Td)	((Td)->u.u_t.original_text)
#endif
#define TOKEN_DATA_FUNC(Td)		((Td)->u.func)

typedef enum token_type token_type;
typedef enum token_data_type token_data_type;

void input_init (void);
token_type peek_token (void);
token_type next_token (token_data *);
void skip_line (void);

/* push back input */
void push_file (FILE *, const char *);
void push_macro (builtin_func *);
struct obstack *push_string_init (void);
const char *push_string_finish (void);
void push_wrapup (const char *);
boolean pop_wrapup (void);

/* current input file, and line */
extern const char *current_file;
extern int current_line;

/* left and right quote, begin and end comment */
extern STRING bcomm, ecomm;
extern STRING lquote, rquote;

#define DEF_LQUOTE "`"
#define DEF_RQUOTE "\'"
#define DEF_BCOMM "#"
#define DEF_ECOMM "\n"

void set_quotes (const char *, const char *);
void set_comment (const char *, const char *);
#ifdef ENABLE_CHANGEWORD
void set_word_regexp (const char *);
#endif

/* File: output.c --- output functions.  */
extern int current_diversion;
extern int output_current_line;

void output_init (void);
void shipout_text (struct obstack *, const char *, int);
void make_diversion (int);
void insert_diversion (int);
void insert_file (FILE *);
void freeze_diversions (FILE *);

/* File symtab.c  --- symbol table definitions.  */

/* Operation modes for lookup_symbol ().  */
enum symbol_lookup
{
  SYMBOL_LOOKUP,
  SYMBOL_INSERT,
  SYMBOL_DELETE,
  SYMBOL_PUSHDEF,
  SYMBOL_POPDEF
};

/* Symbol table entry.  */
struct symbol
{
  struct symbol *next;
  boolean traced : 1;
  boolean shadowed : 1;
  boolean macro_args : 1;
  boolean blind_no_args : 1;
  boolean deleted : 1;
  int pending_expansions;

  char *name;
  token_data data;
};

#define SYMBOL_NEXT(S)		((S)->next)
#define SYMBOL_TRACED(S)	((S)->traced)
#define SYMBOL_SHADOWED(S)	((S)->shadowed)
#define SYMBOL_MACRO_ARGS(S)	((S)->macro_args)
#define SYMBOL_BLIND_NO_ARGS(S)	((S)->blind_no_args)
#define SYMBOL_DELETED(S)	((S)->deleted)
#define SYMBOL_PENDING_EXPANSIONS(S) ((S)->pending_expansions)
#define SYMBOL_NAME(S)		((S)->name)
#define SYMBOL_TYPE(S)		(TOKEN_DATA_TYPE (&(S)->data))
#define SYMBOL_TEXT(S)		(TOKEN_DATA_TEXT (&(S)->data))
#define SYMBOL_FUNC(S)		(TOKEN_DATA_FUNC (&(S)->data))

typedef enum symbol_lookup symbol_lookup;
typedef struct symbol symbol;
typedef void hack_symbol ();

#define HASHMAX 509		/* default, overridden by -Hsize */

extern symbol **symtab;

void free_symbol (symbol *sym);
void symtab_init (void);
symbol *lookup_symbol (const char *, symbol_lookup);
void hack_all_symbols (hack_symbol *, const char *);

/* File: macro.c  --- macro expansion.  */

void expand_input (void);
void call_macro (symbol *, int, token_data **, struct obstack *);

/* File: builtin.c  --- builtins.  */

struct builtin
{
  const char *name;
  boolean gnu_extension : 1;
  boolean groks_macro_args : 1;
  boolean blind_if_no_args : 1;
  builtin_func *func;
};

struct predefined
{
  const char *unix_name;
  const char *gnu_name;
  const char *func;
};

typedef struct builtin builtin;
typedef struct predefined predefined;

void builtin_init (void);
void define_builtin (const char *, const builtin *, symbol_lookup);
void define_user_macro (const char *, const char *, symbol_lookup);
void undivert_all (void);
void expand_user_macro (struct obstack *, symbol *, int, token_data **);
void m4_placeholder (struct obstack *, int, token_data **);

const builtin *find_builtin_by_addr (builtin_func *);
const builtin *find_builtin_by_name (const char *);

/* File: path.c  --- path search for include files.  */

void include_init (void);
void include_env_init (void);
void add_include_directory (const char *);
FILE *path_search (const char *, const char **);

/* File: eval.c  --- expression evaluation.  */

/* eval_t and unsigned_eval_t should be at least 32 bits.  */
typedef int eval_t;
typedef unsigned int unsigned_eval_t;

boolean evaluate (const char *, eval_t *);

/* File: format.c  --- printf like formatting.  */

void format (struct obstack *, int, token_data **);

/* File: freeze.c --- frozen state files.  */

void produce_frozen_state (const char *);
void reload_frozen_state (const char *);

/* Debugging the memory allocator.  */

#ifdef WITH_DMALLOC
# define DMALLOC_FUNC_CHECK
# include <dmalloc.h>
#endif

/* Other debug stuff.  */

#ifdef DEBUG
# define DEBUG_INCL   1
# define DEBUG_INPUT  1
# define DEBUG_MACRO  1
# define DEBUG_OUTPUT 1
# define DEBUG_STKOVF 1
# define DEBUG_SYM    1
#endif

/* Convert a possibly-signed character to an unsigned character.  This is
   a bit safer than casting to unsigned char, since it catches some type
   errors that the cast doesn't.  */
static inline unsigned char to_uchar (char ch) { return ch; }
