define(`countdown', `$1 ifelse(eval($1 > 0), 1, `countdown(decr($1))', `Liftoff')')
debugmode(`aeqc')
traceon(`countdown')
countdown(2)
