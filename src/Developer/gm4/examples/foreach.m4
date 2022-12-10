divert(-1)
# foreach(x, (item_1, item_2, ..., item_n), stmt)
define(`foreach', `pushdef(`$1', `')_foreach(`$1', `$2', `$3')popdef(`$1')')
define(`_arg1', `$1')
define(`_foreach', 
	`ifelse(`$2', `()', ,
		`define(`$1', _arg1$2)$3`'_foreach(`$1', (shift$2), `$3')')')
# traceon(`define', `foreach', `_foreach', `ifelse')
divert
foreach(`x', `(foo, bar, foobar)', `Word was: x
')
# Something more complex, from Pierre Gaumond <gaumondp@ere.umontreal.ca>.
define(`case', `  $1)
    $2=" -$1";;
')dnl
define(`_cat', `$1$2')dnl
`case' "$1" in
foreach(`x', ((a, vara), (b, varb), (c, varc)), `_cat(`case', x)')dnl
esac
