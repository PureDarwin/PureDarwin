#!/bin/sh -p
dtrace=/usr/sbin/dtrace

############################################################################
# ASSERTION:
#	To verify that the $target variable is set to zero when -c or -W
#	are not used

$dtrace -xnolibs -qs /dev/stdin <<EOF
BEGIN
/\$target == 0/
{
	exit(0);
}
BEGIN
/ \$target != 0/
{
	exit(1);
}
EOF

exit $?
