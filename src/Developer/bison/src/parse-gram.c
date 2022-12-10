/* A Bison parser, made by GNU Bison 2.2a.  */

/* Skeleton implementation for Bison's Yacc-like parsers in C

   Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004, 2005, 2006
   Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Bison version.  */
#define YYBISON_VERSION "2.2a"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 1

/* Using locations.  */
#define YYLSP_NEEDED 1

/* Substitute the variable and function names.  */
#define yyparse gram_parse
#define yylex   gram_lex
#define yyerror gram_error
#define yylval  gram_lval
#define yychar  gram_char
#define yydebug gram_debug
#define yynerrs gram_nerrs
#define yylloc gram_lloc

/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     GRAM_EOF = 0,
     STRING = 258,
     INT = 259,
     PERCENT_TOKEN = 260,
     PERCENT_NTERM = 261,
     PERCENT_TYPE = 262,
     PERCENT_DESTRUCTOR = 263,
     PERCENT_PRINTER = 264,
     PERCENT_UNION = 265,
     PERCENT_LEFT = 266,
     PERCENT_RIGHT = 267,
     PERCENT_NONASSOC = 268,
     PERCENT_PREC = 269,
     PERCENT_DPREC = 270,
     PERCENT_MERGE = 271,
     PERCENT_DEBUG = 272,
     PERCENT_DEFAULT_PREC = 273,
     PERCENT_DEFINE = 274,
     PERCENT_DEFINES = 275,
     PERCENT_ERROR_VERBOSE = 276,
     PERCENT_EXPECT = 277,
     PERCENT_EXPECT_RR = 278,
     PERCENT_FILE_PREFIX = 279,
     PERCENT_GLR_PARSER = 280,
     PERCENT_INITIAL_ACTION = 281,
     PERCENT_LEX_PARAM = 282,
     PERCENT_LOCATIONS = 283,
     PERCENT_NAME_PREFIX = 284,
     PERCENT_NO_DEFAULT_PREC = 285,
     PERCENT_NO_LINES = 286,
     PERCENT_NONDETERMINISTIC_PARSER = 287,
     PERCENT_OUTPUT = 288,
     PERCENT_PARSE_PARAM = 289,
     PERCENT_PURE_PARSER = 290,
     PERCENT_REQUIRE = 291,
     PERCENT_SKELETON = 292,
     PERCENT_START = 293,
     PERCENT_TOKEN_TABLE = 294,
     PERCENT_VERBOSE = 295,
     PERCENT_YACC = 296,
     TYPE = 297,
     EQUAL = 298,
     SEMICOLON = 299,
     PIPE = 300,
     ID = 301,
     ID_COLON = 302,
     PERCENT_PERCENT = 303,
     PROLOGUE = 304,
     EPILOGUE = 305,
     BRACED_CODE = 306
   };
#endif
/* Tokens.  */
#define GRAM_EOF 0
#define STRING 258
#define INT 259
#define PERCENT_TOKEN 260
#define PERCENT_NTERM 261
#define PERCENT_TYPE 262
#define PERCENT_DESTRUCTOR 263
#define PERCENT_PRINTER 264
#define PERCENT_UNION 265
#define PERCENT_LEFT 266
#define PERCENT_RIGHT 267
#define PERCENT_NONASSOC 268
#define PERCENT_PREC 269
#define PERCENT_DPREC 270
#define PERCENT_MERGE 271
#define PERCENT_DEBUG 272
#define PERCENT_DEFAULT_PREC 273
#define PERCENT_DEFINE 274
#define PERCENT_DEFINES 275
#define PERCENT_ERROR_VERBOSE 276
#define PERCENT_EXPECT 277
#define PERCENT_EXPECT_RR 278
#define PERCENT_FILE_PREFIX 279
#define PERCENT_GLR_PARSER 280
#define PERCENT_INITIAL_ACTION 281
#define PERCENT_LEX_PARAM 282
#define PERCENT_LOCATIONS 283
#define PERCENT_NAME_PREFIX 284
#define PERCENT_NO_DEFAULT_PREC 285
#define PERCENT_NO_LINES 286
#define PERCENT_NONDETERMINISTIC_PARSER 287
#define PERCENT_OUTPUT 288
#define PERCENT_PARSE_PARAM 289
#define PERCENT_PURE_PARSER 290
#define PERCENT_REQUIRE 291
#define PERCENT_SKELETON 292
#define PERCENT_START 293
#define PERCENT_TOKEN_TABLE 294
#define PERCENT_VERBOSE 295
#define PERCENT_YACC 296
#define TYPE 297
#define EQUAL 298
#define SEMICOLON 299
#define PIPE 300
#define ID 301
#define ID_COLON 302
#define PERCENT_PERCENT 303
#define PROLOGUE 304
#define EPILOGUE 305
#define BRACED_CODE 306




/* Copy the first part of user declarations.  */
#line 1 "parse-gram.y"
/* Bison Grammar Parser                             -*- C -*-

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


/* Enabling traces.  */
#ifndef YYDEBUG
# define YYDEBUG 1
#endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 1
#endif

/* Enabling the token table.  */
#ifndef YYTOKEN_TABLE
# define YYTOKEN_TABLE 0
#endif

#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
#line 94 "parse-gram.y"
{
  symbol *symbol;
  symbol_list *list;
  int integer;
  char *chars;
  assoc assoc;
  uniqstr uniqstr;
}
/* Line 193 of yacc.c.  */
#line 290 "parse-gram.c"
	YYSTYPE;
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif

#if ! defined YYLTYPE && ! defined YYLTYPE_IS_DECLARED
typedef struct YYLTYPE
{
  int first_line;
  int first_column;
  int last_line;
  int last_column;
} YYLTYPE;
# define yyltype YYLTYPE /* obsolescent; will be withdrawn */
# define YYLTYPE_IS_DECLARED 1
# define YYLTYPE_IS_TRIVIAL 1
#endif


/* Copy the second part of user declarations.  */


/* Line 216 of yacc.c.  */
#line 315 "parse-gram.c"

#ifdef short
# undef short
#endif

#ifdef YYTYPE_UINT8
typedef YYTYPE_UINT8 yytype_uint8;
#else
typedef unsigned char yytype_uint8;
#endif

#ifdef YYTYPE_INT8
typedef YYTYPE_INT8 yytype_int8;
#elif (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
typedef signed char yytype_int8;
#else
typedef short int yytype_int8;
#endif

#ifdef YYTYPE_UINT16
typedef YYTYPE_UINT16 yytype_uint16;
#else
typedef unsigned short int yytype_uint16;
#endif

#ifdef YYTYPE_INT16
typedef YYTYPE_INT16 yytype_int16;
#else
typedef short int yytype_int16;
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif ! defined YYSIZE_T && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned int
# endif
#endif

#define YYSIZE_MAXIMUM ((YYSIZE_T) -1)

#ifndef YY_
# if YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(msgid) dgettext ("bison-runtime", msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(msgid) msgid
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(e) ((void) (e))
#else
# define YYUSE(e) /* empty */
#endif

/* Identity function, used to suppress warnings about constant conditions.  */
#ifndef lint
# define YYID(n) (n)
#else
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static int
YYID (int i)
#else
static int
YYID (i)
    int i;
