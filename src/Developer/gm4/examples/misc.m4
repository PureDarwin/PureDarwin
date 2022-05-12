divert(-1)
define(`HOST', `vale')
define(`TMP', maketemp(`/tmp/hejXXXXXX'))
syscmd(`ypmatch' HOST `hosts | awk "{print \$1}"'  > TMP)
define(`IP', include(TMP))
syscmd(`rm -f' TMP)
divert

IP
