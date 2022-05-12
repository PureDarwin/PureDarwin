changequote([[,]])dnl
define([[quoteall]], [[patsubst([[[[$*]]]], [[,[ 	]+]], [[,]])]])dnl
define([[group]], quoteall(include([[/etc/group]])))dnl
dnl
group()dnl