#endif
{
  return i;
}
#endif

#if ! defined yyoverflow || YYERROR_VERBOSE

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#     ifndef _STDLIB_H
#      define _STDLIB_H 1
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's `empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (YYID (0))
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined _STDLIB_H \
       && ! ((defined YYMALLOC || defined malloc) \
	     && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef _STDLIB_H
#    define _STDLIB_H 1
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* ! defined yyoverflow || YYERROR_VERBOSE */


#if (! defined yyoverflow \
     && (! defined __cplusplus \
	 || (defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL \
	     && defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yytype_int16 yyss;
  YYSTYPE yyvs;
    YYLTYPE yyls;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (yytype_int16) + sizeof (YYSTYPE) + sizeof (YYLTYPE)) \
      + 2 * YYSTACK_GAP_MAXIMUM)

/* Copy COUNT objects from FROM to TO.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(To, From, Count) \
      __builtin_memcpy (To, From, (Count) * sizeof (*(From)))
#  else
#   define YYCOPY(To, From, Count)		\
      do					\
	{					\
	  YYSIZE_T yyi;				\
	  for (yyi = 0; yyi < (Count); yyi++)	\
	    (To)[yyi] = (From)[yyi];		\
	}					\
      while (YYID (0))
#  endif
# endif

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack)					\
    do									\
      {									\
	YYSIZE_T yynewbytes;						\
	YYCOPY (&yyptr->Stack, Stack, yysize);				\
	Stack = &yyptr->Stack;						\
	yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
	yyptr += yynewbytes / sizeof (*yyptr);				\
      }									\
    while (YYID (0))

#endif

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  3
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   161

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  52
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  26
/* YYNRULES -- Number of rules.  */
#define YYNRULES  82
/* YYNRULES -- Number of states.  */
#define YYNSTATES  111

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   306

#define YYTRANSLATE(YYX)						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const yytype_uint8 yyprhs[] =
{
       0,     0,     3,     8,     9,    12,    14,    16,    18,    21,
      25,    27,    29,    32,    35,    39,    41,    43,    45,    47,
      51,    53,    55,    59,    61,    63,    66,    69,    71,    73,
      75,    77,    79,    81,    84,    86,    89,    92,    94,    96,
      97,   101,   102,   106,   110,   114,   116,   118,   120,   121,
     123,   125,   128,   130,   132,   135,   138,   142,   144,   147,
     149,   152,   154,   157,   160,   161,   165,   167,   171,   174,
     175,   178,   181,   185,   189,   193,   195,   197,   198,   201,
     203,   205,   206
};

/* YYRHS -- A `-1'-separated list of the rules' RHS.  */
static const yytype_int8 yyrhs[] =
{
      53,     0,    -1,    54,    48,    66,    77,    -1,    -1,    54,
      55,    -1,    56,    -1,    49,    -1,    17,    -1,    19,    76,
      -1,    19,    76,    76,    -1,    20,    -1,    21,    -1,    22,
       4,    -1,    23,     4,    -1,    24,    43,    76,    -1,    25,
      -1,    26,    -1,    27,    -1,    28,    -1,    29,    43,    76,
      -1,    31,    -1,    32,    -1,    33,    43,    76,    -1,    34,
      -1,    35,    -1,    36,    76,    -1,    37,    76,    -1,    39,
      -1,    40,    -1,    41,    -1,    44,    -1,    60,    -1,    57,
      -1,    38,    72,    -1,    10,    -1,     8,    63,    -1,     9,
      63,    -1,    18,    -1,    30,    -1,    -1,     6,    58,    65,
      -1,    -1,     5,    59,    65,    -1,     7,    42,    63,    -1,
      61,    62,    63,    -1,    11,    -1,    12,    -1,    13,    -1,
      -1,    42,    -1,    72,    -1,    63,    72,    -1,    42,    -1,
      46,    -1,    46,     4,    -1,    46,    75,    -1,    46,     4,
      75,    -1,    64,    -1,    65,    64,    -1,    67,    -1,    66,
      67,    -1,    68,    -1,    56,    44,    -1,     1,    44,    -1,
      -1,    47,    69,    70,    -1,    71,    -1,    70,    45,    71,
      -1,    70,    44,    -1,    -1,    71,    72,    -1,    71,    73,
      -1,    71,    14,    72,    -1,    71,    15,     4,    -1,    71,
      16,    42,    -1,    46,    -1,    75,    -1,    -1,    74,    51,
      -1,     3,    -1,     3,    -1,    -1,    48,    50,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   202,   202,   210,   212,   216,   217,   218,   219,   224,
     225,   226,   227,   228,   229,   230,   235,   239,   240,   241,
     242,   243,   244,   245,   246,   247,   248,   249,   250,   251,
     252,   256,   257,   258,   262,   278,   285,   292,   296,   303,
     303,   308,   308,   313,   323,   338,   339,   340,   344,   345,
     351,   352,   357,   361,   366,   372,   378,   389,   390,   399,
     400,   406,   407,   408,   415,   415,   419,   420,   421,   426,
     427,   429,   430,   432,   434,   439,   440,   456,   456,   462,
     471,   476,   478
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || YYTOKEN_TABLE
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "\"end of file\"", "error", "$undefined", "\"string\"", "\"integer\"",
  "\"%token\"", "\"%nterm\"", "\"%type\"", "\"%destructor {...}\"",
  "\"%printer {...}\"", "\"%union {...}\"", "\"%left\"", "\"%right\"",
  "\"%nonassoc\"", "\"%prec\"", "\"%dprec\"", "\"%merge\"", "\"%debug\"",
  "\"%default-prec\"", "\"%define\"", "\"%defines\"", "\"%error-verbose\"",
  "\"%expect\"", "\"%expect-rr\"", "\"%file-prefix\"", "\"%glr-parser\"",
  "\"%initial-action {...}\"", "\"%lex-param {...}\"", "\"%locations\"",
  "\"%name-prefix\"", "\"%no-default-prec\"", "\"%no-lines\"",
  "\"%nondeterministic-parser\"", "\"%output\"", "\"%parse-param {...}\"",
  "\"%pure-parser\"", "\"%require\"", "\"%skeleton\"", "\"%start\"",
  "\"%token-table\"", "\"%verbose\"", "\"%yacc\"", "\"type\"", "\"=\"",
  "\";\"", "\"|\"", "\"identifier\"", "\"identifier:\"", "\"%%\"",
  "\"%{...%}\"", "\"epilogue\"", "\"{...}\"", "$accept", "input",
  "declarations", "declaration", "grammar_declaration",
  "symbol_declaration", "@1", "@2", "precedence_declaration",
  "precedence_declarator", "type.opt", "symbols.1", "symbol_def",
  "symbol_defs.1", "grammar", "rules_or_grammar_declaration", "rules",
  "@3", "rhses.1", "rhs", "symbol", "action", "@4", "string_as_id",
  "string_content", "epilogue.opt", 0
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[YYLEX-NUM] -- Internal token number corresponding to
   token YYLEX-NUM.  */
static const yytype_uint16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   290,   291,   292,   293,   294,
     295,   296,   297,   298,   299,   300,   301,   302,   303,   304,
     305,   306
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,    52,    53,    54,    54,    55,    55,    55,    55,    55,
      55,    55,    55,    55,    55,    55,    55,    55,    55,    55,
      55,    55,    55,    55,    55,    55,    55,    55,    55,    55,
      55,    56,    56,    56,    56,    56,    56,    56,    56,    58,
      57,    59,    57,    57,    60,    61,    61,    61,    62,    62,
      63,    63,    64,    64,    64,    64,    64,    65,    65,    66,
      66,    67,    67,    67,    69,    68,    70,    70,    70,    71,
      71,    71,    71,    71,    71,    72,    72,    74,    73,    75,
      76,    77,    77
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     4,     0,     2,     1,     1,     1,     2,     3,
       1,     1,     2,     2,     3,     1,     1,     1,     1,     3,
       1,     1,     3,     1,     1,     2,     2,     1,     1,     1,
       1,     1,     1,     2,     1,     2,     2,     1,     1,     0,
       3,     0,     3,     3,     3,     1,     1,     1,     0,     1,
       1,     2,     1,     1,     2,     2,     3,     1,     2,     1,
       2,     1,     2,     2,     0,     3,     1,     3,     2,     0,
       2,     2,     3,     3,     3,     1,     1,     0,     2,     1,
       1,     0,     2
};

/* YYDEFACT[STATE-NAME] -- Default rule to reduce with in state
   STATE-NUM when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       3,     0,     0,     1,    41,    39,     0,     0,     0,    34,
      45,    46,    47,     7,    37,     0,    10,    11,     0,     0,
       0,    15,    16,    17,    18,     0,    38,    20,    21,     0,
      23,    24,     0,     0,     0,    27,    28,    29,    30,     0,
       6,     4,     5,    32,    31,    48,     0,     0,     0,    79,
      75,    35,    50,    76,    36,    80,     8,    12,    13,     0,
       0,     0,    25,    26,    33,     0,    64,     0,     0,    59,
      61,    49,     0,    52,    53,    57,    42,    40,    43,    51,
       9,    14,    19,    22,    63,    69,    62,     0,    60,     2,
      44,    54,    55,    58,    65,    66,    82,    56,    68,    69,
       0,     0,     0,    70,    71,     0,    67,    72,    73,    74,
      78
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int8 yydefgoto[] =
{
      -1,     1,     2,    41,    67,    43,    47,    46,    44,    45,
      72,    51,    75,    76,    68,    69,    70,    85,    94,    95,
      52,   104,   105,    53,    56,    89
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -69
static const yytype_int8 yypact[] =
{
     -69,     5,   112,   -69,   -69,   -69,   -35,     0,     0,   -69,
     -69,   -69,   -69,   -69,   -69,    13,   -69,   -69,    20,    31,
     -18,   -69,   -69,   -69,   -69,    -6,   -69,   -69,   -69,    -5,
     -69,   -69,    13,    13,     0,   -69,   -69,   -69,   -69,    69,
     -69,   -69,   -69,   -69,   -69,    -2,   -38,   -38,     0,   -69,
     -69,     0,   -69,   -69,     0,   -69,    13,   -69,   -69,    13,
      13,    13,   -69,   -69,   -69,    -8,   -69,     3,    21,   -69,
     -69,   -69,     0,   -69,     6,   -69,   -38,   -38,     0,   -69,
     -69,   -69,   -69,   -69,   -69,   -69,   -69,     2,   -69,   -69,
       0,    39,   -69,   -69,   -33,    -1,   -69,   -69,   -69,   -69,
       0,    44,     1,   -69,   -69,     4,    -1,   -69,   -69,   -69,
     -69
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int8 yypgoto[] =
{
     -69,   -69,   -69,   -69,    47,   -69,   -69,   -69,   -69,   -69,
     -69,    -7,   -58,     7,   -69,   -15,   -69,   -69,   -69,   -42,
     -34,   -69,   -69,   -68,    30,   -69
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If zero, do what YYDEFACT says.
   If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -82
static const yytype_int8 yytable[] =
{
      64,    54,    49,    49,    73,     3,    92,    48,    74,    49,
      91,    98,    99,   100,   101,   102,    55,    79,    93,    93,
      79,   -81,    65,    97,    57,    59,     4,     5,     6,     7,
       8,     9,    10,    11,    12,    58,    84,    60,    61,    14,
      71,    78,    49,   109,    79,    50,    50,    86,   108,    42,
     -77,    26,    96,    88,    77,   110,    79,   106,     0,    34,
       0,   103,    62,    63,     0,    90,   107,     0,    66,    87,
      65,     0,   103,     0,     4,     5,     6,     7,     8,     9,
      10,    11,    12,     0,     0,     0,    80,    14,     0,    81,
      82,    83,     0,     0,     0,     0,     0,     0,     0,    26,
       0,     0,     0,     0,     0,     0,     0,    34,     0,     0,
       0,     0,     0,     0,     0,     0,    66,     4,     5,     6,
       7,     8,     9,    10,    11,    12,     0,     0,     0,    13,
      14,    15,    16,    17,    18,    19,    20,    21,    22,    23,
      24,    25,    26,    27,    28,    29,    30,    31,    32,    33,
      34,    35,    36,    37,     0,     0,    38,     0,     0,     0,
      39,    40
};

static const yytype_int8 yycheck[] =
{
      34,     8,     3,     3,    42,     0,    74,    42,    46,     3,
       4,    44,    45,    14,    15,    16,     3,    51,    76,    77,
      54,     0,     1,    91,     4,    43,     5,     6,     7,     8,
       9,    10,    11,    12,    13,     4,    44,    43,    43,    18,
      42,    48,     3,    42,    78,    46,    46,    44,     4,     2,
      51,    30,    50,    68,    47,    51,    90,    99,    -1,    38,
      -1,    95,    32,    33,    -1,    72,   100,    -1,    47,    48,
       1,    -1,   106,    -1,     5,     6,     7,     8,     9,    10,
      11,    12,    13,    -1,    -1,    -1,    56,    18,    -1,    59,
      60,    61,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    30,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    38,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    47,     5,     6,     7,
       8,     9,    10,    11,    12,    13,    -1,    -1,    -1,    17,
      18,    19,    20,    21,    22,    23,    24,    25,    26,    27,
      28,    29,    30,    31,    32,    33,    34,    35,    36,    37,
      38,    39,    40,    41,    -1,    -1,    44,    -1,    -1,    -1,
      48,    49
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,    53,    54,     0,     5,     6,     7,     8,     9,    10,
      11,    12,    13,    17,    18,    19,    20,    21,    22,    23,
      24,    25,    26,    27,    28,    29,    30,    31,    32,    33,
      34,    35,    36,    37,    38,    39,    40,    41,    44,    48,
      49,    55,    56,    57,    60,    61,    59,    58,    42,     3,
      46,    63,    72,    75,    63,     3,    76,     4,     4,    43,
      43,    43,    76,    76,    72,     1,    47,    56,    66,    67,
      68,    42,    62,    42,    46,    64,    65,    65,    63,    72,
      76,    76,    76,    76,    44,    69,    44,    48,    67,    77,
      63,     4,    75,    64,    70,    71,    50,    75,    44,    45,
      14,    15,    16,    72,    73,    74,    71,    72,     4,    42,
      51
};

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		(-2)
#define YYEOF		0

#define YYACCEPT	goto yyacceptlab
#define YYABORT		goto yyabortlab
#define YYERROR		goto yyerrorlab


/* Like YYERROR except do call yyerror.  This remains here temporarily
   to ease the transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  */

#define YYFAIL		goto yyerrlab

#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)					\
do								\
  if (yychar == YYEMPTY && yylen == 1)				\
    {								\
      yychar = (Token);						\
      yylval = (Value);						\
      yytoken = YYTRANSLATE (yychar);				\
      YYPOPSTACK (1);						\
      goto yybackup;						\
    }								\
  else								\
    {								\
      yyerror (YY_("syntax error: cannot back up")); \
      YYERROR;							\
    }								\
while (YYID (0))


#define YYTERROR	1
#define YYERRCODE	256


/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

#define YYRHSLOC(Rhs, K) ((Rhs)[K])
#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)				\
    do									\
      if (YYID (N))                                                    \
	{								\
	  (Current).first_line   = YYRHSLOC (Rhs, 1).first_line;	\
	  (Current).first_column = YYRHSLOC (Rhs, 1).first_column;	\
	  (Current).last_line    = YYRHSLOC (Rhs, N).last_line;		\
	  (Current).last_column  = YYRHSLOC (Rhs, N).last_column;	\
	}								\
      else								\
	{								\
	  (Current).first_line   = (Current).last_line   =		\
	    YYRHSLOC (Rhs, 0).last_line;				\
	  (Current).first_column = (Current).last_column =		\
	    YYRHSLOC (Rhs, 0).last_column;				\
	}								\
    while (YYID (0))
#endif


/* YY_LOCATION_PRINT -- Print the location on the stream.
   This macro was not mandated originally: define only if we know
   we won't break user code: when these are the locations we know.  */

#ifndef YY_LOCATION_PRINT
# if YYLTYPE_IS_TRIVIAL
#  define YY_LOCATION_PRINT(File, Loc)			\
     fprintf (File, "%d.%d-%d.%d",			\
	      (Loc).first_line, (Loc).first_column,	\
	      (Loc).last_line,  (Loc).last_column)
# else
#  define YY_LOCATION_PRINT(File, Loc) ((void) 0)
# endif
#endif


/* YYLEX -- calling `yylex' with the right arguments.  */

#ifdef YYLEX_PARAM
# define YYLEX yylex (&yylval, &yylloc, YYLEX_PARAM)
#else
# define YYLEX yylex (&yylval, &yylloc)
#endif

/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)			\
do {						\
  if (yydebug)					\
    YYFPRINTF Args;				\
} while (YYID (0))

