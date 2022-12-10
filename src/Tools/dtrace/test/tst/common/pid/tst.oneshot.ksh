#!/bin/sh -p
dtrace=/usr/sbin/dtrace

############################################################################
# ASSERTION:
#	To verify that oneshot probes only fire once
#
# SECTION: oneshot provider
############################################################################

$dtrace -xnolibs -c ./tst.oneshot.exe -qs /dev/stdin <<EOF
oneshot\$target::f:entry {
	hit++;
}
oneshot\$target::f:entry / hit > 1 / {
	exit(1);
}

oneshot\$target::g:entry {
	exit(0);
}
EOF
exit $?
