#pragma	ident	"@(#)err.copyoutbadaddr.ksh	1.1	06/08/28 SMI"


dtrace_script()
{
	
	$dtrace -w -s /dev/stdin <<EOF

	/*
 	* ASSERTION:
 	*	Verify that copyout() handles bad addresses.
 	*
 	* SECTION: Actions and Subroutines/copyout()
 	* 
 	*/

	BEGIN 
	{
        	ptr = alloca(sizeof (char *));
        	copyinto(curpsinfo->pr_envp, sizeof (char *), ptr);
        	copyout(ptr, 0, sizeof (char *));
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
