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
	$dtrace -xnolibs -qs /dev/stdin <<EOF
	profile-1234hz
	/arg0 != 0/
	{
		@[mod(arg0)] = count();
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
		$date > /dev/null
	done
}

if [ -f /usr/lib/dtrace/darwin.d ]; then
date=/bin/date
else
date=/usr/bin/date
fi

dtrace=/usr/sbin/dtrace

spinny &
child=$!

#
# The only thing we can be sure of is that some module named "unix" (or
# "genunix") did some work -- so that's all we'll check.
#
if [ -f /usr/lib/dtrace/darwin.d ]; then
script | tee /dev/fd/2 | grep 'kernel\|mach' > /dev/null
else
script | tee /dev/fd/2 | grep unix > /dev/null
fi
status=$? 

kill $child
exit $status
