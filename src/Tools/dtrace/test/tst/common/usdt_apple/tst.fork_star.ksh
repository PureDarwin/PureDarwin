#!/bin/sh -p
dtrace=/usr/sbin/dtrace

############################################################################
# ASSERTION:
#	To verify that a process that just forked can be traced right away
#	if a * probe is used


$dtrace -xnolibs -c ./tst.fork_star.exe -qs /dev/stdin <<EOF
test_provider*:::called_after_fork
{
	exit(0)
}
EOF
