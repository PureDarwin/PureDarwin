define(`countdown', `$1 ifelse(eval($1 > 0), 1, `countdown(decr($1))', `Done')')
