# An ordinary comment
define(`foo', # A comment in a macro
`Macro `foo' expansion')
foo
define(`comment', `*** Macro `comment' expansion ***')
changecom(`@', `@')
foo
