// To apply, at the top of xnu.git:
// $ spatch --max-width=120 --use-gitgrep --in-place --include-headers --sp-file tools/cocci/remove-cassert.cocci -dir .

@@
expression E;
@@

(
- _CASSERT(E)
+ static_assert(E)
)
