# Several input lines, expanding to one
define(`foo', ``foo' line one.
`foo' line two.
`foo' line three.') xyz
foo
# Several input lines, expanding to none
define(`foo', ``foo' line one.
`foo' line two.
`foo' line three.')dnl
# one input line, expanding to several output lines
foo foo