# define YY_SYMBOL_PRINT(Title, Type, Value, Location)			  \
do {									  \
  if (yydebug)								  \
    {									  \
      YYFPRINTF (stderr, "%s ", Title);					  \
      yy_symbol_print (stderr,						  \
		  Type, Value, Location); \
      YYFPRINTF (stderr, "\n");						  \
    }									  \
} while (YYID (0))


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp)
#else
static void
yy_symbol_value_print (yyoutput, yytype, yyvaluep, yylocationp)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
    YYLTYPE const * const yylocationp;
#endif
{
  if (!yyvaluep)
    return;
  YYUSE (yylocationp);
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# else
  YYUSE (yyoutput);
# endif
  switch (yytype)
    {
      case 3: /* "\"string\"" */
#line 179 "parse-gram.y"
	{ fprintf (stderr, "\"%s\"", (yyvaluep->chars)); };
#line 979 "parse-gram.c"
	break;
      case 4: /* "\"integer\"" */
#line 192 "parse-gram.y"
	{ fprintf (stderr, "%d", (yyvaluep->integer)); };
#line 984 "parse-gram.c"
	break;
      case 8: /* "\"%destructor {...}\"" */
#line 181 "parse-gram.y"
	{ fprintf (stderr, "{\n%s\n}", (yyvaluep->chars)); };
#line 989 "parse-gram.c"
	break;
      case 9: /* "\"%printer {...}\"" */
#line 181 "parse-gram.y"
	{ fprintf (stderr, "{\n%s\n}", (yyvaluep->chars)); };
#line 994 "parse-gram.c"
	break;
      case 10: /* "\"%union {...}\"" */
#line 181 "parse-gram.y"
	{ fprintf (stderr, "{\n%s\n}", (yyvaluep->chars)); };
#line 999 "parse-gram.c"
	break;
      case 26: /* "\"%initial-action {...}\"" */
#line 181 "parse-gram.y"
	{ fprintf (stderr, "{\n%s\n}", (yyvaluep->chars)); };
#line 1004 "parse-gram.c"
	break;
      case 27: /* "\"%lex-param {...}\"" */
#line 181 "parse-gram.y"
	{ fprintf (stderr, "{\n%s\n}", (yyvaluep->chars)); };
#line 1009 "parse-gram.c"
	break;
      case 34: /* "\"%parse-param {...}\"" */
#line 181 "parse-gram.y"
	{ fprintf (stderr, "{\n%s\n}", (yyvaluep->chars)); };
#line 1014 "parse-gram.c"
	break;
      case 42: /* "\"type\"" */
#line 190 "parse-gram.y"
	{ fprintf (stderr, "<%s>", (yyvaluep->uniqstr)); };
#line 1019 "parse-gram.c"
	break;
      case 46: /* "\"identifier\"" */
#line 194 "parse-gram.y"
	{ fprintf (stderr, "%s", (yyvaluep->symbol)->tag); };
#line 1024 "parse-gram.c"
	break;
      case 47: /* "\"identifier:\"" */
#line 196 "parse-gram.y"
	{ fprintf (stderr, "%s:", (yyvaluep->symbol)->tag); };
#line 1029 "parse-gram.c"
	break;
      case 49: /* "\"%{...%}\"" */
#line 181 "parse-gram.y"
	{ fprintf (stderr, "{\n%s\n}", (yyvaluep->chars)); };
#line 1034 "parse-gram.c"
	break;
      case 50: /* "\"epilogue\"" */
#line 181 "parse-gram.y"
	{ fprintf (stderr, "{\n%s\n}", (yyvaluep->chars)); };
#line 1039 "parse-gram.c"
	break;
      case 72: /* "symbol" */
#line 194 "parse-gram.y"
	{ fprintf (stderr, "%s", (yyvaluep->symbol)->tag); };
#line 1044 "parse-gram.c"
	break;
      case 75: /* "string_as_id" */
#line 194 "parse-gram.y"
	{ fprintf (stderr, "%s", (yyvaluep->symbol)->tag); };
#line 1049 "parse-gram.c"
	break;
      case 76: /* "string_content" */
#line 179 "parse-gram.y"
	{ fprintf (stderr, "\"%s\"", (yyvaluep->chars)); };
#line 1054 "parse-gram.c"
	break;
      default:
	break;
    }
}


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp)
#else
static void
yy_symbol_print (yyoutput, yytype, yyvaluep, yylocationp)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
    YYLTYPE const * const yylocationp;
