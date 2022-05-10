#!/bin/sh -p
dtrace=/usr/sbin/dtrace

############################################################################
# ASSERTION:
#	To verify that a binary launched with dtrace -W is controlled and 
#	start tracing correctly.

# Start a subshell that will run tst.has_initializers.exe periodically

(
	while [ 1 -eq 1 ]
	do
		./tst.has_initializers.exe
		sleep 0.01
	done
) &
SUBPID=$!


script()
{
	$dtrace -xnolibs -W tst.has_initializers.exe -qs /dev/stdin <<EOF
	pid\$target::main_binary_function:entry
	{
		trace("Called");
		exit(0);
	}
EOF
}


script | tee /dev/fd/2 | grep 'Called'
status=$?

kill -TERM $SUBPID

exit $status
