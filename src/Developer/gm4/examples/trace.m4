divert(-1)

# move(from, to)
define(`move', `Move one disk from `$1' to `$2'.
')

# _hanoi (cnt, from, to, aux)
define(`_hanoi', `ifelse(eval(`$1'<=1), 1, `move($2, $3)',
`_hanoi(decr($1), $2, $4, $3)move($2, $3)_hanoi(decr($1), $4, $3, $2)')')

# hanoi (cnt)
define(`hanoi', `_hanoi(`$1', source, destination, auxilliary)')
divert`'dnl

# Debugmode t
debugmode(`t')
hanoi(2)

# Debugmode taeq
debugmode(`taeq')
hanoi(2)

# Debugmode OFF
debugmode
hanoi(2)

# Debugmode ae
debugmode(`ae')
traceon(`move', `_hanoi')
hanoi(2)