#endif
{
  if (yytype < YYNTOKENS)
    YYFPRINTF (yyoutput, "token %s (", yytname[yytype]);
  else
    YYFPRINTF (yyoutput, "nterm %s (", yytname[yytype]);

  YY_LOCATION_PRINT (yyoutput, *yylocationp);
  YYFPRINTF (yyoutput, ": ");
  yy_symbol_value_print (yyoutput, yytype, yyvaluep, yylocationp);
  YYFPRINTF (yyoutput, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_stack_print (yytype_int16 *bottom, yytype_int16 *top)
#else
static void
yy_stack_print (bottom, top)
    yytype_int16 *bottom;
    yytype_int16 *top;
#endif
{
  YYFPRINTF (stderr, "Stack now");
  for (; bottom <= top; ++bottom)
    YYFPRINTF (stderr, " %d", *bottom);
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)				\
do {								\
  if (yydebug)							\
    yy_stack_print ((Bottom), (Top));				\
} while (YYID (0))


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_reduce_print (YYSTYPE *yyvsp, YYLTYPE *yylsp, int yyrule)
#else
static void
yy_reduce_print (yyvsp, yylsp, yyrule)
    YYSTYPE *yyvsp;
    YYLTYPE *yylsp;
    int yyrule;
#endif
{
  int yynrhs = yyr2[yyrule];
  int yyi;
  unsigned long int yylno = yyrline[yyrule];
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu):\n",
	     yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      fprintf (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr, yyrhs[yyprhs[yyrule] + yyi],
		       &(yyvsp[(yyi + 1) - (yynrhs)])
		       , &(yylsp[(yyi + 1) - (yynrhs)])		       );
      fprintf (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)		\
do {					\
  if (yydebug)				\
    yy_reduce_print (yyvsp, yylsp, Rule); \
} while (YYID (0))

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
# define YY_SYMBOL_PRINT(Title, Type, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef	YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif



#if YYERROR_VERBOSE

# ifndef yystrlen
#  if defined __GLIBC__ && defined _STRING_H
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static YYSIZE_T
yystrlen (const char *yystr)
#else
static YYSIZE_T
yystrlen (yystr)
    const char *yystr;
#endif
{
  YYSIZE_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++)
    continue;
  return yylen;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static char *
yystpcpy (char *yydest, const char *yysrc)
#else
static char *
yystpcpy (yydest, yysrc)
    char *yydest;
    const char *yysrc;
#endif
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif

# ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYSIZE_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYSIZE_T yyn = 0;
      char const *yyp = yystr;

      for (;;)
	switch (*++yyp)
	  {
	  case '\'':
	  case ',':
	    goto do_not_strip_quotes;

	  case '\\':
	    if (*++yyp != '\\')
	      goto do_not_strip_quotes;
	    /* Fall through.  */
	  default:
	    if (yyres)
	      yyres[yyn] = *yyp;
	    yyn++;
	    break;

	  case '"':
	    if (yyres)
	      yyres[yyn] = '\0';
	    return yyn;
	  }
    do_not_strip_quotes: ;
    }

  if (! yyres)
    return yystrlen (yystr);

  return yystpcpy (yyres, yystr) - yyres;
}
# endif

