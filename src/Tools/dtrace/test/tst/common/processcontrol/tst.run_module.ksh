#!/bin/sh -p
dtrace=/usr/sbin/dtrace

############################################################################
# ASSERTION:
#	To verify that a binary launched with dtrace -c with the default options
#	is controlled and can hit a probe specified by module and function name
#
#	This relies on the /usr/bin/true being on the file system and having a
#	libSystem having a initializer named libSystem_initializer

script()
{
	$dtrace -Z -xnolibs -xevaltime=preinit -c ./tst.has_initializers.exe -qs /dev/stdin <<EOF
	pid\$target:test_lib.dylib:function_called_by_initializer:entry
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
