dnl
dnl convert to upper- resp. lowercase
define(`upcase', `translit(`$*', `a-z', `A-Z')')
define(`downcase', `translit(`$*', `A-Z', `a-z')')
dnl
dnl capitalize a single word
define(`capitalize1', `regexp(`$1', `^\(\w\)\(\w*\)', `upcase(`\1')`'downcase(`\2')')')
define(`capitalize', `patsubst(`$1', `\w+', ``'capitalize1(`\0')')')
