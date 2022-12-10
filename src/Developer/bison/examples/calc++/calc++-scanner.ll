%{                                            /* -*- C++ -*- */
# include <cstdlib>
# include <errno.h>
# include <limits.h>
# include <string>
# include "calc++-driver.hh"
# include "calc++-parser.hh"

/* Work around an incompatibility in flex (at least versions
   2.5.31 through 2.5.33): it generates code that does
   not conform to C89.  See Debian bug 333231
   <http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=333231>.  */
# undef yywrap
# define yywrap() 1

/* By default yylex returns int, we use token_type.
   Unfortunately yyterminate by default returns 0, which is
   not of token_type.  */
#define yyterminate() return token::END
%}

%option noyywrap nounput batch debug

id    [a-zA-Z][a-zA-Z_0-9]*
int   [0-9]+
blank [ \t]

%{
# define YY_USER_ACTION  yylloc->columns (yyleng);
%}
%%
%{
  yylloc->step ();
%}
{blank}+   yylloc->step ();
[\n]+      yylloc->lines (yyleng); yylloc->step ();

%{
  typedef yy::calcxx_parser::token token;
%}
           /* Convert ints to the actual type of tokens.  */
[-+*/]     return yy::calcxx_parser::token_type (yytext[0]);
":="       return token::ASSIGN;
{int}      {
  errno = 0;
  long n = strtol (yytext, NULL, 10);
  if (! (INT_MIN <= n && n <= INT_MAX && errno != ERANGE))
    driver.error (*yylloc, "integer is out of range");
  yylval->ival = n;
  return token::NUMBER;
}
{id}       yylval->sval = new std::string (yytext); return token::IDENTIFIER;
.          driver.error (*yylloc, "invalid character");
%%

void
calcxx_driver::scan_begin ()
{
  yy_flex_debug = trace_scanning;
  if (!(yyin = fopen (file.c_str (), "r")))
    error (std::string ("cannot open ") + file);
}

void
calcxx_driver::scan_end ()
{
  fclose (yyin);
}