/* Copy into YYRESULT an error message about the unexpected token
   YYCHAR while in state YYSTATE.  Return the number of bytes copied,
   including the terminating null byte.  If YYRESULT is null, do not
   copy anything; just return the number of bytes that would be
   copied.  As a special case, return 0 if an ordinary "syntax error"
   message will do.  Return YYSIZE_MAXIMUM if overflow occurs during
   size calculation.  */
static YYSIZE_T
yysyntax_error (char *yyresult, int yystate, int yychar)
{
  int yyn = yypact[yystate];

  if (! (YYPACT_NINF < yyn && yyn <= YYLAST))
    return 0;
  else
    {
      int yytype = YYTRANSLATE (yychar);
      YYSIZE_T yysize0 = yytnamerr (0, yytname[yytype]);
      YYSIZE_T yysize = yysize0;
      YYSIZE_T yysize1;
      int yysize_overflow = 0;
      enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
      char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
      int yyx;

# if 0
      /* This is so xgettext sees the translatable formats that are
	 constructed on the fly.  */
      YY_("syntax error, unexpected %s");
      YY_("syntax error, unexpected %s, expecting %s");
      YY_("syntax error, unexpected %s, expecting %s or %s");
      YY_("syntax error, unexpected %s, expecting %s or %s or %s");
      YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s");
# endif
      char *yyfmt;
      char const *yyf;
      static char const yyunexpected[] = "syntax error, unexpected %s";
      static char const yyexpecting[] = ", expecting %s";
      static char const yyor[] = " or %s";
      char yyformat[sizeof yyunexpected
		    + sizeof yyexpecting - 1
		    + ((YYERROR_VERBOSE_ARGS_MAXIMUM - 2)
		       * (sizeof yyor - 1))];
      char const *yyprefix = yyexpecting;

      /* Start YYX at -YYN if negative to avoid negative indexes in
	 YYCHECK.  */
      int yyxbegin = yyn < 0 ? -yyn : 0;

      /* Stay within bounds of both yycheck and yytname.  */
      int yychecklim = YYLAST - yyn + 1;
      int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
      int yycount = 1;

      yyarg[0] = yytname[yytype];
      yyfmt = yystpcpy (yyformat, yyunexpected);

      for (yyx = yyxbegin; yyx < yyxend; ++yyx)
	if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR)
	  {
	    if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
	      {
		yycount = 1;
		yysize = yysize0;
		yyformat[sizeof yyunexpected - 1] = '\0';
		break;
	      }
	    yyarg[yycount++] = yytname[yyx];
	    yysize1 = yysize + yytnamerr (0, yytname[yyx]);
	    yysize_overflow |= (yysize1 < yysize);
	    yysize = yysize1;
	    yyfmt = yystpcpy (yyfmt, yyprefix);
	    yyprefix = yyor;
	  }

      yyf = YY_(yyformat);
      yysize1 = yysize + yystrlen (yyf);
      yysize_overflow |= (yysize1 < yysize);
      yysize = yysize1;

      if (yysize_overflow)
	return YYSIZE_MAXIMUM;

      if (yyresult)
	{
	  /* Avoid sprintf, as that infringes on the user's name space.
	     Don't have undefined behavior even if the translation
	     produced a string with the wrong number of "%s"s.  */
	  char *yyp = yyresult;
	  int yyi = 0;
	  while ((*yyp = *yyf) != '\0')
	    {
	      if (*yyp == '%' && yyf[1] == 's' && yyi < yycount)
		{
		  yyp += yytnamerr (yyp, yyarg[yyi++]);
		  yyf += 2;
		}
	      else
		{
		  yyp++;
		  yyf++;
		}
	    }
	}
      return yysize;
    }
}
#endif /* YYERROR_VERBOSE */


/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep, YYLTYPE *yylocationp)
#else
static void
yydestruct (yymsg, yytype, yyvaluep, yylocationp)
    const char *yymsg;
    int yytype;
    YYSTYPE *yyvaluep;
    YYLTYPE *yylocationp;
#endif
{
  YYUSE (yyvaluep);
  YYUSE (yylocationp);

  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  switch (yytype)
    {

      default:
	break;
    }
}


/* Prevent warnings from -Wmissing-prototypes.  */

