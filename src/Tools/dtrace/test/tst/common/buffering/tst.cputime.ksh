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
#ident	"@(#)tst.cputime.ksh	1.1	06/08/28 SMI"

script()
{
	dtrace -C -s /dev/stdin -x bufpolicy=$1 $1 <<EOF

	#pragma D option quiet
	#pragma D option statusrate=1hz

	uint64_t total;
	int thresh;

	BEGIN
	{
		start = timestamp;
		thresh = 10;
	}

	fbt::thread_run:entry
	{
		this->t = (thread_t)arg3;
		this->pid = xlate <psinfo_t>(this->t).pr_pid;
		self->on = (this->pid == \$pid) ? vtimestamp : 0;
	}

	fbt::thread_run:entry
	/self->on/
	{
		total += vtimestamp - self->on;
	}

	tick-100msec
	/i++ == 10/
	{
		exit(0);
	}

	END
	/((total * 100) / (timestamp - start)) > thresh/
	{
		printf("'%s' buffering policy took %d%% of CPU; ",
		    \$\$1, ((total * 100) / (timestamp - start)));
		printf("expected no more than %d%%!\n", thresh);
		exit(1);
	}
EOF
}

for policy in "fill ring switch"; do
	script $policy

	status=$?

	if [ "$status" -ne 0 ]; then
		exit $status
	fi
done

exit 0
