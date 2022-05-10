#!/bin/sh -p
dtrace=/usr/sbin/dtrace

############################################################################
# ASSERTION:
#	To verify that a binary launched with dtrace -c with the default options
#	is controlled and can start tracing correctly.

script()
{
	$dtrace -xnolibs -c ./tst.has_initializers.exe -qs /dev/stdin <<EOF
	pid\$target::main_binary_function:entry
	{
		trace("Called");
		exit(0);
	}
EOF
}


script | tee /dev/fd/2 | grep 'Called'
status=$?

exit $status
