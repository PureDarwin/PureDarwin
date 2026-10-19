#!/bin/zsh
# runs the CoreGraphics test once at boot, results on the console between CGTEST markers
sleep 3
echo "CGTEST BEGIN"
/usr/local/libexec/cgtests/cg_test /usr/local/libexec/cgtests
echo "CGTEST exit=$?"
echo "CGTEST END"
