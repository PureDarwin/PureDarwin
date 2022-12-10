#!/bin/sh



# ASSERTION:
#  Test printf() with a simple string argument.
#
#  SECTION: Output Formatting/printf()
#
#

script()
{
	dtrace -qs /dev/stdin <<EOF
	BEGIN
	{
		printf("symbol = %a", &\`real_ncpus);
		exit(0);
	}
EOF
}

script | tee /dev/fd/2 | grep -E  'kernel.+real_ncpus' > /dev/null

exit $?