#ifdef YYPARSE_PARAM
#if defined __STDC__ || defined __cplusplus
int yyparse (void *YYPARSE_PARAM);
#else
int yyparse ();
#endif
#else /* ! YYPARSE_PARAM */
#if defined __STDC__ || defined __cplusplus
int yyparse (void);
#else
int yyparse ();
#endif
#endif /* ! YYPARSE_PARAM */






/*----------.
| yyparse.  |
`----------*/

#ifdef YYPARSE_PARAM
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (void *YYPARSE_PARAM)
#else
int
yyparse (YYPARSE_PARAM)
    void *YYPARSE_PARAM;
#endif
#else /* ! YYPARSE_PARAM */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (void)
#else
int
yyparse ()

#endif
#endif
{
  /* The look-ahead symbol.  */
int yychar;

/* The semantic value of the look-ahead symbol.  */
YYSTYPE yylval;

/* Number of syntax errors so far.  */
int yynerrs;
/* Location data for the look-ahead symbol.  */
YYLTYPE yylloc;

  int yystate;
  int yyn;
  int yyresult;
  /* Number of tokens to shift before error messages enabled.  */
  int yyerrstatus;
  /* Look-ahead token as an internal (translated) token number.  */
  int yytoken = 0;
#if YYERROR_VERBOSE
  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYSIZE_T yymsg_alloc = sizeof yymsgbuf;
#endif

  /* Three stacks and their tools:
     `yyss': related to states,
     `yyvs': related to semantic values,
     `yyls': related to locations.

     Refer to the stacks thru separate pointers, to allow yyoverflow
     to reallocate them elsewhere.  */

  /* The state stack.  */
  yytype_int16 yyssa[YYINITDEPTH];
  yytype_int16 *yyss = yyssa;
  yytype_int16 *yyssp;

  /* The semantic value stack.  */
  YYSTYPE yyvsa[YYINITDEPTH];
  YYSTYPE *yyvs = yyvsa;
  YYSTYPE *yyvsp;

  /* The location stack.  */
  YYLTYPE yylsa[YYINITDEPTH];
  YYLTYPE *yyls = yylsa;
  YYLTYPE *yylsp;
  /* The locations where the error started and ended.  */
  YYLTYPE yyerror_range[2];

#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N), yylsp -= (N))

  YYSIZE_T yystacksize = YYINITDEPTH;

  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;
  YYLTYPE yyloc;

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY;		/* Cause a token to be read.  */

  /* Initialize stack pointers.
     Waste one element of value and location stack
     so that they stay on the same level as the state stack.
     The wasted elements are never initialized.  */

  yyssp = yyss;
  yyvsp = yyvs;
  yylsp = yyls;
#if YYLTYPE_IS_TRIVIAL
  /* Initialize the default location before parsing starts.  */
  yylloc.first_line   = yylloc.last_line   = 1;
  yylloc.first_column = yylloc.last_column = 0;
#endif


  /* User initialization code.  */
#line 84 "parse-gram.y"
{
  /* Bison's grammar can initial empty locations, hence a default
     location is needed. */
  yylloc.start.file   = yylloc.end.file   = current_file;
  yylloc.start.line   = yylloc.end.line   = 1;
  yylloc.start.column = yylloc.end.column = 0;
}
/* Line 1078 of yacc.c.  */
#line 1573 "parse-gram.c"
  yylsp[0] = yylloc;
  goto yysetstate;

/*------------------------------------------------------------.
| yynewstate -- Push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
 yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyss + yystacksize - 1 <= yyssp)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
	/* Give user a chance to reallocate the stack.  Use copies of
	   these so that the &'s don't force the real ones into
	   memory.  */
	YYSTYPE *yyvs1 = yyvs;
	yytype_int16 *yyss1 = yyss;
	YYLTYPE *yyls1 = yyls;

	/* Each stack pointer address is followed by the size of the
	   data in use in that stack, in bytes.  This used to be a
	   conditional around just the two extra args, but that might
	   be undefined if yyoverflow is a macro.  */
	yyoverflow (YY_("memory exhausted"),
		    &yyss1, yysize * sizeof (*yyssp),
		    &yyvs1, yysize * sizeof (*yyvsp),
		    &yyls1, yysize * sizeof (*yylsp),
		    &yystacksize);
	yyls = yyls1;
	yyss = yyss1;
	yyvs = yyvs1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyexhaustedlab;
# else
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
	goto yyexhaustedlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
	yystacksize = YYMAXDEPTH;

      {
	yytype_int16 *yyss1 = yyss;
	union yyalloc *yyptr =
	  (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
	if (! yyptr)
	  goto yyexhaustedlab;
	YYSTACK_RELOCATE (yyss);
	YYSTACK_RELOCATE (yyvs);
	YYSTACK_RELOCATE (yyls);
#  undef YYSTACK_RELOCATE
	if (yyss1 != yyssa)
	  YYSTACK_FREE (yyss1);
      }
# endif
#endif /* no yyoverflow */

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;
      yylsp = yyls + yysize - 1;

      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
		  (unsigned long int) yystacksize));

      if (yyss + yystacksize - 1 <= yyssp)
	YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", yystate));

  goto yybackup;

