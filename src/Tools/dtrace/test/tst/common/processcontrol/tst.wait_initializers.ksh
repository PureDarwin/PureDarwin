#!/bin/sh -p
dtrace=/usr/sbin/dtrace

############################################################################
# ASSERTION:
#	To verify that a binary launched with dtrace -W can trace before main 
#	in initializers
#
#	This relies on the /usr/bin/true being on the file system and libSystem
#	having an initializer called libSystem_initializer.


# Start a subshell that will run /usr/bin/true periodically

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
	pid\$target::function_called_by_initializer:entry
	{
		trace("Called");
		exit(0);
	}
	pid\$target::main_binary_function:entry
	{
		trace("Library initializer not called or called after main");
		exit(1);
	}

EOF
}


script | tee /dev/fd/2 | grep 'Called'
status=$?

kill -TERM $SUBPID

exit $status
