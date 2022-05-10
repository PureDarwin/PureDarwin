#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License (the "License").
# You may not use this file except in compliance with the License.
#
# You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
# or http://www.opensolaris.org/os/licensing.
# See the License for the specific language governing permissions
# and limitations under the License.
#
# When distributing Covered Code, include this CDDL HEADER in each
# file and include the License file at usr/src/OPENSOLARIS.LICENSE.
# If applicable, add the following below this CDDL HEADER, with the
# fields enclosed by brackets "[]" replaced with your own identifying
# information: Portions Copyright [yyyy] [name of copyright owner]
#
# CDDL HEADER END
#

#
# Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# ident	"@(#)tst.chill.ksh	1.1	06/08/28 SMI"
#

dtrace_script()
{
	
	$dtrace -s /dev/stdin <<EOF

	/*
 	* ASSERTION:
 	*	Positive test of chill()
 	*
 	* SECTION: Actions and Subroutines/chill()
 	* 
 	* NOTES: This test does no verification - it's not possible.  So,
 	* 	we just run this and make sure it runs.
 	*/

	BEGIN 
	{
		i = 0;
	}

	syscall:::entry
	/i <= 5/
	{
		chill($chillns);
		i++;
	}

	syscall:::entry
	/i > 5/
	{
		exit(0);
	}
EOF
}

if [ -f /usr/lib/dtrace/darwin.d ]; then
		chillns=1000
else
		chillns=100000000
fi
dtrace="/usr/sbin/dtrace -w"
dtrace_script &
child=$!

wait $child
status=$?

exit $status
