#!/bin/sh -p
dtrace=/usr/sbin/dtrace

############################################################################
# ASSERTION:
#	To verify that a binary launched with dtrace -c with the default options
#	is controlled and can hit a probe specified by module and function name
#	with glob matching
#

# NOTE:
# We run this with '-Z', because at the time of evaluation, only
# dyld is loaded.

script()
{
	$dtrace -Z -xnolibs -xevaltime=preinit -c ./tst.has_initializers.exe -qs /dev/stdin <<EOF
	pid\$target:test_lib*:function_called_by_initializer:entry
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

exit $status
