#line 7421 "../../doc/bison.texinfo"
%skeleton "lalr1.cc"                          /*  -*- C++ -*- */
%require "2.1a"
%defines
%define "parser_class_name" "calcxx_parser"
#line 7437 "../../doc/bison.texinfo"
%{
# include <string>
class calcxx_driver;
%}
#line 7450 "../../doc/bison.texinfo"
// The parsing context.
%parse-param { calcxx_driver& driver }
%lex-param   { calcxx_driver& driver }
#line 7463 "../../doc/bison.texinfo"
%locations
%initial-action
{
  // Initialize the initial location.
  @$.begin.filename = @$.end.filename = &driver.file;
};
#line 7477 "../../doc/bison.texinfo"
%debug
%error-verbose
#line 7487 "../../doc/bison.texinfo"
// Symbols.
%union
{
  int          ival;
  std::string *sval;
};
#line 7502 "../../doc/bison.texinfo"
%{
# include "calc++-driver.hh"
%}
#line 7517 "../../doc/bison.texinfo"
%token        END      0 "end of file"
%token        ASSIGN     ":="
%token <sval> IDENTIFIER "identifier"
%token <ival> NUMBER     "number"
%type  <ival> exp        "expression"
#line 7531 "../../doc/bison.texinfo"
%printer    { debug_stream () << *$$; } "identifier"
%destructor { delete $$; } "identifier"

%printer    { debug_stream () << $$; } "number" "expression"
#line 7542 "../../doc/bison.texinfo"
%%
%start unit;
unit: assignments exp  { driver.result = $2; };

assignments: assignments assignment {}
           | /* Nothing.  */        {};

assignment: "identifier" ":=" exp { driver.variables[*$1] = $3; };

%left '+' '-';
%left '*' '/';
exp: exp '+' exp   { $$ = $1 + $3; }
   | exp '-' exp   { $$ = $1 - $3; }
   | exp '*' exp   { $$ = $1 * $3; }
   | exp '/' exp   { $$ = $1 / $3; }
   | "identifier"  { $$ = driver.variables[*$1]; }
   | "number"      { $$ = $1; };
%%
#line 7568 "../../doc/bison.texinfo"
void
yy::calcxx_parser::error (const yy::calcxx_parser::location_type& l,
                          const std::string& m)
{
  driver.error (l, m);
}
