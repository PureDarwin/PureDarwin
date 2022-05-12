divert(-1)
define(`nargs', `$#')
define(`concat', `ifelse(1, $#, `$1', `$1` 'concat(shift($@))')')
traceon(`concat', `nargs')
divert

nargs
nargs()
nargs(1,2,3,4,5,6)

concat()
concat(`hej', `med', `dig')
concat(`hej', `med', `dig', `en gang igen')
concat(an, awful, lot, of, argument, at, least, more, that, ten, silly, arguments)