/*-----------.
| yybackup.  |
`-----------*/
yybackup:

  /* Do appropriate processing given the current state.  Read a
     look-ahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to look-ahead token.  */
  yyn = yypact[yystate];
  if (yyn == YYPACT_NINF)
    goto yydefault;

  /* Not known => get a look-ahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid look-ahead symbol.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = YYLEX;
    }

  if (yychar <= YYEOF)
    {
      yychar = yytoken = YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yyn == 0 || yyn == YYTABLE_NINF)
	goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  if (yyn == YYFINAL)
    YYACCEPT;

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the look-ahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);

  /* Discard the shifted token unless it is eof.  */
  if (yychar != YYEOF)
    yychar = YYEMPTY;

  yystate = yyn;
  *++yyvsp = yylval;
  *++yylsp = yylloc;
  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- Do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     `$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];

  /* Default location.  */
  YYLLOC_DEFAULT (yyloc, (yylsp - yylen), yylen);
  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
        case 6:
#line 217 "parse-gram.y"
    { prologue_augment ((yyvsp[(1) - (1)].chars), (yylsp[(1) - (1)])); }
    break;

  case 7:
#line 218 "parse-gram.y"
    { debug_flag = true; }
    break;

  case 8:
#line 220 "parse-gram.y"
    {
      static char one[] = "1";
      muscle_insert ((yyvsp[(2) - (2)].chars), one);
    }
    break;

  case 9:
#line 224 "parse-gram.y"
    { muscle_insert ((yyvsp[(2) - (3)].chars), (yyvsp[(3) - (3)].chars)); }
    break;

  case 10:
#line 225 "parse-gram.y"
    { defines_flag = true; }
    break;

  case 11:
#line 226 "parse-gram.y"
    { error_verbose = true; }
    break;

  case 12:
#line 227 "parse-gram.y"
    { expected_sr_conflicts = (yyvsp[(2) - (2)].integer); }
    break;

  case 13:
#line 228 "parse-gram.y"
    { expected_rr_conflicts = (yyvsp[(2) - (2)].integer); }
    break;

  case 14:
#line 229 "parse-gram.y"
    { spec_file_prefix = (yyvsp[(3) - (3)].chars); }
    break;

  case 15:
#line 231 "parse-gram.y"
    {
      nondeterministic_parser = true;
      glr_parser = true;
    }
    break;

  case 16:
#line 236 "parse-gram.y"
    {
      muscle_code_grow ("initial_action", (yyvsp[(1) - (1)].chars), (yylsp[(1) - (1)]));
    }
    break;

  case 17:
#line 239 "parse-gram.y"
    { add_param ("lex_param", (yyvsp[(1) - (1)].chars), (yylsp[(1) - (1)])); }
    break;

  case 18:
#line 240 "parse-gram.y"
    { locations_flag = true; }
    break;

  case 19:
#line 241 "parse-gram.y"
    { spec_name_prefix = (yyvsp[(3) - (3)].chars); }
    break;

  case 20:
#line 242 "parse-gram.y"
    { no_lines_flag = true; }
    break;

  case 21:
#line 243 "parse-gram.y"
    { nondeterministic_parser = true; }
    break;

  case 22:
#line 244 "parse-gram.y"
    { spec_outfile = (yyvsp[(3) - (3)].chars); }
    break;

  case 23:
#line 245 "parse-gram.y"
    { add_param ("parse_param", (yyvsp[(1) - (1)].chars), (yylsp[(1) - (1)])); }
    break;

  case 24:
#line 246 "parse-gram.y"
    { pure_parser = true; }
    break;

  case 25:
#line 247 "parse-gram.y"
    { version_check (&(yylsp[(2) - (2)]), (yyvsp[(2) - (2)].chars)); }
    break;

  case 26:
#line 248 "parse-gram.y"
    { skeleton = (yyvsp[(2) - (2)].chars); }
    break;

  case 27:
#line 249 "parse-gram.y"
    { token_table_flag = true; }
    break;

  case 28:
#line 250 "parse-gram.y"
    { report_flag = report_states; }
    break;

  case 29:
#line 251 "parse-gram.y"
    { yacc_flag = true; }
    break;

  case 33:
#line 259 "parse-gram.y"
    {
      grammar_start_symbol_set ((yyvsp[(2) - (2)].symbol), (yylsp[(2) - (2)]));
    }
    break;

  case 34:
#line 263 "parse-gram.y"
    {
      char const *body = (yyvsp[(1) - (1)].chars);

      if (typed)
	{
	  /* Concatenate the union bodies, turning the first one's
	     trailing '}' into '\n', and omitting the second one's '{'.  */
	  char *code = muscle_find ("stype");
	  code[strlen (code) - 1] = '\n';
	  body++;
	}

      typed = true;
      muscle_code_grow ("stype", body, (yylsp[(1) - (1)]));
    }
    break;

  case 35:
#line 279 "parse-gram.y"
    {
      symbol_list *list;
      for (list = (yyvsp[(2) - (2)].list); list; list = list->next)
	symbol_destructor_set (list->sym, (yyvsp[(1) - (2)].chars), (yylsp[(1) - (2)]));
      symbol_list_free ((yyvsp[(2) - (2)].list));
    }
    break;

  case 36:
#line 286 "parse-gram.y"
    {
      symbol_list *list;
      for (list = (yyvsp[(2) - (2)].list); list; list = list->next)
	symbol_printer_set (list->sym, (yyvsp[(1) - (2)].chars), (yylsp[(1) - (2)]));
      symbol_list_free ((yyvsp[(2) - (2)].list));
    }
    break;

  case 37:
#line 293 "parse-gram.y"
    {
      default_prec = true;
    }
    break;

  case 38:
#line 297 "parse-gram.y"
    {
      default_prec = false;
    }
    break;

  case 39:
#line 303 "parse-gram.y"
    { current_class = nterm_sym; }
    break;

  case 40:
#line 304 "parse-gram.y"
    {
      current_class = unknown_sym;
      current_type = NULL;
    }
    break;

  case 41:
#line 308 "parse-gram.y"
    { current_class = token_sym; }
    break;

  case 42:
#line 309 "parse-gram.y"
    {
      current_class = unknown_sym;
      current_type = NULL;
    }
    break;

  case 43:
#line 314 "parse-gram.y"
    {
      symbol_list *list;
      for (list = (yyvsp[(3) - (3)].list); list; list = list->next)
	symbol_type_set (list->sym, (yyvsp[(2) - (3)].uniqstr), (yylsp[(2) - (3)]));
      symbol_list_free ((yyvsp[(3) - (3)].list));
    }
    break;

  case 44:
#line 324 "parse-gram.y"
    {
      symbol_list *list;
      ++current_prec;
      for (list = (yyvsp[(3) - (3)].list); list; list = list->next)
	{
	  symbol_type_set (list->sym, current_type, (yylsp[(2) - (3)]));
	  symbol_precedence_set (list->sym, current_prec, (yyvsp[(1) - (3)].assoc), (yylsp[(1) - (3)]));
	}
      symbol_list_free ((yyvsp[(3) - (3)].list));
      current_type = NULL;
    }
    break;

  case 45:
#line 338 "parse-gram.y"
    { (yyval.assoc) = left_assoc; }
    break;

  case 46:
#line 339 "parse-gram.y"
    { (yyval.assoc) = right_assoc; }
    break;

  case 47:
#line 340 "parse-gram.y"
    { (yyval.assoc) = non_assoc; }
    break;

  case 48:
#line 344 "parse-gram.y"
    { current_type = NULL; }
    break;

  case 49:
#line 345 "parse-gram.y"
    { current_type = (yyvsp[(1) - (1)].uniqstr); }
    break;

  case 50:
#line 351 "parse-gram.y"
    { (yyval.list) = symbol_list_new ((yyvsp[(1) - (1)].symbol), (yylsp[(1) - (1)])); }
    break;

  case 51:
#line 352 "parse-gram.y"
    { (yyval.list) = symbol_list_prepend ((yyvsp[(1) - (2)].list), (yyvsp[(2) - (2)].symbol), (yylsp[(2) - (2)])); }
    break;

  case 52:
#line 358 "parse-gram.y"
    {
       current_type = (yyvsp[(1) - (1)].uniqstr);
     }
    break;

  case 53:
#line 362 "parse-gram.y"
    {
       symbol_class_set ((yyvsp[(1) - (1)].symbol), current_class, (yylsp[(1) - (1)]), true);
       symbol_type_set ((yyvsp[(1) - (1)].symbol), current_type, (yylsp[(1) - (1)]));
     }
    break;

  case 54:
#line 367 "parse-gram.y"
    {
      symbol_class_set ((yyvsp[(1) - (2)].symbol), current_class, (yylsp[(1) - (2)]), true);
      symbol_type_set ((yyvsp[(1) - (2)].symbol), current_type, (yylsp[(1) - (2)]));
      symbol_user_token_number_set ((yyvsp[(1) - (2)].symbol), (yyvsp[(2) - (2)].integer), (yylsp[(2) - (2)]));
    }
    break;

  case 55:
#line 373 "parse-gram.y"
    {
      symbol_class_set ((yyvsp[(1) - (2)].symbol), current_class, (yylsp[(1) - (2)]), true);
      symbol_type_set ((yyvsp[(1) - (2)].symbol), current_type, (yylsp[(1) - (2)]));
      symbol_make_alias ((yyvsp[(1) - (2)].symbol), (yyvsp[(2) - (2)].symbol), (yyloc));
    }
    break;

  case 56:
#line 379 "parse-gram.y"
    {
      symbol_class_set ((yyvsp[(1) - (3)].symbol), current_class, (yylsp[(1) - (3)]), true);
      symbol_type_set ((yyvsp[(1) - (3)].symbol), current_type, (yylsp[(1) - (3)]));
      symbol_user_token_number_set ((yyvsp[(1) - (3)].symbol), (yyvsp[(2) - (3)].integer), (yylsp[(2) - (3)]));
      symbol_make_alias ((yyvsp[(1) - (3)].symbol), (yyvsp[(3) - (3)].symbol), (yyloc));
    }
    break;

  case 63:
#line 409 "parse-gram.y"
    {
      yyerrok;
    }
    break;

  case 64:
#line 415 "parse-gram.y"
    { current_lhs = (yyvsp[(1) - (1)].symbol); current_lhs_location = (yylsp[(1) - (1)]); }
    break;

  case 66:
#line 419 "parse-gram.y"
    { grammar_current_rule_end ((yylsp[(1) - (1)])); }
    break;

  case 67:
#line 420 "parse-gram.y"
    { grammar_current_rule_end ((yylsp[(3) - (3)])); }
    break;

  case 69:
#line 426 "parse-gram.y"
    { grammar_current_rule_begin (current_lhs, current_lhs_location); }
    break;

  case 70:
#line 428 "parse-gram.y"
    { grammar_current_rule_symbol_append ((yyvsp[(2) - (2)].symbol), (yylsp[(2) - (2)])); }
    break;

  case 72:
#line 431 "parse-gram.y"
    { grammar_current_rule_prec_set ((yyvsp[(3) - (3)].symbol), (yylsp[(3) - (3)])); }
    break;

  case 73:
#line 433 "parse-gram.y"
    { grammar_current_rule_dprec_set ((yyvsp[(3) - (3)].integer), (yylsp[(3) - (3)])); }
    break;

  case 74:
#line 435 "parse-gram.y"
    { grammar_current_rule_merge_set ((yyvsp[(3) - (3)].uniqstr), (yylsp[(3) - (3)])); }
    break;

  case 75:
#line 439 "parse-gram.y"
    { (yyval.symbol) = (yyvsp[(1) - (1)].symbol); }
    break;

  case 76:
#line 440 "parse-gram.y"
    { (yyval.symbol) = (yyvsp[(1) - (1)].symbol); }
    break;

  case 77:
#line 456 "parse-gram.y"
    { grammar_current_rule_action_append (last_string, last_braced_code_loc); }
    break;

  case 79:
#line 463 "parse-gram.y"
    {
      (yyval.symbol) = symbol_get (quotearg_style (c_quoting_style, (yyvsp[(1) - (1)].chars)), (yylsp[(1) - (1)]));
      symbol_class_set ((yyval.symbol), token_sym, (yylsp[(1) - (1)]), false);
    }
    break;

  case 80:
#line 472 "parse-gram.y"
    { (yyval.chars) = (yyvsp[(1) - (1)].chars); }
    break;

  case 82:
#line 479 "parse-gram.y"
    {
      muscle_code_grow ("epilogue", (yyvsp[(2) - (2)].chars), (yylsp[(2) - (2)]));
      scanner_last_string_free ();
    }
    break;


/* Line 1267 of yacc.c.  */
#line 2159 "parse-gram.c"
      default: break;
    }
  YY_SYMBOL_PRINT ("-> $$ =", yyr1[yyn], &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;
  *++yylsp = yyloc;

  /* Now `shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


