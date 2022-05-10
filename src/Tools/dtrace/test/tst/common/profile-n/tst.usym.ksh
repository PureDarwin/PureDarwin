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
script()
{
	$dtrace -qs /dev/stdin <<EOF
	profile-1234hz
	/arg1 != 0/
	{
		@[usym(arg1)] = count();
	}

	tick-100ms
	/i++ == 20/
	{
		exit(0);
	}
EOF
}

spinny()
{
	while true; do
		i=$((i+1))
	done
}

dtrace=/usr/sbin/dtrace

spinny &
child=$!

#
# This test is essentially the same as that in the ufunc test; see that
# test for the rationale.
#
if [ -f /usr/lib/dtrace/darwin.d ] ; then
# script | tee /dev/fd/2 | grep '0x9000' > /dev/null
script | tee /dev/fd/2 | grep 'sh`[0-9a-zA-Z_]' > /dev/null
else
script | tee /dev/fd/2 | grep 'sh`[a-zA-Z_]' > /dev/null
fi
status=$? 

kill $child
exit $status
