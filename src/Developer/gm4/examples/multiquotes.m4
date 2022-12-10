traceon
changequote([,])dnl
changequote([``], [''])dnl
````traceon''''
define(``foo'', ````FOO'''')dnl
dumpdef(``foo'')dnl
changequote(``!'', ``!'')dnl
!foo!
foo
dumpdef(!foo!)dnl
define(!bar!, !BAR!)
bar
changequote(!>*>*>*>*>!, !<*<*<*<*<!)dnl five of each
>*>*>*>*>foo bar<*<*<*<*<
foo bar
>*>*>*>*>*>*><*<*<*<*<*<*<
dumpdef(>*>*>*>*>foo<*<*<*<*<, >*>*>*>*>bar<*<*<*<*<)dnl
