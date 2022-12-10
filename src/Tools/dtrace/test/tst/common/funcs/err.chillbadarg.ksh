#pragma	ident	"@(#)err.chillbadarg.ksh	1.1	06/08/28 SMI"

dtrace_script()
{
	
	$dtrace -w -s /dev/stdin <<EOF
	/*
 	* ASSERTION:
 	*	Verify that chill() refuses args greater than 
	* 	500 milliseconds.
 	*
 	* SECTION: Actions and Subroutines/chill()
 	* 
 	*/

	BEGIN 
	{
		chill(500000001);
		exit(1);
	}

	ERROR
	{
		exit(1)
	}
EOF
}

dtrace="/usr/sbin/dtrace"

dtrace_script &
child=$!

wait $child
status=$?

exit $status