/*------------------------------------.
| yyerrlab -- here on detecting error |
`------------------------------------*/
yyerrlab:
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if ! YYERROR_VERBOSE
      yyerror (YY_("syntax error"));
#else
      {
	YYSIZE_T yysize = yysyntax_error (0, yystate, yychar);
	if (yymsg_alloc < yysize && yymsg_alloc < YYSTACK_ALLOC_MAXIMUM)
	  {
	    YYSIZE_T yyalloc = 2 * yysize;
	    if (! (yysize <= yyalloc && yyalloc <= YYSTACK_ALLOC_MAXIMUM))
	      yyalloc = YYSTACK_ALLOC_MAXIMUM;
	    if (yymsg != yymsgbuf)
	      YYSTACK_FREE (yymsg);
	    yymsg = (char *) YYSTACK_ALLOC (yyalloc);
	    if (yymsg)
	      yymsg_alloc = yyalloc;
	    else
	      {
		yymsg = yymsgbuf;
		yymsg_alloc = sizeof yymsgbuf;
	      }
	  }

	if (0 < yysize && yysize <= yymsg_alloc)
	  {
	    (void) yysyntax_error (yymsg, yystate, yychar);
	    yyerror (yymsg);
	  }
	else
	  {
	    yyerror (YY_("syntax error"));
	    if (yysize != 0)
	      goto yyexhaustedlab;
	  }
      }
#endif
    }

  yyerror_range[0] = yylloc;

  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse look-ahead token after an
	 error, discard it.  */

      if (yychar <= YYEOF)
	{
	  /* Return failure if at end of input.  */
	  if (yychar == YYEOF)
	    YYABORT;
	}
      else
	{
	  yydestruct ("Error: discarding",
		      yytoken, &yylval, &yylloc);
	  yychar = YYEMPTY;
	}
    }

  /* Else will try to reuse look-ahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:

  /* Pacify compilers like GCC when the user code never invokes
     YYERROR and the label yyerrorlab therefore never appears in user
     code.  */
  if (/*CONSTCOND*/ 0)
     goto yyerrorlab;

  yyerror_range[0] = yylsp[1-yylen];
  /* Do not reclaim the symbols of the rule which action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;	/* Each real token shifted decrements this.  */

  for (;;)
    {
      yyn = yypact[yystate];
      if (yyn != YYPACT_NINF)
	{
	  yyn += YYTERROR;
	  if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYTERROR)
	    {
	      yyn = yytable[yyn];
	      if (0 < yyn)
		break;
	    }
	}

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
	YYABORT;

      yyerror_range[0] = *yylsp;
      yydestruct ("Error: popping",
		  yystos[yystate], yyvsp, yylsp);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  if (yyn == YYFINAL)
    YYACCEPT;

  *++yyvsp = yylval;

  yyerror_range[1] = yylloc;
  /* Using YYLLOC is tempting, but would change the location of
     the look-ahead.  YYLOC is available though.  */
  YYLLOC_DEFAULT (yyloc, (yyerror_range - 1), 2);
  *++yylsp = yyloc;

  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", yystos[yyn], yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturn;

/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturn;

#ifndef yyoverflow
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
  if (yychar != YYEOF && yychar != YYEMPTY)
     yydestruct ("Cleanup: discarding lookahead",
		 yytoken, &yylval, &yylloc);
  /* Do not reclaim the symbols of the rule which action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
		  yystos[*yyssp], yyvsp, yylsp);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
#if YYERROR_VERBOSE
  if (yymsg != yymsgbuf)
    YYSTACK_FREE (yymsg);
#endif
  /* Make sure YYID is used.  */
  return YYID (yyresult);
}


#line 485 "parse-gram.y"



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

