#!/bin/sh -p
dtrace=/usr/sbin/dtrace

############################################################################
# ASSERTION:
#	To verify that a binary launched with dtrace -c fills the $target
#	variable with the process pid
#
#	This relies on the /usr/bin/true being on the file system



$dtrace -xnolibs -c /usr/bin/true -qs /dev/stdin <<EOF
BEGIN
/\$target != 0/
{
	exit(0);
}
BEGIN
/ \$target == 0/
{
	exit(1);
}
EOF

exit $?
