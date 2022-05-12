define(`reverse', `ifelse(eval($# > 1), 1, `reverse(shift($@)), `$1'', ``$1'')')
``'' => reverse
``hej'' => reverse(hej)
``hej, med, dig'' => reverse(hej, med, dig)
